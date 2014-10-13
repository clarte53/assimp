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

#include "Optional.h"

#include <boost/noncopyable.hpp>
#include <condition_variable>
#include <mutex>

#if (defined _MSC_VER)
#	pragma warning (disable:4503)
#endif

namespace Assimp {

	struct _3DXMLStructure : boost::noncopyable {

		template<typename T>
		struct list_less {

				bool operator()(const std::list<T>& lhs, const std::list<T>& rhs) const;

		}; // struct list_less

		template<typename T>
		struct shared_less {

			bool operator()(const std::shared_ptr<T>& lhs, const std::shared_ptr<T>& rhs) const;

		}; // struct shared_less

		struct URI {

			std::string uri;

			std::string filename;

			std::string extension;

			Optional<unsigned int> id;

			bool external;

			URI();

			URI(const URI& other);

			URI(URI&& other);

			bool operator==(const URI& other) const;

			bool operator<(const URI& other) const;

		}; // struct URI

		struct ID {

			std::string filename;

			unsigned int id;

			ID(const std::string& filename, unsigned int id);

			ID(const ID& other);

			ID(ID&& other);

			ID& operator=(const ID& other);

			ID& operator=(ID&& other);
			
			bool operator==(const ID& other) const;

			bool operator<(const ID& other) const;

		}; // struct ID
		
		struct MaterialApplication {

			enum MappingSide {FRONT, BACK, FRONT_AND_BACK};

			enum TextureBlendFunction {REPLACE, ADD, ALPHA_TRANSPARENCY, LIGHTMAP, BURN, INVERT};

			unsigned int channel;

			MappingSide side;

			TextureBlendFunction blend_function;

			ID id;

			MaterialApplication(const std::string& filename, unsigned int id);

			MaterialApplication(const MaterialApplication& other);

			MaterialApplication(MaterialApplication&& other);

			MaterialApplication& operator=(const MaterialApplication& other);

			MaterialApplication& operator=(MaterialApplication&& other);

			bool operator==(const MaterialApplication& other) const;

			bool operator<(const MaterialApplication& other) const;

		}; // struct MaterialApplication

		struct MaterialAttributes : public boost::noncopyable {

			typedef std::shared_ptr<MaterialAttributes> ID;

			aiColor4D color;

			std::list<MaterialApplication> materials;

			bool is_color;

			unsigned int index;

			std::map<unsigned int, unsigned int> uv_translation;

			MaterialAttributes();

			MaterialAttributes(MaterialAttributes&& other);

			bool operator<(const MaterialAttributes& other) const;

		}; // struct MaterialAttributes
		
		struct MaterialDomain : public boost::noncopyable {

			unsigned int id;
					
			bool has_name;

			std::string name;

			std::unique_ptr<aiMaterial> material;

			MaterialDomain();

			MaterialDomain(MaterialDomain&& other);

		}; // struct MaterialDomain

		struct MaterialDomainInstance : public boost::noncopyable {

			unsigned int id;

			bool has_name;

			std::string name;
			
			MaterialDomain* instance_of;

			MaterialDomainInstance();

			MaterialDomainInstance(MaterialDomainInstance&& other);

		}; // struct MaterialDomainInstance

		struct CATRepresentationImage : public boost::noncopyable {

				unsigned int id;

				bool has_name;

				std::string name;

				unsigned int index;

				std::unique_ptr<aiTexture> texture;

				CATRepresentationImage();

				CATRepresentationImage(CATRepresentationImage&& other);

		}; // struct CATRepresentationImage

		struct CATMatReference : public boost::noncopyable {

			unsigned int id;
					
			bool has_name;

			std::string name;

			std::map<ID, MaterialDomainInstance> materials;

			std::unique_ptr<aiMaterial> merged_material;

			CATMatReference();

			CATMatReference(CATMatReference&& other);

		}; // struct CATMatReference

		struct CATMatConnection : public boost::noncopyable {

			unsigned int channel;

			std::list<ID> references;

			std::list<ID> materials;

			CATMatConnection();

			CATMatConnection(CATMatConnection&& other);

		}; // struct CATMatConnection

		struct Instance3D;

		struct InstanceRep;

		struct Reference3D : public boost::noncopyable {

			unsigned int id;
					
			bool has_name;

			std::string name;

			int nb_references;

			int total_references;

			std::map<ID, Instance3D> instances;

			std::map<ID, InstanceRep> meshes;

			Reference3D();

			Reference3D(Reference3D&& other);

		}; // struct Reference3D

		struct ReferenceRep : public boost::noncopyable {

			struct Geometry : public boost::noncopyable {

				enum Type {POINTS = 0, LINES, MESH, NB_TYPES /* do not use as a type */};

				std::unique_ptr<aiMesh> mesh;

				Type type;

				Geometry(Type type);

				Geometry(Type type, aiMesh* mesh);

				Geometry(Geometry&& other);

			}; // class Geometry

			typedef std::multimap<MaterialAttributes::ID, Geometry, shared_less<MaterialAttributes>> Meshes;
					
			unsigned int id;
					
			bool has_name;
					
			std::string name;

			int nb_references;

			std::map<unsigned int, std::list<unsigned int>> indexes;

			Meshes meshes;

			ReferenceRep();

			ReferenceRep(ReferenceRep&& other);

		}; // struct ReferenceRep

		struct Instance3D : public boost::noncopyable {
					
			unsigned int id;
					
			bool has_name;

			std::unique_ptr<aiNode> node;

			Reference3D* instance_of;

			Optional<unsigned int> material_index;

			Instance3D();

			Instance3D(Instance3D&& other);

		}; // struct Instance3D

		struct InstanceRep : public boost::noncopyable {
					
			unsigned int id;

			bool has_name;

			std::string name;

			ReferenceRep* instance_of;

			InstanceRep();

			InstanceRep(InstanceRep&& other);

		}; // struct InstanceRep

		class Dependencies : public boost::noncopyable {

			protected:

				std::set<std::string> files_parsed;

				std::queue<std::string> files_to_parse;

				std::condition_variable* thread_notifier;

				std::mutex mutex;

			public:

				Dependencies(std::condition_variable* notifier);

				Dependencies(Dependencies&& other);

				void add(const std::string& file);

				std::string next();

		}; // class Dependencies

		_3DXMLStructure(aiScene* _scene, std::condition_variable* notifier);

		_3DXMLStructure(_3DXMLStructure&& other);

		// ------------------------------------------------------------------------------------------------
		// Owned by _3DXMLParser::Build* section

		aiScene* scene;

		// ------------------------------------------------------------------------------------------------
		// Owned by _3DXMLParser::ReadProductStructure section

		Optional<unsigned int> ref_root_index;

		std::map<ID, Reference3D> references_node;

		std::map<ID, ReferenceRep> representations;

		// ------------------------------------------------------------------------------------------------
		// Owned by _3DXMLParser::ReadCATMaterialRef section

		Optional<unsigned int> mat_root_index;

		std::map<ID, CATMatReference> references_mat;

		std::map<ID, MaterialDomain> materials;

		// ------------------------------------------------------------------------------------------------
		// Owned by _3DXMLParser::ReadCATRepImage section

		std::map<ID, CATRepresentationImage> textures;

		// ------------------------------------------------------------------------------------------------
		// Owned by _3DXMLParser::ReadCATMaterial section

		std::list<CATMatConnection> mat_connections;

		// ------------------------------------------------------------------------------------------------
		// Shared content

		Dependencies dependencies;

	}; // struct _3DXMLStructure

	template<typename T>
	bool _3DXMLStructure::list_less<T>::operator()(const std::list<T>& lhs, const std::list<T>& rhs) const {
		bool less = false;
		bool is_smaller = lhs.size() < rhs.size();

		const std::list<T>& min = (is_smaller ? lhs : rhs);
		const std::list<T>& max = (is_smaller ? rhs : lhs);

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

	template<typename T>
	bool _3DXMLStructure::shared_less<T>::operator()(const std::shared_ptr<T>& lhs, const std::shared_ptr<T>& rhs) const {
		return (lhs && rhs ? (*lhs) < (*rhs) : (bool) rhs);
	}

} // end of namespace Assimp

#endif // AI_3DXMLSTRUCTURE_H_INC
