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

#include "3DXMLStructure.h"

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::URI::URI() : uri(""), filename(""), extension(""), id(), external(false) {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::URI::URI(URI&& other) : uri(std::move(other.uri)), filename(std::move(other.filename)), extension(std::move(other.extension)), id(std::move(other.id)), external(other.external) {
	
	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::URI::operator==(const URI& other) const {
		return (uri.compare(other.uri) == 0);
	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::URI::operator<(const URI& other) const {
		return (uri.compare(other.uri) < 0);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ID::ID(const std::string& _filename, unsigned int _id) : filename(_filename), id(_id) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ID::ID(const ID& other) : filename(other.filename), id(other.id) {

	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ID::ID(ID&& other) : filename(std::move(other.filename)), id(other.id) {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ID& _3DXMLStructure::ID::operator=(const ID& other) {
		if(this != &other) {
			filename = other.filename;
			id = other.id;
		}

		return *this;
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ID& _3DXMLStructure::ID::operator=(ID&& other) {
		if(this != &other) {
			filename = std::move(other.filename);
			id = other.id;
		}

		return *this;
	}
	
	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::ID::operator==(const ID& other) const {
		return id == other.id && filename.compare(other.filename) == 0;
	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::ID::operator<(const ID& other) const {
		int comp = filename.compare(other.filename);
		return comp < 0 || (comp == 0 && id < other.id);
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialApplication::MaterialApplication() : channel(0), side(FRONT), blend_function(REPLACE), materials() {

	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialApplication::MaterialApplication(MaterialApplication&& other) : channel(other.channel), side(other.side), blend_function(other.blend_function), materials(std::move(other.materials)) {

	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::MaterialApplication::operator==(const MaterialApplication& other) const {
		return channel == other.channel && side == other.side && blend_function == other.blend_function && std::equal(materials.begin(), materials.end(), other.materials.begin());
	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::MaterialApplication::operator<(const MaterialApplication& other) const {
		return channel < other.channel || (
			channel == other.channel && (
				side < other.side || (
					side == other.side && (
						blend_function < other.blend_function || (
							blend_function == other.blend_function && (
								less(materials, other.materials)
							)
						)
					)
				)
			)
		);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialAttributes::MaterialAttributes() : color(), materials() {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialAttributes::MaterialAttributes(MaterialAttributes&& other) : color(other.color), materials(std::move(other.materials)) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::MaterialAttributes::operator<(const MaterialAttributes& other) const {
		return color < other.color || (
			color == other.color && less(materials, other.materials)
		);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialDomain::MaterialDomain() : id(0), has_name(false), name(""), material(new aiMaterial()) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialDomain::MaterialDomain(MaterialDomain&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), material(std::move(other.material)) {
		other.material = nullptr;
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialDomainInstance::MaterialDomainInstance() : id(0), has_name(false), name(""), instance_of(nullptr) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialDomainInstance::MaterialDomainInstance(MaterialDomainInstance&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), instance_of(other.instance_of) {
		other.instance_of = nullptr;
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATMatReference::CATMatReference() : id(0), has_name(false), name("") {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATMatReference::CATMatReference(CATMatReference&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Reference3D::Reference3D() : id(0), has_name(false), name(""), nb_references(0) {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Reference3D::Reference3D(Reference3D&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), nb_references(other.nb_references) {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::ReferenceRep() : id(0), has_name(false), name(""), meshes(), index_begin(0), index_end(0) {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::ReferenceRep(ReferenceRep&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), meshes(std::move(other.meshes)), index_begin(other.index_begin), index_end(other.index_end) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Instance3D::Instance3D() : id(0), has_name(false), node(new aiNode()), instance_of(nullptr) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Instance3D::Instance3D(Instance3D&& other) : id(other.id), has_name(other.has_name), node(std::move(other.node)), instance_of(other.instance_of) {
		other.node = nullptr;
		other.instance_of = nullptr;
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::InstanceRep::InstanceRep() : id(0), has_name(false), name(""), instance_of(nullptr) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::InstanceRep::InstanceRep(InstanceRep&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), instance_of(other.instance_of) {
		other.instance_of = nullptr;
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::_3DXMLStructure(aiScene* _scene) : scene(_scene), ref_root_index(), references_node(), references_mat(), representations(), materials(), files_to_parse() {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::_3DXMLStructure(_3DXMLStructure&& other) : scene(other.scene), ref_root_index(std::move(other.ref_root_index)), references_node(std::move(other.references_node)), references_mat(std::move(other.references_mat)), representations(std::move(other.representations)), materials(std::move(other.materials)), files_to_parse(std::move(other.files_to_parse)) {
		other.scene = nullptr;
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER