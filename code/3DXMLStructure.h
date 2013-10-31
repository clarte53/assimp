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

/** @file 3DXMLStructure.h
 *  @brief Defines the structure helper classes for the 3DXML loader
 */

#ifndef AI_3DXMLSTRUCTURE_H_INC
#define AI_3DXMLSTRUCTURE_H_INC

#include <list>
#include <map>
#include <string>

#if (defined _MSC_VER)
#pragma warning (disable:4503)
#endif

namespace Assimp {

	struct _3DXMLStructure {

		template<typename T>
		static bool less(const std::list<T>& l1, const std::list<T>& l2);

		struct URI {

			std::string uri;

			bool external;

			bool has_id;

			std::string filename;

			std::string extension;

			unsigned int id;

			URI();

		}; // struct URI

		struct ID {

			std::string filename;

			unsigned int id;

			ID(std::string _filename, unsigned int _id);
			
			bool operator==(const ID& other) const;

			bool operator<(const ID& other) const;

		}; // struct ID
		
		struct MaterialApplication {

			enum Operation { REPLACE, ADD, ALPHA_TRANSPARENCY };

			unsigned int channel;

			bool two_sided;

			Operation operation;

			std::list<ID> materials;

			MaterialApplication();

			bool operator==(const MaterialApplication& other) const;

			bool operator<(const MaterialApplication& other) const;

		}; // struct MaterialApplication

		struct SurfaceAttributes {

			aiColor3D color;

			std::list<MaterialApplication> materials;

			SurfaceAttributes();

			bool operator<(const SurfaceAttributes& other) const;

		}; // struct SurfaceAttributes

		struct Instance3D;

		struct InstanceRep;

		struct Reference3D {

			unsigned int id;
					
			bool has_name;

			std::string name;

			std::map<ID, Instance3D> instances;

			std::map<ID, InstanceRep> meshes;

			unsigned int nb_references;

			Reference3D();

		}; // struct Reference3D

		struct ReferenceRep {

			template<typename T>
			struct shared_less {

				bool operator()(const std::shared_ptr<T>& lhs, const std::shared_ptr<T>& rhs) const {
					return (lhs && rhs ? (*lhs) < (*rhs) : (bool) rhs);
				}

			}; // struct shared_less

			typedef std::shared_ptr<_3DXMLStructure::SurfaceAttributes> MatID;
			typedef std::map<MatID, ScopeGuard<aiMesh>, shared_less<_3DXMLStructure::SurfaceAttributes>> Meshes;
					
			unsigned int id;
					
			bool has_name;
					
			std::string name;

			Meshes meshes;

			unsigned int index_begin;

			unsigned int index_end;

			ReferenceRep();

		}; // struct ReferenceRep

		struct Instance3D {
					
			unsigned int id;
					
			bool has_name;

			ScopeGuard<aiNode> node;

			Reference3D* instance_of;

			Instance3D();

		}; // struct Instance3D

		struct InstanceRep {
					
			unsigned int id;

			bool has_name;

			std::string name;

			ReferenceRep* instance_of;

			InstanceRep();

		}; // struct InstanceRep

		aiScene* scene;

		std::map<ID, Reference3D> references;

		std::map<ID, ReferenceRep> representations;

		std::set<std::string> files_to_parse;

		unsigned int root_index;

		bool has_root_index;

		_3DXMLStructure(aiScene* _scene);

	}; // struct _3DXMLStructure

	template<typename T>
	bool _3DXMLStructure::less(const std::list<T>& l1, const std::list<T>& l2) {
		bool less = false;
		bool is_smaller = l1.size() < l2.size();

		const std::list<T>& min = (is_smaller ? l1 : l2);
		const std::list<T>& max = (is_smaller ? l2 : l1);

		auto match = std::mismatch(min.begin(), min.end(), max.begin());

		if(match.first == min.end()) {
			// All the common elements are equals, we must use the size to determine which one is "less"
			less = is_smaller;
		} else {
			// We have one element which is "less" than it's conterpart
			if(is_smaller) {
				less = *(match.first) < *(match.second);
			} else {
				// The lists are inversed, so we must inverse the result
				less = *(match.second) < *(match.first);
			}
		}

		return less;
	}

} // end of namespace Assimp

#endif // AI_3DXMLSTRUCTURE_H_INC
