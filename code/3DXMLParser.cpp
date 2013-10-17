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

#include <algorithm>
#include <cctype>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::XMLReader(Q3BSP::Q3BSPZipArchive* archive, const std::string& file) : mFileName(file), mArchive(archive), mStream(NULL), mReader(NULL) {
		Open(file);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::~XMLReader() {
		Close();
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLParser::XMLReader::Open(const std::string& file) {
		if(mStream == NULL && mReader == NULL) {
			mFileName = file;

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
	// Aborts the file reading with an exception
	void _3DXMLParser::XMLReader::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML XML parser: %s - %s") % mFileName % error));
	}

	// ------------------------------------------------------------------------------------------------
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& file) : mCurrentFile(file), mFunctionMap() {
		// Initialize the mapping with the parsing functions
		Initialize();

		// Load the compressed archive
		Q3BSP::Q3BSPZipArchive archive(mCurrentFile);
		if (! archive.isOpen()) {
			ThrowException("Failed to open file " + mCurrentFile + "." );
		}

		// Create a xml parser for the manifest
		XMLReader manifest(&archive, "Manifest.xml");

		// Read the name of the main XML file in the manifest
		ReadManifest(manifest);

		// Cleanning up
		manifest.Close();

		FunctionMapType::const_iterator parent_it = mFunctionMap.find("");

		if(parent_it != mFunctionMap.end()) {
			// Save the name of the main 3DXML file
			std::string main_file = mCurrentFile;

			// Create a xml parser for the root file
			XMLReader root_file(&archive, mCurrentFile);

			// Parse the main 3DXML file
			ParseElement(root_file, parent_it->second, "Model_3dxml", NULL);

			// Cleanning up
			root_file.Close();

			// Parse other referenced 3DXML files until all references are resolved
			while(mContent.files_to_parse.size() != 0) {
				std::set<std::string>::iterator it = mContent.files_to_parse.begin();

				if(it != mContent.files_to_parse.end()) {
					// Set the name of the current file
					mCurrentFile = *it;

					// Create a xml parser for the file
					XMLReader file(&archive, mCurrentFile);

					// Parse the 3DXML file
					ParseElement(file, parent_it->second, "Model_3dxml", NULL);


					// Remove the file from the list of files to parse
					it = mContent.files_to_parse.erase(it);

					// Cleanning up
					file.Close();
				}
			}

			if(mContent.has_root_index) {
				std::map<Content::ID, Content::Reference3D>::const_iterator it = mContent.references.find(Content::ID(main_file, mContent.root_index));

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
	void _3DXMLParser::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mCurrentFile % error));
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
	void _3DXMLParser::ParseElement(const XMLReader& reader, const SpecializedMapType& parent, const std::string& name, void* data) {
		if(reader.IsElement(name)) {
			SpecializedMapType::const_iterator func_it = parent.find(name);

			if(func_it != parent.end()) {
				void* new_data = (func_it->second)(reader, data);

				while(reader.Next()) {
					if(reader.IsElement()) {
						FunctionMapType::const_iterator parent_it = mFunctionMap.find(name);

						if(parent_it != mFunctionMap.end()) {
							ParseElement(reader, parent_it->second, reader.GetNodeName(), new_data);
						}
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Parse one uri and split it into it's different components
	void _3DXMLParser::ParseURI(const std::string& uri, Content::URI& result) const {
		static const unsigned int size_prefix = 10;

		if(uri.substr(0, size_prefix).compare("urn:3DXML:") == 0) {
			result.external = true;

			std::size_t delim = uri.find_last_of('#');

			if(delim != uri.npos) {
				result.has_id = true;

				std::string id = uri.substr(delim + 1, uri.npos);
				ParseID(id, result.id);

				result.filename = uri.substr(size_prefix, delim - 1);
			} else {
				result.has_id = false;
				result.id = 0;
				result.filename = uri.substr(size_prefix, uri.npos);
			}
		} else if(std::count_if(uri.begin(), uri.end(), std::isdigit) == uri.size()) {
			result.external = false;
			result.has_id = true;
			result.filename = mCurrentFile;

			ParseID(uri, result.id);
		} else {
			ThrowException("The URI \"" + uri + "\" has an invalid format.");
		}

		ParseExtension(result.filename, result.extension);
	}

	// ------------------------------------------------------------------------------------------------
	// Parse one the extension part of a filename
	void _3DXMLParser::ParseExtension(const std::string& filename, std::string& extension) {
		std::size_t pos = filename.find_last_of('.');

		if(pos != filename.npos) {
			extension = filename.substr(pos + 1, filename.npos);
		} else {
			extension = "";
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Parse one numerical id from a string
	void _3DXMLParser::ParseID(const std::string& data, unsigned int& id) {
		std::istringstream stream(data);

		stream >> id;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the name of the main XML file in the Manifest
	void _3DXMLParser::ReadManifest(const XMLReader& reader) {
		bool found = false;

		while(! found && reader.Next()) {
			// handle the root element "Manifest"
			if(reader.IsElement("Manifest")) {
				while(! found && reader.Next()) {
					// Read the Root element
					if(reader.IsElement("Root")) {
						mCurrentFile = *(reader.GetContent<std::string>(true));
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
	void* _3DXMLParser::ReadModel_3dxml(const XMLReader& reader, void* data) {
		// Nothing specific to do, everything is already done by ParseElement()

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the header section
	void* _3DXMLParser::ReadHeader(const XMLReader& reader, void* data) {
		// Nothing to do (who cares about header information anyway)

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the product structure section
	void* _3DXMLParser::ReadProductStructure(const XMLReader& reader, void* data) {
		XMLReader::Optional<unsigned int> root = reader.GetAttribute<unsigned int>("root");

		if(root) {
			mContent.root_index = *root;
			mContent.has_root_index = true;
		}

		return data;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterialRef section
	void* _3DXMLParser::ReadCATMaterialRef(const XMLReader& reader, void* data) {
		//TODO

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterial section
	void* _3DXMLParser::ReadCATMaterial(const XMLReader& reader, void* data) {
		//TODO

		return data;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the Reference3D section
	void* _3DXMLParser::ReadReference3D(const XMLReader& reader, void* data) {
		// no need to worry about id -> id is mandatory and if not present an exception has already been raised
		unsigned int id = *(reader.GetAttribute<unsigned int>("id", true));
		std::string name;

		// Parse the name of this reference
		XMLReader::Optional<std::string> name_opt = reader.GetAttribute<std::string>("name");
		while(reader.Next()) {
			// handle the name of the element
			if(reader.IsElement("PLMExternal_ID")) {
				name_opt = reader.GetContent<std::string>(true);
				break;
			}
		}

		// Test if the name exist, otherwise use the id as name
		if(name_opt) {
			name = *name_opt;
		} else {
			// No name: take the id as the name
			ToString(id, name);
		}

		mContent.references[Content::ID(mCurrentFile, id)].name = name; // Create the Reference3D if not present and asign the name of this reference.
		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The Reference3D will be completed by the Instance3D and InstanceRep

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the Instance3D section
	void* _3DXMLParser::ReadInstance3D(const XMLReader& reader, void* data) {
		// no need to worry about id -> id is mandatory and if not present an exception has already been raised
		unsigned int id = *(reader.GetAttribute<unsigned int>("id", true));
		unsigned int aggregated_by;
		Content::URI instance_of;
		std::string relative_matrix;
		std::string name;

		// Parse the sub elements of this node
		XMLReader::Optional<std::string> name_opt = reader.GetAttribute<std::string>("name");
		while(reader.Next()) {
			// handle the name of the element
			if(reader.IsElement("PLMExternal_ID")) {
				name_opt = reader.GetContent<std::string>(true);
			} else if(reader.IsElement("IsAggregatedBy")) {
				aggregated_by = *(reader.GetContent<unsigned int>(true));
			} else if(reader.IsElement("IsInstanceOf")) {
				std::string uri = *(reader.GetContent<std::string>(true));

				ParseURI(uri, instance_of);
			} else if(reader.IsElement("RelativeMatrix")) {
				relative_matrix = *(reader.GetContent<std::string>(true));
			}
		}

		// Test if the name exist, otherwise use the id as name
		if(name_opt) {
			name = *name_opt;
		} else {
			// No name: take the id as the name
			ToString(id, name);
		}

		// Create the associated node
		aiNode* node = new aiNode(name);
		aiMatrix4x4& transformation = node->mTransformation;

		// Save the transformation matrix
		std::istringstream matrix(relative_matrix);
		matrix
			>> transformation.a1 >> transformation.a2 >> transformation.a3
			>> transformation.b1 >> transformation.b2 >> transformation.b3
			>> transformation.c1 >> transformation.c2 >> transformation.c3
			>> transformation.a4 >> transformation.b4 >> transformation.c4;
		transformation.d1 = transformation.d2 = transformation.d3 = 0.0;
		transformation.d4 = 1.0;

		// If the reference is on another file and does not already exist, add it to the list of files to parse
		if(instance_of.external && instance_of.has_id &&
				instance_of.filename.compare(mCurrentFile) != 0 &&
				mContent.references.find(Content::ID(instance_of.filename, instance_of.id)) == mContent.references.end()) {

			mContent.files_to_parse.insert(instance_of.filename);
		}

		// Save the reference to the parent Reference3D
		Content::Reference3D& parent = mContent.references[Content::ID(mCurrentFile, aggregated_by)];
		Content::Instance3D& instance = parent.instances[Content::ID(mCurrentFile, id)];

		// Save the information corresponding to this instance
		instance.node = node;
		if(instance_of.has_id) {
			// Create the refered Reference3D if necessary
			instance.instance_of = &(mContent.references[Content::ID(instance_of.filename, instance_of.id)]);
		} else {
			ThrowException("The Instance3D \"" + name + "\" refers to an invalid reference without id.");
		}

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the ReferenceRep section
	void* _3DXMLParser::ReadReferenceRep(const XMLReader& reader, void* data) {
		// no need to worry about id -> id is mandatory and if not present an exception has already been raised
		unsigned int id = *(reader.GetAttribute<unsigned int>("id", true));

		//TODO

		return data;
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void* _3DXMLParser::ReadInstanceRep(const XMLReader& reader, void* data) {
		// no need to worry about id -> id is mandatory and if not present an exception has already been raised
		unsigned int id = *(reader.GetAttribute<unsigned int>("id", true));

		//TODO

		return data;
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
