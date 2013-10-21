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

#include "3DXMLParser.h"
#include "../include/assimp/mesh.h"

#include <list>
#include <sstream>

namespace Assimp {

	class _3DXMLRepresentation {

		protected:

			/** Current xml reader */
			_3DXMLParser::XMLReader mReader;

			/** List containing the parsed meshes */
			std::list<aiMesh*>& mMeshes;

			/** The mesh currently parsed */
			aiMesh* mCurrentMesh;

		public: 

			_3DXMLRepresentation(Q3BSP::Q3BSPZipArchive* archive, const std::string& filename, std::list<aiMesh*>& meshes);

		protected:

			/** Aborts the file reading with an exception */
			void ThrowException(const std::string& error) const;

			template<typename T>
			void ParseArray(const std::string& content, Array<T>& array);

			template<>
			void ParseArray<aiVector3D>(const std::string& content, Array<aiVector3D>& array);

			void ParseMultiArray(const std::string& content, MultiArray<aiColor4D>& array, unsigned int channel, bool alpha = true);

			void ParseMultiArray(const std::string& content, MultiArray<aiVector3D>& array, unsigned int channel, unsigned int dimension);

			void ReadVisualizationRep();

			void ReadBagRep();

			void ReadPolygonRep();

			void ReadFaces();

			void ReadVertexBuffer();

	}; // class _3DXMLRepresentation

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	void _3DXMLRepresentation::ParseArray(const std::string& content, Array<T>& array) {
		std::istringstream stream(content);
		T value;

		while(! stream.eof()) {
			stream >> value;

			if(stream.fail()) {
				ThrowException("Can not convert array value to \"" + std::string(typeid(T).name()) +"\".");
			}

			array.Add(value);
		}
	}

	template<>
	void _3DXMLRepresentation::ParseArray<aiVector3D>(const std::string& content, Array<aiVector3D>& array) {
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

			array.Add(aiVector3D(x, y, z));
		}
	}
	
} // end of namespace Assimp

#endif // AI_3DXMLREPRESENTATION_H_INC
