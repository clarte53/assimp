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
	_3DXMLRepresentation::_3DXMLRepresentation(Q3BSP::Q3BSPZipArchive* archive, const std::string& filename, std::list<aiMesh*>& meshes) : mReader(archive, filename), mMeshes(meshes) {
		while(mReader.Next()) {
			// handle the root element "Manifest"
			if(mReader.IsElement("XMLRepresentation")) {
				while(mReader.Next()) {
					// Read the Root elements
					if(mReader.IsElement("Root")) {
						mCurrentMesh = new aiMesh();

						ReadVisualizationRep();

						mMeshes.push_back(mCurrentMesh);
					} else if(mReader.TestEndElement("XMLRepresentation")) {
						break;
					}
				}
			}
		}

		mReader.Close();
	}

	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLRepresentation::ThrowException(const std::string& error) const {
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader.GetFilename() % error));
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseMultiArray(const std::string& content, MultiArray<aiColor4D>& array, unsigned int channel, bool alpha) const {
		std::istringstream stream(content);
		float r, g, b, a;

		Array<aiColor4D>& data = array.Get(channel);

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

			data.Add(aiColor4D(r, g, b, a));
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ParseMultiArray(const std::string& content, MultiArray<aiVector3D>& array, unsigned int channel, unsigned int dimension) const {
		static const std::size_t dim_max = 3;

		float values[dim_max] = {0, 0, 0};
		
		std::istringstream stream(content);
		
		Array<aiVector3D>& data = array.Get(channel);

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

			data.Add(aiVector3D(values[0], values[1], values[2]));
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
		while(mReader.Next()) {
			if(mReader.IsElement("Rep")) {
				ReadVisualizationRep();
			} else if(mReader.IsEndElement("Root") || mReader.IsEndElement("Rep")) {
				break;
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadPolygonRep() {
		while(mReader.Next()) {
			if(mReader.IsElement("SurfaceAttributes")) {
				//TODO
			} else if(mReader.IsElement("LineAttributes")) {
				//TODO
			} else if(mReader.IsElement("PolygonalLOD")) {
				//TODO
			} else if(mReader.IsElement("Faces")) {
				ReadFaces();
			} else if(mReader.IsElement("Edges")) {
				//TODO
			} else if(mReader.IsElement("VertexBuffer")) {
				ReadVertexBuffer();
			} else if(mReader.IsEndElement("Root") || mReader.IsEndElement("Rep")) {
				break;
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadFaces() {
		static const unsigned int nb_vertices = 3;

		while(mReader.Next()) {
			if(mReader.IsElement("SurfaceAttributes")) {
				//TODO
			} else if(mReader.IsElement("Face")) {
				std::vector<unsigned int> data;

				_3DXMLParser::XMLReader::Optional<std::string> triangles = mReader.GetAttribute<std::string>("triangles");
				if(triangles) {
					data.clear();

					ParseTriangles(*triangles, data);

					for(unsigned int i = 0; i < data.size(); i += 3) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						for(unsigned int j = 0; j < nb_vertices; j++) {
							face.mIndices[j] = data[i + j];
						}

						mCurrentMesh->Faces.Add(face);
					}
				}

				_3DXMLParser::XMLReader::Optional<std::string> strips = mReader.GetAttribute<std::string>("strips");
				if(strips) {
					data.clear();

					ParseTriangles(*strips, data);

					for(unsigned int i = 0; i < data.size() - (nb_vertices - 1); i++) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						for(unsigned int j = 0; j < nb_vertices; j++) {
							face.mIndices[j] = data[i + j];
						}

						mCurrentMesh->Faces.Add(face);
					}
				}

				_3DXMLParser::XMLReader::Optional<std::string> fans = mReader.GetAttribute<std::string>("fans");
				if(fans) {
					data.clear();

					ParseTriangles(*fans, data);

					for(unsigned int i = 0; i < data.size() - (nb_vertices - 1); i++) {
						aiFace face;

						face.mNumIndices = nb_vertices;
						face.mIndices = new unsigned int[nb_vertices];

						face.mIndices[0] = data[0];
						for(unsigned int j = 1; j < nb_vertices; j++) {
							face.mIndices[j] = data[i + j];
						}

						mCurrentMesh->Faces.Add(face);
					}
				}
			} else if(mReader.TestEndElement("Faces")) {
				break;
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLRepresentation::ReadVertexBuffer() {
		while(mReader.Next()) {
			if(mReader.IsElement("Positions")) {
				std::string vertices = *(mReader.GetContent<std::string>(true));

				ParseArray(vertices, mCurrentMesh->Vertices);
			} else if(mReader.IsElement("Normals")) {
				std::string normals = *(mReader.GetContent<std::string>(true));

				ParseArray(normals, mCurrentMesh->Normals);
			} else if(mReader.IsElement("TextureCoordinates")) {
				_3DXMLParser::XMLReader::Optional<unsigned int> channel_opt = mReader.GetAttribute<unsigned int>("channel");
				std::string format = *(mReader.GetAttribute<std::string>("dimension", true));
				std::string coordinates = *(mReader.GetContent<std::string>(true));

				unsigned int channel = 0;
				if(channel_opt) {
					channel = *channel_opt;
				}

				if(format.size() != 2 || format[1] != 'D' || ! std::isdigit(format[0])) {
					ThrowException("Invalid texture coordinate format \"" + format + "\".");
				}

				unsigned int dimension = 0;
				std::string value(1, format[0]);
				
				dimension = std::atoi(value.c_str());

				if(dimension == 0 || dimension > 3) {
					ThrowException("Invalid dimension for texture coordinate format \"" + format + "\".");
				}

				ParseMultiArray(coordinates, mCurrentMesh->TextureCoords, channel, dimension);
			} else if(mReader.IsElement("DiffuseColors")) {
				std::string format = *(mReader.GetAttribute<std::string>("format", true));
				std::string color = *(mReader.GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					ParseMultiArray(color, mCurrentMesh->Colors, 0, false);
				} else if(format.compare("RGBA") == 0) {
					ParseMultiArray(color, mCurrentMesh->Colors, 0, true);
				} else {
					ThrowException("Unsupported color format \"" + format + "\".");
				}
			} else if(mReader.IsElement("SpecularColors")) {
				std::string format = *(mReader.GetAttribute<std::string>("format", true));
				std::string color = *(mReader.GetContent<std::string>(true));

				if(format.compare("RGB") == 0) {
					ParseMultiArray(color, mCurrentMesh->Colors, 1, false);
				} else if(format.compare("RGBA") == 0) {
					ParseMultiArray(color, mCurrentMesh->Colors, 1, true);
				} else {
					ThrowException("Unsupported color format \"" + format + "\".");
				}
			} else if(mReader.IsEndElement("VertexBuffer")) {
				break;
			}
		}

		if(mCurrentMesh->Vertices.Size() == 0) {
			ThrowException("The vertex buffer does not contain any vertex.");
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
