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
	_3DXMLStructure::ID::ID(const std::string& filename, unsigned int id) : filename(filename), id(id) {
	
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
	_3DXMLStructure::MaterialApplication::MaterialApplication(const std::string& filename, unsigned int id) : channel(0), side(FRONT), blend_function(REPLACE), id(filename, id) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialApplication::MaterialApplication(const MaterialApplication& other) : channel(other.channel), side(other.side), blend_function(other.blend_function), id(other.id) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialApplication::MaterialApplication(MaterialApplication&& other) : channel(other.channel), side(other.side), blend_function(other.blend_function), id(std::move(other.id)) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialApplication& _3DXMLStructure::MaterialApplication::operator=(const MaterialApplication& other) {
		if(this != &other) {
			channel = other.channel;
			side = other.side;
			blend_function = other.blend_function;
			id = other.id;
		}

		return *this;
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialApplication& _3DXMLStructure::MaterialApplication::operator=(MaterialApplication&& other) {
		if(this != &other) {
			channel = other.channel;
			side = other.side;
			blend_function = other.blend_function;
			id = other.id;
		}

		return *this;
	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::MaterialApplication::operator==(const MaterialApplication& other) const {
		return channel == other.channel && side == other.side && blend_function == other.blend_function && id == other.id;
	}

	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::MaterialApplication::operator<(const MaterialApplication& other) const {
		return channel < other.channel || (
			channel == other.channel && (
				side < other.side || (
					side == other.side && (
						blend_function < other.blend_function || (
							blend_function == other.blend_function && (
								id < other.id
							)
						)
					)
				)
			)
		);
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialAttributes::MaterialAttributes() : color(), materials(), is_color(false), index(0), uv_translation() {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::MaterialAttributes::MaterialAttributes(MaterialAttributes&& other) : color(other.color), materials(std::move(other.materials)), is_color(other.is_color), index(other.index), uv_translation(std::move(other.uv_translation)) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	bool _3DXMLStructure::MaterialAttributes::operator<(const MaterialAttributes& other) const {
		return (
			is_color < other.is_color || (
				is_color == other.is_color && (
					color < other.color || (
						color == other.color && (
							list_less<MaterialApplication>()(materials, other.materials)
						)
					)
				)
			)
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
	_3DXMLStructure::CATRepresentationImage::CATRepresentationImage() : id(0), has_name(false), name(""), index(0), texture(new aiTexture()) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATRepresentationImage::CATRepresentationImage(CATRepresentationImage&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), index(other.index), texture(std::move(other.texture)) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATMatReference::CATMatReference() : id(0), has_name(false), name(""), materials(), merged_material(nullptr) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATMatReference::CATMatReference(CATMatReference&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), materials(std::move(other.materials)), merged_material(std::move(other.merged_material)) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATMatConnection::CATMatConnection() : channel(0), references(), materials() {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::CATMatConnection::CATMatConnection(CATMatConnection&& other) : channel(other.channel), references(std::move(other.references)), materials(std::move(other.materials)) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Reference3D::Reference3D() : id(0), has_name(false), name(""), nb_references(0), instances(), meshes() {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Reference3D::Reference3D(Reference3D&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), nb_references(other.nb_references), instances(std::move(other.instances)), meshes(std::move(other.meshes)) {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::Geometry::Geometry(Type type) : mesh(new aiMesh()), type(type) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::Geometry::Geometry(Type type, aiMesh* mesh) : mesh(mesh), type(type) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::Geometry::Geometry(Geometry&& other) : mesh(std::move(other.mesh)), type(other.type) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::ReferenceRep() : id(0), has_name(false), name(""), nb_references(0), indexes(), meshes() {
	
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::ReferenceRep::ReferenceRep(ReferenceRep&& other) : id(other.id), has_name(other.has_name), name(std::move(other.name)), nb_references(other.nb_references), indexes(std::move(other.indexes)), meshes(std::move(other.meshes)) {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Instance3D::Instance3D() : id(0), has_name(false), node(new aiNode()), instance_of(nullptr), material_index() {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Instance3D::Instance3D(Instance3D&& other) : id(other.id), has_name(other.has_name), node(std::move(other.node)), instance_of(other.instance_of), material_index(std::move(other.material_index)) {
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
	_3DXMLStructure::Dependencies::Dependencies() : files_parsed(), files_to_parse() {

	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::Dependencies::Dependencies(Dependencies&& other) : files_parsed(std::move(other.files_parsed)), files_to_parse(std::move(other.files_to_parse)) {

	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLStructure::Dependencies::add(const std::string& file) {
		auto done = files_parsed.find(file);

		if(done == files_parsed.end()) {
			files_to_parse.insert(file);
		}
	}

	// ------------------------------------------------------------------------------------------------
	std::string _3DXMLStructure::Dependencies::next() {
		std::string result = "";

		if(! files_to_parse.empty()) {
			result = *(files_to_parse.begin());

			files_parsed.insert(result);
			files_to_parse.erase(result);
		}

		return result;
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::_3DXMLStructure(aiScene* _scene) : scene(_scene), ref_root_index(), references_node(), representations(), mat_root_index(), references_mat(), materials(), mat_connections(), dependencies() {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	_3DXMLStructure::_3DXMLStructure(_3DXMLStructure&& other) : scene(other.scene), ref_root_index(std::move(other.ref_root_index)), references_node(std::move(other.references_node)), representations(std::move(other.representations)), mat_root_index(std::move(other.mat_root_index)), references_mat(std::move(other.references_mat)), materials(std::move(other.materials)), mat_connections(std::move(other.mat_connections)), dependencies(std::move(other.dependencies)) {
		other.scene = nullptr;
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
