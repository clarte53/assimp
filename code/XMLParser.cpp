/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2013, assimp team
All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the
following conditions are met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other
materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
contributors may be used to endorse or promote products
derived from this software without specific prior
written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/

/** @file XMLParser.cpp
 *  @brief Implementation of the XML parser helper class
 */

#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER
#ifndef ASSIMP_BUILD_NO_Q3BSP_IMPORTER

#include "XMLParser.h"

#include "Q3BSPZipArchive.h"

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	const unsigned int XMLParser::XSD::unbounded = std::numeric_limits<unsigned int>::max();

	// ------------------------------------------------------------------------------------------------
	void XMLParser::OpenInArchive(const std::string& file) { PROFILER;
		if(mArchive->Exists(file.c_str())) { PROFILER;
			// Open the manifest files
			mStream = mArchive->Open(file.c_str());
			if(mStream == nullptr) {
				// because Q3BSPZipArchive (now) correctly close all open files automatically on destruction,
				// we do not have to worry about closing the stream explicitly on exceptions

				ThrowException(file + " not found.");
			}

			PROFILER;

			// generate a XML reader for it
			// the pointer is automatically deleted at the end of the function, even if some exceptions are raised
			std::unique_ptr<CIrrXML_IOStreamReader> IOWrapper(new CIrrXML_IOStreamReader(mStream));

			PROFILER;

			mReader = irr::io::createIrrXMLReader(IOWrapper.get());
			if(mReader == nullptr) {
				ThrowException("Unable to create XML parser for file \"" + file + "\".");
			}

			mFileName = file;
		} else {
			ThrowException("The file \"" + file + "\" does not exist in the zip archive.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	XMLParser::XMLParser(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& file) : mFileName(""), mArchive(archive), mStream(nullptr), mReader(nullptr) { PROFILER;
		Open(file);
	}

	// ------------------------------------------------------------------------------------------------
	XMLParser::~XMLParser() { PROFILER;
		Close();
	}

	// ------------------------------------------------------------------------------------------------
	void XMLParser::Open(const std::string& file) { PROFILER;
		if(mStream == nullptr && mReader == nullptr) {
			if(mArchive->isOpen()) { PROFILER;
				try {
					OpenInArchive(file);
				} catch(...) {
					// Try with a different filename encoding
					std::string filename = file;
					BaseImporter::ConvertUTF8toISO8859_1(filename);

					OpenInArchive(filename);
				}
			} else {
				ThrowException("The zip archive can not be opened.");
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void XMLParser::Close() { PROFILER;
		if(mStream != nullptr) {
			mArchive->Close(mStream);
			mStream = nullptr;
		}

		if(mReader != nullptr) {
			delete mReader;
			mReader = nullptr;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void XMLParser::ThrowException(const std::string& error) const { PROFILER;
		throw DeadlyImportError(boost::str(boost::format("XML parser: %s - %s") % mFileName % error));
	}

	// ------------------------------------------------------------------------------------------------
	// Skip an element
	void XMLParser::SkipElement() const { PROFILER;
		SkipUntilEnd(mReader->getNodeName());
	}

	// ------------------------------------------------------------------------------------------------
	// Skip recursively until the end of element "name"
	void XMLParser::SkipUntilEnd(const std::string& name) const { PROFILER;
		irr::io::EXML_NODE node_type = mReader->getNodeType();
		bool is_same_name = name.compare(mReader->getNodeName()) == 0;
		unsigned int depth = 0;

		// Are we already on the ending element or an <element />?
		if(node_type != irr::io::EXN_UNKNOWN && ! mReader->isEmptyElement() && (node_type != irr::io::EXN_ELEMENT_END || ! is_same_name)) {
			// If not, parse the next elements...
			while(mReader->read()) {
				node_type = mReader->getNodeType();
				is_same_name = name.compare(mReader->getNodeName()) == 0;

				// ...recursively...
				if(node_type == irr::io::EXN_ELEMENT && is_same_name) {
					depth++;
				} else if(node_type == irr::io::EXN_ELEMENT_END && is_same_name) {
					// ...until we find the corresponding ending element
					if(depth == 0) {
						break;
					} else {
						depth--;
					}
				}
			}
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	template<>
	std::string XMLParser::ToString(const std::string& value) const { PROFILER;
		return value;
	}

	// ------------------------------------------------------------------------------------------------
	template<>
	std::string XMLParser::FromString(const std::string& string) const { PROFILER;
		return string;
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
