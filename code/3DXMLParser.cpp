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

	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& pFile) : mFileName(pFile), mReader(NULL) {
		// Load the compressed archive
		Q3BSP::Q3BSPZipArchive Archive(pFile);
		if (! Archive.isOpen()) {
			throw DeadlyImportError( "Failed to open file " + pFile + "." );
		}

		// Open the manifest files
		IOStream* stream(Archive.Open("Manifest.xml"));
		if(stream == NULL) {
			ThrowException("Manifest.xml not found.");
		}

		// generate a XML reader for it
		boost::scoped_ptr<CIrrXML_IOStreamReader> IOWrapper(new CIrrXML_IOStreamReader(stream));
		mReader = irr::io::createIrrXMLReader(IOWrapper.get());
		if(mReader == NULL) {
			// we have to do some minimal manual cleanning, the rest is already taken care of
			Archive.Close(stream);

			ThrowException( "Unable to create XML reader for Manifest.xml.");
		}

		// Read the name of the main XML file in the manifest
		std::string mainFile = "";
		getMainFile(mainFile);
		if(mainFile == "") {
			// we have to do some minimal manual cleanning, the rest is already taken care of
			Archive.Close(stream);

			ThrowException( "Unable to find the name of the main XML file in Manifest.xml.");
		}

		delete mReader;
		mReader = NULL;

		Archive.Close(stream);
	}

	_3DXMLParser::~_3DXMLParser() {
		if(mReader != NULL) {
			delete mReader;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::ThrowException(const std::string& pError) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mFileName % pError));
	}

	// ------------------------------------------------------------------------------------------------
	// Reads the text contents of an element, returns NULL if not given. Skips leading whitespace.
	const char* _3DXMLParser::TestTextContent(std::string* pElementName) {
		// present node should be the beginning of an element
		if(mReader->getNodeType() != irr::io::EXN_ELEMENT) {
			return NULL;
		}

		*pElementName = mReader->getNodeName();

		// present node should not be empty
		if(mReader->isEmptyElement()) {
			return NULL;
		}

		// read contents of the element
		if(! mReader->read() || mReader->getNodeType() != irr::io::EXN_TEXT) {
			return NULL;
		}

		// skip leading whitespace
		const char* text = mReader->getNodeData();
		SkipSpacesAndLineEnd(&text);

		return text;
	}

	// ------------------------------------------------------------------------------------------------
	// Reads the text contents of an element, throws an exception if not given. Skips leading whitespace.
	const char* _3DXMLParser::GetTextContent() {
		std::string elementName;
		const char* text = TestTextContent(&elementName);

		if(! text) {
			ThrowException("Invalid contents in element \"" + elementName + "\".");
		}

		return text;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the name of the main XML file in the Manifest
	void _3DXMLParser::getMainFile(std::string& pFile) throw() {
		bool found = false;

		while(! found && mReader->read()) {
			// handle the root element "Manifest"
			if(mReader->getNodeType() == irr::io::EXN_ELEMENT && IsElement("Manifest")) {
				while(! found && mReader->read()) {
					// Read the Root element
					if(mReader->getNodeType() == irr::io::EXN_ELEMENT && IsElement("Root")) {
						pFile = GetTextContent();
						found = true;
					}
				}
			}
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
