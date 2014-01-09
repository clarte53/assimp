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
#include "HighResProfiler.h"
#include "Q3BSPZipArchive.h"
#include "SceneCombiner.h"

namespace Assimp {

	const unsigned int _3DXMLParser::mixed_material_index = std::numeric_limits<unsigned int>::max();

	// ------------------------------------------------------------------------------------------------
	// Constructor to be privately used by Importer
	_3DXMLParser::_3DXMLParser(const std::string& file, aiScene* scene) : mWorkers(), mTasks(), mCondition(), mMutex(), mError(""), mFinished(false), mArchive(new Q3BSP::Q3BSPZipArchive(file)), mContent(scene, &mCondition) { PROFILER;
		// Load the compressed archive
		if (! mArchive->isOpen()) {
			ThrowException(nullptr, "Failed to open file " + file + "." );
		}

		// Create a xml parser for the manifest
		std::unique_ptr<XMLParser> parser(new XMLParser(mArchive, "Manifest.xml"));

		// Read the name of the main XML file in the manifest
		std::string main_file;
		ReadManifest(parser.get(), main_file);

		// Free the parser
		parser.reset();

		// Add the main file to the list of files to parse
		mContent.dependencies.add(main_file);

		// Add the material reference file to the list of file to parse if it exist
		std::string mat_file = "CATMaterialRef.3dxml";
		if(mArchive->Exists(mat_file.c_str())) {
			mContent.dependencies.add(mat_file);
		}

		// Add the image reference file to the list of file to parse if it exist
		std::string img_file = "CATRepImage.3dxml";
		if(mArchive->Exists(img_file.c_str())) {
			mContent.dependencies.add(img_file);
		}


		std::size_t nb_threads = std::thread::hardware_concurrency();

		if(nb_threads == 0) {
			nb_threads = 1;
		}

		mFinished = false;

		mWorkers.resize(nb_threads);
		for(std::size_t index = 0; index < nb_threads; ++index) {
			mWorkers[index].second = false;
		}
		for(std::size_t index = 0; index < nb_threads; ++index) {
			mWorkers[index].first = std::thread([this, index]() {
				bool finished_global = false;
				bool finished;

				// While there is still work to do
				while(! finished_global) {
					// Task to execute
					std::function<void()> task;

					finished = true;

					// Check if we have some unresolved dependencies
					std::string filename = mContent.dependencies.next();

					if(filename != "") {
						task = [this, filename]() {
							// Create a xml parser
							std::unique_ptr<XMLParser> parser(new XMLParser(mArchive, filename));

							// Parse the 3DXML file
							//TODO: make sure the different sections can not be accessed concurrently (should not happen, but who knows...)
							ReadFile(parser.get());
						};

						finished = false;
					}

					// No dependencies? Get the next pending task
					if(finished) {
						std::unique_lock<std::mutex> lock(mMutex);
							if(! mTasks.empty()) {
								task = mTasks.front();
								mTasks.pop();

								finished = false;
							}
						lock.unlock();
					}

					if(! finished) {
						try {
							// Do the actual work
							task();
						} catch(DeadlyImportError& error) {
							std::unique_lock<std::mutex> lock(mMutex);
								// Record the error message
								mError = error.what();
							
								// Stop all the threads
								mFinished = true;

								// Notify all the sleeping threads
								mCondition.notify_all();
							lock.unlock();
						}
					} else {
						// check whether everyone as finished
						std::unique_lock<std::mutex> lock(mMutex);
							// Set the state of this worker to 'finished'
							mWorkers[index].second = true;

							if(! mFinished) {
								finished = true; // just to be sure
								for(std::size_t i = 0; i < mWorkers.size(); ++i) {
									if(! mWorkers[i].second) {
										finished = false;
										break;
									}
								}

								mFinished = finished;
							}

							finished_global = mFinished;
						lock.unlock();

						if(finished_global) {
							// Warn all the sleeping workers that the task is done
							mCondition.notify_all();
						} else {
							std::unique_lock<std::mutex> lock(mMutex);
								// Sleep until some work becomes available
								mCondition.wait(lock);

								// Reset the state of this worker to signal that we started to work again
								mWorkers[index].second = false;
							lock.unlock();
						}
					}

					// Just get the global finished flag
					// We could have used std::atomic<bool> for mFinished instead, but as usual Visual Studio is a shitty compiler when it comes to C++11
					std::unique_lock<std::mutex> lock(mMutex);
						finished_global = mFinished;
					lock.unlock();
				}
			});
		}

		// Wait for all the workers to finish their work
		for(std::size_t i = 0; i < mWorkers.size(); ++i) {
			mWorkers[i].first.join();
		}

		// Check for a potential error to propagate
		if(mError != "") {
			throw DeadlyImportError(mError);
		}

		// Construct the materials & meshes from the parsed data
		BuildMaterials(parser.get());

		// Create the root node
		BuildRoot(parser.get(), main_file);
	}

	_3DXMLParser::~_3DXMLParser() { PROFILER;

	}

	// ------------------------------------------------------------------------------------------------
	// Parse one uri and split it into it's different components
	void _3DXMLParser::ParseURI(const XMLParser* parser, const std::string& uri, _3DXMLStructure::URI& result) { PROFILER;
		static const unsigned int size_prefix = 10;

		result.uri = uri;

		if(uri.substr(0, size_prefix).compare("urn:3DXML:") == 0) {
			result.external = true;

			std::size_t begin = uri.find_last_of(':');
			std::size_t end = uri.find_last_of('#');
			std::size_t ext = uri.find_last_of('.');

			if(begin == uri.npos) {
				ThrowException(parser, "The URI \"" + uri + "\" has an invalid format.");
			}

			if(end != uri.npos && end > ext) {
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
	void _3DXMLParser::ParseExtension(const std::string& filename, std::string& extension) { PROFILER;
		std::size_t pos = filename.find_last_of('.');

		if(pos != filename.npos) {
			extension = filename.substr(pos + 1, filename.npos);
		} else {
			extension = "";
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Parse one numerical id from a string
	void _3DXMLParser::ParseID(const std::string& data, unsigned int& id) { PROFILER;
		std::istringstream stream(data);

		stream >> id;
	}

	// ------------------------------------------------------------------------------------------------
	// Thread safe logging
	void _3DXMLParser::LogMessage(Logger::ErrorSeverity type, const std::string& message) {
		static std::mutex mutex;

		std::unique_lock<std::mutex> lock(mutex);

		switch(type) {
			case Logger::Err:
				DefaultLogger::get()->error(message);
				break;
			case Logger::Warn:
				DefaultLogger::get()->warn(message);
				break;
			case Logger::Info:
				DefaultLogger::get()->info(message);
				break;
			case Logger::Debugging:
				DefaultLogger::get()->debug(message);
				break;
		}

		lock.unlock();
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLParser::ThrowException(const XMLParser* parser, const std::string& error) { PROFILER;
		if(parser != nullptr) {
			throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % parser->GetFilename() % error));
		} else {
			throw DeadlyImportError(boost::str(boost::format("3DXML: %s") % error));
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Construct a new colored material
	void _3DXMLParser::BuildColorMaterial(std::unique_ptr<aiMaterial>& material, const std::string& name, const aiColor4D& color) {
		material.reset(new aiMaterial());

		aiString name_str(name);
		material->AddProperty(&name_str, AI_MATKEY_NAME);

		material->AddProperty(&color, 1, AI_MATKEY_COLOR_AMBIENT);
		material->AddProperty(&color, 1, AI_MATKEY_COLOR_DIFFUSE);

		if(color.a != 1.0) {
			// Also add transparency
			material->AddProperty(&color.a, 1, AI_MATKEY_OPACITY);
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Construct the materials from the parsed data
	void _3DXMLParser::BuildMaterials(const XMLParser* parser) { PROFILER;
		// Just because Microsoft Visual 2012 does not support (yet another) feature of C++11 (ie initialization lists),
		// reader beware, you will see this hack a lot in 3DXML related classes.
		//TODO: switch to a more readable syntax the day Microsoft will do their work correctly (so you should probably just forget about this TODO)
		static const std::vector<aiTextureType> supported_textures = [](){
			std::vector<aiTextureType> texture_types;

			texture_types.push_back(aiTextureType_DIFFUSE);
			texture_types.push_back(aiTextureType_REFLECTION);

			return std::move(texture_types);
		}();
	
		// Add the textures to the scene
		unsigned int index_tex = 0;
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::CATRepresentationImage>::iterator it_tex(mContent.textures.begin()), end_tex(mContent.textures.end()); it_tex != end_tex; ++it_tex, ++index_tex) { PROFILER;
			it_tex->second.index = index_tex;
			mContent.scene->Textures.Set(index_tex, it_tex->second.texture.release());
		}

		// Merge all the materials composing a CATMatReference into a single material
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::CATMatReference>::iterator it_ref(mContent.references_mat.begin()), end_ref(mContent.references_mat.end()); it_ref != end_ref; ++it_ref) { PROFILER;
			// Is the merged material already computed?
			if(! it_ref->second.merged_material) {
				// List of the different instances of materials referenced by this CATMatReference
				// (Normally, in schema 4.3 it should be only 1 instance, but we have no garantees that it will remain the case in future versions.
				// Therefore we merge the different instanciated materials together)
				std::vector<aiMaterial*> mat_list_ref;

				// Get all the instances
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::MaterialDomainInstance>::const_iterator it_dom(it_ref->second.materials.begin()), end_dom(it_ref->second.materials.end()); it_dom != end_dom; ++it_dom) {
					if(it_dom->second.instance_of->material) {
						mat_list_ref.push_back(it_dom->second.instance_of->material.get());
					}
				}

				// If we have at least one instance
				if(! mat_list_ref.empty()) {
					// Merge the different materials together
					aiMaterial* material_ptr = NULL;
					SceneCombiner::MergeMaterials(&material_ptr, mat_list_ref.begin(), mat_list_ref.end());

					// Save the merged material
					it_ref->second.merged_material.reset(material_ptr);

					// Set the texture to use the correct index in the scene
					for(unsigned int i = 0; i < supported_textures.size(); ++i) {
						aiString texture_name;
						if(material_ptr->Get(_AI_MATKEY_TEXTURE_BASE, supported_textures[i], 0, texture_name) == AI_SUCCESS) {
							material_ptr->RemoveProperty(_AI_MATKEY_TEXTURE_BASE, supported_textures[i], 0);

							_3DXMLStructure::URI uri;

							ParseURI(parser, texture_name.C_Str(), uri);

							if(uri.id) {
								std::map<_3DXMLStructure::ID, _3DXMLStructure::CATRepresentationImage>::iterator it_img = mContent.textures.find(_3DXMLStructure::ID(uri.filename, *uri.id));

								if(it_img != mContent.textures.end()) {
									aiString texture_index;
									texture_index.data[0] = '*'; // Special prefix for embeded textures
									texture_index.length = 1 + ASSIMP_itoa10(texture_index.data + 1, MAXLEN - 1, it_img->second.index);

									material_ptr->AddProperty(&texture_index, _AI_MATKEY_TEXTURE_BASE, supported_textures[i], 0);
								} else {
									ThrowException(parser, "In CATMatReference \"" + parser->ToString(it_ref->second.id) + "\": texture \"" + uri.uri + "\" not found.");
								}
							} else {
								ThrowException(parser, "In CATMatReference \"" + parser->ToString(it_ref->second.id) + "\": invalid reference to texture \"" + uri.uri + "\" without id.");
							}
						}
					}

					// Save the name of the material
					aiString name;
					if(it_ref->second.has_name) {
						name = it_ref->second.name;
					} else {
						name = parser->ToString(it_ref->second.id);
					}
					material_ptr->AddProperty(&name, AI_MATKEY_NAME);
				} else {
					it_ref->second.merged_material.reset(nullptr);

					LogMessage(Logger::Err, "In CATMatReference \"" + parser->ToString(it_ref->second.id) + "\": no materials defined.");
				}
			}
		}

		// Update the ReferenceRep to share the same MaterialAttributes
		std::set<_3DXMLStructure::MaterialAttributes::ID, _3DXMLStructure::shared_less<_3DXMLStructure::MaterialAttributes>> mat_attributes;
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::ReferenceRep>::iterator it_rep(mContent.representations.begin()), end_rep(mContent.representations.end()); it_rep != end_rep; ++it_rep) {
			for(_3DXMLStructure::ReferenceRep::Meshes::iterator it_mesh(it_rep->second.meshes.begin()), end_mesh(it_rep->second.meshes.end()); it_mesh != end_mesh; /* increment depends on content */) {
				// Set the names of the parsed meshes with this ReferenceRep name
				it_mesh->second.mesh->mName = it_rep->second.name;

				// Check if the surface attributes already exist
				std::set<_3DXMLStructure::MaterialAttributes::ID>::iterator it = mat_attributes.find(it_mesh->first);

				if(it == mat_attributes.end()) {
					// Add the MaterialAttribute to the list of existing attributes
					std::pair<std::set<_3DXMLStructure::MaterialAttributes::ID>::iterator, bool> result = mat_attributes.insert(it_mesh->first);

					if(! result.second) {
						ThrowException(parser, "In ReferenceRep \"" + parser->ToString(it_rep->second.id) + "\": impossible to add the new material attributes.");
					}

					++it_mesh;
				} else if(it_mesh->first == *it) {
					// Already done? Just need to move to the next element
					++it_mesh;
				} else {
					// Set the current material attributes to be a reference of the shared MaterialAttributes
					_3DXMLStructure::ReferenceRep::Geometry geometry(std::move(it_mesh->second));

					// Erase already return the next element, so no need to increment
					it_mesh = it_rep->second.meshes.erase(it_mesh);

					it_rep->second.meshes.emplace(*it, std::move(geometry));
				}
			}
		}

		// Generate the final materials of the scene and store their indices
		unsigned int generated_mat_counter = 1;
		unsigned int color_mat_counter = 1;
		for(auto it_mat(mat_attributes.begin()), end_mat(mat_attributes.end()); it_mat != end_mat; ++ it_mat) { PROFILER;
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
							if(it_mat_ref->second.merged_material) {
								mat_list_app.emplace_back(std::make_pair(&(*it_app), it_mat_ref->second.merged_material.get()));
							}
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

							// Set the correct channel for this material
							// TODO: do it only for relevant properties?
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
					
						if(! mat_list_final.empty()) {
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
						} else {
							// Generate a substitute material
							BuildColorMaterial(material, "Generated material " + parser->ToString(generated_mat_counter++), aiColor4D(0.5, 0.5, 0.5, 1.0));
						}
					} catch(...) {
						// Cleaning up memory
						for(std::vector<aiMaterial*>::iterator it(mat_list_final.begin()), end(mat_list_final.end()); it != end; ++it) {
							delete *it;
						}
						mat_list_final.clear();

						// Rethrow the exception
						throw;
					}
				}
				
				// If a color is defined
				if((*it_mat)->is_color) {
					if(material) {
						// We use the color as the ambient component of the defined material
						material->RemoveProperty(AI_MATKEY_COLOR_AMBIENT);
						material->AddProperty(&((*it_mat)->color), 1, AI_MATKEY_COLOR_AMBIENT);
					} else {
						// We generate a new material based on the color
						BuildColorMaterial(material, "Color Material " + parser->ToString(color_mat_counter++), (*it_mat)->color);
					}
				}
			} else {
				// Default material
				BuildColorMaterial(material, "Default material", aiColor4D(0.5, 0.5, 0.5, 1.0));
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

		// Create a map of the different Instance3D for direct access
		std::map<_3DXMLStructure::ID, _3DXMLStructure::Instance3D*> instances;
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::Reference3D>::iterator it_ref(mContent.references_node.begin()), end_ref(mContent.references_node.end()); it_ref != end_ref; ++it_ref) { PROFILER;
			for(std::map<_3DXMLStructure::ID, _3DXMLStructure::Instance3D>::iterator it_inst(it_ref->second.instances.begin()), end_inst(it_ref->second.instances.end()); it_inst != end_inst; ++it_inst) {
				instances.emplace(it_inst->first, &(it_inst->second));
			}
		}

		// Build the materials defined in the CATMaterial section
		std::map<std::list<_3DXMLStructure::ID>, unsigned int, _3DXMLStructure::list_less<_3DXMLStructure::ID>> mat_con_indexes;
		for(std::list<_3DXMLStructure::CATMatConnection>::const_iterator it_con(mContent.mat_connections.begin()), end_con(mContent.mat_connections.end()); it_con != end_con; ++it_con) { PROFILER;
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
			for(std::list<_3DXMLStructure::ID>::const_iterator it_ref(it_con->references.begin()), end_ref(it_con->references.end()); it_ref != end_ref; ++it_ref) { PROFILER;
				std::map<_3DXMLStructure::ID, _3DXMLStructure::Instance3D*>::iterator it_inst = instances.find(*it_ref);

				if(it_inst != instances.end()) {
					it_inst->second->material_index = Optional<unsigned int>(index);
				} else {
					ThrowException(parser, "Invalid CATMatConnection referencing unknown Instance3D \"" + parser->ToString(it_ref->id) + "\".");
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Count the number of materials instantiated for each geometry
	void _3DXMLParser::BuildMaterialCount(const XMLParser* parser, _3DXMLStructure::Reference3D& ref, std::map<_3DXMLStructure::ReferenceRep*, std::set<unsigned int>>& materials_per_geometry, Optional<unsigned int> material_index) { PROFILER;
		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::InstanceRep>::const_iterator it_mesh(ref.meshes.begin()), end_mesh(ref.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
			const _3DXMLStructure::InstanceRep& rep = it_mesh->second;

			if(rep.instance_of != nullptr) {
				// Get the index of material to use
				unsigned int index_mat = mixed_material_index;
				if(material_index) {
					index_mat = *material_index;
				}

				materials_per_geometry[rep.instance_of].insert(index_mat);
			} else {
				ThrowException(parser, "One InstanceRep of Reference3D \"" + parser->ToString(ref.id) + "\" is unresolved.");
			}
		}

		for(std::map<_3DXMLStructure::ID, _3DXMLStructure::Instance3D>::iterator it_child(ref.instances.begin()), end_child(ref.instances.end()); it_child != end_child; ++it_child) {
			_3DXMLStructure::Instance3D& child = it_child->second;

			if(child.instance_of != nullptr) {
				if(! child.material_index) {
					child.material_index = material_index;
				}

				BuildMaterialCount(parser, *child.instance_of, materials_per_geometry, child.material_index);
			} else {
				ThrowException(parser, "One Instance3D of Reference3D \"" + parser->ToString(ref.id) + "\" is unresolved.");
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Add the meshes to the scene
	void _3DXMLParser::BuildMeshes(const XMLParser* parser, _3DXMLStructure::ReferenceRep& rep, unsigned int material_index) { PROFILER;
		std::map<unsigned int, std::list<unsigned int>>::iterator it_indexes = rep.indexes.find(material_index);

		// Are the meshes added to the scene yet?
		if(it_indexes == rep.indexes.end()) {
			// Get the index of this mesh in the scene
			unsigned int index_mesh = mContent.scene->Meshes.Size();

			// Get a reference to the list of indexes to use
			std::list<unsigned int>& list_indexes = rep.indexes[material_index];

			// Decrement the counter of instances to this Reference3D (used for memory managment)
			if(rep.nb_references > 0) {
				rep.nb_references--;
			}

			for(_3DXMLStructure::ReferenceRep::Meshes::iterator it_meshes(rep.meshes.begin()), end_meshes(rep.meshes.end()); it_meshes != end_meshes; ++it_meshes) {
				std::unique_ptr<aiMesh>* mesh = &(it_meshes->second.mesh);

				if(*mesh) {
					// Duplicate the mesh for the new material
					aiMesh* mesh_ptr = NULL;

					if(rep.nb_references == 0) {
						mesh_ptr = mesh->release();
					} else {
						SceneCombiner::Copy(&mesh_ptr, mesh->get());
					}

					// Save the new mesh in the scene
					mContent.scene->Meshes.Set(index_mesh, mesh_ptr);

					// Get the index of the material
					unsigned int index_mat = material_index;
					if(material_index == mixed_material_index) {
						// Get the index of the material defined for this specific mesh
						index_mat = (it_meshes->first ? it_meshes->first->index : 0);
					}

					// Save the index of the material to use
					mesh_ptr->mMaterialIndex = index_mat;

					// Save the index of the mesh depending for the special material index corresponding to mixed materials defined at the mesh level
					list_indexes.push_back(index_mesh);

					// Increment the index for the next mesh
					index_mesh++;
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Create the root node
	void _3DXMLParser::BuildRoot(const XMLParser* parser, const std::string& main_file) { PROFILER;
		if(mContent.ref_root_index) {
			std::map<_3DXMLStructure::ID, _3DXMLStructure::Reference3D>::iterator it_root = mContent.references_node.find(_3DXMLStructure::ID(main_file, *(mContent.ref_root_index)));

			if(it_root != mContent.references_node.end()) {
				_3DXMLStructure::Reference3D& root = it_root->second;

				if(root.nb_references == 0) {
					// mRootNode is contained inside the scene, which is already protected against memory leaks
					mContent.scene->mRootNode = new aiNode(root.name);

					// Set up the global orientation of the scene. In 3DXML, the Z axis is used as the up axis.
					mContent.scene->mRootNode->mTransformation *= aiMatrix4x4(
						 1,  0,  0,  0,
						 0,  0,  1,  0,
						 0, -1,  0,  0,
						 0,  0,  0,  1
					);

					// Set the number of references for each geometry (depending on the instantiated materials)
					std::map<_3DXMLStructure::ReferenceRep*, std::set<unsigned int>> materials_per_geometry;
					BuildMaterialCount(parser, root, materials_per_geometry, Optional<unsigned int>());
					for(std::map<_3DXMLStructure::ReferenceRep*, std::set<unsigned int>>::const_iterator it_rep(materials_per_geometry.begin()), end_rep(materials_per_geometry.end()); it_rep != end_rep; ++it_rep) {
						it_rep->first->nb_references = it_rep->second.size();
					}

					// Build the hierarchy recursively
					BuildStructure(parser, root, mContent.scene->mRootNode, Optional<unsigned int>());
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
	void _3DXMLParser::BuildStructure(const XMLParser* parser, _3DXMLStructure::Reference3D& ref, aiNode* node, Optional<unsigned int> material_index) { PROFILER;
		// Decrement the counter of instances to this Reference3D (used for memory managment)
		if(ref.nb_references > 0) {
			ref.nb_references--;
		}

		if(node != nullptr) {
			// Copy the indexes of the meshes contained into this instance into the proper aiNode
			if(node->mNumMeshes == 0) {
				unsigned int index_node = 0; // Because (node->mNumMeshes == 0)
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::InstanceRep>::const_iterator it_mesh(ref.meshes.begin()), end_mesh(ref.meshes.end()); it_mesh != end_mesh; ++it_mesh) {
					const _3DXMLStructure::InstanceRep& rep = it_mesh->second;

					if(rep.instance_of != nullptr) {
						if(! rep.instance_of->meshes.empty()) {
							// Get the index of material to use
							unsigned int index_mat = mixed_material_index;
							if(material_index) {
								index_mat = *material_index;
							}

							// Add the meshes to the scene if necessary
							BuildMeshes(parser, *(rep.instance_of), index_mat);

							// Get the list of indexes of the meshes for the specified material
							std::map<unsigned int, std::list<unsigned int>>::const_iterator it_list = rep.instance_of->indexes.find(index_mat);

							// Add the indexes of the meshes to the current node
							if(it_list != rep.instance_of->indexes.end() && ! it_list->second.empty()) {
								for(std::list<unsigned int>::const_iterator it_index_mesh(it_list->second.begin()), end_index_mesh(it_list->second.end()); it_index_mesh != end_index_mesh; ++it_index_mesh) {
									node->Meshes.Set(index_node++, *it_index_mesh);
								}
							} else {
								ThrowException(parser, "No mesh corresponds to the given material \"" + parser->ToString(index_mat) + "\".");
							}
						} else {
							// If the representation format is not supported, it is normal to have empty ReferenceRep. Therefore, we should gracefully ignore such nodes.
							LogMessage(Logger::Warn, "No meshes defined in ReferenceRep \"" + parser->ToString(rep.instance_of->id) + "\".");
						}
					} else {
						ThrowException(parser, "One InstanceRep of Reference3D \"" + parser->ToString(ref.id) + "\" is unresolved.");
					}
				}
			}

			// Copy the children nodes of this instance into the proper node
			if(node->mNumChildren == 0) {
				for(std::map<_3DXMLStructure::ID, _3DXMLStructure::Instance3D>::iterator it_child(ref.instances.begin()), end_child(ref.instances.end()); it_child != end_child; ++it_child) {
					_3DXMLStructure::Instance3D& child = it_child->second;

					if(child.node.get() != nullptr && child.instance_of != nullptr) {
						// Test if the node name is an id to see if we better take the Reference3D instead
						if(! child.has_name && child.instance_of->has_name) {
							child.node->mName = child.instance_of->name;
						}

						// Construct the hierarchy recursively
						BuildStructure(parser, *child.instance_of, child.node.get(), child.material_index);

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
	void _3DXMLParser::ReadManifest(const XMLParser* parser, std::string& main_file) { PROFILER;
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
	void _3DXMLParser::ReadFile(const XMLParser* parser) { PROFILER;
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
			map.emplace_back("CATRepImage", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATRepImage(parser);}, 0, 1));

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
	void _3DXMLParser::ReadHeader(const XMLParser* parser) { PROFILER;
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
	void _3DXMLParser::ReadProductStructure(const XMLParser* parser) { PROFILER;
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
	void _3DXMLParser::ReadReference3D(const XMLParser* parser) { PROFILER;
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
	void _3DXMLParser::ReadInstance3D(const XMLParser* parser) { PROFILER;
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

				// Add the reference to the list of dependencies
				params.me->mContent.dependencies.add(params.instance_of.filename);
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
	void _3DXMLParser::ReadReferenceRep(const XMLParser* parser) { PROFILER;
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

		// Get the container for this representation
		_3DXMLStructure::ReferenceRep* rep = &(mContent.representations[_3DXMLStructure::ID(parser->GetFilename(), id)]);
		rep->id = id;
		rep->meshes.clear();
		rep->indexes.clear();

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			rep->name = *(params.name_opt);
			rep->has_name = true;
		} else {
			// No name: take the id as the name
			rep->name = parser->ToString(id);
			rep->has_name = false;
		}

		// Parse the external URI to the file containing the representation
		ParseURI(parser, file, uri);
		if(! uri.external) {
			ThrowException(parser, "In ReferenceRep \"" + parser->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Check the representation format and call the correct parsing function accordingly
		if(format.compare("TESSELLATED") == 0) {
			if(uri.extension.compare("3DRep") == 0) { PROFILER;
				std::unique_lock<std::mutex> lock(mMutex);
					mTasks.emplace([this, rep, uri]() {
						try {
							// Parse the geometry representation
							_3DXMLRepresentation representation(mArchive, uri.filename, rep->meshes, mContent.dependencies);
						} catch(DeadlyImportError& error) {
							std::ostringstream stream;
							stream << "In ReferenceRep \"" << rep->id << "\": unable to load the representation. " << error.what();

							LogMessage(Logger::Err, stream.str());

							rep->meshes.clear();
						}
					});

					mCondition.notify_one();
				lock.unlock();
			} else {
				ThrowException(parser, "In ReferenceRep \"" + parser->ToString(id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
			}
		} else {
			// Unsupported format. We warn the user and ignore it
			LogMessage(Logger::Warn, "In ReferenceRep \"" + parser->ToString(id) + "\": unsupported representation format \"" + format + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the InstanceRep section
	void _3DXMLParser::ReadInstanceRep(const XMLParser* parser) { PROFILER;
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

				// Add the file to the list of dependencies
				params.me->mContent.dependencies.add(instance_of.filename);

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
		if(params.mesh != nullptr) {
			if(params.name_opt) {
				params.mesh->name = *(params.name_opt);
				params.mesh->has_name = true;
			} else {
				// No name: take the id as the name
				params.mesh->name = parser->ToString(params.id);
				params.mesh->has_name = false;
			}
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterialRef section
	void _3DXMLParser::ReadCATMaterialRef(const XMLParser* parser) { PROFILER;
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

		mContent.mat_root_index = parser->GetAttribute<unsigned int>("root");

		params.me = this;

		parser->ParseElement(mapping, params);
	}
	
	// ------------------------------------------------------------------------------------------------
	// Read the CATMatReference section
	void _3DXMLParser::ReadCATMatReference(const XMLParser* parser) { PROFILER;
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

		// Nothing else to do because of the weird indirection scheme of 3DXML
		// The CATMaterialRef will be completed by the MaterialDomainInstance
	}

	// ------------------------------------------------------------------------------------------------
	// Read the MaterialDomain section
	void _3DXMLParser::ReadMaterialDomain(const XMLParser* parser) { PROFILER;
		struct Params {
			Optional<std::string> name_opt;
			bool rendering;
		} params;

		static const XMLParser::XSD::Choice<Params> mapping(([](){ // Fix for Virtools. Schema: Sequence
			XMLParser::XSD::Choice<Params>::type map; // Fix for Virtools. Schema: Sequence

			// Parse PLM_ExternalID element
			map.emplace("PLM_ExternalID", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				params.name_opt = parser->GetContent<std::string>(true);
			}, 0, 1));

			// Parse V_MatDomain element
			map.emplace("V_MatDomain", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string domain = *(parser->GetContent<std::string>(true));

				params.rendering = (domain.compare("Rendering") == 0);
			}, 0, 1));

			return std::move(map);
		})(), 0, 2); // Fix for Virtools. Schema: {1, 1}

		params.rendering = false;
		params.name_opt = parser->GetAttribute<std::string>("name");
		unsigned int id = *(parser->GetAttribute<unsigned int>("id", true));
		std::string format = *(parser->GetAttribute<std::string>("format", true));
		std::string file = *(parser->GetAttribute<std::string>("associatedFile", true));
		_3DXMLStructure::URI uri;

		parser->ParseElement(mapping, params);

		// Get the container for this representation
		_3DXMLStructure::MaterialDomain* mat = &(mContent.materials[_3DXMLStructure::ID(parser->GetFilename(), id)]);
		mat->id = id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			mat->name = *(params.name_opt);
			mat->has_name = true;
		} else {
			// No name: take the id as the name
			mat->name = parser->ToString(id);
			mat->has_name = false;
		}

		// Parse the external URI to the file containing the representation
		ParseURI(parser, file, uri);
		if(! uri.external) {
			ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference another file in the same archive.");
		}

		// Check the representation format and call the correct parsing function accordingly
		if(params.rendering) {
			if(format.compare("TECHREP") == 0) {
				if(uri.extension.compare("3DRep") == 0) { PROFILER;
					std::unique_lock<std::mutex> lock(mMutex);
						mTasks.emplace([this, mat, uri]() {
							try {
								_3DXMLMaterial material(mArchive, uri.filename, mat->material.get(), mContent.dependencies);
							} catch(DeadlyImportError& error) {
								std::ostringstream stream;
								stream << "In MaterialDomain \"" << mat->id << "\": unable to load the material. " << error.what();

								LogMessage(Logger::Err, stream.str());

								mat->material.reset(nullptr);
							}
						});

						mCondition.notify_one();
					lock.unlock();
				} else {
					ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": unsupported extension \"" + uri.extension + "\" for associated file.");
				}
			} else {
				ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": unsupported representation format \"" + format + "\".");
			}
		} else {
			//ThrowException(parser, "In MaterialDomain \"" + parser->ToString(id) + "\": unsupported material domain.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the MaterialDomainInstance section
	void _3DXMLParser::ReadMaterialDomainInstance(const XMLParser* parser) { PROFILER;
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

				// Add the file to the dependencies
				params.me->mContent.dependencies.add(instance_of.filename);

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
		if(params.material != nullptr) {
			if(params.name_opt) {
				params.material->name = *(params.name_opt);
				params.material->has_name = true;
			} else {
				// No name: take the id as the name
				params.material->name = parser->ToString(params.id);
				params.material->has_name = false;
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATRepImage section
	void _3DXMLParser::ReadCATRepImage(const XMLParser* parser) {
		struct Params {
			_3DXMLParser* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse CATRepresentationImage element
			map.emplace_back("CATRepresentationImage", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){params.me->ReadCATRepresentationImage(parser);}, 0, XMLParser::XSD::unbounded));

			return std::move(map);
		})(), 1, 1);

		params.me = this;

		parser->ParseElement(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATRepresentationImage section
	void _3DXMLParser::ReadCATRepresentationImage(const XMLParser* parser) {
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

		// Get the container for this representation
		_3DXMLStructure::CATRepresentationImage* img = &(mContent.textures[_3DXMLStructure::ID(parser->GetFilename(), id)]);
		img->id = id;

		// Test if the name exist, otherwise use the id as name
		if(params.name_opt) {
			img->name = *(params.name_opt);
			img->has_name = true;
		} else {
			// No name: take the id as the name
			img->name = parser->ToString(id);
			img->has_name = false;
		}

		// Parse the external URI to the file containing the representation
		ParseURI(parser, file, uri);
		if(! uri.external) {
			ThrowException(parser, "In CATRepresentationImage \"" + parser->ToString(id) + "\": invalid associated file \"" + file + "\". The field must reference a texture file in the same archive.");
		}

		std::unique_lock<std::mutex> lock(mMutex);
			mTasks.emplace([this, img, uri]() {
				try {
					aiTexture* texture = img->texture.get();

					if(mArchive->isOpen()) {
						if(mArchive->Exists(uri.filename.c_str())) {
							// Open the manifest files
							IOStream* stream = mArchive->Open(uri.filename.c_str());
							if(stream == nullptr) {
								// because Q3BSPZipArchive (now) correctly close all open files automatically on destruction,
								// we do not have to worry about closing the stream explicitly on exceptions

								throw DeadlyImportError(uri.filename + " not found.");
							}

							// Assume compressed textures are used, even if it is not the case.
							// Therefore, the user can load the data directly in whichever way he needs.
							texture->mHeight = 0;
							texture->mWidth = stream->FileSize();

							std::string extension = uri.extension;
							std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

							if(extension.compare("jpeg") == 0) {
								extension = "jpg";
							}

							extension.copy(texture->achFormatHint, 3);
							texture->achFormatHint[3] = '\0';

							texture->pcData = (aiTexel*) new unsigned char[texture->mWidth];
							size_t readSize = stream->Read((void*) texture->pcData, texture->mWidth, 1);

							(void) readSize;
							ai_assert(readSize == texture->mWidth);

							mArchive->Close(stream);
						} else {
							throw DeadlyImportError("The texture file \"" + uri.filename + "\" does not exist in the zip archive.");
						}
					} else {
						throw DeadlyImportError("The zip archive can not be opened.");
					}
				} catch(DeadlyImportError& error) {
					std::ostringstream stream;
					stream << "In CATRepresentationImage \"" << img->id << "\": unable to load the texture \"" << uri.filename << "\". " << error.what();

					LogMessage(Logger::Err, stream.str());

					img->texture.reset(nullptr);
				}
			});

			mCondition.notify_one();
		lock.unlock();
	}

	// ------------------------------------------------------------------------------------------------
	// Read the CATMaterial section
	void _3DXMLParser::ReadCATMaterial(const XMLParser* parser) { PROFILER;
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
	void _3DXMLParser::ReadCATMatConnection(const XMLParser* parser) { PROFILER;
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
						static const std::string reference = "Reference";

						std::string semantic = *(parser->GetContent<std::string>(true));

						if(semantic.compare(0, reference.size(), reference) != 0) {
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

								params.me->mContent.dependencies.add(uri.filename);

								switch(params.current_role) {
									case TOREFERENCE:
										params.connection->materials.emplace_back(uri.filename, *(uri.id));
										break;
									case MADEOF:
									case DRESSBY:
										params.connection->references.emplace_back(uri.filename, *(uri.id));
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
