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

/** @file 3DXMLMaterial.h
 *  @brief Defines the material parser helper class for the 3DXML loader
 */

#ifndef AI_3DXMLMATERIAL_H_INC
#define AI_3DXMLMATERIAL_H_INC

#include "3DXMLStructure.h"
#include "Optional.h"
#include "XMLParser.h"

#include <sstream>

namespace Assimp {

	class _3DXMLMaterial : noncopyable {
		
		protected:

			enum MappingType {NONE, ENVIRONMENT_MAPPING, IMPLICIT_MAPPING, OPERATOR_MAPPING};

			struct GlobalData {

				public:

					MappingType mapping_type;

					aiTextureMapping mapping_operator;

					aiUVTransform transform;

					bool has_transform;

					float ambient_coef;

					float diffuse_coef;

					float emissive_coef;

			};

			/** xml reader */
			XMLParser mReader;

			aiMaterial* mMaterial;

			_3DXMLStructure::Dependencies& mDependencies;

		public:

			_3DXMLMaterial(std::shared_ptr<ZipArchiveIOSystem> archive, const std::string& filename, aiMaterial* material, _3DXMLStructure::Dependencies& dependencies);

			virtual ~_3DXMLMaterial();

		protected:

			/** Aborts the file reading with an exception */
			void ThrowException(const std::string& error) const;

			void SetCoefficient(float coef, const char* key, unsigned int type, unsigned int index);

			void ReadFeature(GlobalData* data);

			template<typename T>
			inline T ReadValue(const std::string& value_str) const;

			template<typename T>
			std::vector<T> ReadValues(const std::string& values_str) const;
			
	}; // end of class _3DXMLMaterial

	template<typename T>
	inline T _3DXMLMaterial::ReadValue(const std::string& value_str) const {
		return mReader.FromString<T>(value_str);
	}

	template<typename T>
	std::vector<T> _3DXMLMaterial::ReadValues(const std::string& values_str) const {
		std::istringstream stream(values_str);
		std::vector<T> result;

		while(! stream.eof()) {
			int next = stream.peek();
			while(! stream.eof() && (next == ',' || next == '[' || next == ']' || next == ' ')) {
				stream.get();
				next = stream.peek();
			}

			if(! stream.eof()) {
				result.push_back(mReader.FromString<T>(stream));
			}
		}

		// We're using C++11, therefore the vector will be automatically moved by the compiler
		return result;
	}

} // end of namespace Assimp

#endif // AI_3DXMLMATERIAL_H_INC
