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

/** @file 3DXMLParser.cpp
 *  @brief Implementation of the 3DXML parser helper
 */

#include "3DXMLParser.h"

#ifndef ASSIMP_BUILD_NO_Q3BSP_IMPORTER

#include "ParsingUtils.h"
#include "Q3BSPZipArchive.h"

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::XMLReader(Q3BSP::Q3BSPZipArchive* pArchive, const std::string& pFile) : mArchive(pArchive), mStream(NULL), mReader(NULL) {
		Open(pFile);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::~XMLReader() {
		Close();
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLParser::XMLReader::Open(const std::string& pFile) {
		if(mStream == NULL && mReader == NULL) {
			// Open the manifest files
			mStream = mArchive->Open(pFile.c_str());
			if(mStream == NULL) {
				// because Q3BSPZipArchive (now) correctly close all open files automatically on destruction,
				// we do not have to worry about closing the stream explicitly on exceptions

				throw DeadlyImportError("3DXML: " + pFile + " not found.");
			}

			// generate a XML reader for it
			// the pointer is automatically deleted at the end of the function, even if some exceptions are raised
			boost::scoped_ptr<CIrrXML_IOStreamReader> IOWrapper(new CIrrXML_IOStreamReader(mStream));
			mReader = irr::io::createIrrXMLReader(IOWrapper.get());
			if(mReader == NULL) {
				throw DeadlyImportError("3DXML: Unable to create XML reader for file \"" + pFile + "\".");
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLParser::XMLReader::Close() {
		if(mStream != NULL && mReader != NULL) {
			delete mReader;
			mReader = NULL;

			mArchive->Close(mStream);
			mStream = NULL;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Reads the text contents of an element, throws an exception if not given. Skips leading whitespace.
	std::string _3DXMLParser::XMLReader::GetTextContent() {
		// present node should be the beginning of an element
		if(mReader->getNodeType() != irr::io::EXN_ELEMENT) {
			throw DeadlyImportError("3DXML: the current node is not an xml element.");
		}

		// present node should not be empty
		if(mReader->isEmptyElement()) {
			throw DeadlyImportError("3DXML: Can not get content of the empty element \"" + std::string(mReader->getNodeName()) + "\".");
		}

		// read contents of the element
		if(! mReader->read() || mReader->getNodeType() != irr::io::EXN_TEXT) {
			throw DeadlyImportError("3DXML: The content of the element \"" + std::string(mReader->getNodeName()) + "\" is not composed of text.");
		}

		// skip leading whitespace
		const char* text = mReader->getNodeData();
		SkipSpacesAndLineEnd(&text);

		if(text == NULL) {
			throw DeadlyImportError("3DXML: Invalid content in element \"" + std::string(mReader->getNodeName()) + "\".");
		}

		return text;
	}

	// ------------------------------------------------------------------------------------------------
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& pFile) : mFileName(pFile), mRootFileName("") {
		// Load the compressed archive
		Q3BSP::Q3BSPZipArchive Archive(pFile);
		if (! Archive.isOpen()) {
			throw DeadlyImportError( "Failed to open file " + pFile + "." );
		}

		XMLReader Manifest(&Archive, "Manifest.xml");

		// Read the name of the main XML file in the manifest
		ReadManifest(Manifest);

		std::cerr << "3DXML main file: " << mRootFileName << std::endl;

		// Cleanning up
		Manifest.Close();
	}

	_3DXMLParser::~_3DXMLParser() {

	}
	
	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::ThrowException(const std::string& pError) {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mFileName % pError));
	}

	// ------------------------------------------------------------------------------------------------
	// Read the name of the main XML file in the Manifest
	void _3DXMLParser::ReadManifest(XMLReader& pReader) {
		bool found = false;

		while(! found && pReader.Next()) {
			// handle the root element "Manifest"
			if(pReader.IsElement("Manifest")) {
				while(! found && pReader.Next()) {
					// Read the Root element
					if(pReader.IsElement("Root")) {
						mRootFileName = pReader.GetTextContent();
						found = true;
					}
				}
			}
		}

		if(! found) {
			ThrowException("Unable to find the name of the main XML file in the manifest.");
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
