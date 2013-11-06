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

#include <cctype>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLRepresentation::_3DXMLRepresentation(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& filename, _3DXMLStructure::ReferenceRep::Meshes& meshes) : mReader(archive, filename), mMeshes(meshes), mCurrentSurface(nullptr), mCurrentLine(nullptr), mDependencies() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Root element
			map.emplace_back("Root", XMLParser::XSD::Element<Params>([](Params& params){
				params.me->ReadVisualizationRep();
				
				// Duplicate the vertices to avoid different faces sharing the same (and to pass the ValidateDataStructure test...)
				for(_3DXMLStructure::ReferenceRep::Meshes::iterator it(params.me->mMeshes.begin()), end(params.me->mMeshes.end()); it != end; ++it) {
					std::unique_ptr<aiMesh> processed_mesh(new aiMesh());
					unsigned int vertice_index = 0;

					for(unsigned int i = 0; i < it->second->Faces.Size(); i++) {
						aiFace face = it->second->Faces.Get(i);

						aiFace processed_face;
						for(unsigned int j = 0; j < face.Indices.Size(); j++) {
							unsigned int index = face.Indices.Get(j);

							processed_face.Indices.Set(j, vertice_index);

							if(it->second->HasPositions()) {
								processed_mesh->Vertices.Set(vertice_index, it->second->Vertices.Get(index));
							}
							if(it->second->HasNormals()) {
								processed_mesh->Normals.Set(vertice_index, it->second->Normals.Get(index));
							}
							if(it->second->HasTangentsAndBitangents()) {
								processed_mesh->Tangents.Set(vertice_index, it->second->Tangents.Get(index));
								processed_mesh->Bitangents.Set(vertice_index, it->second->Bitangents.Get(index));
							}
							for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_TEXTURECOORDS; k++) {
								if(it->second->HasTextureCoords(k)) {
									processed_mesh->TextureCoords.Get(k).Set(vertice_index, it->second->TextureCoords.Get(k).Get(index));
								}
							}
							for(unsigned int k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
								if(it->second->HasVertexColors(k)) {
									processed_mesh->Colors.Get(k).Set(vertice_index, it->second->Colors.Get(k).Get(index));
								}
							}

							vertice_index++;
						}

						processed_mesh->Faces.Set(i, processed_face);
					}
				
					it->second.reset(processed_mesh.release());
				}
			}, 1, 1));
			
			return std::move(map);
		})(), 1, 1);


		params.me = this;

		// Parse the 3DRep file
		while(mReader.Next()) {
			if(mReader.IsElement("XMLRepresentation")) {
				mReader.ParseElements(&mapping, params);
			} else {
				mReader.SkipElement();
			}
		}

		mReader.Close();
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLRepresentation::~_3DXMLRepresentation() {

	}

	const std::set<_3DXMLStructure::ID>& _3DXMLRepresentation::GetDependencies() const {
		return mDependencies;
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLRepresentation::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader.GetFilename() % error));
	}
	
	// ------------------------------------------------------------------------------------------------
	aiMesh* _3DXMLRepresentation::GetMesh(const _3DXMLStructure::ReferenceRep::MatID& material) const {
		auto position = mMeshes.find(material);

		if(position == mMeshes.end()) {
			auto insert = mMeshes.emplace(material, std::unique_ptr<aiMesh>(new aiMesh()));

			if(! insert.second) {
				ThrowException("Impossible to create a new mesh for the new material.");
			}

			position = insert.first;
		}

		return position->second.get();
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseArray(const std::string& content, std::vector<aiVector3D>& array) const {
		std::istringstream stream(content);
		float x, y, z;

		while(! stream.eof()) {
			stream >> x >> y >> z;

			if(! stream.eof()) {
				stream.ignore(std::numeric_limits<std::streamsize>::max(), ',');
			}

			if(stream.fail()) {
				ThrowException("Can not convert array value to \"aiVector3D\".");
			}

			array.push_back(aiVector3D(x, y, z));
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseArray(const std::string& content, Array<aiVector3D>& array, unsigned int start_index) const {
		std::istringstream stream(content);
		float x, y, z;

		unsigned int index = start_index;
		while(! stream.eof()) {
			stream >> x >> y >> z;

			if(! stream.eof()) {
				stream.ignore(std::numeric_limits<std::streamsize>::max(), ',');
			}

			if(stream.fail()) {
				ThrowException("Can not convert array value to \"aiVector3D\".");
			}

			array.Set(index++, aiVector3D(x, y, z));
		}
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
	void _3DXMLRepresentation::ParseTriangles(const std::string& content, std::list<std::vector<unsigned int>>& triangles) const {
		std::istringstream stream(content);
		unsigned int value;

		triangles.emplace_back();
		triangles.back().reserve(128); // better use too much memory than to have multiple reallocations

		while(! stream.eof()) {
			char next = stream.peek();
			while(! stream.eof() && (next == ',' || next == ' ')) {
				if(next == ',') {
					triangles.emplace_back();
					triangles.back().reserve(128); // better use too much memory than to have multiple reallocations
				}

				stream.get();
				next = stream.peek();
			}

			if(! stream.eof()) {
				stream >> value;

				if(stream.fail()) {
					ThrowException("Can not convert face value to \"unsigned int\".");
				}

				triangles.back().push_back(value);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVisualizationRep() {
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
	void _3DXMLRepresentation::ReadBagRep() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Rep element
			map.emplace_back("Rep", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadVisualizationRep();}, 1, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;

		mReader.ParseElements(&mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadPolygonalRep() {
		struct Params {
			_3DXMLRepresentation* me;
			std::vector<std::pair<_3DXMLStructure::ReferenceRep::MatID, std::vector<aiVector3D>>> lines;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse SurfaceAttributes element
			map.emplace_back("SurfaceAttributes", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadSurfaceAttributes();}, 0, 1));
			
			// Parse LineAttributes element
			map.emplace_back("LineAttributes", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadLineAttributes();}, 0, 1));
			
			// Parse PolygonalLOD element
			//map.emplace_back("PolygonalLOD", XMLParser::XSD::Element<Params>([](Params& params){ }, 0, XMLParser::XSD::unbounded));
			
			// Parse Faces element
			map.emplace_back("Faces", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadFaces();}, 0, XMLParser::XSD::unbounded));
			
			// Parse Edges element
			map.emplace_back("Edges", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadEdges(params.lines);}, 0, XMLParser::XSD::unbounded));
			
			// Parse VertexBuffer element
			map.emplace_back("VertexBuffer", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadVertexBuffer();}, 0, 1));
			
			return std::move(map);
		})(), 0, XMLParser::XSD::unbounded);

		_3DXMLStructure::ReferenceRep::MatID old_surface = mCurrentSurface;
		_3DXMLStructure::ReferenceRep::MatID old_line = mCurrentLine;

		params.me = this;

		mReader.ParseElements(&mapping, params);

		mCurrentSurface = old_surface;
		mCurrentLine = old_line;

		// Add the lines after the faces and vertices have been already added to avoid messing with the vertice indexes
		for(std::vector<std::pair<_3DXMLStructure::ReferenceRep::MatID, std::vector<aiVector3D>>>::iterator it(params.lines.begin()), end(params.lines.end()); it != end; ++it) {
			if(! it->second.empty()) {
				aiMesh* mesh = GetMesh(it->first);

				unsigned int index = mesh->Vertices.Size();

				mesh->Vertices.Set(index++, (it->second)[0]);

				for(unsigned int i = 1; i < it->second.size(); i++, index++) {
					mesh->Vertices.Set(index, (it->second)[i]);
						
					aiFace face;
					face.Indices.Set(face.Indices.Size(), index - 1);
					face.Indices.Set(face.Indices.Size(), index);

					mesh->Faces.Set(mesh->Faces.Size(), face);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadFaces() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse SurfaceAttributes element
			map.emplace_back("SurfaceAttributes", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadSurfaceAttributes();}, 0, 1));
			
			// Parse Face element
			map.emplace_back("Face", XMLParser::XSD::Element<Params>([](Params& params){
				static const XMLParser::XSD::Sequence<Params> mapping(([](){
					XMLParser::XSD::Sequence<Params>::type map;

					// Parse SurfaceAttributes element
					map.emplace_back("SurfaceAttributes", XMLParser::XSD::Element<Params>([](Params& params){
						params.me->ReadSurfaceAttributes();
					}, 0, 1));

					return std::move(map);
				})(), 1, 1);

				static const unsigned int nb_vertices = 3;

				Optional<std::string> triangles = params.me->mReader.GetAttribute<std::string>("triangles");
				Optional<std::string> strips = params.me->mReader.GetAttribute<std::string>("strips");
				Optional<std::string> fans = params.me->mReader.GetAttribute<std::string>("fans");
				
				_3DXMLStructure::ReferenceRep::MatID old_surface = params.me->mCurrentSurface;

				params.me->mReader.ParseElements(&mapping, params);

				std::list<std::vector<unsigned int>> data;

				aiMesh* mesh = params.me->GetMesh(params.me->mCurrentSurface);

				unsigned int face_offset = mesh->Vertices.Size();
				unsigned int index = mesh->Faces.Size();

				if(triangles) {
					data.clear();

					params.me->ParseTriangles(*triangles, data);

					for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
						for(unsigned int i = 0; i < it->size(); i += 3) {
							aiFace face;

							face.mNumIndices = nb_vertices;
							face.mIndices = new unsigned int[nb_vertices];

							for(unsigned int j = 0; j < nb_vertices; j++) {
								face.mIndices[j] = (*it)[i + j] + face_offset;
							}

							mesh->Faces.Set(index++, face);
						}
					}
				}
				
				if(strips) {
					data.clear();

					params.me->ParseTriangles(*strips, data);

					for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
						bool inversed = false;
						for(unsigned int i = 0; i < it->size() - (nb_vertices - 1); i++) {
							aiFace face;

							face.mNumIndices = nb_vertices;
							face.mIndices = new unsigned int[nb_vertices];

							for(unsigned int j = 0; j < nb_vertices; j++) {
								if(! inversed) {
									face.mIndices[j] = (*it)[i + j] + face_offset;
								} else {
									face.mIndices[j] = (*it)[i + nb_vertices - (j + 1)] + face_offset;
								}
							}

							inversed = ! inversed;

							mesh->Faces.Set(index++, face);
						}
					}
				}
				
				if(fans) {
					data.clear();

					params.me->ParseTriangles(*fans, data);

					for(std::list<std::vector<unsigned int>>::iterator it(data.begin()), end(data.end()); it != end; ++it) {
						for(unsigned int i = 0; i < it->size() - (nb_vertices - 1); i++) {
							aiFace face;

							face.mNumIndices = nb_vertices;
							face.mIndices = new unsigned int[nb_vertices];
							
							face.mIndices[0] = (*it)[0] + face_offset;
							for(unsigned int j = 1; j < nb_vertices; j++) {
								face.mIndices[j] = (*it)[i + j] + face_offset;
							}
							
							mesh->Faces.Set(index++, face);
						}
					}
				}

				params.me->mCurrentSurface = old_surface;
			}, 1, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		_3DXMLStructure::ReferenceRep::MatID old_surface = mCurrentSurface;

		params.me = this;

		mReader.ParseElements(&mapping, params);

		mCurrentSurface = old_surface;
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadEdges(std::vector<std::pair<_3DXMLStructure::ReferenceRep::MatID, std::vector<aiVector3D>>>& lines) {
		struct Params {
			_3DXMLRepresentation* me;
			std::vector<std::pair<_3DXMLStructure::ReferenceRep::MatID, std::vector<aiVector3D>>>* lines;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse LineAttributes element
			map.emplace_back("LineAttributes", XMLParser::XSD::Element<Params>([](Params& params){params.me->ReadLineAttributes();}, 0, 1));

			// Parse Polyline element
			map.emplace_back("Polyline", XMLParser::XSD::Element<Params>([](Params& params){
				static const XMLParser::XSD::Sequence<Params> mapping(([](){
					XMLParser::XSD::Sequence<Params>::type map;

					// Parse LineAttributes element
					map.emplace_back("LineAttributes", XMLParser::XSD::Element<Params>([](Params& params){
						params.me->ReadLineAttributes();
					}, 0, 1));

					return std::move(map);
				})(), 1, 1);

				std::string vertices = *(params.me->mReader.GetAttribute<std::string>("vertices", true));

				_3DXMLStructure::ReferenceRep::MatID old_line = params.me->mCurrentLine;

				params.me->mReader.ParseElements(&mapping, params);

				params.lines->emplace_back(params.me->mCurrentLine, std::vector<aiVector3D>());
				params.me->ParseArray(vertices, params.lines->back().second);

				params.me->mCurrentLine = old_line;
			}, 1, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		_3DXMLStructure::ReferenceRep::MatID old_line = mCurrentLine;

		params.me = this;
		params.lines = &lines;

		mReader.ParseElements(&mapping, params);

		mCurrentLine = old_line;
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVertexBuffer() {
		struct Params {
			_3DXMLRepresentation* me;
			aiMesh* mesh;
			unsigned int start_index;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Positions element
			map.emplace_back("Positions", XMLParser::XSD::Element<Params>([](Params& params){
				std::string vertices = *(params.me->mReader.GetContent<std::string>(true));

				params.me->ParseArray(vertices, params.mesh->Vertices, params.start_index);
			}, 1, 1));
			
			// Parse Normals element
			map.emplace_back("Normals", XMLParser::XSD::Element<Params>([](Params& params){
				std::string normals = *(params.me->mReader.GetContent<std::string>(true));

				params.me->ParseArray(normals, params.mesh->Normals, params.start_index);
			}, 0, 1));
			
			// Parse TextureCoordinates element
			map.emplace_back("TextureCoordinates", XMLParser::XSD::Element<Params>([](Params& params){
				Optional<unsigned int> channel_opt = params.me->mReader.GetAttribute<unsigned int>("channel");
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

				params.me->ParseMultiArray(coordinates, params.mesh->TextureCoords, channel, params.start_index, dimension);
			}, 0, XMLParser::XSD::unbounded));
			
			// Parse DiffuseColors element
			map.emplace_back("DiffuseColors", XMLParser::XSD::Element<Params>([](Params& params){
				std::string format = *(params.me->mReader.GetAttribute<std::string>("format", true));
				std::string color = *(params.me->mReader.GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					params.me->ParseMultiArray(color, params.mesh->Colors, 0, params.start_index, false);
				} else if(format.compare("RGBA") == 0) {
					params.me->ParseMultiArray(color, params.mesh->Colors, 0, params.start_index, true);
				} else {
					params.me->ThrowException("Unsupported color format \"" + format + "\".");
				}
			}, 0, 1));
			
			// Parse SpecularColors element
			map.emplace_back("SpecularColors", XMLParser::XSD::Element<Params>([](Params& params){
				std::string format = *(params.me->mReader.GetAttribute<std::string>("format", true));
				std::string color = *(params.me->mReader.GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					params.me->ParseMultiArray(color, params.mesh->Colors, 1, params.start_index, false);
				} else if(format.compare("RGBA") == 0) {
					params.me->ParseMultiArray(color, params.mesh->Colors, 1, params.start_index, true);
				} else {
					params.me->ThrowException("Unsupported color format \"" + format + "\".");
				}
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.mesh = params.me->GetMesh(params.me->mCurrentSurface);
		params.start_index = params.mesh->Vertices.Size();

		mReader.ParseElements(&mapping, params);

		if(params.mesh->Vertices.Size() == 0) {
			ThrowException("The vertex buffer does not contain any vertex.");
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadSurfaceAttributes() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		//TODO: SurfaceAttribute is a choice and MaterialApplication is a sequence in {1, unbounded}, therefore we are not strictly compliant with 3DXML schemas v4.x
		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Color element
			map.emplace_back("Color", XMLParser::XSD::Element<Params>([](Params& params){
				float red = *(params.me->mReader.GetAttribute<float>("red", true));
				float green = *(params.me->mReader.GetAttribute<float>("green", true));
				float blue = *(params.me->mReader.GetAttribute<float>("blue", true));
				Optional<float> alpha = params.me->mReader.GetAttribute<float>("alpha");

				aiColor4D& color = params.me->mCurrentSurface->color;
				color.r = red;
				color.g = green;
				color.b = blue;

				if(alpha) {
					color.a = alpha;
				} else {
					color.a = 1.0;
				}
			}, 0, 1));

			// Parse MaterialApplication elements
			map.emplace_back("MaterialApplication", XMLParser::XSD::Element<Params>([](Params& params){
				params.me->ReadMaterialApplication();
			}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		if(mReader.HasElements()) {
			mCurrentSurface = _3DXMLStructure::ReferenceRep::MatID(new _3DXMLStructure::MaterialAttributes());

			params.me = this;

			mReader.ParseElements(&mapping, params);
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadMaterialApplication() {
		struct Params {
			_3DXMLRepresentation* me;
			_3DXMLStructure::MaterialAttributes* attributes;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse MaterialId element
			map.emplace_back("MaterialId", XMLParser::XSD::Element<Params>([](Params& params){
				std::string uri_str = *(params.me->mReader.GetAttribute<std::string>("id", true));

				_3DXMLStructure::URI uri;
				_3DXMLParser::ParseURI(&params.me->mReader, uri_str, uri);

				if(! uri.id) {
					params.me->ThrowException("The MaterialId refers to an invalid reference \"" + uri.uri + "\" without id.");
				}

				params.attributes->materials.emplace_back(uri.filename, *(uri.id));

				// If the reference is on another file and does not already exist, add it to the list of files to parse
				_3DXMLStructure::ID& id = params.attributes->materials.back().id;
				if(uri.external && uri.id && uri.filename.compare(params.me->mReader.GetFilename()) != 0 &&
						params.me->mDependencies.find(id) == params.me->mDependencies.end()) {

					params.me->mDependencies.emplace(id);
				}
			}, 1, 1));
			
			return std::move(map);
		})(), 1, 1);

		// Get the channel attribute
		unsigned int channel = *(mReader.GetAttribute<unsigned int>("mappingChannel", true));

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
				DefaultLogger::get()->warn("Unsupported mapping side \"" + *side_opt + "\". Using FRONT side instead.");

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
				DefaultLogger::get()->warn("Unsupported texture blending function \"" + *blend_opt + "\". Using REPLACE function instead.");

				blend_function = _3DXMLStructure::MaterialApplication::REPLACE;
			}
		} else {
			blend_function = _3DXMLStructure::MaterialApplication::REPLACE;
		}

		params.me = this;
		params.attributes = &(*mCurrentSurface);

		mReader.ParseElements(&mapping, params);

		_3DXMLStructure::MaterialApplication& application = params.attributes->materials.back();

		application.channel = channel;
		application.side = side;
		application.blend_function = blend_function;
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadLineAttributes() {
		struct Params {
			_3DXMLRepresentation* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Color element
			map.emplace_back("Color", XMLParser::XSD::Element<Params>([](Params& params){
				float red = *(params.me->mReader.GetAttribute<float>("red", true));
				float green = *(params.me->mReader.GetAttribute<float>("green", true));
				float blue = *(params.me->mReader.GetAttribute<float>("blue", true));
				Optional<float> alpha = params.me->mReader.GetAttribute<float>("alpha");

				aiColor4D& color = params.me->mCurrentLine->color;
				color.r = red;
				color.g = green;
				color.b = blue;

				if(alpha) {
					color.a = alpha;
				} else {
					color.a = 1.0;
				}
			}, 0, 1));
			
			return std::move(map);
		})(), 1, 1);

		if(mReader.HasElements()) {
			mCurrentLine = _3DXMLStructure::ReferenceRep::MatID(new _3DXMLStructure::MaterialAttributes());

			params.me = this;
			//TODO: support lineType & thickness?

			mReader.ParseElements(&mapping, params);
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
