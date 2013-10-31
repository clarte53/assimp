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
#include "Q3BSPZipArchive.h"
#include "SceneCombiner.h"

#include <algorithm>
#include <cctype>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& file, aiScene* scene) : mArchive(new Q3BSP::Q3BSPZipArchive(file)), mReader(NULL), mContent(scene) {
		// Load the compressed archive
		if (! mArchive->isOpen()) {
			ThrowException("Failed to open file " + file + "." );
		}

		// Create a xml parser for the manifest
		mReader = ScopeGuard<XMLParser>(new XMLParser(mArchive, "Manifest.xml"));

		// Read the name of the main XML file in the manifest
		std::string main_file;
		ReadManifest(main_file);

		// Create a xml parser for the root file
		mReader = ScopeGuard<XMLParser>(new XMLParser(mArchive, main_file));

		// Parse the main 3DXML file
		ParseFile();

		// Parse other referenced 3DXML files until all references are resolved
		while(mContent.files_to_parse.size() != 0) {
			std::set<std::string>::const_iterator it = mContent.files_to_parse.begin();

			if(it != mContent.files_to_parse.end()) {
				// Create a xml parser for the file
				mReader = ScopeGuard<XMLParser>(new XMLParser(mArchive, *it));

				// Parse the 3DXML file
				ParseFile();

				// Remove the file from the list of files to parse
				it = mContent.files_to_parse.erase(it);
			}
		}
			
		// Add the meshes into the scene
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::ReferenceRep>::iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			it_rep->second.index_begin = mContent.scene->Meshes.Size();
			it_rep->second.index_end = it_rep->second.index_begin + it_rep->second.meshes.size() - 1;

			unsigned int index = mContent.scene->Meshes.Size();
			for(_3DXMLStructure::ReferenceRep::Meshes::iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
				// Set the names of the parsed meshes with this ReferenceRep name
				it_mesh->second->mName = it_rep->second.name;

				// Realease the ownership of the mesh to the protected scene
				mContent.scene->Meshes.Set(index++, it_mesh->second.dismiss());
			}
		}

		// Create the root node
		if(mContent.has_root_index) {
			std::map<_3DXMLStructure::ID, _3DXMLStructure::Reference3D>::iterator it_root = mContent.references.find(_3DXMLStructure::ID(main_file, mContent.root_index));

			if(it_root != mContent.references.end()) {
				_3DXMLStructure::Reference3D& root = it_root->second;

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

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Header element
			map.emplace_back("Header", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadHeader();}, 1, 1));

			// Parse ProductStructure element
			map.emplace_back("ProductStructure", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadProductStructure();}, 0, 1));

			// Parse PROCESS element
			//map.emplace_back("PROCESS", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadPROCESS();}, 0, 1));

			// Parse DefaultView element
			//map.emplace_back("DefaultView", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadDefaultView();}, 0, 1));

			// Parse DELFmiFunctionalModelImplementCnx element
			//map.emplace_back("DELFmiFunctionalModelImplementCnx", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadDELFmiFunctionalModelImplementCnx();}, 0, 1));

			// Parse CATMaterialRef element
			map.emplace_back("CATMaterialRef", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadCATMaterialRef();}, 0, 1));

			// Parse CATRepImage element
			//map.emplace_back("CATRepImage", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadCATRepImage();}, 0, 1));

			// Parse CATMaterial element
			map.emplace_back("CATMaterial", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadCATMaterial();}, 0, 1));

			// Parse DELPPRContextModelProcessCnx element
			//map.emplace_back("DELPPRContextModelProcessCnx", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadDELPPRContextModelProcessCnx();}, 0, 1));

			// Parse DELRmiResourceModelImplCnx element
			//map.emplace_back("DELRmiResourceModelImplCnx", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadDELRmiResourceModelImplCnx();}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);


		params.me = this;

		// Parse the main 3DXML file
		while(mReader->Next()) {
			if(mReader->IsElement("Model_3dxml")) {
				mReader->ParseElements(mapping, params);
			} else {
				mReader->SkipElement();
			}
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse one uri and split it into it's different components
	void _3DXMLParser::ParseURI(const std::string& uri, _3DXMLStructure::URI& result) const {
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
		} else if((std::size_t) std::count_if(uri.begin(), uri.end(), ::isdigit) == uri.size()) {
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
	void _3DXMLParser::BuildStructure(_3DXMLStructure::Reference3D& ref, aiNode* node) const {
		// Decrement the counter of instances to this Reference3D (used for memory managment)
		if(ref.nb_references > 0) {
			ref.nb_references--;
		}

		if(node != NULL) {
			// Copy the indexes of the meshes contained into this instance into the proper aiNode
			if(node->Meshes.Size() == 0) {
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::InstanceRep>::const_iterator it_mesh(ref.meshes.begin()), end_mesh(ref.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
					const _3DXMLStructure::InstanceRep& rep = it_mesh->second;

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
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::Instance3D>::iterator it_child(ref.instances.begin()), end_child(ref.instances.end()); it_child != end_child; ++ it_child) {
					_3DXMLStructure::Instance3D& child = it_child->second;

					if(child.node.get() != NULL && child.instance_of != NULL) {
						// Test if the node name is an id to see if we better take the Reference3D instead
						if(! child.has_name && child.instance_of->has_name) {
							child.node->mName = child.instance_of->name;
						}

						// Construct the hierarchy recursively
						BuildStructure(*child.instance_of, child.node.get());

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

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Root element
			map.emplace_back("Root", XMLParser::XSD::Element<Params>([](Params& params){
				*params.file = *(params.me->mReader->GetContent<std::string>(true));
				params.found = true;
			}, 1, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.file = &main_file;
		params.found = false;

		while(! params.found && mReader->Next()) {
			// handle the root element "Manifest"
			if(mReader->IsElement("Manifest")) {
				mReader->ParseElements(mapping, params);
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

		static const XMLParser::XSD::Choice<Params> mapping(([](){
			XMLParser::XSD::Choice<Params>::type map;

			// Parse Reference3D element
			map.emplace("Reference3D", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadReference3D();}, 0, XMLParser::XSD::unbounded));

			// Parse Instance3D element
			map.emplace("Instance3D", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadInstance3D();}, 0, XMLParser::XSD::unbounded));

			// Parse ReferenceRep element
			map.emplace("ReferenceRep", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadReferenceRep();}, 0, XMLParser::XSD::unbounded));

			// Parse InstanceRep element
			map.emplace("InstanceRep", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadInstanceRep();}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, XMLParser::XSD::unbounded);

		XMLParser::Optional<unsigned int> root = mReader->GetAttribute<unsigned int>("root");

		if(root) {
			mContent.root_index = *root;
			mContent.has_root_index = true;
		}

		params.me = this;

		mReader->ParseElements(mapping, params);
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
			XMLParser::Optional<std::string> name_opt;
			unsigned int id;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));

		mReader->ParseElements(mapping, params);

		_3DXMLStructure::Reference3D& ref = mContent.references[_3DXMLStructure::ID(mReader->GetFilename(), params.id)]; // Create the Reference3D if not present.
				
		// Save id and name for future error / log messages
		ref.id = params.id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			ref.name = *(params.name_opt);
			ref.has_name = true;
		} else {
			// No name: take the id as the name
			ref.name = mReader->ToString(params.id);
			ref.has_name = false;
		}

		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The Reference3D will be completed by the Instance3D and InstanceRep
	}

	// ------------------------------------------------------------------------------------------------
	// Read the Instance3D section
	void _3DXMLParser::ReadInstance3D() {
		struct Params {
			_3DXMLParser* me;
			XMLParser::Optional<std::string> name_opt;
			_3DXMLStructure::Instance3D instance;
			_3DXMLStructure::URI instance_of;
			unsigned int aggregated_by;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);		
			}, 0, 1));

			// Parse IsAggregatedBy element
			map.emplace_back("IsAggregatedBy", XMLParser::XSD::Element<Params>([](Params& params){
				params.aggregated_by = *(params.me->mReader->GetContent<unsigned int>(true));
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](Params& params){
				std::string uri = *(params.me->mReader->GetContent<std::string>(true));

				// Parse the URI to get its different components
				params.me->ParseURI(uri, params.instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(params.instance_of.external && params.instance_of.has_id &&
						params.instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references.find(_3DXMLStructure::ID(params.instance_of.filename, params.instance_of.id)) == params.me->mContent.references.end()) {

					params.me->mContent.files_to_parse.insert(params.instance_of.filename);
				}
			}, 1, 1));

			// Parse RelativeMatrix element
			map.emplace_back("RelativeMatrix", XMLParser::XSD::Element<Params>([](Params& params){
				std::string relative_matrix = *(params.me->mReader->GetContent<std::string>(true));

				aiMatrix4x4& transformation = params.instance.node->mTransformation;

				// Save the transformation matrix
				std::istringstream matrix(relative_matrix);
				float value;
				matrix >> value; transformation.a1 = value;
				matrix >> value; transformation.b1 = value;
				matrix >> value; transformation.c1 = value;
				matrix >> value; transformation.a2 = value;
				matrix >> value; transformation.b2 = value;
				matrix >> value; transformation.c2 = value;
				matrix >> value; transformation.a3 = value;
				matrix >> value; transformation.b3 = value;
				matrix >> value; transformation.c3 = value;
				matrix >> value; transformation.a4 = value;
				matrix >> value; transformation.b4 = value;
				matrix >> value; transformation.c4 = value;
				transformation.d1 = transformation.d2 = transformation.d3 = 0.0;
				transformation.d4 = 1.0;
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.instance.id = *(mReader->GetAttribute<unsigned int>("id", true));

		mReader->ParseElements(mapping, params);

		// Test if the name exist, otherwise use the id as name
		std::string name;
		if(params.name_opt) {
			params.instance.node->mName = *(params.name_opt);
			params.instance.has_name = true;
		} else {
			// No name: take the id as the name
			params.instance.node->mName = mReader->ToString(params.instance.id);
			params.instance.has_name = false;
		}

		// Save the information corresponding to this instance
		if(params.instance_of.has_id) {
			// Create the refered Reference3D if necessary
			params.instance.instance_of = &(mContent.references[_3DXMLStructure::ID(params.instance_of.filename, params.instance_of.id)]);

			// Update the number of instances of this Reference3D
			params.instance.instance_of->nb_references++;
		} else {
			ThrowException("In Instance3D \"" + mReader->ToString(params.instance.id) + "\": the instance refers to an invalid reference \"" + params.instance_of.uri + "\" without id.");
		}

		// Save the reference to the parent Reference3D
		_3DXMLStructure::Reference3D& parent = params.me->mContent.references[_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.aggregated_by)];
				
		// Insert the instance into the aggregating Reference3D
		// The ownership of the node pointer owned by ScopeGuard is automatically transfered to the inserted instance
		auto result = parent.instances.insert(std::make_pair(_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.instance.id), params.instance));

		if(! result.second) {
			params.me->ThrowException("In Instance3D \"" + params.me->mReader->ToString(params.instance.id) + "\": the instance is already aggregated by the Reference3D \"" + params.me->mReader->ToString(params.aggregated_by) + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the ReferenceRep section
	void _3DXMLParser::ReadReferenceRep() {
		struct Params {
			_3DXMLParser* me;
			XMLParser::Optional<std::string> name_opt;
			unsigned int id;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}, 0, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));
		std::string format = *(mReader->GetAttribute<std::string>("format", true));
		std::string file = *(mReader->GetAttribute<std::string>("associatedFile", true));
		_3DXMLStructure::URI uri;

		mReader->ParseElements(mapping, params);

		// Parse the external URI to the file containing the representation
		ParseURI(file, uri);
		if(! uri.external) {
			ThrowException("In ReferenceRep \"" + mReader->ToString(params.id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Get the container for this representation
		_3DXMLStructure::ReferenceRep& rep = mContent.representations[_3DXMLStructure::ID(mReader->GetFilename(), params.id)];
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
		if(params.name_opt) {
			rep.name = *(params.name_opt);
			rep.has_name = true;
		} else {
			// No name: take the id as the name
			rep.name = params.me->mReader->ToString(params.id);
			rep.has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void _3DXMLParser::ReadInstanceRep() {
		struct Params {
			_3DXMLParser* me;
			XMLParser::Optional<std::string> name_opt;
			unsigned int id;
			_3DXMLStructure::InstanceRep* mesh;
			_3DXMLStructure::URI instance_of;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}, 0, 1));

			// Parse IsAggregatedBy element
			map.emplace_back("IsAggregatedBy", XMLParser::XSD::Element<Params>([](Params& params){
				unsigned int aggregated_by = *(params.me->mReader->GetContent<unsigned int>(true));

				// Save the reference to the parent Reference3D
				_3DXMLStructure::Reference3D& parent = params.me->mContent.references[_3DXMLStructure::ID(params.me->mReader->GetFilename(), aggregated_by)];
				params.mesh = &(parent.meshes[_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.id)]);

				// Save id for future error / log message
				params.mesh->id = params.id;
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](Params& params){
				std::string uri = *(params.me->mReader->GetContent<std::string>(true));

				// Parse the URI to get its different components
				params.me->ParseURI(uri, params.instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(params.instance_of.external && params.instance_of.has_id &&
						params.instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references.find(_3DXMLStructure::ID(params.instance_of.filename, params.instance_of.id)) == params.me->mContent.references.end()) {

					params.me->mContent.files_to_parse.insert(params.instance_of.filename);
				}
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));
		params.mesh = NULL;

		mReader->ParseElements(mapping, params);

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			params.mesh->name = *(params.name_opt);
		} else {
			// No name: take the id as the name
			params.mesh->name = params.me->mReader->ToString(params.id);
		}

		// Create the refered ReferenceRep if necessary
		params.mesh->instance_of = &(mContent.representations[_3DXMLStructure::ID(params.instance_of.filename, params.instance_of.id)]);
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
