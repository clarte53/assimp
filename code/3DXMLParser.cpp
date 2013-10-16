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

#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER
#ifndef ASSIMP_BUILD_NO_Q3BSP_IMPORTER

#include "3DXMLParser.h"

#include "ParsingUtils.h"
#include "Q3BSPZipArchive.h"

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::XMLReader(Q3BSP::Q3BSPZipArchive* pArchive, const std::string& pFile) : mFileName(pFile), mArchive(pArchive), mStream(NULL), mReader(NULL) {
		Open(pFile);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::~XMLReader() {
		Close();
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLParser::XMLReader::Open(const std::string& pFile) {
		if(mStream == NULL && mReader == NULL) {
			mFileName = pFile;

			// Open the manifest files
			mStream = mArchive->Open(mFileName.c_str());
			if(mStream == NULL) {
				// because Q3BSPZipArchive (now) correctly close all open files automatically on destruction,
				// we do not have to worry about closing the stream explicitly on exceptions

				ThrowException(mFileName + " not found.");
			}

			// generate a XML reader for it
			// the pointer is automatically deleted at the end of the function, even if some exceptions are raised
			boost::scoped_ptr<CIrrXML_IOStreamReader> IOWrapper(new CIrrXML_IOStreamReader(mStream));
			mReader = irr::io::createIrrXMLReader(IOWrapper.get());
			if(mReader == NULL) {
				ThrowException("Unable to create XML reader for file \"" + mFileName + "\".");
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
	std::string _3DXMLParser::XMLReader::GetTextContent() const {
		// present node should be the beginning of an element
		if(mReader->getNodeType() != irr::io::EXN_ELEMENT) {
			ThrowException("the current node is not an xml element.");
		}

		// present node should not be empty
		if(mReader->isEmptyElement()) {
			ThrowException("Can not get content of the empty element \"" + std::string(mReader->getNodeName()) + "\".");
		}

		// read contents of the element
		if(! mReader->read() || mReader->getNodeType() != irr::io::EXN_TEXT) {
			ThrowException("The content of the element \"" + std::string(mReader->getNodeName()) + "\" is not composed of text.");
		}

		// skip leading whitespace
		const char* text = mReader->getNodeData();
		SkipSpacesAndLineEnd(&text);

		if(text == NULL) {
			ThrowException("Invalid content in element \"" + std::string(mReader->getNodeName()) + "\".");
		}

		return text;
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::XMLReader::ThrowException(const std::string& pError) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML XML parser: %s - %s") % mFileName % pError));
	}

	// ------------------------------------------------------------------------------------------------
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& pFile) : mFileName(pFile), mFunctionMap() {
		// Initialize the mapping with the parsing functions
		Initialize();

		// Load the compressed archive
		Q3BSP::Q3BSPZipArchive Archive(pFile);
		if (! Archive.isOpen()) {
			ThrowException("Failed to open file " + pFile + "." );
		}

		// Create a xml parser for the manifest
		XMLReader Manifest(&Archive, "Manifest.xml");

		// Read the name of the main XML file in the manifest
		std::string filename;
		ReadManifest(Manifest, filename);

		// Cleanning up
		Manifest.Close();

		// Create a xml parser for the root file
		XMLReader RootFile(&Archive, filename);

		FunctionMapType::const_iterator parent_it = mFunctionMap.find("");

		if(parent_it != mFunctionMap.end()) {
			ParseElement(RootFile, parent_it->second, "Model_3dxml", NULL);

			if(mContent.has_root_index) {
				std::map<unsigned int, Content::Reference3D>::const_iterator it = mContent.references.find(mContent.root_index);

				if(it != mContent.references.end()) {
					//TODO
				}
			} else {
				// TODO: no root node specifically named -> we must analyze the node structure to find the root or create an artificial root node
			}
		}
	}

	_3DXMLParser::~_3DXMLParser() {

	}
	
	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::ThrowException(const std::string& pError) {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mFileName % pError));
	}
	
	void _3DXMLParser::Initialize() {
		// Saving the mapping between element names and parsing functions
		if(mFunctionMap.size() == 0) {
			#define INSERT(parent, token) mFunctionMap[parent][#token] = std::bind(&_3DXMLParser::Read##token, this, std::placeholders::_1, std::placeholders::_2)

			std::string parent = "";
			INSERT(parent, Model_3dxml);
				
			parent = "Model_3dxml";
			INSERT(parent, Header);
			INSERT(parent, ProductStructure);
			//INSERT(parent, PROCESS);
			//INSERT(parent, DefaultView);
			//INSERT(parent, DELFmiFunctionalModelImplementCnx);
			INSERT(parent, CATMaterialRef);
			//INSERT(parent, CATRepImage);
			INSERT(parent, CATMaterial);
			//INSERT(parent, DELPPRContextModelProcessCnx);
			//INSERT(parent, DELRmiResourceModelImplCnx);

			parent = "ProductStructure";
			INSERT(parent, Reference3D);
			INSERT(parent, Instance3D);
			INSERT(parent, ReferenceRep);
			INSERT(parent, InstanceRep);

			#undef INSERT
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse one element and its sub elements
	void _3DXMLParser::ParseElement(const XMLReader& pReader, const SpecializedMapType& parent, const std::string& name, void* data) {
		if(pReader.IsElement(name)) {
			SpecializedMapType::const_iterator func_it = parent.find(name);

			if(func_it != parent.end()) {
				void* new_data = (func_it->second)(pReader, data);

				while(pReader.Next()) {
					if(pReader.IsElement()) {
						FunctionMapType::const_iterator parent_it = mFunctionMap.find(name);

						if(parent_it != mFunctionMap.end()) {
							ParseElement(pReader, parent_it->second, pReader.GetNodeName(), new_data);
						}
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the name of the main XML file in the Manifest
	void _3DXMLParser::ReadManifest(const XMLReader& pReader, std::string& filename) {
		bool found = false;

		while(! found && pReader.Next()) {
			// handle the root element "Manifest"
			if(pReader.IsElement("Manifest")) {
				while(! found && pReader.Next()) {
					// Read the Root element
					if(pReader.IsElement("Root")) {
						filename = pReader.GetTextContent();
						found = true;
					}
				}
			}
		}

		if(! found) {
			ThrowException("Unable to find the name of the main XML file in the manifest.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read 3DXML file
	void* _3DXMLParser::ReadModel_3dxml(const XMLReader& pReader, void* data) {
		// Nothing to do, everything is already done by ParseElement()

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the header section
	void* _3DXMLParser::ReadHeader(const XMLReader& pReader, void* data) {
		// Nothing to do (who cares about header information anyway)

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the product structure section
	void* _3DXMLParser::ReadProductStructure(const XMLReader& pReader, void* data) {
		XMLReader::Optional<unsigned int> root = pReader.GetAttribute<unsigned int>("root");

		if(root) {
			mContent.root_index = *root;
			mContent.has_root_index = true;
		}

		return data;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterialRef section
	void* _3DXMLParser::ReadCATMaterialRef(const XMLReader& pReader, void* data) {
		//TODO

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterial section
	void* _3DXMLParser::ReadCATMaterial(const XMLReader& pReader, void* data) {
		//TODO

		return data;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the Reference3D section
	void* _3DXMLParser::ReadReference3D(const XMLReader& pReader, void* data) {
		XMLReader::Optional<unsigned int> id = pReader.GetAttribute<unsigned int>("id", true);

		XMLReader::Optional<std::string> name = pReader.GetAttribute<std::string>("name");
		while(pReader.Next()) {
			// handle the name of the element
			if(pReader.IsElement("PLMExternal_ID")) {
				name = XMLReader::Optional<std::string>(pReader.GetTextContent());
			}
		}

		if(mContent.has_root_index && id && mContent.root_index == *id) {
			// This the root node, but it is only a reference (not an instance), so we create a new node which will be the root
			aiNode* node = NULL;

			if(name) {
				node = new aiNode(*name);
			} else {
				std::stringstream name;
				name << *id; // no need to worry about id -> id is mandatory and if not present an exception has already been raised

				node = new aiNode(name.str());
			}

			mContent.scene->mRootNode = node;
		}

		//TODO

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the Instance3D section
	void* _3DXMLParser::ReadInstance3D(const XMLReader& pReader, void* data) {
		XMLReader::Optional<unsigned int> id = pReader.GetAttribute<unsigned int>("id", true);

		//TODO

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the ReferenceRep section
	void* _3DXMLParser::ReadReferenceRep(const XMLReader& pReader, void* data) {
		XMLReader::Optional<unsigned int> id = pReader.GetAttribute<unsigned int>("id", true);

		//TODO

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void* _3DXMLParser::ReadInstanceRep(const XMLReader& pReader, void* data) {
		XMLReader::Optional<unsigned int> id = pReader.GetAttribute<unsigned int>("id", true);

		//TODO

		return data;
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
