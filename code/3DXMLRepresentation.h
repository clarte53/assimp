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

/** @file 3DXMLRepresentation.h
 *  @brief Defines the parser of representations helper class for the 3DXML loader
 */

#ifndef AI_3DXMLREPRESENTATION_H_INC
#define AI_3DXMLREPRESENTATION_H_INC

#include "3DXMLStructure.h"
#include "Optional.h"
#include "XMLParser.h"

namespace Assimp {

	class _3DXMLRepresentation : noncopyable {

		protected:

			struct Face {

				_3DXMLStructure::MaterialAttributes::ID surface_attribute;

				Optional<std::string> triangles;

				Optional<std::string> strips;

				Optional<std::string> fans;

			}; // struct Face

			struct Faces {

				_3DXMLStructure::MaterialAttributes::ID surface_attribute;

				std::list<Face> faces;

			}; // struct Faces

			struct Polyline {

				_3DXMLStructure::MaterialAttributes::ID line_attribute;

				Optional<std::string> vertices;

			}; // struct Polyline

			struct Edges {


				_3DXMLStructure::MaterialAttributes::ID line_attribute;

				std::list<Polyline> edges;

			}; // struct Edges

			struct PolygonalRep {

				_3DXMLStructure::MaterialAttributes::ID surface_attribute;

				_3DXMLStructure::MaterialAttributes::ID line_attribute;

				std::list<Faces> surfaces;

				std::list<Edges> lines;

				aiMesh vertex_buffer;

				_3DXMLStructure::ReferenceRep::Meshes meshes;

			}; // struct PolygonalRep

			/** xml reader */
			XMLParser mReader;

			/** Data corresponding to the currently parsed PolygonalRep */
			std::unique_ptr<PolygonalRep> mCurrentRep;

			/** List containing the parsed meshes */
			_3DXMLStructure::ReferenceRep::Meshes& mMeshes;

			/** List of files this representation depends on */
			_3DXMLStructure::Dependencies& mDependencies;

		public: 

			_3DXMLRepresentation(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& filename, _3DXMLStructure::ReferenceRep::Meshes& meshes, _3DXMLStructure::Dependencies& dependencies);

			virtual ~_3DXMLRepresentation();

		protected:

			static void PropagateAttributes(_3DXMLStructure::MaterialAttributes::ID& parent, _3DXMLStructure::MaterialAttributes::ID& child);

			/** Aborts the file reading with an exception */
			void ThrowException(const std::string& error) const;

			void ParseArray(const std::string& content, std::vector<aiVector3D>& array) const;

			void ParseArray(const std::string& content, Array<aiVector3D>& array, unsigned int start_index) const;

			void ParseMultiArray(const std::string& content, MultiArray<aiColor4D>& array, unsigned int channel, unsigned int start_index,  bool alpha = true) const;

			void ParseMultiArray(const std::string& content, MultiArray<aiVector3D>& array, unsigned int channel, unsigned int start_index, unsigned int dimension) const;

			void ParseTriangles(const std::string& content, std::list<std::vector<unsigned int>>& triangles) const;

			void ParseFaces(const Face& face);

			void ParseEdges(const Polyline& edge);

			void ParseVertexBuffer();

			void ParsePolygonalRep();

			void ReadVisualizationRep();

			void ReadBagRep();

			void ReadPolygonalRep();

			void ReadFaces();

			void ReadEdges();

			void ReadVertexBuffer();

			void ReadSurfaceAttributes(_3DXMLStructure::MaterialAttributes::ID& attributes);

			void ReadMaterialApplication(_3DXMLStructure::MaterialAttributes* attributes);

			void ReadLineAttributes(_3DXMLStructure::MaterialAttributes::ID& attributes);

	}; // class _3DXMLRepresentation

} // end of namespace Assimp

#endif // AI_3DXMLREPRESENTATION_H_INC
