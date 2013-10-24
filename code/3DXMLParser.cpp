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

#include "3DXMLRepresentation.h"
#include "ParsingUtils.h"
#include "Q3BSPZipArchive.h"
#include "SceneCombiner.h"

#include <algorithm>
#include <cctype>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLParser::XMLReader::XMLReader(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& file) : mFileName(file), mArchive(archive), mStream(NULL), mReader(NULL) {
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
			mStream = ScopeGuard<IOStream>(mArchive->Open(mFileName.c_str()));
			if(mStream == NULL) {
				// because Q3BSPZipArchive (now) correctly close all open files automatically on destruction,
				// we do not have to worry about closing the stream explicitly on exceptions

				ThrowException(mFileName + " not found.");
			}

			// generate a XML reader for it
			// the pointer is automatically deleted at the end of the function, even if some exceptions are raised
			ScopeGuard<CIrrXML_IOStreamReader> IOWrapper(new CIrrXML_IOStreamReader(mStream));
			mReader = ScopeGuard<irr::io::IrrXMLReader>(irr::io::createIrrXMLReader(IOWrapper.get()));
			if(mReader == NULL) {
				ThrowException("Unable to create XML reader for file \"" + mFileName + "\".");
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLParser::XMLReader::Close() {
		mArchive->Close(mStream.dismiss());
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::XMLReader::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML parser: %s - %s") % mFileName % error));
	}

	// ------------------------------------------------------------------------------------------------
	// Skip an element
	void _3DXMLParser::XMLReader::SkipElement() const {
		SkipUntilEnd(mReader->getNodeName());
	}

	// ------------------------------------------------------------------------------------------------
	// Skip recursively until the end of element "name"
	void _3DXMLParser::XMLReader::SkipUntilEnd(const std::string& name) const {
		irr::io::EXML_NODE node_type = mReader->getNodeType();
		bool is_same_name = name.compare(mReader->getNodeName()) == 0;
		unsigned int depth = 0;

		// Are we already on the ending element or an <element />?
		if(! mReader->isEmptyElement() && (node_type != irr::io::EXN_ELEMENT_END || ! is_same_name)) {
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
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& file, aiScene* scene) : mArchive(new Q3BSP::Q3BSPZipArchive(file)), mReader(NULL), mContent(scene) {
		// Load the compressed archive
		if (! mArchive->isOpen()) {
			ThrowException("Failed to open file " + file + "." );
		}

		// Create a xml parser for the manifest
		mReader = ScopeGuard<XMLReader>(new XMLReader(mArchive, "Manifest.xml"));

		// Read the name of the main XML file in the manifest
		std::string main_file;
		ReadManifest(main_file);

		// Create a xml parser for the root file
		mReader = ScopeGuard<XMLReader>(new XMLReader(mArchive, main_file));

		// Parse the main 3DXML file
		ParseFile();

		// Parse other referenced 3DXML files until all references are resolved
		while(mContent.files_to_parse.size() != 0) {
			std::set<std::string>::const_iterator it = mContent.files_to_parse.begin();

			if(it != mContent.files_to_parse.end()) {
				// Create a xml parser for the file
				mReader = ScopeGuard<XMLReader>(new XMLReader(mArchive, *it));

				// Parse the 3DXML file
				ParseFile();

				// Remove the file from the list of files to parse
				it = mContent.files_to_parse.erase(it);
			}
		}
			
		// Add the meshes into the scene
		for(std::map<Content::ID, Content::ReferenceRep>::iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			it_rep->second.index_begin = mContent.scene->Meshes.Size();
			it_rep->second.index_end = it_rep->second.index_begin + it_rep->second.meshes.size() - 1;

			unsigned int index = mContent.scene->Meshes.Size();
			for(std::list<ScopeGuard<aiMesh>>::iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
				// Realease the ownership of the mesh to the protected scene
				mContent.scene->Meshes.Set(index++, it_mesh->dismiss());
			}
		}

		// Create the root node
		if(mContent.has_root_index) {
			std::map<Content::ID, Content::Reference3D>::iterator it_root = mContent.references.find(Content::ID(main_file, mContent.root_index));

			if(it_root != mContent.references.end()) {
				Content::Reference3D& root = it_root->second;

				if(root.nb_references == 0) {
					// mRootNode is contained inside the scene, which is already protected against memory leaks
					mContent.scene->mRootNode = new aiNode(root.name);

					// Build the hierarchy recursively
					BuildStructure(root, mContent.scene->mRootNode);
				} else {
					ThrowException("The root Reference3D should not be instantiated.");
				}
			} else {
				ThrowException("Unresolved root Reference3D \"" + mReader->ToString(mContent.root_index) + "\".");
			}
		} else {
			// TODO: no root node specifically named -> we must analyze the node structure to find the root or create an artificial root node
			ThrowException("No root Reference3D specified.");
		}
	}

	_3DXMLParser::~_3DXMLParser() {

	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader->GetFilename() % error));
	}

	// ------------------------------------------------------------------------------------------------
	// Parse a 3DXML file
	void _3DXMLParser::ParseFile() {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse Header element
			map.insert(std::make_pair("Header", [](Params& params){params.me->ReadHeader();}));

			// Parse ProductStructure element
			map.insert(std::make_pair("ProductStructure", [](Params& params){params.me->ReadProductStructure();}));

			//TODO PROCESS
			//TODO DefaultView
			//TODO DELFmiFunctionalModelImplementCnx

			// Parse CATMaterialRef element
			map.insert(std::make_pair("CATMaterialRef", [](Params& params){params.me->ReadCATMaterialRef();}));

			//TODO CATRepImage

			// Parse CATMaterial element
			map.insert(std::make_pair("CATMaterial", [](Params& params){params.me->ReadCATMaterial();}));

			//TODO DELPPRContextModelProcessCnx
			//TODO DELRmiResourceModelImplCnx
			
			return map;
		})());


		params.me = this;

		// Parse the main 3DXML file
		while(mReader->Next()) {
			if(mReader->IsElement("Model_3dxml")) {
				mReader->ParseNode(mapping, params);
			} else {
				mReader->SkipElement();
			}
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse one uri and split it into it's different components
	void _3DXMLParser::ParseURI(const std::string& uri, Content::URI& result) const {
		static const unsigned int size_prefix = 10;

		result.uri = uri;

		if(uri.substr(0, size_prefix).compare("urn:3DXML:") == 0) {
			result.external = true;

			std::size_t begin = uri.find_last_of(':');
			std::size_t end = uri.find_last_of('#');

			if(begin == uri.npos) {
				ThrowException("The URI \"" + uri + "\" has an invalid format.");
			}

			if(end != uri.npos) {
				result.has_id = true;

				std::string id = uri.substr(end + 1, uri.npos);
				ParseID(id, result.id);

				result.filename = uri.substr(begin + 1, end - (begin + 1));
			} else {
				result.has_id = false;
				result.id = 0;
				result.filename = uri.substr(begin + 1, uri.npos);
			}
		} else if(std::count_if(uri.begin(), uri.end(), std::isdigit) == uri.size()) {
			result.external = false;
			result.has_id = true;
			result.filename = mReader->GetFilename();

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
	// Add the meshes indices and children nodes into the given node recursively
	void _3DXMLParser::BuildStructure(Content::Reference3D& ref, aiNode* node) const {
		if(node != NULL) {
			// Copy the indexes of the meshes contained into this instance into the proper aiNode
			if(node->Meshes.Size() == 0) {
				for(std::map<Content::ID, Content::InstanceRep>::const_iterator it_mesh(ref.meshes.begin()), end_mesh(ref.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
					const Content::InstanceRep& rep = it_mesh->second;

					if(rep.instance_of != NULL) {
						for(unsigned int i = node->Meshes.Size(), index = rep.instance_of->index_begin; index <= rep.instance_of->index_end; i++, index++) {
							node->Meshes.Set(i, index);
						}
					} else {
						ThrowException("One InstanceRep of Reference3D \"" + mReader->ToString(ref.id) + "\" is unresolved.");
					}
				}
			}

			// Copy the children nodes of this instance into the proper node
			if(node->Children.Size() == 0) {
				for(std::map<Content::ID, Content::Instance3D>::iterator it_child(ref.instances.begin()), end_child(ref.instances.end()); it_child != end_child; ++ it_child) {
					Content::Instance3D& child = it_child->second;

					if(child.node.get() != NULL && child.instance_of != NULL) {
						// Construct the hierarchy recursively
						BuildStructure(*child.instance_of, child.node.get());

						// Decrement the counter of instances to this Reference3D (used for memory managment)
						if(ref.nb_references > 0) {
							ref.nb_references--;
						}

						// If the counter of references is null, this mean this instance is the last instance of this Reference3D
						if(ref.nb_references == 0) {
							// Therefore we can copy the child node directly into the children array
							child.node->mParent = node;
							node->Children.Set(node->Children.Size(), child.node.dismiss());
						} else {
							// Otherwise we need to make a deep copy of the child node in order to avoid duplicate nodes in the scene hierarchy
							// (which would cause assimp to deallocate them multiple times, therefore making the application crash)
							aiNode* copy_node = NULL;

							SceneCombiner::Copy(&copy_node, child.node);

							copy_node->mParent = node;
							node->Children.Set(node->Children.Size(), copy_node);
						}
					} else {
						ThrowException("One Instance3D of Reference3D \"" + mReader->ToString(ref.id) + "\" is unresolved.");
					}
				}
			}
		} else {
			ThrowException("Invalid Instance3D of Reference3D \"" + mReader->ToString(ref.id) + "\" with null aiNode.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the name of the main XML file in the Manifest
	void _3DXMLParser::ReadManifest(std::string& main_file) {
		struct Params {
			_3DXMLParser* me;
			std::string* file;
			bool found;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse Root element
			map.insert(std::make_pair("Root", [](Params& params){
				*params.file = *(params.me->mReader->GetContent<std::string>(true));
				params.found = true;
			}));
			
			return map;
		})());

		params.me = this;
		params.file = &main_file;
		params.found = false;

		while(! params.found && mReader->Next()) {
			// handle the root element "Manifest"
			if(mReader->IsElement("Manifest")) {
				mReader->ParseNode(mapping, params);
			} else {
				mReader->SkipElement();
			}
		}

		if(! params.found) {
			ThrowException("Unable to find the name of the main XML file in the manifest.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read 3DXML file
	void _3DXMLParser::ReadModel_3dxml() {
		// Nothing specific to do, everything is already done by ParseElement()
	}

	// ------------------------------------------------------------------------------------------------
	// Read the header section
	void _3DXMLParser::ReadHeader() {
		// Nothing to do (who cares about header information anyway)
	}

	// ------------------------------------------------------------------------------------------------
	// Read the product structure section
	void _3DXMLParser::ReadProductStructure() {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse Reference3D element
			map.insert(std::make_pair("Reference3D", [](Params& params){params.me->ReadReference3D();}));

			// Parse Instance3D element
			map.insert(std::make_pair("Instance3D", [](Params& params){params.me->ReadInstance3D();}));

			// Parse ReferenceRep element
			map.insert(std::make_pair("ReferenceRep", [](Params& params){params.me->ReadReferenceRep();}));

			// Parse InstanceRep element
			map.insert(std::make_pair("InstanceRep", [](Params& params){params.me->ReadInstanceRep();}));
			
			return map;
		})());

		XMLReader::Optional<unsigned int> root = mReader->GetAttribute<unsigned int>("root");

		if(root) {
			mContent.root_index = *root;
			mContent.has_root_index = true;
		}

		params.me = this;

		mReader->ParseNode(mapping, params);
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterialRef section
	void _3DXMLParser::ReadCATMaterialRef() {
		//TODO
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterial section
	void _3DXMLParser::ReadCATMaterial() {
		//TODO
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the Reference3D section
	void _3DXMLParser::ReadReference3D() {
		struct Params {
			_3DXMLParser* me;
			XMLReader::Optional<std::string> name_opt;
			unsigned int id;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse PLM_ExternalID element
			map.insert(std::make_pair("PLM_ExternalID", [](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}));
			
			return map;
		})());

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));

		mReader->ParseNode(mapping, params);

		Content::Reference3D& ref = mContent.references[Content::ID(mReader->GetFilename(), params.id)]; // Create the Reference3D if not present.
				
		// Save id and name for future error / log messages
		ref.id = params.id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			ref.name = *(params.name_opt);
		} else {
			// No name: take the id as the name
			ref.name = mReader->ToString(params.id);
		}

		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The Reference3D will be completed by the Instance3D and InstanceRep
	}

	// ------------------------------------------------------------------------------------------------
	// Read the Instance3D section
	void _3DXMLParser::ReadInstance3D() {
		struct Params {
			_3DXMLParser* me;
			XMLReader::Optional<std::string> name_opt;
			Content::Instance3D instance;
			Content::URI instance_of;
			unsigned int aggregated_by;
			bool has_aggregated_by;
			bool has_instance_of;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse PLM_ExternalID element
			map.insert(std::make_pair("PLM_ExternalID", [](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);		
			}));

			// Parse IsAggregatedBy element
			map.insert(std::make_pair("IsAggregatedBy", [](Params& params){
				params.aggregated_by = *(params.me->mReader->GetContent<unsigned int>(true));

				params.has_aggregated_by = true;
			}));

			// Parse IsInstanceOf element
			map.insert(std::make_pair("IsInstanceOf", [](Params& params){
				std::string uri = *(params.me->mReader->GetContent<std::string>(true));

				// Parse the URI to get its different components
				params.me->ParseURI(uri, params.instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(params.instance_of.external && params.instance_of.has_id &&
						params.instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references.find(Content::ID(params.instance_of.filename, params.instance_of.id)) == params.me->mContent.references.end()) {

					params.me->mContent.files_to_parse.insert(params.instance_of.filename);
				}

				params.has_instance_of = true;
			}));

			// Parse RelativeMatrix element
			map.insert(std::make_pair("RelativeMatrix", [](Params& params){
				std::string relative_matrix = *(params.me->mReader->GetContent<std::string>(true));

				aiMatrix4x4& transformation = params.instance.node->mTransformation;

				// Save the transformation matrix
				std::istringstream matrix(relative_matrix);
				matrix
					>> transformation.a1 >> transformation.b1 >> transformation.c1
					>> transformation.a2 >> transformation.b2 >> transformation.c2
					>> transformation.a3 >> transformation.b3 >> transformation.c3
					>> transformation.a4 >> transformation.b4 >> transformation.c4;
				transformation.d1 = transformation.d2 = transformation.d3 = 0.0;
				transformation.d4 = 1.0;
			}));

			return map;
		})());

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.instance.id = *(mReader->GetAttribute<unsigned int>("id", true));
		params.has_aggregated_by = false;
		params.has_instance_of = false;

		if(! mReader->HasElements()) {
			ThrowException("In Instance3D \"" + mReader->ToString(params.instance.id) + "\": the instance has no sub elements. It must at least define \"IsAggregatedBy\" and \"IsInstanceOf\" elements.");
		}

		mReader->ParseNode(mapping, params);
		
		if(! params.has_aggregated_by) {
			ThrowException("In Instance3D \"" + mReader->ToString(params.instance.id) + "\": the instance has no sub element \"IsAggregatedBy\".");
		}
		
		if(! params.has_instance_of) {
			ThrowException("In Instance3D \"" + mReader->ToString(params.instance.id) + "\": the instance has no sub element \"IsInstanceOf\".");
		}

		// Test if the name exist, otherwise use the id as name
		std::string name;
		if(params.name_opt) {
			params.instance.node->mName = *(params.name_opt);
		} else {
			// No name: take the id as the name
			params.instance.node->mName = mReader->ToString(params.instance.id);
		}

		// Save the information corresponding to this instance
		if(params.instance_of.has_id) {
			// Create the refered Reference3D if necessary
			params.instance.instance_of = &(mContent.references[Content::ID(params.instance_of.filename, params.instance_of.id)]);

			// Update the number of instances of this Reference3D
			params.instance.instance_of->nb_references++;
		} else {
			ThrowException("In Instance3D \"" + mReader->ToString(params.instance.id) + "\": the instance refers to an invalid reference \"" + params.instance_of.uri + "\" without id.");
		}

		// Save the reference to the parent Reference3D
		Content::Reference3D& parent = params.me->mContent.references[Content::ID(params.me->mReader->GetFilename(), params.aggregated_by)];
				
		// Insert the instance into the aggregating Reference3D
		// The ownership of the node pointer owned by ScopeGuard is automatically transfered to the inserted instance
		auto result = parent.instances.insert(std::make_pair(Content::ID(params.me->mReader->GetFilename(), params.instance.id), params.instance));

		if(! result.second) {
			params.me->ThrowException("In Instance3D \"" + params.me->mReader->ToString(params.instance.id) + "\": the instance is already aggregated by the Reference3D \"" + params.me->mReader->ToString(params.aggregated_by) + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the ReferenceRep section
	void _3DXMLParser::ReadReferenceRep() {
		struct Params {
			_3DXMLParser* me;
			XMLReader::Optional<std::string> name_opt;
			unsigned int id;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse PLM_ExternalID element
			map.insert(std::make_pair("PLM_ExternalID", [](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}));

			return map;
		})());

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));
		std::string format = *(mReader->GetAttribute<std::string>("format", true));
		std::string file = *(mReader->GetAttribute<std::string>("associatedFile", true));
		Content::URI uri;

		mReader->ParseNode(mapping, params);

		// Parse the external URI to the file containing the representation
		ParseURI(file, uri);
		if(! uri.external) {
			ThrowException("In ReferenceRep \"" + mReader->ToString(params.id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Get the container for this representation
		Content::ReferenceRep& rep = mContent.representations[Content::ID(mReader->GetFilename(), params.id)];
		rep.id = params.id;
		rep.index_begin = 0;
		rep.index_end = 0;
		rep.meshes.clear();

		// Check the representation format and call the correct parsing function accordingly
		if(format.compare("TESSELLATED") == 0) {
			if(uri.extension.compare("3DRep") == 0) {
				_3DXMLRepresentation representation(mArchive, uri.filename, rep.meshes);
			} else {
				ThrowException("In ReferenceRep \"" + mReader->ToString(params.id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
			}
		} else {
			ThrowException("In ReferenceRep \"" + mReader->ToString(params.id) + "\": unsupported representation format \"" + format + "\".");
		}

		// Test if the name exist, otherwise use the id as name
		std::string name;
		if(params.name_opt) {
			name = *(params.name_opt);
		} else {
			// No name: take the id as the name
			name = params.me->mReader->ToString(params.id);
		}

		// Set the names of the parsed meshes with this ReferenceRep name
		for(std::list<ScopeGuard<aiMesh>>::const_iterator it(rep.meshes.begin()), end(rep.meshes.end()); it != end; ++it) {
			(*it)->mName = name;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void _3DXMLParser::ReadInstanceRep() {
		struct Params {
			_3DXMLParser* me;
			XMLReader::Optional<std::string> name_opt;
			unsigned int id;
			Content::InstanceRep* mesh;
			Content::URI instance_of;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse PLM_ExternalID element
			map.insert(std::make_pair("PLM_ExternalID", [](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}));

			// Parse IsAggregatedBy element
			map.insert(std::make_pair("IsAggregatedBy", [](Params& params){
				unsigned int aggregated_by = *(params.me->mReader->GetContent<unsigned int>(true));

				// Save the reference to the parent Reference3D
				Content::Reference3D& parent = params.me->mContent.references[Content::ID(params.me->mReader->GetFilename(), aggregated_by)];
				params.mesh = &(parent.meshes[Content::ID(params.me->mReader->GetFilename(), params.id)]);

				// Save id for future error / log message
				params.mesh->id = params.id;
			}));

			// Parse IsInstanceOf element
			map.insert(std::make_pair("IsInstanceOf", [](Params& params){
				std::string uri = *(params.me->mReader->GetContent<std::string>(true));

				// Parse the URI to get its different components
				params.me->ParseURI(uri, params.instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(params.instance_of.external && params.instance_of.has_id &&
						params.instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references.find(Content::ID(params.instance_of.filename, params.instance_of.id)) == params.me->mContent.references.end()) {

					params.me->mContent.files_to_parse.insert(params.instance_of.filename);
				}
			}));

			return map;
		})());

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));
		params.mesh = NULL;

		if(! mReader->HasElements()) {
			ThrowException("In InstanceRep \"" + mReader->ToString(params.id) + "\": the instance has no sub elements. It must at least define \"IsAggregatedBy\" and \"IsInstanceOf\" elements.");
		}

		mReader->ParseNode(mapping, params);

		if(params.mesh == NULL) {
			ThrowException("In InstanceRep \"" + mReader->ToString(params.id) + "\": the instance has no sub element \"IsAggregatedBy\".");
		}
		
		if(params.instance_of.uri.compare("") == 0) {
			ThrowException("In InstanceRep \"" + mReader->ToString(params.id) + "\": the instance has no sub element \"IsInstanceOf\".");
		}

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			params.mesh->name = *(params.name_opt);
		} else {
			// No name: take the id as the name
			params.mesh->name = params.me->mReader->ToString(params.id);
		}

		// Create the refered ReferenceRep if necessary
		params.mesh->instance_of = &(mContent.representations[Content::ID(params.instance_of.filename, params.instance_of.id)]);
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
