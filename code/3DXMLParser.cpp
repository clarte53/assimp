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

#include "3DXMLMaterial.h"
#include "3DXMLRepresentation.h"
#include "Q3BSPZipArchive.h"
#include "SceneCombiner.h"

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& file, aiScene* scene) : mArchive(new Q3BSP::Q3BSPZipArchive(file)), mReader(nullptr), mContent(scene) {
		// Load the compressed archive
		if (! mArchive->isOpen()) {
			ThrowException("Failed to open file " + file + "." );
		}

		// Create a xml parser for the manifest
		mReader.reset(new XMLParser(mArchive, "Manifest.xml"));

		// Read the name of the main XML file in the manifest
		std::string main_file;
		ReadManifest(main_file);

		// Add the main file to the list of files to parse
		mContent.files_to_parse.insert(main_file);

		// Parse other referenced 3DXML files until all references are resolved
		while(mContent.files_to_parse.size() != 0) {
			std::set<std::string>::const_iterator it = mContent.files_to_parse.begin();

			if(it != mContent.files_to_parse.end()) {
				// Create a xml parser for the file
				mReader.reset(new XMLParser(mArchive, *it));
				
				DefaultLogger::get()->error("Parsing 1 \"" + *it + "\" (" + mReader->GetFilename() + ").");

				// Parse the 3DXML file
				ReadFile();

				// Remove the file from the list of files to parse
				it = mContent.files_to_parse.erase(it);
			}
		}

		// Get all the MaterialAttributes of the scene
		std::map<_3DXMLStructure::ReferenceRep::MatID, unsigned int> attributes;
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::ReferenceRep>::const_iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			for(_3DXMLStructure::ReferenceRep::Meshes::const_iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
				attributes.emplace(it_mesh->first, 0); // No need to check the output, we don't care if the element is inserted or is already present
			}
		}

		// Generate the final materials of the scene and store their indices
		for(std::map<_3DXMLStructure::ReferenceRep::MatID, unsigned int>::iterator it_mat(attributes.begin()), end_mat(attributes.end()); it_mat != end_mat; ++ it_mat) {
			// The final material
			std::unique_ptr<aiMaterial> material(nullptr);
			
			// If the material ID is defined (ie not the default material)
			if(it_mat->first) {
				// If the material ID contains some MaterialApplications (ie it is not just a simple color) 
				if(! it_mat->first->materials.empty()) {
					// List of the materials corresponding to the different applications
					std::vector<std::pair<const _3DXMLStructure::MaterialApplication*, std::unique_ptr<aiMaterial>>> mat_list_app;

					// For each application
					for(std::list<_3DXMLStructure::MaterialApplication>::const_iterator it_app(it_mat->first->materials.begin()), end_app(it_mat->first->materials.end()); it_app != end_app; ++it_app) {						
						// Find the corresponding CATMatReference which reference the material
						std::map<_3DXMLStructure::ID, _3DXMLStructure::CATMatReference>::const_iterator it_mat_ref = mContent.references_mat.find(it_app->id);

						// Found?
						if(it_mat_ref != mContent.references_mat.end()) {
							// List of the different instances of materials referenced by this CATMatReference
							// (Normally, in schema 4.3 it should be only 1 instance, but we have no garantees that it will remain the case in future versions.
							// Therefore we merge the different instanciated materials together)
							std::vector<aiMaterial*> mat_list_ref;

							// Get all the instances
							for(std::map<_3DXMLStructure::ID, _3DXMLStructure::MaterialDomainInstance>::const_iterator it_dom(it_mat_ref->second.materials.begin()), end_dom(it_mat_ref->second.materials.end()); it_dom != end_dom; ++it_dom) {
								mat_list_ref.push_back(it_dom->second.instance_of->material.get());
							}

							// If we have at least one instance
							if(! mat_list_ref.empty()) {
								// Merge the different materials together
								aiMaterial* material_ptr = NULL;
								SceneCombiner::MergeMaterials(&material_ptr, mat_list_ref.begin(), mat_list_ref.end());

								// Save the merged material
								mat_list_app.emplace_back();
								mat_list_app.back().first = &(*it_app);
								mat_list_app.back().second.reset(material_ptr);

								// Save the name of the material
								aiString name;
								if(it_mat_ref->second.has_name) {
									name = it_mat_ref->second.name;
								} else {
									name = mReader->ToString(it_mat_ref->second.id);
								}
								material_ptr->AddProperty(&name, AI_MATKEY_NAME);
							} else {
								ThrowException("In CATMatReference \"" + mReader->ToString(it_mat_ref->second.id) + "\": invalid reference without any instance.");
							}
						}  else {
							ThrowException("Invalid MaterialApplication referencing unknown CATMatReference \"" + mReader->ToString(it_app->id.id) + "\".");
						}
					}

					// Save the materials to merge together
					std::vector<aiMaterial*> mat_list_final;

					// Set the channel of the different materials
					for(std::vector<std::pair<const _3DXMLStructure::MaterialApplication*, std::unique_ptr<aiMaterial>>>::iterator it_mat_app(mat_list_app.begin()), end_mat_app(mat_list_app.end()); it_mat_app != end_mat_app; ++it_mat_app) {
						aiMaterial* mat = it_mat_app->second.get();
						unsigned int channel = it_mat_app->first->channel;

						for(unsigned int i = 0; i < mat->mNumProperties; ++i) {
							mat->mProperties[i]->mIndex = channel;
						}

						// Should we disable backface culling?
						int two_sided = 0;
						if(it_mat_app->first->side != _3DXMLStructure::MaterialApplication::FRONT) {
							two_sided = 1;
						}
						mat->AddProperty(&two_sided, 1, AI_MATKEY_TWOSIDED);

						//TODO: MaterialApplication.TextureBlendFunction

						mat_list_final.push_back(mat);
					}
					
					// Merge the different applications together based on their channel
					aiMaterial* material_ptr = NULL;
					SceneCombiner::MergeMaterials(&material_ptr, mat_list_final.begin(), mat_list_final.end());

					// Save the final material
					material.reset(material_ptr);
				} else { // It must be a simple color material
					material.reset(new aiMaterial());

					material->AddProperty(&(it_mat->first->color), 1, AI_MATKEY_COLOR_DIFFUSE);

					if(it_mat->first->color.a != 1.0) {
						// Also add transparency
						material->AddProperty(&(it_mat->first->color.a), 1, AI_MATKEY_OPACITY);
					}
				}
			} else {
				// Default material
				material.reset(new aiMaterial());

				aiString name("Default Material");
				material->AddProperty(&name, AI_MATKEY_NAME);
			}

			// Get the index for this material
			unsigned int index = mContent.scene->Materials.Size();

			// Save the material and index
			mContent.scene->Materials.Set(index, material.release());
			it_mat->second = index;
		}
			
		// Add the meshes into the scene
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::ReferenceRep>::iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			unsigned int index = mContent.scene->Meshes.Size();
			
			it_rep->second.index_begin = index;
			it_rep->second.index_end = index;

			for(_3DXMLStructure::ReferenceRep::Meshes::iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
				// Set the names of the parsed meshes with this ReferenceRep name
				it_mesh->second.mesh->mName = it_rep->second.name;

				// Set the index of the used material
				std::map<_3DXMLStructure::ReferenceRep::MatID, unsigned int>::iterator it_mat = attributes.find(it_mesh->first);

				if(it_mat != attributes.end()) {
					it_mesh->second.mesh->mMaterialIndex = it_mat->second;
				} else {
					ThrowException("In Mesh \"" + it_rep->second.name + "\": no material defined.");
				}

				// Realease the ownership of the mesh to the protected scene
				mContent.scene->Meshes.Set(index++, it_mesh->second.mesh.release());

				// Update the number of meshes in this ReferenceRep
				it_rep->second.index_end++;
			}
		}

		// Create the root node
		if(mContent.ref_root_index) {
			std::map<_3DXMLStructure::ID, _3DXMLStructure::Reference3D>::iterator it_root = mContent.references_node.find(_3DXMLStructure::ID(main_file, *(mContent.ref_root_index)));

			if(it_root != mContent.references_node.end()) {
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
				ThrowException("Unresolved root Reference3D \"" + mReader->ToString(*(mContent.ref_root_index)) + "\".");
			}
		} else {
			// TODO: no root node specifically named -> we must analyze the node structure to find the root or create an artificial root node
			ThrowException("No root Reference3D specified.");
		}
	}

	_3DXMLParser::~_3DXMLParser() {

	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse one uri and split it into it's different components
	void _3DXMLParser::ParseURI(const XMLParser* parser, const std::string& uri, _3DXMLStructure::URI& result) {
		static const unsigned int size_prefix = 10;

		result.uri = uri;

		if(uri.substr(0, size_prefix).compare("urn:3DXML:") == 0) {
			result.external = true;

			std::size_t begin = uri.find_last_of(':');
			std::size_t end = uri.find_last_of('#');

			if(begin == uri.npos) {
				parser->ThrowException("The URI \"" + uri + "\" has an invalid format.");
			}

			if(end != uri.npos) {
				std::string id_str = uri.substr(end + 1, uri.npos);
				unsigned int id;

				ParseID(id_str, id);
				result.id = Optional<unsigned int>(id);

				result.filename = uri.substr(begin + 1, end - (begin + 1));
			} else {
				result.id = Optional<unsigned int>();
				result.filename = uri.substr(begin + 1, uri.npos);
			}
		} else if((std::size_t) std::count_if(uri.begin(), uri.end(), ::isdigit) == uri.size()) {
			unsigned int id;

			ParseID(uri, id);

			result.external = false;
			result.id = Optional<unsigned int>(id);
			result.filename = parser->GetFilename();
		} else {
			parser->ThrowException("The URI \"" + uri + "\" has an invalid format.");
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
	// Aborts the file reading with an exception
	void _3DXMLParser::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader->GetFilename() % error));
	}

	// ------------------------------------------------------------------------------------------------
	// Add the meshes indices and children nodes into the given node recursively
	void _3DXMLParser::BuildStructure(_3DXMLStructure::Reference3D& ref, aiNode* node) const {
		// Decrement the counter of instances to this Reference3D (used for memory managment)
		if(ref.nb_references > 0) {
			ref.nb_references--;
		}

		if(node != nullptr) {
			// Copy the indexes of the meshes contained into this instance into the proper aiNode
			if(node->Meshes.Size() == 0) {
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::InstanceRep>::const_iterator it_mesh(ref.meshes.begin()), end_mesh(ref.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
					const _3DXMLStructure::InstanceRep& rep = it_mesh->second;

					if(rep.instance_of != nullptr) {
						for(unsigned int i = node->Meshes.Size(), index = rep.instance_of->index_begin; index < rep.instance_of->index_end; i++, index++) {
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

					if(child.node.get() != nullptr && child.instance_of != nullptr) {
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
							node->Children.Set(node->Children.Size(), child.node.release());
						} else {
							// Otherwise we need to make a deep copy of the child node in order to avoid duplicate nodes in the scene hierarchy
							// (which would cause assimp to deallocate them multiple times, therefore making the application crash)
							aiNode* copy_node = nullptr;

							SceneCombiner::Copy(&copy_node, child.node.get());

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
				mReader->ParseElements(&mapping, params);
			} else {
				mReader->SkipElement();
			}
		}

		if(! params.found) {
			ThrowException("Unable to find the name of the main XML file in the manifest.");
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse a 3DXML file
	void _3DXMLParser::ReadFile() {
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
			DefaultLogger::get()->error("Parsing 2 \"" + mReader->GetNodeName() + "\" in \"" + mReader->GetFilename() + "\".");
			if(mReader->IsElement("Model_3dxml")) {
				DefaultLogger::get()->error("Parsing 3 \"Model_3dxml\" in \"" + mReader->GetFilename() + "\".");
				mReader->ParseElements(&mapping, params);
			} else {
				mReader->SkipElement();
			}
		}
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

		mContent.ref_root_index = mReader->GetAttribute<unsigned int>("root");

		params.me = this;

		mReader->ParseElements(&mapping, params);
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the Reference3D section
	void _3DXMLParser::ReadReference3D() {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
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
		unsigned int id = *(mReader->GetAttribute<unsigned int>("id", true));

		mReader->ParseElements(&mapping, params);

		_3DXMLStructure::Reference3D& ref = mContent.references_node[_3DXMLStructure::ID(mReader->GetFilename(), id)]; // Create the Reference3D if not present.
				
		// Save id and name for future error / log messages
		ref.id = id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			ref.name = *(params.name_opt);
			ref.has_name = true;
		} else {
			// No name: take the id as the name
			ref.name = mReader->ToString(id);
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
			Optional<std::string> name_opt;
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
				params.me->ParseURI(params.me->mReader.get(), uri, params.instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(params.instance_of.external && params.instance_of.id &&
						params.instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references_node.find(_3DXMLStructure::ID(params.instance_of.filename, *(params.instance_of.id))) == params.me->mContent.references_node.end()) {

					params.me->mContent.files_to_parse.emplace(params.instance_of.filename);
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

		mReader->ParseElements(&mapping, params);

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
		if(params.instance_of.id) {
			// Create the refered Reference3D if necessary
			params.instance.instance_of = &(mContent.references_node[_3DXMLStructure::ID(params.instance_of.filename, *(params.instance_of.id))]);

			// Update the number of instances of this Reference3D
			params.instance.instance_of->nb_references++;
		} else {
			ThrowException("In Instance3D \"" + mReader->ToString(params.instance.id) + "\": the instance refers to an invalid reference \"" + params.instance_of.uri + "\" without id.");
		}

		// Save the reference to the parent Reference3D
		_3DXMLStructure::Reference3D& parent = params.me->mContent.references_node[_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.aggregated_by)];
				
		// Insert the instance into the aggregating Reference3D
		// The ownership of the node pointer owned by std::unique_ptr is automatically transfered to the inserted instance
		auto result = parent.instances.emplace(_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.instance.id), std::move(params.instance));

		if(! result.second) {
			params.me->ThrowException("In Instance3D \"" + params.me->mReader->ToString(params.instance.id) + "\": the instance is already aggregated by the Reference3D \"" + params.me->mReader->ToString(params.aggregated_by) + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the ReferenceRep section
	void _3DXMLParser::ReadReferenceRep() {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
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
		unsigned int id = *(mReader->GetAttribute<unsigned int>("id", true));
		std::string format = *(mReader->GetAttribute<std::string>("format", true));
		std::string file = *(mReader->GetAttribute<std::string>("associatedFile", true));
		_3DXMLStructure::URI uri;

		mReader->ParseElements(&mapping, params);

		// Parse the external URI to the file containing the representation
		ParseURI(mReader.get(), file, uri);
		if(! uri.external) {
			ThrowException("In ReferenceRep \"" + mReader->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Get the container for this representation
		_3DXMLStructure::ReferenceRep& rep = mContent.representations[_3DXMLStructure::ID(mReader->GetFilename(), id)];
		rep.id = id;
		rep.index_begin = 0;
		rep.index_end = 0;
		rep.meshes.clear();

		// Check the representation format and call the correct parsing function accordingly
		if(format.compare("TESSELLATED") == 0) {
			if(uri.extension.compare("3DRep") == 0) {
				// Parse the geometry representation
				_3DXMLRepresentation representation(mArchive, uri.filename, rep.meshes);

				// Get the dependencies for this geometry
				const std::set<_3DXMLStructure::ID>& dependencies = representation.GetDependencies();

				// Add the dependencies to the list of files to parse if necessary
				for(std::set<_3DXMLStructure::ID>::const_iterator it(dependencies.begin()), end(dependencies.end()); it != end; ++it) {
					auto found = mContent.materials.find(*it);

					if(found == mContent.materials.end()) {
						mContent.files_to_parse.emplace(it->filename);
					}
				}
			} else {
				ThrowException("In ReferenceRep \"" + mReader->ToString(id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
			}
		} else {
			ThrowException("In ReferenceRep \"" + mReader->ToString(id) + "\": unsupported representation format \"" + format + "\".");
		}

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			rep.name = *(params.name_opt);
			rep.has_name = true;
		} else {
			// No name: take the id as the name
			rep.name = params.me->mReader->ToString(id);
			rep.has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void _3DXMLParser::ReadInstanceRep() {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
			_3DXMLStructure::InstanceRep* mesh;
			unsigned int id;
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
				_3DXMLStructure::Reference3D& parent = params.me->mContent.references_node[_3DXMLStructure::ID(params.me->mReader->GetFilename(), aggregated_by)];
				params.mesh = &(parent.meshes[_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.id)]);

				// Save id for future error / log message
				params.mesh->id = params.id;
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](Params& params){
				std::string uri = *(params.me->mReader->GetContent<std::string>(true));
				_3DXMLStructure::URI instance_of;

				// Parse the URI to get its different components
				params.me->ParseURI(params.me->mReader.get(), uri, instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(instance_of.external && instance_of.id &&
						instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references_node.find(_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))) == params.me->mContent.references_node.end()) {

					params.me->mContent.files_to_parse.emplace(instance_of.filename);
				}

				if(instance_of.id) {
					// Create the refered ReferenceRep if necessary
					params.mesh->instance_of = &(params.me->mContent.representations[_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))]);
				} else {
					params.me->ThrowException("In InstanceRep \"" + params.me->mReader->ToString(params.id) + "\": the uri \"" + uri + "\" has no id component.");
				}
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));
		params.mesh = nullptr;

		mReader->ParseElements(&mapping, params);

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			params.mesh->name = *(params.name_opt);
			params.mesh->has_name = true;
		} else {
			// No name: take the id as the name
			params.mesh->name = params.me->mReader->ToString(params.id);
			params.mesh->has_name = false;
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterialRef section
	void _3DXMLParser::ReadCATMaterialRef() {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Choice<Params> mapping(([](){
			XMLParser::XSD::Choice<Params>::type map;

			// Parse CATMatReference element
			map.emplace("CATMatReference", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadCATMatReference();}, 0, XMLParser::XSD::unbounded));

			// Parse MaterialDomain element
			map.emplace("MaterialDomain", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadMaterialDomain();}, 0, XMLParser::XSD::unbounded));

			// Parse MaterialDomainInstance element
			map.emplace("MaterialDomainInstance", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadMaterialDomainInstance();}, 0, XMLParser::XSD::unbounded));

			return std::move(map);
		})(), 1, XMLParser::XSD::unbounded);

		DefaultLogger::get()->warn("CATMaterialRef: start.");

		mContent.mat_root_index = mReader->GetAttribute<unsigned int>("root");

		params.me = this;

		mReader->ParseElements(&mapping, params);
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMatReference section
	void _3DXMLParser::ReadCATMatReference() {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
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
		unsigned int id = *(mReader->GetAttribute<unsigned int>("id", true));

		mReader->ParseElements(&mapping, params);

		_3DXMLStructure::CATMatReference& ref = mContent.references_mat[_3DXMLStructure::ID(mReader->GetFilename(), id)]; // Create the CATMaterialRef if not present.
		
		// Save id and name for future error / log messages
		ref.id = id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			ref.name = *(params.name_opt);
			ref.has_name = true;
		} else {
			// No name: take the id as the name
			ref.name = mReader->ToString(id);
			ref.has_name = false;
		}
		
		DefaultLogger::get()->warn("CATMaterialRef: reference \"" + ref.name + "\" (" + mReader->ToString(ref.id) + ").");

		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The CATMaterialRef will be completed by the MaterialDomainInstance
	}

	// ------------------------------------------------------------------------------------------------
	// Read the MaterialDomain section
	void _3DXMLParser::ReadMaterialDomain() {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
			bool rendering;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](Params& params){
				params.name_opt = params.me->mReader->GetContent<std::string>(true);
			}, 0, 1));

			// Parse V_MatDomain element
			map.emplace_back("V_MatDomain", XMLParser::XSD::Element<Params>([](Params& params){
				std::string domain = *(params.me->mReader->GetContent<std::string>(true));

				params.rendering = (domain.compare("Rendering") == 0);
			}, 0, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.rendering = false;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		unsigned int id = *(mReader->GetAttribute<unsigned int>("id", true));
		std::string format = *(mReader->GetAttribute<std::string>("format", true));
		std::string file = *(mReader->GetAttribute<std::string>("associatedFile", true));
		_3DXMLStructure::URI uri;

		mReader->ParseElements(&mapping, params);

		// Parse the external URI to the file containing the representation
		ParseURI(mReader.get(), file, uri);
		if(! uri.external) {
			ThrowException("In MaterialDomain \"" + mReader->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Get the container for this representation
		_3DXMLStructure::MaterialDomain& mat = mContent.materials[_3DXMLStructure::ID(mReader->GetFilename(), id)];
		mat.id = id;

		// Check the representation format and call the correct parsing function accordingly
		if(params.rendering) {
			if(format.compare("TECHREP") == 0) {
				if(uri.extension.compare("3DRep") == 0) {
					_3DXMLMaterial material(mArchive, uri.filename, mat.material.get());
				} else {
					ThrowException("In MaterialDomain \"" + mReader->ToString(id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
				}
			} else {
				ThrowException("In MaterialDomain \"" + mReader->ToString(id) + "\": unsupported representation format \"" + format + "\".");
			}
		} else {
			//params.me->ThrowException("In MaterialDomain \"" + params.me->mReader->ToString(id) + "\": unsupported material domain.");
		}

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			mat.name = *(params.name_opt);
			mat.has_name = true;
		} else {
			// No name: take the id as the name
			mat.name = params.me->mReader->ToString(id);
			mat.has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the MaterialDomainInstance section
	void _3DXMLParser::ReadMaterialDomainInstance() {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
			_3DXMLStructure::MaterialDomainInstance* material;
			unsigned int id;
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
				_3DXMLStructure::CATMatReference& parent = params.me->mContent.references_mat[_3DXMLStructure::ID(params.me->mReader->GetFilename(), aggregated_by)];
				params.material = &(parent.materials[_3DXMLStructure::ID(params.me->mReader->GetFilename(), params.id)]);

				// Save id for future error / log message
				params.material->id = params.id;
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](Params& params){
				std::string uri = *(params.me->mReader->GetContent<std::string>(true));
				_3DXMLStructure::URI instance_of;

				// Parse the URI to get its different components
				params.me->ParseURI(params.me->mReader.get(), uri, instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(instance_of.external && instance_of.id &&
						instance_of.filename.compare(params.me->mReader->GetFilename()) != 0 &&
						params.me->mContent.references_mat.find(_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))) == params.me->mContent.references_mat.end()) {

					params.me->mContent.files_to_parse.emplace(instance_of.filename);
				}

				if(instance_of.id) {
					// Create the refered ReferenceRep if necessary
					params.material->instance_of = &(params.me->mContent.materials[_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))]);
				} else {
					params.me->ThrowException("In MaterialDomainInstance \"" + params.me->mReader->ToString(params.id) + "\": the uri \"" + uri + "\" has no id component.");
				}
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = mReader->GetAttribute<std::string>("name");
		params.id = *(mReader->GetAttribute<unsigned int>("id", true));
		params.material = nullptr;

		mReader->ParseElements(&mapping, params);

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			params.material->name = *(params.name_opt);
			params.material->has_name = true;
		} else {
			// No name: take the id as the name
			params.material->name = params.me->mReader->ToString(params.id);
			params.material->has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterial section
	void _3DXMLParser::ReadCATMaterial() {
		//TODO
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
