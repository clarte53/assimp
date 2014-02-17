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

/** @file 3DXMLRepresentation.cpp
 *  @brief Implementation of the 3DXML parser of representation helper
 */

#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER
#ifndef ASSIMP_BUILD_NO_Q3BSP_IMPORTER

#include "3DXMLRepresentation.h"

#include "3DXMLParser.h"
#include "fast_atof.h"
#include "HighResProfiler.h"
#include "ParsingUtils.h"
#include "ProcessHelper.h"
#include "SceneCombiner.h"

#include <cctype>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLRepresentation::_3DXMLRepresentation(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& filename, _3DXMLStructure::ReferenceRep::Meshes& meshes, _3DXMLStructure::Dependencies& dependencies) : mReader(archive, filename), mCurrentRep(nullptr), mMeshes(meshes), mDependencies(dependencies) { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Root element
			map.emplace_back("Root", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){params.me->ReadVisualizationRep();}, 1, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;

		// Parse the 3DRep file
		while(mReader.Next()) {
			if(mReader.IsElement("XMLRepresentation")) {
				mReader.ParseElement(mapping, params);
			} else {
				mReader.SkipElement();
			}
		}

		mReader.Close();

		// Merge the meshes that can be merged together and move the result to the final output list
		if(! mMeshes.empty()) {
			_3DXMLStructure::ReferenceRep::Meshes output;

			auto range = std::make_pair(mMeshes.begin(), mMeshes.begin());

			do {
				// Get all the meshes for a given material ID
				range = mMeshes.equal_range(range.second->first);

				if(range.first != range.second) {
					std::map<unsigned int, std::vector<aiMesh*>> data[_3DXMLStructure::ReferenceRep::Geometry::NB_TYPES];

					// Sort the meshes by type of included components (normals, texture coords, etc.)
					for(auto it(range.first); it != range.second; ++it) {
						unsigned int index = GetMeshVFormatUnique(it->second.mesh.get());
						data[it->second.type][index].push_back(it->second.mesh.release());
					}

					// Merge the different meshes compatible together
					for(unsigned int i = 0; i < _3DXMLStructure::ReferenceRep::Geometry::NB_TYPES; ++i) {
						for(auto it(data[i].begin()), end(data[i].end()); it != end; ++it) {
							if(! it->second.empty()) {
								aiMesh* mesh = NULL;

								if(it->second.size() > 1) {
									SceneCombiner::MergeMeshes(&mesh, 0, it->second.begin(), it->second.end());
								} else {
									mesh = *(it->second.begin());
								}

								output.emplace(range.first->first, _3DXMLStructure::ReferenceRep::Geometry((_3DXMLStructure::ReferenceRep::Geometry::Type) i, mesh));
							}
						}
					}
				}
			} while(range.second != mMeshes.end());

			std::swap(mMeshes, output);
		}
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLRepresentation::~_3DXMLRepresentation() { PROFILER;

	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::PropagateAttributes(_3DXMLStructure::MaterialAttributes::ID& parent, _3DXMLStructure::MaterialAttributes::ID& child) {
		// Propagate the surface attributes if none are defined at the sub-level
		if(parent) {
			if(! child) {
				child = parent;
			} else if(parent != child && child->is_color && child->materials.empty()) {
				// Copy the complex material of the parent to allow both complex and simple material representations
				for(auto it_attr(parent->materials.begin()), end_attr(parent->materials.end()); it_attr != end_attr; ++it_attr) {
					child->materials.emplace_back(*it_attr);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLRepresentation::ThrowException(const std::string& error) const { PROFILER;
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader.GetFilename() % error));
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseArray(const std::string& content, std::vector<aiVector3D>& array) const { PROFILER;
		const char* str = content.c_str();
		double x, y, z;

		try {
			while(*str != '\0') {
				str = fast_atoreal_move<double>(str, x, false);

				SkipSpacesAndLineEnd(&str);

				str = fast_atoreal_move<double>(str, y, false);

				SkipSpacesAndLineEnd(&str);

				str = fast_atoreal_move<double>(str, z, false);

				SkipSpacesAndLineEnd(&str);

				if(*str == ',') {
					++str;

					SkipSpacesAndLineEnd(&str);
				}

				array.emplace_back((float) x, (float) y, (float) z);
			}
		} catch(...) {
			array.clear();
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseArray(const std::string& content, Array<aiVector3D>& array, unsigned int start_index) const { PROFILER;
		const char* str = content.c_str();
		double x, y, z;
		unsigned int index = start_index;

		try {
			while(*str != '\0') {
				str = fast_atoreal_move<double>(str, x, false);

				SkipSpacesAndLineEnd(&str);

				str = fast_atoreal_move<double>(str, y, false);

				SkipSpacesAndLineEnd(&str);

				str = fast_atoreal_move<double>(str, z, false);

				SkipSpacesAndLineEnd(&str);

				if(*str == ',') {
					++str;

					SkipSpacesAndLineEnd(&str);
				}

				array.Set(index++, aiVector3D((float) x, (float) y, (float) z));
			}
		} catch(...) {
			array.Reset();
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseMultiArray(const std::string& content, MultiArray<aiColor4D>& array, unsigned int channel, unsigned int start_index, bool alpha) const { PROFILER;
		const char* str = content.c_str();
		double r, g, b, a;
		unsigned int index = start_index;

		Array<aiColor4D>& data = array.Get(channel);

		try {
			while(*str != '\0') {
				str = fast_atoreal_move<double>(str, r, false);

				SkipSpacesAndLineEnd(&str);

				str = fast_atoreal_move<double>(str, g, false);

				SkipSpacesAndLineEnd(&str);

				str = fast_atoreal_move<double>(str, b, false);

				SkipSpacesAndLineEnd(&str);

				if(alpha) {
					str = fast_atoreal_move<double>(str, a, false);

					SkipSpacesAndLineEnd(&str);
				} else {
					a = 0.0f;
				}

				if(*str == ',') {
					++str;

					SkipSpacesAndLineEnd(&str);
				}

				data.Set(index++, aiColor4D((float) r, (float) g, (float) b, (float) a));
			}
		} catch(...) {
			data.Reset();
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseMultiArray(const std::string& content, MultiArray<aiVector3D>& array, unsigned int channel, unsigned int start_index, unsigned int dimension) const { PROFILER;
		const char* str = content.c_str();
		static const std::size_t dim_max = 3;

		double values[dim_max] = {0, 0, 0};
		unsigned int index = start_index;
		
		Array<aiVector3D>& data = array.Get(channel);

		try {
			while(*str != '\0') {
				for(unsigned int i = 0; i < dimension; i++) {
					str = fast_atoreal_move<double>(str, values[i], false);

					SkipSpacesAndLineEnd(&str);
				}

				if(*str == ',') {
					++str;

					SkipSpacesAndLineEnd(&str);
				}

				data.Set(index++, aiVector3D((float) values[0], (float) values[1], (float) values[2]));
			}
		} catch(...) {
			data.Reset();
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseTriangles(const std::string& content, std::list<std::vector<unsigned int>>& triangles) const { PROFILER;
		const char* str = content.c_str();
		unsigned int value;

		triangles.emplace_back();
		triangles.back().reserve(128); // better use too much memory than to have multiple reallocations

		try {
			while(*str != '\0') {
				while(*str != '\0' && (*str == ',' || *str == ' ')) {
					if(*str == ',') {
						triangles.emplace_back();
						triangles.back().reserve(128); // better use too much memory than to have multiple reallocations
					}

					++str;
				}

				if(*str != '\0') {
					value = static_cast<unsigned int>(strtoul10_64(str, &str));

					SkipSpacesAndLineEnd(&str);

					triangles.back().push_back(value);
				}
			}
		} catch(...) {
			triangles.clear();
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseFaces(const Face& face) {
		static const unsigned int nb_vertices = 3;

		std::list<std::vector<unsigned int>> data;

		if(face.triangles || face.strips || face.fans) {
			auto it = mCurrentRep->meshes.emplace(face.surface_attribute,
				_3DXMLStructure::ReferenceRep::Geometry(_3DXMLStructure::ReferenceRep::Geometry::MESH)
			);

			aiMesh* mesh = it->second.mesh.get();

			if(face.triangles) {
				data.clear();

				ParseTriangles(*(face.triangles), data);

				// Compute the number of faces we will add to the mesh and allocate the necessary memory in one pass
				unsigned int size = 0;
				for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
					size += (it->size() / 3);
				}
				mesh->Faces.Reserve(mesh->mNumFaces + size);

				for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
					for(unsigned int i = 0; i < it->size(); i += 3) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						for(unsigned int j = 0; j < nb_vertices; j++) {
							face.mIndices[j] = (*it)[i + j];
						}

						mesh->Faces.Set(mesh->mNumFaces, face);
					}
				}
			}

			if(face.strips) {
				data.clear();

				ParseTriangles(*(face.strips), data);

				// Compute the number of faces we will add to the mesh and allocate the necessary memory in one pass
				unsigned int size = 0;
				for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
					size += it->size() - 2;
				}
				mesh->Faces.Reserve(mesh->mNumFaces + size);

				for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
					bool inversed = false;

					for(unsigned int i = 0; i < it->size() - (nb_vertices - 1); i++) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						for(unsigned int j = 0; j < nb_vertices; j++) {
							if(! inversed) {
								face.mIndices[j] = (*it)[i + j];
							} else {
								face.mIndices[j] = (*it)[i + nb_vertices - (j + 1)];
							}
						}

						inversed = ! inversed;

						mesh->Faces.Set(mesh->mNumFaces, face);
					}
				}
			}

			if(face.fans) {
				data.clear();

				ParseTriangles(*(face.fans), data);

				// Compute the number of faces we will add to the mesh and allocate the necessary memory in one pass
				unsigned int size = 0;
				for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
					size += it->size() - 2;
				}
				mesh->Faces.Reserve(mesh->mNumFaces + size);

				for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {		
					for(unsigned int i = 0; i < it->size() - (nb_vertices - 1); i++) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						face.mIndices[0] = (*it)[0];
						for(unsigned int j = 1; j < nb_vertices; j++) {
							face.mIndices[j] = (*it)[i + j];
						}

						mesh->Faces.Set(mesh->mNumFaces, face);
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseEdges(const Polyline& edge) {
		std::vector<aiVector3D> lines;
		
		if(edge.vertices) {
			ParseArray(*(edge.vertices), lines);
		}

		if(! lines.empty()) {
			auto it = mCurrentRep->meshes.emplace(edge.line_attribute,
				_3DXMLStructure::ReferenceRep::Geometry(_3DXMLStructure::ReferenceRep::Geometry::LINES)
			);

			aiMesh* mesh = it->second.mesh.get();

			const unsigned int nb_faces = lines.size() - 1;
			unsigned int index = mesh->mNumVertices;

			// Compute the number of faces and vertices we will add to the mesh and allocate the necessary memory in one pass
			mesh->Faces.Reserve(mesh->mNumFaces + nb_faces);
			mesh->Vertices.Reserve(mesh->mNumVertices + nb_faces * 2);

			for(unsigned int i = 0; i < lines.size() - 1; i++, index += 2) {
				mesh->Vertices.Set(index, lines[i]);
				mesh->Vertices.Set(index + 1, lines[i + 1]);

				aiFace face;
				face.Indices.Set(face.mNumIndices, index);
				face.Indices.Set(face.mNumIndices, index + 1);

				mesh->Faces.Set(mesh->mNumFaces, face);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseVertexBuffer() {
		if(mCurrentRep && mCurrentRep->vertex_buffer.mNumVertices != 0) {
			// Duplicate the vertices to avoid different faces sharing the same (and to pass the ValidateDataStructure test...)
			for(_3DXMLStructure::ReferenceRep::Meshes::iterator it(mCurrentRep->meshes.begin()), end(mCurrentRep->meshes.end()); it != end; ++it) {
				if(it->second.type == _3DXMLStructure::ReferenceRep::Geometry::MESH) {
					aiMesh* mesh = it->second.mesh.get();

					// Compute the final number of vertices for this mesh
					unsigned int final_vertices_size = mesh->mNumVertices;
					for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
						final_vertices_size += mesh->mFaces[i].mNumIndices;
					}

					// Make sure all the source arrays are exactly the same size
					if(mCurrentRep->vertex_buffer.HasPositions()) {
						mCurrentRep->vertex_buffer.Vertices.Reserve(mCurrentRep->vertex_buffer.mNumVertices);
					}
					if(mCurrentRep->vertex_buffer.HasNormals()) {
						mCurrentRep->vertex_buffer.Normals.Reserve(mCurrentRep->vertex_buffer.mNumVertices);
					}
					if(mCurrentRep->vertex_buffer.HasTangentsAndBitangents()) {
						mCurrentRep->vertex_buffer.Tangents.Reserve(mCurrentRep->vertex_buffer.mNumVertices);
						mCurrentRep->vertex_buffer.Bitangents.Reserve(mCurrentRep->vertex_buffer.mNumVertices);
					}
					for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_TEXTURECOORDS; k++) {
						if(mCurrentRep->vertex_buffer.HasTextureCoords(k)) {
							mCurrentRep->vertex_buffer.TextureCoords.Get(k).Reserve(mCurrentRep->vertex_buffer.mNumVertices);
						}
					}
					for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
						if(mCurrentRep->vertex_buffer.HasVertexColors(k)) {
							mCurrentRep->vertex_buffer.Colors.Get(k).Reserve(mCurrentRep->vertex_buffer.mNumVertices);
						}
					}

					// Make sure the destination arrays are allocated to the correct size, even if no data is present
					if(mesh->HasPositions() || mCurrentRep->vertex_buffer.HasPositions()) {
						mesh->Vertices.Reserve(final_vertices_size);
					}
					if(mesh->HasNormals() || mCurrentRep->vertex_buffer.HasNormals()) {
						mesh->Normals.Reserve(final_vertices_size);
					}
					if(mesh->HasTangentsAndBitangents() || mCurrentRep->vertex_buffer.HasTangentsAndBitangents()) {
						mesh->Tangents.Reserve(final_vertices_size);
						mesh->Bitangents.Reserve(final_vertices_size);
					}
					for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_TEXTURECOORDS; k++) {
						if(mesh->HasTextureCoords(k) || mCurrentRep->vertex_buffer.HasTextureCoords(k)) {
							mesh->mNumUVComponents[k] = std::max(mesh->mNumUVComponents[k], mCurrentRep->vertex_buffer.mNumUVComponents[k]);
							mesh->TextureCoords.Get(k).Reserve(final_vertices_size);
						}
					}
					for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
						if(mesh->HasVertexColors(k) || mCurrentRep->vertex_buffer.HasVertexColors(k)) {
							mesh->Colors.Get(k).Reserve(final_vertices_size);
						}
					}

					unsigned int vertice_index = mesh->mNumVertices;

					// Duplicate the vertices to avoid joined vertices (or validation of the structure will fail)
					for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
						aiFace& face = mesh->mFaces[i];

						for(unsigned int j = 0; j < face.mNumIndices; j++) {
							unsigned int index = face.mIndices[j];

							face.mIndices[j] = vertice_index;

							if(mCurrentRep->vertex_buffer.HasPositions()) {
								mesh->mVertices[vertice_index] = mCurrentRep->vertex_buffer.mVertices[index];
							}
							if(mCurrentRep->vertex_buffer.HasNormals()) {
								mesh->mNormals[vertice_index] = mCurrentRep->vertex_buffer.mNormals[index];
							}
							if(mCurrentRep->vertex_buffer.HasTangentsAndBitangents()) {
								mesh->mTangents[vertice_index] = mCurrentRep->vertex_buffer.mTangents[index];
								mesh->mBitangents[vertice_index] = mCurrentRep->vertex_buffer.mBitangents[index];
							}
							for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_TEXTURECOORDS; k++) {
								if(mCurrentRep->vertex_buffer.HasTextureCoords(k)) {
									mesh->mTextureCoords[k][vertice_index] = mCurrentRep->vertex_buffer.mTextureCoords[k][index];
								}
							}
							for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
								if(mCurrentRep->vertex_buffer.HasVertexColors(k)) {
									mesh->mColors[k][vertice_index] = mCurrentRep->vertex_buffer.mColors[k][index];
								}
							}

							vertice_index++;
						}
					}

					mesh->mNumVertices = vertice_index;

					// Simplify vertex colors
					for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
						if(! mesh->HasVertexColors(k)) {
							bool found = false;

							for(unsigned int x = k + 1; ! found && x < AI_MAX_NUMBER_OF_COLOR_SETS; x++) {
								if(mesh->HasVertexColors(x)) {
									mesh->mColors[k] = mesh->mColors[x];
									mesh->mColors[x] = NULL;
									found = true;
								}
							}
						}
					}
				}
			}
		}
	}

	void _3DXMLRepresentation::ParsePolygonalRep() {
		if(mCurrentRep) {
			// Iterate over the groups of faces
			for(auto it_surfaces(mCurrentRep->surfaces.begin()), end_surfaces(mCurrentRep->surfaces.end()); it_surfaces != end_surfaces; ++it_surfaces) {
				PropagateAttributes(mCurrentRep->surface_attribute, it_surfaces->surface_attribute);

				// Iterate over the faces in each group of faces
				for(auto it_faces(it_surfaces->faces.begin()), end_faces(it_surfaces->faces.end()); it_faces != end_faces; ++it_faces) {
					PropagateAttributes(it_surfaces->surface_attribute, it_faces->surface_attribute);

					// Parse the current face
					ParseFaces(*it_faces);
				}
			}

			// Iterate over the groups of lines
			for(auto it_lines(mCurrentRep->lines.begin()), end_lines(mCurrentRep->lines.end()); it_lines != end_lines; ++it_lines) {
				PropagateAttributes(mCurrentRep->line_attribute, it_lines->line_attribute);

				// Iterate over the lines in each group of lines
				for(auto it_edges(it_lines->edges.begin()), end_edges(it_lines->edges.end()); it_edges != end_edges; ++it_edges) {
					PropagateAttributes(it_lines->line_attribute, it_edges->line_attribute);

					// Parse the current line
					ParseEdges(*it_edges);
				}
			}

			// Parse the vertex buffer data
			ParseVertexBuffer();

			// Copy the generated meshes into the global map of meshes
			for(auto it(mCurrentRep->meshes.begin()), end(mCurrentRep->meshes.end()); it != end; ++it) {
				mMeshes.emplace(it->first, std::move(it->second));
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVisualizationRep() { PROFILER;
		std::string type = *(mReader.GetAttribute<std::string>("xsi:type", true));

		if(type.compare("BagRepType") == 0) {
			ReadBagRep();
		} else if(type.compare("PolygonalRepType") == 0) {
			ReadPolygonalRep();
		} else {
			ThrowException("Unsupported type of VisualizationRep \"" + type + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadBagRep() { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Rep element
			map.emplace_back("Rep", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){params.me->ReadVisualizationRep();}, 1, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;

		mReader.ParseElement(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadPolygonalRep() { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse SurfaceAttributes element
			map.emplace_back("SurfaceAttributes", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadSurfaceAttributes(params.me->mCurrentRep->surface_attribute);
			}, 0, 1));
			
			// Parse LineAttributes element
			map.emplace_back("LineAttributes", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadLineAttributes(params.me->mCurrentRep->line_attribute);
			}, 0, 1));
			
			// Parse PolygonalLOD element
			//map.emplace_back("PolygonalLOD", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
			//
			//}, 0, XMLParser::XSD::unbounded));
			
			// Parse Faces element
			map.emplace_back("Faces", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadFaces();
			}, 0, XMLParser::XSD::unbounded));
			
			// Parse Edges element
			map.emplace_back("Edges", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadEdges();
			}, 0, XMLParser::XSD::unbounded));
			
			// Parse VertexBuffer element
			map.emplace_back("VertexBuffer", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadVertexBuffer();
			}, 0, 1));
			
			return std::move(map);
		})(), 0, XMLParser::XSD::unbounded);

		mCurrentRep.reset(new PolygonalRep());

		params.me = this;

		mReader.ParseElement(mapping, params);

		ParsePolygonalRep();
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadFaces() { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
			Faces* surface;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse SurfaceAttributes element
			map.emplace_back("SurfaceAttributes", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadSurfaceAttributes(params.surface->surface_attribute);
			}, 0, 1));
			
			// Parse Face element
			map.emplace_back("Face", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				static const XMLParser::XSD::Sequence<Params> mapping(([](){
					XMLParser::XSD::Sequence<Params>::type map;

					// Parse SurfaceAttributes element
					map.emplace_back("SurfaceAttributes", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
						params.me->ReadSurfaceAttributes(params.surface->faces.back().surface_attribute);
					}, 0, 1));

					return std::move(map);
				})(), 1, 1);

				// Create a new face in this group of faces
				params.surface->faces.emplace_back();

				Face& face = params.surface->faces.back();

				face.triangles = parser->GetAttribute<std::string>("triangles");
				face.strips = parser->GetAttribute<std::string>("strips");
				face.fans = parser->GetAttribute<std::string>("fans");

				parser->ParseElement(mapping, params);
			}, 1, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		if(mCurrentRep) {
			// Add a new group of faces to the current PolygonalRep
			mCurrentRep->surfaces.emplace_back();

			params.me = this;
			params.surface = &(mCurrentRep->surfaces.back());

			mReader.ParseElement(mapping, params);
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadEdges() { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
			Edges* lines;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse LineAttributes element
			map.emplace_back("LineAttributes", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadLineAttributes(params.lines->line_attribute);
			}, 0, 1));

			// Parse Polyline element
			map.emplace_back("Polyline", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				static const XMLParser::XSD::Sequence<Params> mapping(([](){
					XMLParser::XSD::Sequence<Params>::type map;

					// Parse LineAttributes element
					map.emplace_back("LineAttributes", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
						params.me->ReadLineAttributes(params.lines->edges.back().line_attribute);
					}, 0, 1));

					return std::move(map);
				})(), 1, 1);

				// Create a new line in this group of lines
				params.lines->edges.emplace_back();

				Polyline& edge = params.lines->edges.back();

				edge.vertices = parser->GetAttribute<std::string>("vertices");

				parser->ParseElement(mapping, params);
			}, 1, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		if(mCurrentRep) {
			// Add a new group of lines to the current PolygonalRep
			mCurrentRep->lines.emplace_back();

			params.me = this;
			params.lines = &(mCurrentRep->lines.back());

			mReader.ParseElement(mapping, params);
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVertexBuffer() { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
			aiMesh* vertex_buffer;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Positions element
			map.emplace_back("Positions", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string vertices = *(parser->GetContent<std::string>(true));

				params.me->ParseArray(vertices, params.vertex_buffer->Vertices, 0);
			}, 1, 1));
			
			// Parse Normals element
			map.emplace_back("Normals", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string normals = *(parser->GetContent<std::string>(true));

				params.me->ParseArray(normals, params.vertex_buffer->Normals, 0);
			}, 0, 1));
			
			// Parse TextureCoordinates element
			map.emplace_back("TextureCoordinates", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				Optional<unsigned int> channel_opt = parser->GetAttribute<unsigned int>("channel");
				std::string format = *(parser->GetAttribute<std::string>("dimension", true));
				std::string coordinates = *(parser->GetContent<std::string>(true));

				unsigned int channel = 0;
				if(channel_opt) {
					channel = *channel_opt;
				}

				if(channel >= AI_MAX_NUMBER_OF_TEXTURECOORDS) {
					params.me->ThrowException("Invalid out-of-bound channel \"" + parser->ToString(channel) + "\" (max " + parser->ToString(AI_MAX_NUMBER_OF_TEXTURECOORDS) + ").");
				}

				if(format.size() != 2 || format[1] != 'D' || ! std::isdigit(format[0])) {
					params.me->ThrowException("Invalid texture coordinate format \"" + format + "\".");
				}

				unsigned int dimension = 0;
				std::string value(1, format[0]);
				
				dimension = std::atoi(value.c_str());

				if(dimension == 0 || dimension > 3) {
					params.me->ThrowException("Invalid dimension for texture coordinate format \"" + format + "\".");
				}

				params.vertex_buffer->mNumUVComponents[channel] = dimension;

				params.me->ParseMultiArray(coordinates, params.vertex_buffer->TextureCoords, channel, 0, dimension);
			}, 0, XMLParser::XSD::unbounded));
			
			// Parse DiffuseColors element
			map.emplace_back("DiffuseColors", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string format = *(parser->GetAttribute<std::string>("format", true));
				std::string color = *(parser->GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					params.me->ParseMultiArray(color, params.vertex_buffer->Colors, 0, 0, false);
				} else if(format.compare("RGBA") == 0) {
					params.me->ParseMultiArray(color, params.vertex_buffer->Colors, 0, 0, true);
				} else {
					params.me->ThrowException("Unsupported color format \"" + format + "\".");
				}
			}, 0, 1));
			
			// Parse SpecularColors element
			map.emplace_back("SpecularColors", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string format = *(parser->GetAttribute<std::string>("format", true));
				std::string color = *(parser->GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					params.me->ParseMultiArray(color, params.vertex_buffer->Colors, 1, 0, false);
				} else if(format.compare("RGBA") == 0) {
					params.me->ParseMultiArray(color, params.vertex_buffer->Colors, 1, 0, true);
				} else {
					params.me->ThrowException("Unsupported color format \"" + format + "\".");
				}
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		if(mCurrentRep->vertex_buffer.mNumVertices != 0) {
			ThrowException("Multiple VertexBuffer detected for one PolygonalRep.");
		}

		params.me = this;
		params.vertex_buffer = &(mCurrentRep->vertex_buffer);

		mReader.ParseElement(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadSurfaceAttributes(_3DXMLStructure::MaterialAttributes::ID& attributes) { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
			_3DXMLStructure::MaterialAttributes* attributes;
		} params;

		//TODO: SurfaceAttribute is a choice and MaterialApplication is a sequence in {1, unbounded}, therefore we are not strictly compliant with 3DXML schemas v4.x
		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Color element
			map.emplace_back("Color", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				float red = *(parser->GetAttribute<float>("red", true));
				float green = *(parser->GetAttribute<float>("green", true));
				float blue = *(parser->GetAttribute<float>("blue", true));
				Optional<float> alpha = parser->GetAttribute<float>("alpha");

				aiColor4D& color = params.attributes->color;
				color.r = red;
				color.g = green;
				color.b = blue;

				if(alpha) {
					color.a = *alpha;
				} else {
					color.a = 1.0;
				}

				// Record that we defined a color
				params.attributes->is_color = true;
			}, 0, 1));

			// Parse MaterialApplication elements
			map.emplace_back("MaterialApplication", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				params.me->ReadMaterialApplication(params.attributes);
			}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		if(mReader.HasElements()) {
			if(attributes) {
				ThrowException("Multiple SurfaceAttributes detected for one PolygonalRep.");
			}

			// Create the new surface attributes
			attributes = _3DXMLStructure::MaterialAttributes::ID(new _3DXMLStructure::MaterialAttributes());

			params.me = this;
			params.attributes = &(*attributes);

			mReader.ParseElement(mapping, params);
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadMaterialApplication(_3DXMLStructure::MaterialAttributes* attributes) { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
			_3DXMLStructure::MaterialAttributes* attributes;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse MaterialId element
			map.emplace_back("MaterialId", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				std::string uri_str = *(parser->GetAttribute<std::string>("id", true));

				_3DXMLStructure::URI uri;
				_3DXMLParser::ParseURI(parser, uri_str, uri);

				if(! uri.id) {
					params.me->ThrowException("The MaterialId refers to an invalid reference \"" + uri.uri + "\" without id.");
				}

				params.attributes->materials.emplace_back(uri.filename, *(uri.id));

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				if(uri.external && uri.id && uri.filename.compare(parser->GetFilename()) != 0) {

					params.me->mDependencies.add(uri.filename);
				}
			}, 1, 1));
			
			return std::move(map);
		})(), 1, 1);

		// Get the channel attribute
		// This attribute is required in schema 4.3 but seem optional in previous versions
		Optional<unsigned int> channel_opt = mReader.GetAttribute<unsigned int>("mappingChannel");
		unsigned int channel = 0;

		if(channel_opt) {
			channel = *channel_opt;
		}

		// Get the side of the material
		Optional<std::string> side_opt = mReader.GetAttribute<std::string>("mappingSide");
		_3DXMLStructure::MaterialApplication::MappingSide side;
		if(side_opt) {
			static const std::map<std::string, _3DXMLStructure::MaterialApplication::MappingSide> mapping_sides([]() {
				std::map<std::string, _3DXMLStructure::MaterialApplication::MappingSide> map;

				map.emplace("FRONT", _3DXMLStructure::MaterialApplication::FRONT);
				map.emplace("BACK", _3DXMLStructure::MaterialApplication::BACK);
				map.emplace("FRONT_AND_BACK", _3DXMLStructure::MaterialApplication::FRONT_AND_BACK);

				return std::move(map);
			}());

			auto it = mapping_sides.find(*side_opt);

			if(it != mapping_sides.end()) {
				side = it->second;
			} else {
				_3DXMLParser::LogMessage(Logger::Warn, "Unsupported mapping side \"" + *side_opt + "\". Using FRONT side instead.");

				side = _3DXMLStructure::MaterialApplication::FRONT;
			}
		} else {
			side = _3DXMLStructure::MaterialApplication::FRONT;
		}

		// Get the blending function
		Optional<std::string> blend_opt = mReader.GetAttribute<std::string>("blendType");
		_3DXMLStructure::MaterialApplication::TextureBlendFunction blend_function;
		if(blend_opt) {
			static const std::map<std::string, _3DXMLStructure::MaterialApplication::TextureBlendFunction> blend_functions([]() {
				std::map<std::string, _3DXMLStructure::MaterialApplication::TextureBlendFunction> map;

				map.emplace("REPLACE", _3DXMLStructure::MaterialApplication::REPLACE);
				map.emplace("ADD", _3DXMLStructure::MaterialApplication::ADD);
				map.emplace("ALPHA_TRANSPARENCY", _3DXMLStructure::MaterialApplication::ALPHA_TRANSPARENCY);
				map.emplace("LIGHTMAP", _3DXMLStructure::MaterialApplication::LIGHTMAP);
				map.emplace("BURN", _3DXMLStructure::MaterialApplication::BURN);
				map.emplace("INVERT", _3DXMLStructure::MaterialApplication::INVERT);

				return std::move(map);
			}());

			auto it = blend_functions.find(*blend_opt);

			if(it != blend_functions.end()) {
				blend_function = it->second;
			} else {
				_3DXMLParser::LogMessage(Logger::Warn, "Unsupported texture blending function \"" + *blend_opt + "\". Using REPLACE function instead.");

				blend_function = _3DXMLStructure::MaterialApplication::REPLACE;
			}
		} else {
			blend_function = _3DXMLStructure::MaterialApplication::REPLACE;
		}

		params.me = this;
		params.attributes = attributes;

		mReader.ParseElement(mapping, params);

		_3DXMLStructure::MaterialApplication& application = params.attributes->materials.back();

		application.channel = channel;
		application.side = side;
		application.blend_function = blend_function;
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadLineAttributes(_3DXMLStructure::MaterialAttributes::ID& attributes) { PROFILER;
		struct Params {
			_3DXMLRepresentation* me;
			_3DXMLStructure::MaterialAttributes* attributes;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Color element
			map.emplace_back("Color", XMLParser::XSD::Element<Params>([](const XMLParser* parser, Params& params){
				float red = *(parser->GetAttribute<float>("red", true));
				float green = *(parser->GetAttribute<float>("green", true));
				float blue = *(parser->GetAttribute<float>("blue", true));
				Optional<float> alpha = parser->GetAttribute<float>("alpha");

				aiColor4D& color = params.attributes->color;
				color.r = red;
				color.g = green;
				color.b = blue;

				if(alpha) {
					color.a = *alpha;
				} else {
					color.a = 1.0;
				}

				// Record that we defined a color
				params.attributes->is_color = true;
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		if(mReader.HasElements()) {
			if(attributes) {
				ThrowException("Multiple LineAttributes detected for one PolygonalRep.");
			}

			// Create the new line attributes
			attributes = _3DXMLStructure::MaterialAttributes::ID(new _3DXMLStructure::MaterialAttributes());

			params.me = this;
			params.attributes = &(*attributes);
			//TODO: support lineType & thickness?

			mReader.ParseElement(mapping, params);
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
