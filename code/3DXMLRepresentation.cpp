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

#include "ParsingUtils.h"
#include "Q3BSPZipArchive.h"

#include <cctype>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLRepresentation::_3DXMLRepresentation(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& filename, std::list<ScopeGuard<aiMesh>>& meshes) : mReader(archive, filename), mMeshes(meshes), mCurrentMesh(NULL) {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse Root element
			map.insert(std::make_pair("Root", [](Params& params){
				params.me->mCurrentMesh = ScopeGuard<aiMesh>(new aiMesh());

				params.me->ReadVisualizationRep();
				
				// Duplicate the vertices to avoid different faces sharing the same (and to pass the ValidateDataStructure test...)
				ScopeGuard<aiMesh> processed_mesh(new aiMesh());
				unsigned int vertice_index = 0;
				for(unsigned int i = 0; i < params.me->mCurrentMesh->Faces.Size(); i++) {
					aiFace& face = params.me->mCurrentMesh->Faces.Get(i);

					aiFace processed_face;
					for(unsigned int j = 0; j < face.Indices.Size(); j++) {
						unsigned int index = face.Indices.Get(j);

						processed_face.Indices.Set(j, vertice_index);

						if(params.me->mCurrentMesh->HasPositions()) {
							processed_mesh->Vertices.Set(vertice_index, params.me->mCurrentMesh->Vertices.Get(index));
						}
						if(params.me->mCurrentMesh->HasNormals()) {
							processed_mesh->Normals.Set(vertice_index, params.me->mCurrentMesh->Normals.Get(index));
						}
						if(params.me->mCurrentMesh->HasTangentsAndBitangents()) {
							processed_mesh->Tangents.Set(vertice_index, params.me->mCurrentMesh->Tangents.Get(index));
							processed_mesh->Bitangents.Set(vertice_index, params.me->mCurrentMesh->Bitangents.Get(index));
						}
						for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_TEXTURECOORDS; k++) {
							if(params.me->mCurrentMesh->HasTextureCoords(k)) {
								processed_mesh->TextureCoords.Get(k).Set(vertice_index, params.me->mCurrentMesh->TextureCoords.Get(k).Get(index));
							}
						}
						for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
							if(params.me->mCurrentMesh->HasVertexColors(k)) {
								processed_mesh->Colors.Get(k).Set(vertice_index, params.me->mCurrentMesh->Colors.Get(k).Get(index));
							}
						}

						vertice_index++;
					}

					processed_mesh->Faces.Set(i, processed_face);
				}
				
				params.me->mMeshes.push_back(processed_mesh);
			}));
			
			return map;
		})());


		params.me = this;

		// Parse the main 3DXML file
		while(mReader.Next()) {
			if(mReader.IsElement("XMLRepresentation")) {
				mReader.ParseNode(mapping, params);
			} else {
				mReader.SkipElement();
			}
		}

		mReader.Close();
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLRepresentation::~_3DXMLRepresentation() {

	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLRepresentation::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader.GetFilename() % error));
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseMultiArray(const std::string& content, MultiArray<aiColor4D>& array, unsigned int channel, unsigned int start_index, bool alpha) const {
		std::istringstream stream(content);
		float r, g, b, a;

		Array<aiColor4D>& data = array.Get(channel);

		unsigned int index = start_index;
		while(! stream.eof()) {
			stream >> r >> g >> b;

			if(alpha) {
				stream >> a;
			} else {
				a = 0;
			}

			if(! stream.eof()) {
				stream.ignore(std::numeric_limits<std::streamsize>::max(), ',');
			}

			if(stream.fail()) {
				ThrowException("Can not convert array value to \"aiColor4D\".");
			}

			data.Set(index++, aiColor4D(r, g, b, a));
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseMultiArray(const std::string& content, MultiArray<aiVector3D>& array, unsigned int channel, unsigned int start_index, unsigned int dimension) const {
		static const std::size_t dim_max = 3;

		float values[dim_max] = {0, 0, 0};
		
		std::istringstream stream(content);
		
		Array<aiVector3D>& data = array.Get(channel);

		unsigned int index = start_index;
		while(! stream.eof()) {
			for(unsigned int i = 0; i < dimension; i++) {
				stream >> values[i];
			}
			
			if(! stream.eof()) {
				stream.ignore(std::numeric_limits<std::streamsize>::max(), ',');
			}

			if(stream.fail()) {
				ThrowException("Can not convert array value to \"aiVector3D\".");
			}

			data.Set(index++, aiVector3D(values[0], values[1], values[2]));
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseTriangles(const std::string& content, std::vector<unsigned int>& triangles) const {
		std::istringstream stream(content);
		unsigned int value;

		while(! stream.eof()) {
			char next = stream.peek();
			while(next == ',' || next == ' ') {
				stream.get();
				next = stream.peek();
			}

			if(! stream.eof()) {
				stream >> value;

				if(stream.fail()) {
					ThrowException("Can not convert face value to \"unsigned int\".");
				}

				triangles.push_back(value);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVisualizationRep() {
		std::string type = *(mReader.GetAttribute<std::string>("xsi:type", true));

		if(type.compare("BagRepType") == 0) {
			ReadBagRep();
		} else if(type.compare("PolygonalRepType") == 0) {
			ReadPolygonRep();
		} else {
			ThrowException("Unsupported type of VisualizationRep \"" + type + "\".");
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadBagRep() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse Rep element
			map.insert(std::make_pair("Rep", [](Params& params){params.me->ReadVisualizationRep();}));
			
			return map;
		})());

		params.me = this;

		mReader.ParseNode(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadPolygonRep() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse SurfaceAttributes element
			//map.insert(std::make_pair("SurfaceAttributes", [](Params& params){ }));
			
			// Parse LineAttributes element
			//map.insert(std::make_pair("LineAttributes", [](Params& params){ }));
			
			// Parse PolygonalLOD element
			//map.insert(std::make_pair("PolygonalLOD", [](Params& params){ }));
			
			// Parse Faces element
			map.insert(std::make_pair("Faces", [](Params& params){params.me->ReadFaces();}));
			
			// Parse Edges element
			//map.insert(std::make_pair("Edges", [](Params& params){ }));
			
			// Parse VertexBuffer element
			map.insert(std::make_pair("VertexBuffer", [](Params& params){params.me->ReadVertexBuffer();}));
			
			return map;
		})());

		params.me = this;

		mReader.ParseNode(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadFaces() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse SurfaceAttributes element
			//map.insert(std::make_pair("SurfaceAttributes", [](Params& params){ }));
			
			// Parse Face element
			map.insert(std::make_pair("Face", [](Params& params){
				static const unsigned int nb_vertices = 3;

				std::vector<unsigned int> data;

				unsigned int index = params.me->mCurrentMesh->Faces.Size();

				_3DXMLParser::XMLReader::Optional<std::string> triangles = params.me->mReader.GetAttribute<std::string>("triangles");
				if(triangles) {
					data.clear();

					params.me->ParseTriangles(*triangles, data);

					for(unsigned int i = 0; i < data.size(); i += 3) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						for(unsigned int j = 0; j < nb_vertices; j++) {
							face.mIndices[j] = data[i + j];
						}

						params.me->mCurrentMesh->Faces.Set(index++, face);
					}
				}

				_3DXMLParser::XMLReader::Optional<std::string> strips = params.me->mReader.GetAttribute<std::string>("strips");
				if(strips) {
					data.clear();

					params.me->ParseTriangles(*strips, data);

					for(unsigned int i = 0; i < data.size() - (nb_vertices - 1); i++) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						for(unsigned int j = 0; j < nb_vertices; j++) {
							face.mIndices[j] = data[i + j];
						}

						params.me->mCurrentMesh->Faces.Set(index++, face);
					}
				}

				_3DXMLParser::XMLReader::Optional<std::string> fans = params.me->mReader.GetAttribute<std::string>("fans");
				if(fans) {
					data.clear();

					params.me->ParseTriangles(*fans, data);

					for(unsigned int i = 0; i < data.size() - (nb_vertices - 1); i++) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						face.mIndices[0] = data[0];
						for(unsigned int j = 1; j < nb_vertices; j++) {
							face.mIndices[j] = data[i + j];
						}

						params.me->mCurrentMesh->Faces.Set(index++, face);
					}
				}
			}));
			
			return map;
		})());

		params.me = this;

		mReader.ParseNode(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVertexBuffer() {
		struct Params {
			_3DXMLRepresentation* me;
			unsigned int start_index;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> mapping(([](){
			std::map<std::string, std::function<void(Params&)>> map;

			// Parse Positions element
			map.insert(std::make_pair("Positions", [](Params& params){
				std::string vertices = *(params.me->mReader.GetContent<std::string>(true));

				params.me->ParseArray(vertices, params.me->mCurrentMesh->Vertices, params.start_index);
			}));
			
			// Parse Normals element
			map.insert(std::make_pair("Normals", [](Params& params){
				std::string normals = *(params.me->mReader.GetContent<std::string>(true));

				params.me->ParseArray(normals, params.me->mCurrentMesh->Normals, params.start_index);
			}));
			
			// Parse TextureCoordinates element
			map.insert(std::make_pair("TextureCoordinates", [](Params& params){
				_3DXMLParser::XMLReader::Optional<unsigned int> channel_opt = params.me->mReader.GetAttribute<unsigned int>("channel");
				std::string format = *(params.me->mReader.GetAttribute<std::string>("dimension", true));
				std::string coordinates = *(params.me->mReader.GetContent<std::string>(true));

				unsigned int channel = 0;
				if(channel_opt) {
					channel = *channel_opt;
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

				params.me->ParseMultiArray(coordinates, params.me->mCurrentMesh->TextureCoords, channel, params.start_index, dimension);
			}));
			
			// Parse DiffuseColors element
			map.insert(std::make_pair("DiffuseColors", [](Params& params){
				std::string format = *(params.me->mReader.GetAttribute<std::string>("format", true));
				std::string color = *(params.me->mReader.GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					params.me->ParseMultiArray(color, params.me->mCurrentMesh->Colors, 0, params.start_index, false);
				} else if(format.compare("RGBA") == 0) {
					params.me->ParseMultiArray(color, params.me->mCurrentMesh->Colors, 0, params.start_index, true);
				} else {
					params.me->ThrowException("Unsupported color format \"" + format + "\".");
				}
			}));
			
			// Parse SpecularColors element
			map.insert(std::make_pair("SpecularColors", [](Params& params){
				std::string format = *(params.me->mReader.GetAttribute<std::string>("format", true));
				std::string color = *(params.me->mReader.GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					params.me->ParseMultiArray(color, params.me->mCurrentMesh->Colors, 1, params.start_index, false);
				} else if(format.compare("RGBA") == 0) {
					params.me->ParseMultiArray(color, params.me->mCurrentMesh->Colors, 1, params.start_index, true);
				} else {
					params.me->ThrowException("Unsupported color format \"" + format + "\".");
				}
			}));
			
			return map;
		})());

		params.me = this;
		params.start_index = mCurrentMesh->Vertices.Size();

		mReader.ParseNode(mapping, params);

		if(mCurrentMesh->Vertices.Size() == 0) {
			ThrowException("The vertex buffer does not contain any vertex.");
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
