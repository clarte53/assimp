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
	_3DXMLParser::_3DXMLParser(const std::string& file, aiScene* scene) : mArchive(new Q3BSP::Q3BSPZipArchive(file)), mContent(scene) {
		// Load the compressed archive
		if (! mArchive->isOpen()) {
			ThrowException(nullptr, "Failed to open file " + file + "." );
		}

		// Create a xml parser for the manifest
		std::unique_ptr<XMLParser> parser(new XMLParser(mArchive, "Manifest.xml"));

		// Read the name of the main XML file in the manifest
		std::string main_file;
		ReadManifest(parser.get(), main_file);

		// Add the main file to the list of files to parse
		mContent.files_to_parse.insert(main_file);

		// Parse other referenced 3DXML files until all references are resolved
		while(mContent.files_to_parse.size() != 0) {
			std::set<std::string>::const_iterator it = mContent.files_to_parse.begin();

			if(it != mContent.files_to_parse.end()) {
				// Create a xml parser for the file
				parser.reset(new XMLParser(mArchive, *it));
				
				// Parse the 3DXML file
				ReadFile(parser.get());

				// Remove the file from the list of files to parse
				it = mContent.files_to_parse.erase(it);
			}
		}

		// Construct the materials & meshes from the parsed data
		BuildResources(parser.get());

		// Create the root node
		BuildRoot(parser.get(), main_file);
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
				ThrowException(parser, "The URI \"" + uri + "\" has an invalid format.");
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
			ThrowException(parser, "The URI \"" + uri + "\" has an invalid format.");
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
	void _3DXMLParser::ThrowException(const XMLParser* parser, const std::string& error) {
		if(parser != nullptr) {
			throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % parser->GetFilename() % error));
		} else {
			throw DeadlyImportError(boost::str(boost::format("3DXML: %s") % error));
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Construct the materials from the parsed data
	void _3DXMLParser::BuildResources(const XMLParser* parser) {
		// Merge all the materials composing a CATMatReference into a single material
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::CATMatReference>::iterator it(mContent.references_mat.begin()), end(mContent.references_mat.end()); it != end; ++it) {
			// Is the merged material already computed?
			if(! it->second.merged_material) {
				// List of the different instances of materials referenced by this CATMatReference
				// (Normally, in schema 4.3 it should be only 1 instance, but we have no garantees that it will remain the case in future versions.
				// Therefore we merge the different instanciated materials together)
				std::vector<aiMaterial*> mat_list_ref;

				// Get all the instances
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::MaterialDomainInstance>::const_iterator it_dom(it->second.materials.begin()), end_dom(it->second.materials.end()); it_dom != end_dom; ++it_dom) {
					mat_list_ref.push_back(it_dom->second.instance_of->material.get());
				}

				// If we have at least one instance
				if(! mat_list_ref.empty()) {
					// Merge the different materials together
					aiMaterial* material_ptr = NULL;
					SceneCombiner::MergeMaterials(&material_ptr, mat_list_ref.begin(), mat_list_ref.end());

					// Save the merged material
					it->second.merged_material.reset(material_ptr);

					// Save the name of the material
					aiString name;
					if(it->second.has_name) {
						name = it->second.name;
					} else {
						name = parser->ToString(it->second.id);
					}
					material_ptr->AddProperty(&name, AI_MATKEY_NAME);
				} else {
					ThrowException(parser, "In CATMatReference \"" + parser->ToString(it->second.id) + "\": invalid reference without any instance.");
				}
			}
		}

		// Get all the MaterialAttributes of the scene
		std::set<_3DXMLStructure::ReferenceRep::MatID, _3DXMLStructure::shared_less<_3DXMLStructure::MaterialAttributes>> attributes;
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::ReferenceRep>::const_iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			for(_3DXMLStructure::ReferenceRep::Meshes::const_iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
				attributes.insert(it_mesh->first); // No need to check the output, we don't care if the element is inserted or is already present
			}
		}

		// Generate the final materials of the scene and store their indices
		for(std::set<_3DXMLStructure::ReferenceRep::MatID>::iterator it_mat(attributes.begin()), end_mat(attributes.end()); it_mat != end_mat; ++ it_mat) {
			// The final material
			std::unique_ptr<aiMaterial> material(nullptr);
			
			// If the material ID is defined (ie not the default material)
			if(*it_mat) {
				// If the material ID contains some MaterialApplications (ie it is not just a simple color) 
				if(! (*it_mat)->materials.empty()) {
					// List of the materials corresponding to the different applications
					std::vector<std::pair<const _3DXMLStructure::MaterialApplication*, aiMaterial*>> mat_list_app;

					// For each application
					for(std::list<_3DXMLStructure::MaterialApplication>::const_iterator it_app((*it_mat)->materials.begin()), end_app((*it_mat)->materials.end()); it_app != end_app; ++it_app) {
						// Find the corresponding CATMatReference which reference the material
						std::map<_3DXMLStructure::ID, _3DXMLStructure::CATMatReference>::const_iterator it_mat_ref = mContent.references_mat.find(it_app->id);

						// Found?
						if(it_mat_ref != mContent.references_mat.end()) {
							mat_list_app.emplace_back(std::make_pair(&(*it_app), it_mat_ref->second.merged_material.get()));
						}  else {
							ThrowException(parser, "Invalid MaterialApplication referencing unknown CATMatReference \"" + parser->ToString(it_app->id.id) + "\".");
						}
					}

					// Save the materials to merge together
					std::vector<aiMaterial*> mat_list_final;

					// The pointers in mat_list_final are not protected, therefore we must catch any exception to release the allocated memory correctly
					try {
						// Set the channel of the different materials
						for(std::vector<std::pair<const _3DXMLStructure::MaterialApplication*, aiMaterial*>>::iterator it_mat_app(mat_list_app.begin()), end_mat_app(mat_list_app.end()); it_mat_app != end_mat_app; ++it_mat_app) {
							aiMaterial* mat = NULL;
							SceneCombiner::Copy(&mat, it_mat_app->second);

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

						// Cleaning up memory
						for(std::vector<aiMaterial*>::iterator it(mat_list_final.begin()), end(mat_list_final.end()); it != end; ++it) {
							delete *it;
						}
						mat_list_final.clear();
					} catch(...) {
						// Cleaning up memory
						for(std::vector<aiMaterial*>::iterator it(mat_list_final.begin()), end(mat_list_final.end()); it != end; ++it) {
							delete *it;
						}
						mat_list_final.clear();

						// Rethrow the exception
						throw;
					}
				} else { // It must be a simple color material
					material.reset(new aiMaterial());

					material->AddProperty(&((*it_mat)->color), 1, AI_MATKEY_COLOR_DIFFUSE);

					if((*it_mat)->color.a != 1.0) {
						// Also add transparency
						material->AddProperty(&((*it_mat)->color.a), 1, AI_MATKEY_OPACITY);
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
			
			if(*it_mat) {
				(*it_mat)->index = index;
			} else if(index != 0) {
				ThrowException(parser, "The default material should have index 0 instead of \"" + parser->ToString(index) + "\".");
			}
		}

		// Build the materials defined in the CATMaterial section
		std::map<std::list<_3DXMLStructure::ID>, unsigned int, _3DXMLStructure::list_less<_3DXMLStructure::ID>> mat_con_indexes;
		for(std::list<_3DXMLStructure::CATMatConnection>::const_iterator it_con(mContent.mat_connections.begin()), end_con(mContent.mat_connections.end()); it_con != end_con; ++it_con) {
			unsigned int index;

			auto it_con_indexes = mat_con_indexes.find(it_con->materials);

			// Is the material already processed?
			if(it_con_indexes != mat_con_indexes.end()) {
				index = it_con_indexes->second;
			} else {
				std::unique_ptr<aiMaterial> material(nullptr);
				std::vector<aiMaterial*> materials;

				// The pointers of materials are not protected, so we must use try/catch to deallocate memory in case of exceptions
				try{
					// Get all the materials referenced in the CATMatConnection
					for(std::list<_3DXMLStructure::ID>::const_iterator it_mat(it_con->materials.begin()), end_mat(it_con->materials.end()); it_mat != end_mat; ++it_mat) {
						std::map<_3DXMLStructure::ID, _3DXMLStructure::CATMatReference>::const_iterator it = mContent.references_mat.find(*it_mat);

						if(it != mContent.references_mat.end()) {
							aiMaterial* material_ptr = NULL;

							// Copy the material
							SceneCombiner::Copy(&material_ptr, it->second.merged_material.get());

							// Save the new material
							materials.emplace_back(material_ptr);

							// Apply the channel to all the components of the material
							for(unsigned int i = 0; i < material_ptr->mNumProperties; ++i) {
								material_ptr->mProperties[i]->mIndex = it_con->channel;
							}
						} else {
							ThrowException(parser, "Invalid CATMatConnection referencing unknown CATMatReference \"" + parser->ToString(it_mat->id) + "\".");
						}
					}

					// Merge the different materials together
					aiMaterial* material_ptr = NULL;
					SceneCombiner::MergeMaterials(&material_ptr, materials.begin(), materials.end());

					// Save the merged material
					material.reset(material_ptr);

					// Cleaning up memory
					for(std::vector<aiMaterial*>::iterator it(materials.begin()), end(materials.end()); it != end; ++it) {
						delete *it;
					}
					materials.clear();
				} catch(...) {
					// Cleaning up memory
					for(std::vector<aiMaterial*>::iterator it(materials.begin()), end(materials.end()); it != end; ++it) {
						delete *it;
					}
					materials.clear();

					// Rethrow the exception
					throw;
				}

				// Get the index for this material
				index = mContent.scene->Materials.Size();

				// Save the material and index
				mContent.scene->Materials.Set(index, material.release());
				mat_con_indexes.emplace(it_con->materials, index);
			}

			// Save the index of the material for each referenced mesh
			for(std::list<_3DXMLStructure::ID>::const_iterator it_ref(it_con->references.begin()), end_ref(it_con->references.end()); it_ref != end_ref; ++it_ref) {
				std::map<_3DXMLStructure::ID, _3DXMLStructure::Reference3D>::iterator it_mesh = mContent.references_node.find(*it_ref);

				if(it_mesh != mContent.references_node.end()) {
					it_mesh->second.material_index = Optional<unsigned int>(index);
				} else {
					ThrowException(parser, "Invalid CATMatConnection referencing unknown Reference3D \"" + parser->ToString(it_ref->id) + "\".");
				}
			}
		}

		// Add the meshes into the scene
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::ReferenceRep>::iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			unsigned int index = mContent.scene->Meshes.Size();

			for(_3DXMLStructure::ReferenceRep::Meshes::iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
				// Set the names of the parsed meshes with this ReferenceRep name
				it_mesh->second.mesh->mName = it_rep->second.name;

				//TODO: use index from CATMatConnection if defined and default to the other method otherwise

				// Set the index of the used material
				std::set<_3DXMLStructure::ReferenceRep::MatID>::iterator it_mat = attributes.find(it_mesh->first);

				if(it_mat != attributes.end()) {
					it_mesh->second.mesh->mMaterialIndex = (*it_mat ? (*it_mat)->index : 0);

					// Save the index of the mesh depending for the special material index corresponding to mixed materials defined at the mesh level
					it_rep->second.indexes[std::numeric_limits<unsigned int>::max()].push_back(index);

					// Release the ownership of the mesh to the protected scene
					mContent.scene->Meshes.Set(index++, it_mesh->second.mesh.release());
				} else {
					ThrowException(parser, "In Mesh \"" + it_rep->second.name + "\": no material defined.");
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Create the root node
	void _3DXMLParser::BuildRoot(const XMLParser* parser, const std::string& main_file) {
		if(mContent.ref_root_index) {
			std::map<_3DXMLStructure::ID, _3DXMLStructure::Reference3D>::iterator it_root = mContent.references_node.find(_3DXMLStructure::ID(main_file, *(mContent.ref_root_index)));

			if(it_root != mContent.references_node.end()) {
				_3DXMLStructure::Reference3D& root = it_root->second;

				if(root.nb_references == 0) {
					// mRootNode is contained inside the scene, which is already protected against memory leaks
					mContent.scene->mRootNode = new aiNode(root.name);

					// Build the hierarchy recursively
					BuildStructure(parser, root, mContent.scene->mRootNode);
				} else {
					ThrowException(parser, "The root Reference3D should not be instantiated.");
				}
			} else {
				ThrowException(parser, "Unresolved root Reference3D \"" + parser->ToString(*(mContent.ref_root_index)) + "\".");
			}
		} else {
			// TODO: no root node specifically named -> we must analyze the node structure to find the root or create an artificial root node
			ThrowException(parser, "No root Reference3D specified.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Add the meshes indices and children nodes into the given node recursively
	void _3DXMLParser::BuildStructure(const XMLParser* parser, _3DXMLStructure::Reference3D& ref, aiNode* node) {
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
						unsigned int i = node->Meshes.Size();

						for(std::map<unsigned int, std::list<unsigned int>>::const_iterator it_index_mat(rep.instance_of->indexes.begin()), end_index_mat(rep.instance_of->indexes.end()); it_index_mat != end_index_mat; ++it_index_mat) {
							for(std::list<unsigned int>::const_iterator it_index_mesh(it_index_mat->second.begin()), end_index_mesh(it_index_mat->second.end()); it_index_mesh != end_index_mesh; ++it_index_mesh) {
								node->Meshes.Set(i++, *it_index_mesh);
							}
						}
					} else {
						ThrowException(parser, "One InstanceRep of Reference3D \"" + parser->ToString(ref.id) + "\" is unresolved.");
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
						BuildStructure(parser, *child.instance_of, child.node.get());

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
						ThrowException(parser, "One Instance3D of Reference3D \"" + parser->ToString(ref.id) + "\" is unresolved.");
					}
				}
			}
		} else {
			ThrowException(parser, "Invalid Instance3D of Reference3D \"" + parser->ToString(ref.id) + "\" with null aiNode.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the name of the main XML file in the Manifest
	void _3DXMLParser::ReadManifest(const XMLParser* parser, std::string& main_file) {
		struct Params {
			std::string* file;
			bool found;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Root element
			map.emplace_back("Root", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				*params.file = *(parser->GetContent<std::string>(true));
				params.found = true;
			}, 1, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.file = &main_file;
		params.found = false;

		while(! params.found && parser->Next()) {
			// handle the root element "Manifest"
			if(parser->IsElement("Manifest")) {
				parser->ParseElement(mapping, params);
			} else {
				parser->SkipElement();
			}
		}

		if(! params.found) {
			ThrowException(parser, "Unable to find the name of the main XML file in the manifest.");
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse a 3DXML file
	void _3DXMLParser::ReadFile(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Header element
			map.emplace_back("Header", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadHeader(parser);}, 1, 1));

			// Parse ProductStructure element
			map.emplace_back("ProductStructure", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadProductStructure(parser);}, 0, 1));

			// Parse PROCESS element
			//map.emplace_back("PROCESS", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadPROCESS(parser);}, 0, 1));

			// Parse DefaultView element
			//map.emplace_back("DefaultView", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadDefaultView(parser);}, 0, 1));

			// Parse DELFmiFunctionalModelImplementCnx element
			//map.emplace_back("DELFmiFunctionalModelImplementCnx", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadDELFmiFunctionalModelImplementCnx(parser);}, 0, 1));

			// Parse CATMaterialRef element
			map.emplace_back("CATMaterialRef", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATMaterialRef(parser);}, 0, 1));

			// Parse CATRepImage element
			//map.emplace_back("CATRepImage", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATRepImage(parser);}, 0, 1));

			// Parse CATMaterial element
			map.emplace_back("CATMaterial", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATMaterial(parser);}, 0, 1));

			// Parse DELPPRContextModelProcessCnx element
			//map.emplace_back("DELPPRContextModelProcessCnx", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadDELPPRContextModelProcessCnx(parser);}, 0, 1));

			// Parse DELRmiResourceModelImplCnx element
			//map.emplace_back("DELRmiResourceModelImplCnx", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadDELRmiResourceModelImplCnx(parser);}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);


		params.me = this;

		// Parse the main 3DXML file
		while(parser->Next()) {
			if(parser->IsElement("Model_3dxml")) {
				parser->ParseElement(mapping, params);
			} else {
				parser->SkipElement();
			}
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the header section
	void _3DXMLParser::ReadHeader(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Choice<Params> mapping(([](){
			XMLParser::XSD::Choice<Params>::type map;

			// Parse SchemaVersion element
			map.emplace("SchemaVersion", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				float version = *(parser->GetContent<float>(true));

				if(version < 4.0) {
					params.me->ThrowException(parser, "Unsupported version of 3DXML. Supported versions are 4.0 and later.");
				}
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;

		parser->ParseElement(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	// Read the product structure section
	void _3DXMLParser::ReadProductStructure(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Choice<Params> mapping(([](){
			XMLParser::XSD::Choice<Params>::type map;

			// Parse Reference3D element
			map.emplace("Reference3D", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadReference3D(parser);}, 0, XMLParser::XSD::unbounded));

			// Parse Instance3D element
			map.emplace("Instance3D", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadInstance3D(parser);}, 0, XMLParser::XSD::unbounded));

			// Parse ReferenceRep element
			map.emplace("ReferenceRep", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadReferenceRep(parser);}, 0, XMLParser::XSD::unbounded));

			// Parse InstanceRep element
			map.emplace("InstanceRep", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadInstanceRep(parser);}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, XMLParser::XSD::unbounded);

		mContent.ref_root_index = parser->GetAttribute<unsigned int>("root");

		params.me = this;

		parser->ParseElement(mapping, params);
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the Reference3D section
	void _3DXMLParser::ReadReference3D(const XMLParser* parser) {
		struct Params {
			Optional<std::string> name_opt;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.name_opt = parser->GetAttribute<std::string>("name");
		unsigned int id = *(parser->GetAttribute<unsigned int>("id", true));

		parser->ParseElement(mapping, params);

		_3DXMLStructure::Reference3D& ref = mContent.references_node[_3DXMLStructure::ID(parser->GetFilename(), id)]; // Create the Reference3D if not present.
				
		// Save id and name for future error / log messages
		ref.id = id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			ref.name = *(params.name_opt);
			ref.has_name = true;
		} else {
			// No name: take the id as the name
			ref.name = parser->ToString(id);
			ref.has_name = false;
		}

		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The Reference3D will be completed by the Instance3D and InstanceRep
	}

	// ------------------------------------------------------------------------------------------------
	// Read the Instance3D section
	void _3DXMLParser::ReadInstance3D(const XMLParser* parser) {
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
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));

			// Parse IsAggregatedBy element
			map.emplace_back("IsAggregatedBy", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.aggregated_by = *(parser->GetContent<unsigned int>(true));
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string uri = *(parser->GetContent<std::string>(true));

				// Parse the URI to get its different components
				params.me->ParseURI(parser, uri, params.instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(params.instance_of.external && params.instance_of.id &&
						params.instance_of.filename.compare(parser->GetFilename()) != 0 &&
						params.me->mContent.references_node.find(_3DXMLStructure::ID(params.instance_of.filename, *(params.instance_of.id))) == params.me->mContent.references_node.end()) {

					params.me->mContent.files_to_parse.emplace(params.instance_of.filename);
				}
			}, 1, 1));

			// Parse RelativeMatrix element
			map.emplace_back("RelativeMatrix", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string relative_matrix = *(parser->GetContent<std::string>(true));

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
		params.name_opt = parser->GetAttribute<std::string>("name");
		params.instance.id = *(parser->GetAttribute<unsigned int>("id", true));

		parser->ParseElement(mapping, params);

		// Test if the name exist, otherwise use the id as name
		std::string name;
		if(params.name_opt) {
			params.instance.node->mName = *(params.name_opt);
			params.instance.has_name = true;
		} else {
			// No name: take the id as the name
			params.instance.node->mName = parser->ToString(params.instance.id);
			params.instance.has_name = false;
		}

		// Save the information corresponding to this instance
		if(params.instance_of.id) {
			// Create the refered Reference3D if necessary
			params.instance.instance_of = &(mContent.references_node[_3DXMLStructure::ID(params.instance_of.filename, *(params.instance_of.id))]);

			// Update the number of instances of this Reference3D
			params.instance.instance_of->nb_references++;
		} else {
			ThrowException(parser, "In Instance3D \"" + parser->ToString(params.instance.id) + "\": the instance refers to an invalid reference \"" + params.instance_of.uri + "\" without id.");
		}

		// Save the reference to the parent Reference3D
		_3DXMLStructure::Reference3D& parent = params.me->mContent.references_node[_3DXMLStructure::ID(parser->GetFilename(), params.aggregated_by)];
				
		// Insert the instance into the aggregating Reference3D
		// The ownership of the node pointer owned by std::unique_ptr is automatically transfered to the inserted instance
		auto result = parent.instances.emplace(_3DXMLStructure::ID(parser->GetFilename(), params.instance.id), std::move(params.instance));

		if(! result.second) {
			ThrowException(parser, "In Instance3D \"" + parser->ToString(params.instance.id) + "\": the instance is already aggregated by the Reference3D \"" + parser->ToString(params.aggregated_by) + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the ReferenceRep section
	void _3DXMLParser::ReadReferenceRep(const XMLParser* parser) {
		struct Params {
			Optional<std::string> name_opt;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));

			return std::move(map);
		})(), 1, 1);

		params.name_opt = parser->GetAttribute<std::string>("name");
		unsigned int id = *(parser->GetAttribute<unsigned int>("id", true));
		std::string format = *(parser->GetAttribute<std::string>("format", true));
		std::string file = *(parser->GetAttribute<std::string>("associatedFile", true));
		_3DXMLStructure::URI uri;

		parser->ParseElement(mapping, params);

		// Parse the external URI to the file containing the representation
		ParseURI(parser, file, uri);
		if(! uri.external) {
			ThrowException(parser, "In ReferenceRep \"" + parser->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Get the container for this representation
		_3DXMLStructure::ReferenceRep& rep = mContent.representations[_3DXMLStructure::ID(parser->GetFilename(), id)];
		rep.id = id;
		rep.meshes.clear();
		rep.indexes.clear();

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
				ThrowException(parser, "In ReferenceRep \"" + parser->ToString(id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
			}
		} else {
			ThrowException(parser, "In ReferenceRep \"" + parser->ToString(id) + "\": unsupported representation format \"" + format + "\".");
		}

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			rep.name = *(params.name_opt);
			rep.has_name = true;
		} else {
			// No name: take the id as the name
			rep.name = parser->ToString(id);
			rep.has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void _3DXMLParser::ReadInstanceRep(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
			_3DXMLStructure::InstanceRep* mesh;
			unsigned int id;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));

			// Parse IsAggregatedBy element
			map.emplace_back("IsAggregatedBy", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				unsigned int aggregated_by = *(parser->GetContent<unsigned int>(true));

				// Save the reference to the parent Reference3D
				_3DXMLStructure::Reference3D& parent = params.me->mContent.references_node[_3DXMLStructure::ID(parser->GetFilename(), aggregated_by)];
				params.mesh = &(parent.meshes[_3DXMLStructure::ID(parser->GetFilename(), params.id)]);

				// Save id for future error / log message
				params.mesh->id = params.id;
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string uri = *(parser->GetContent<std::string>(true));
				_3DXMLStructure::URI instance_of;

				// Parse the URI to get its different components
				params.me->ParseURI(parser, uri, instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(instance_of.external && instance_of.id &&
						instance_of.filename.compare(parser->GetFilename()) != 0 &&
						params.me->mContent.references_node.find(_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))) == params.me->mContent.references_node.end()) {

					params.me->mContent.files_to_parse.emplace(instance_of.filename);
				}

				if(instance_of.id) {
					// Create the refered ReferenceRep if necessary
					params.mesh->instance_of = &(params.me->mContent.representations[_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))]);
				} else {
					params.me->ThrowException(parser, "In InstanceRep \"" + parser->ToString(params.id) + "\": the uri \"" + uri + "\" has no id component.");
				}
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = parser->GetAttribute<std::string>("name");
		params.id = *(parser->GetAttribute<unsigned int>("id", true));
		params.mesh = nullptr;

		parser->ParseElement(mapping, params);

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			params.mesh->name = *(params.name_opt);
			params.mesh->has_name = true;
		} else {
			// No name: take the id as the name
			params.mesh->name = parser->ToString(params.id);
			params.mesh->has_name = false;
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterialRef section
	void _3DXMLParser::ReadCATMaterialRef(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Choice<Params> mapping(([](){
			XMLParser::XSD::Choice<Params>::type map;

			// Parse CATMatReference element
			map.emplace("CATMatReference", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATMatReference(parser);}, 0, XMLParser::XSD::unbounded));

			// Parse MaterialDomain element
			map.emplace("MaterialDomain", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadMaterialDomain(parser);}, 0, XMLParser::XSD::unbounded));

			// Parse MaterialDomainInstance element
			map.emplace("MaterialDomainInstance", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadMaterialDomainInstance(parser);}, 0, XMLParser::XSD::unbounded));

			return std::move(map);
		})(), 1, XMLParser::XSD::unbounded);

		DefaultLogger::get()->warn("CATMaterialRef: start.");

		mContent.mat_root_index = parser->GetAttribute<unsigned int>("root");

		params.me = this;

		parser->ParseElement(mapping, params);
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMatReference section
	void _3DXMLParser::ReadCATMatReference(const XMLParser* parser) {
		struct Params {
			Optional<std::string> name_opt;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.name_opt = parser->GetAttribute<std::string>("name");
		unsigned int id = *(parser->GetAttribute<unsigned int>("id", true));

		parser->ParseElement(mapping, params);

		_3DXMLStructure::CATMatReference& ref = mContent.references_mat[_3DXMLStructure::ID(parser->GetFilename(), id)]; // Create the CATMaterialRef if not present.
		
		// Save id and name for future error / log messages
		ref.id = id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			ref.name = *(params.name_opt);
			ref.has_name = true;
		} else {
			// No name: take the id as the name
			ref.name = parser->ToString(id);
			ref.has_name = false;
		}
		
		DefaultLogger::get()->warn("CATMaterialRef: reference \"" + ref.name + "\" (" + parser->ToString(ref.id) + ").");

		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The CATMaterialRef will be completed by the MaterialDomainInstance
	}

	// ------------------------------------------------------------------------------------------------
	// Read the MaterialDomain section
	void _3DXMLParser::ReadMaterialDomain(const XMLParser* parser) {
		struct Params {
			Optional<std::string> name_opt;
			bool rendering;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));

			// Parse V_MatDomain element
			map.emplace_back("V_MatDomain", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string domain = *(parser->GetContent<std::string>(true));

				params.rendering = (domain.compare("Rendering") == 0);
			}, 0, 1));

			return std::move(map);
		})(), 1, 1);

		params.rendering = false;
		params.name_opt = parser->GetAttribute<std::string>("name");
		unsigned int id = *(parser->GetAttribute<unsigned int>("id", true));
		std::string format = *(parser->GetAttribute<std::string>("format", true));
		std::string file = *(parser->GetAttribute<std::string>("associatedFile", true));
		_3DXMLStructure::URI uri;

		parser->ParseElement(mapping, params);

		// Parse the external URI to the file containing the representation
		ParseURI(parser, file, uri);
		if(! uri.external) {
			ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Get the container for this representation
		_3DXMLStructure::MaterialDomain& mat = mContent.materials[_3DXMLStructure::ID(parser->GetFilename(), id)];
		mat.id = id;

		// Check the representation format and call the correct parsing function accordingly
		if(params.rendering) {
			if(format.compare("TECHREP") == 0) {
				if(uri.extension.compare("3DRep") == 0) {
					_3DXMLMaterial material(mArchive, uri.filename, mat.material.get());
				} else {
					ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
				}
			} else {
				ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": unsupported representation format \"" + format + "\".");
			}
		} else {
			//ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": unsupported material domain.");
		}

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			mat.name = *(params.name_opt);
			mat.has_name = true;
		} else {
			// No name: take the id as the name
			mat.name = parser->ToString(id);
			mat.has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the MaterialDomainInstance section
	void _3DXMLParser::ReadMaterialDomainInstance(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
			Optional<std::string> name_opt;
			_3DXMLStructure::MaterialDomainInstance* material;
			unsigned int id;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse PLM_ExternalID element
			map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));

			// Parse IsAggregatedBy element
			map.emplace_back("IsAggregatedBy", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				unsigned int aggregated_by = *(parser->GetContent<unsigned int>(true));

				// Save the reference to the parent Reference3D
				_3DXMLStructure::CATMatReference& parent = params.me->mContent.references_mat[_3DXMLStructure::ID(parser->GetFilename(), aggregated_by)];
				params.material = &(parent.materials[_3DXMLStructure::ID(parser->GetFilename(), params.id)]);

				// Save id for future error / log message
				params.material->id = params.id;
			}, 1, 1));

			// Parse IsInstanceOf element
			map.emplace_back("IsInstanceOf", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string uri = *(parser->GetContent<std::string>(true));
				_3DXMLStructure::URI instance_of;

				// Parse the URI to get its different components
				params.me->ParseURI(parser, uri, instance_of);

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(instance_of.external && instance_of.id &&
						instance_of.filename.compare(parser->GetFilename()) != 0 &&
						params.me->mContent.references_mat.find(_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))) == params.me->mContent.references_mat.end()) {

					params.me->mContent.files_to_parse.emplace(instance_of.filename);
				}

				if(instance_of.id) {
					// Create the refered ReferenceRep if necessary
					params.material->instance_of = &(params.me->mContent.materials[_3DXMLStructure::ID(instance_of.filename, *(instance_of.id))]);
				} else {
					params.me->ThrowException(parser, "In MaterialDomainInstance \"" + parser->ToString(params.id) + "\": the uri \"" + uri + "\" has no id component.");
				}
			}, 1, 1));

			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.name_opt = parser->GetAttribute<std::string>("name");
		params.id = *(parser->GetAttribute<unsigned int>("id", true));
		params.material = nullptr;

		parser->ParseElement(mapping, params);

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			params.material->name = *(params.name_opt);
			params.material->has_name = true;
		} else {
			// No name: take the id as the name
			params.material->name = parser->ToString(params.id);
			params.material->has_name = false;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterial section
	void _3DXMLParser::ReadCATMaterial(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse CATMatConnection element
			map.emplace_back("CATMatConnection", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATMatConnection(parser);}, 0, 1));

			return std::move(map);
		})(), 1, XMLParser::XSD::unbounded);

		params.me = this;

		parser->ParseElement(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMatConnection section
	void _3DXMLParser::ReadCATMatConnection(const XMLParser* parser) {
		enum Role {TOREFERENCE, MADEOF, DRESSBY};

		struct Params {
			_3DXMLParser* me;
			//Optional<std::string> name_opt;
			_3DXMLStructure::CATMatConnection* connection;
			Role current_role;
			unsigned int id;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;
			
			// Parse PLM_ExternalID element
			//map.emplace_back("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				// We actually don't care about the name of this CATMatConnection
			//	params.name_opt = parser->GetContent<std::string>(true);
			//}, 0, 1));
			
			// Parse IsAggregatedBy element
			//map.emplace_back("IsAggregatedBy", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				// We also don't care about who aggregate this CATMatConnection
			//	unsigned int aggregated_by = *(parser->GetContent<unsigned int>(true));
			//}, 1, 1));
			
			// Parse PLMRelation element
			map.emplace_back("PLMRelation", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				static const XMLParser::XSD::Sequence<Params> mapping(([](){
					XMLParser::XSD::Sequence<Params>::type map;

					// Parse C_Semantics element
					map.emplace_back("C_Semantics", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
						std::string semantic = *(parser->GetContent<std::string>(true));

						if(semantic.compare("Reference") != 0) {
							params.me->ThrowException(parser, "In PLMRelation of CATMatConnection \"" + parser->ToString(params.id) + "\": unknown semantic type \"" + semantic + "\".");
						}
					}, 1, 1));

					// Parse C_Role element
					map.emplace_back("C_Role", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
						static const std::map<std::string, Role> mapping(([](){
							std::map<std::string, Role> map;

							map.emplace("CATMaterialToReferenceLink", TOREFERENCE);
							map.emplace("CATMaterialMadeOfLink", MADEOF);
							map.emplace("CATMaterialDressByLink", DRESSBY);

							return std::move(map);
						})());

						std::string role = *(parser->GetContent<std::string>(true));

						std::map<std::string, Role>::const_iterator it = mapping.find(role);

						if(it != mapping.end()) {
							params.current_role = it->second;
						} else {
							params.me->ThrowException(parser, "In PLMRelation of CATMatConnection \"" + parser->ToString(params.id) + "\": unknown role type \"" + role + "\".");
						}
					}, 1, 1));

					// Parse Ids element
					map.emplace_back("Ids", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
						static const XMLParser::XSD::Sequence<Params> mapping(([](){
							XMLParser::XSD::Sequence<Params>::type map;

							// Parse id element
							map.emplace_back("id", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
								std::string id = *(parser->GetContent<std::string>(true));

								_3DXMLStructure::URI uri;
								ParseURI(parser, id, uri);

								if(! uri.id) {
									params.me->ThrowException(parser, "In PLMRelation of CATMatConnection \"" + parser->ToString(params.id) + "\": the reference \"" + id + "\" has no id.");
								}

								switch(params.current_role) {
									case TOREFERENCE:
										params.connection->references.emplace_back(uri.filename, *(uri.id));
										break;
									case MADEOF:
									case DRESSBY:
										params.connection->materials.emplace_back(uri.filename, *(uri.id));
										break;
									default:
										params.me->ThrowException(parser, "In PLMRelation of CATMatConnection \"" + parser->ToString(params.id) + "\": invalid current role.");
								}
							}, 1, 1));

							return std::move(map);
						})(), 1, XMLParser::XSD::unbounded);

						parser->ParseElement(mapping, params);
					}, 1, 1));

					return std::move(map);
				})(), 1, 1);

				parser->ParseElement(mapping, params);
			}, 1, XMLParser::XSD::unbounded));
			
			// Parse V_Layer element
			map.emplace_back("V_Layer", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.connection->channel = *(parser->GetContent<unsigned int>(true)) - 1;
			}, 1, 1));
			
			// Parse V_Applied element
			/*map.emplace_back("V_Applied", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				// 1: MadeOf, 2: DressBy
				unsigned int applied = *(parser->GetContent<unsigned int>(true));

				Role applied_role;
				switch(applied) {
					case 1:
						applied_role = MADEOF;
						break;
					case 2:
						applied_role = DRESSBY;
						break;
					default:
						params.me->ThrowException(parser, "In CATMatConnection \"" + parser->ToString(params.id) + "\": invalid applied type \"" + parser->ToString(applied) + "\".");
				}
			}, 1, 1));*/

			// TODO: V_Matrix_1 .. V_Matrix_12, content: float

			return std::move(map);
		})(), 1, 1);

		mContent.mat_connections.emplace_back();

		params.me = this;
		//params.name_opt = parser->GetAttribute<std::string>("name");
		params.connection = &(mContent.mat_connections.back());
		params.id = *(parser->GetAttribute<unsigned int>("id", true));

		parser->ParseElement(mapping, params);
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
