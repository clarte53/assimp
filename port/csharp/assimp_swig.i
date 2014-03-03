/*
---------------------------------------------------------------------------
Open Asset Import Library (ASSIMP)
---------------------------------------------------------------------------

Copyright (c) 2006-2010, ASSIMP Development Team

All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the following 
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the ASSIMP team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the ASSIMP Development Team.

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
---------------------------------------------------------------------------
*/
%module assimp_swig

%include "typemaps.i"
%include "std_string.i"
%include "arrays_csharp.i"

%{
#include "..\..\include\assimp\defs.h"
#include "..\..\include\assimp\config.h"
#include "..\..\include\assimp\version.h"
#include "..\..\include\assimp\ai_assert.h"
#include "..\..\include\assimp\types.h"
#include "..\..\include\assimp\color4.h"
#include "..\..\include\assimp\vector2.h"
#include "..\..\include\assimp\vector3.h"
#include "..\..\include\assimp\quaternion.h"
#include "..\..\include\assimp\matrix3x3.h"
#include "..\..\include\assimp\matrix4x4.h"
#include "..\..\include\assimp\scene.h"
#include "..\..\include\assimp\mesh.h"
#include "..\..\include\assimp\anim.h"
#include "..\..\include\assimp\material.h"
#include "..\..\include\assimp\texture.h"
#include "..\..\include\assimp\camera.h"
#include "..\..\include\assimp\light.h"
#include "..\..\include\assimp\postprocess.h"
#include "..\..\include\assimp\metadata.h"
#include "..\..\include\assimp\Importer.hpp"
#include "..\..\include\assimp\cimport.h"
#include "..\..\include\assimp\importerdesc.h"
#include "..\..\include\assimp\Exporter.hpp"
#include "..\..\include\assimp\cexport.h"
#include "..\..\include\assimp\IOStream.hpp"
#include "..\..\include\assimp\IOSystem.hpp"
#include "..\..\include\assimp\cfileio.h"
#include "..\..\include\assimp\Logger.hpp"
#include "..\..\include\assimp\DefaultLogger.hpp"
#include "..\..\include\assimp\NullLogger.hpp"
#include "..\..\include\assimp\LogStream.hpp"
#include "..\..\include\assimp\ProgressHandler.hpp"
#include "..\..\include\assimp\Array.hpp"
%}

#define C_STRUCT
#define C_ENUM
#define ASSIMP_API
#define PACK_STRUCT
#define AI_FORCE_INLINE

%rename(op_add) operator+;
%rename(op_add_and_set) operator+=;
%rename(op_sub) operator-;
%rename(op_sub_and_set) operator-=;
%rename(op_mul) operator*;
%rename(op_mul_and_set) operator*=;
%rename(op_div) operator/;
%rename(op_div_and_set) operator/=;
%rename(op_equal) operator==;
%rename(op_not_equal) operator!=;
%rename(op_get) operator[];
%rename(op_set) operator=;
%rename(op_greater) operator>;
%rename(op_lesser) operator<;
%rename(op_new) operator new;
%rename(op_new_array) operator new[];
%rename(op_delete) operator delete;
%rename(op_delete_array) operator delete[];

%include "..\..\include\assimp\Array.hpp"

%extend Interface {
%typemap(cscode) Interface %{
  public interface Unmanagable<T> {
    T Unmanaged();
  }

  public interface DynamicArray<T> {
    uint Size();
    T Get(uint index);
  }

  public interface MutableDynamicArray<T> : DynamicArray<T> {
    bool Set(uint index, T value);
  }

  public interface MultiArray<T> : DynamicArray<T> {

  }

  public interface FixedArray<T> : MutableDynamicArray<T> {

  }

  public interface Array<T> : MutableDynamicArray<T> {
    void Clear();
  }
%}
}

%typemap(cstype) unsigned int*, unsigned int& "uint"
%typemap(csout, excode=SWIGEXCODE) unsigned int*, unsigned int& {
	uint ret = (uint) Marshal.ReadInt32($imcall);$excode
	return ret;
}

%define ENUM_FLAGS_DECL(TYPE)
%typemap(csclassmodifiers) TYPE %{[System.Flags] public enum%}
%enddef

%define DEF_ENUM(TYPE, NAME)
%typemap(cstype)   unsigned int NAME "TYPE";
%typemap(csin)     unsigned int NAME "(uint)$csinput";
%typemap(csvarout) unsigned int NAME %{ get { return (TYPE)$imcall; } %}
%enddef

%define ENUM_TYPEMAP(TYPE)
%typemap(imtype, out="IntPtr")       TYPE *INPUT, TYPE &INPUT "TYPE"
%typemap(cstype, out="$csclassname") TYPE *INPUT, TYPE &INPUT "TYPE"
%typemap(csin)                       TYPE *INPUT, TYPE &INPUT "$csinput"
%typemap(imtype, out="IntPtr")       TYPE *OUTPUT, TYPE &OUTPUT "out TYPE"
%typemap(cstype, out="$csclassname") TYPE *OUTPUT, TYPE &OUTPUT "out TYPE"
%typemap(csin)                       TYPE *OUTPUT, TYPE &OUTPUT "out $csinput"
%enddef

%define ADD_UNMANAGED_OPTION_BASE(NAME, CTYPE)
%typemap(csinterfaces) CTYPE "IDisposable, Interface.Unmanagable<$typemap(cstype, CTYPE)>"
%typemap(cscode) CTYPE %{
  public NAME Unmanaged() {
    this.swigCMemOwn = false;
    return this;
  }
%}
%enddef

%define ADD_UNMANAGED_OPTION(CTYPE)
ADD_UNMANAGED_OPTION_BASE(CTYPE, CTYPE);
%enddef

%define ARRAY_DECL(NAME, CTYPE)
%typemap(csinterfaces) Array<CTYPE> "IDisposable, Interface.Array<$typemap(cstype, CTYPE)>"
%ignore Array<CTYPE>::Array;
%ignore Array<CTYPE>::Create;
%template(NAME##Array) Array<CTYPE>;
%enddef

%define FIXED_ARRAY_DECL(NAME, CTYPE)
%typemap(csinterfaces) Array<CTYPE> "IDisposable, Interface.FixedArray<$typemap(cstype, CTYPE)>"
%ignore FixedArray<CTYPE>::FixedArray;
%ignore FixedArray<CTYPE>::Create;
%template(NAME##FixedArray) FixedArray<CTYPE>;
%enddef

%define MULTI_ARRAY_DECL(NAME, CTYPE)
%typemap(csinterfaces) Array<CTYPE> "IDisposable, Interface.MultiArray<$typemap(cstype, CTYPE)>"
%ignore MultiArray<CTYPE>::MultiArray;
%ignore MultiArray<CTYPE>::Create;
%ignore MultiArray<CTYPE>::Clear;
%ignore MultiArray<CTYPE>::Set;
ARRAY_DECL(NAME, CTYPE);
%template(NAME##MultiArray) MultiArray<CTYPE>;
%enddef

%define MATERIAL(ENUM, TYPE, NAME, KEY)
%newobject aiMaterial::Get##NAME;
%newobject aiMaterial::Set##NAME;
%extend aiMaterial {
	bool Get##NAME(TYPE& OUTPUT) {
		#if ENUM
			typedef int TEMPLATE_TYPE;
		#else
			typedef TYPE TEMPLATE_TYPE;
		#endif
		return $self->Get(KEY, (TEMPLATE_TYPE&) OUTPUT) == AI_SUCCESS;
	}
	bool Set##NAME(const TYPE& INPUT) {
	#if #TYPE == "aiString"
		return $self->AddProperty(&INPUT, KEY) == AI_SUCCESS;
	#else
		#if ENUM
			typedef int TEMPLATE_TYPE;
		#else
			typedef TYPE TEMPLATE_TYPE;
		#endif
		return $self->AddProperty((TEMPLATE_TYPE*) &INPUT, 1, KEY) == AI_SUCCESS;
	#endif
	}
}
%enddef

%define MATERIAL_TEXTURE(ENUM, TYPE, NAME, KEY)
%newobject aiMaterial::Get##NAME;
%newobject aiMaterial::Set##NAME;
%extend aiMaterial {
	bool Get##NAME(aiTextureType type, unsigned int index, TYPE& OUTPUT) {
		#if ENUM
			typedef int TEMPLATE_TYPE;
		#else
			typedef TYPE TEMPLATE_TYPE;
		#endif
		return $self->Get(KEY(type, index), (TEMPLATE_TYPE&) OUTPUT) == AI_SUCCESS;
	}
	bool Set##NAME(aiTextureType type, unsigned int index, const TYPE& INPUT) {
	#if #TYPE == "aiString"
		return $self->AddProperty(&INPUT, KEY(type, index)) == AI_SUCCESS;
	#else
		#if ENUM
			typedef int TEMPLATE_TYPE;
		#else
			typedef TYPE TEMPLATE_TYPE;
		#endif
		return $self->AddProperty((TEMPLATE_TYPE*) &INPUT, 1, KEY(type, index)) == AI_SUCCESS;
	#endif
	}
}
%enddef

/////// aiAnimation 
%ignore aiAnimation::mNumChannels;
%ignore aiAnimation::mChannels;
%ignore aiAnimation::mNumMeshChannels;
%ignore aiAnimation::mMeshChannels;

/////// aiAnimMesh 
%ignore aiAnimMesh::mNumVertices;
%ignore aiAnimMesh::mVertices;
%ignore aiAnimMesh::mNormals;
%ignore aiAnimMesh::mTangents;
%ignore aiAnimMesh::mColors;
%ignore aiAnimMesh::mTextureCoords;

/////// aiBone 
%ignore aiBone::mNumWeights;
%ignore aiBone::mWeights;

/////// aiCamera 
// OK

/////// aiColor3D 
ADD_UNMANAGED_OPTION(aiColor3D);
%ignore aiColor3D::operator[](unsigned int);

/////// aiColor4D 
// cf template section
ADD_UNMANAGED_OPTION_BASE(aiColor4D, aiColor4t<float>);
%ignore aiColor4t<float>::operator[](unsigned int);

/////// aiComponent
ENUM_FLAGS_DECL(aiComponent);

/////// aiDefaultLogStream
ENUM_FLAGS_DECL(aiDefaultLogStream);

/////// aiExportDataBlob
%ignore aiExportDataBlob::data;

/////// aiExportFormatDesc
%immutable aiExportFormatDesc::id;
%immutable aiExportFormatDesc::description;
%immutable aiExportFormatDesc::fileExtension;

/////// aiFace
ADD_UNMANAGED_OPTION(aiFace);
%ignore aiFace::mNumIndices;
%ignore aiFace::mIndices;

/////// aiFile
%ignore aiFile::FileSizeProc;
%ignore aiFile::FlushProc;
%ignore aiFile::ReadProc;
%ignore aiFile::SeekProc;
%ignore aiFile::TellProc;
%ignore aiFile::WriteProc;

/////// aiFileIO
%ignore aiFileIO::CloseProc;
%ignore aiFileIO::OpenProc;

/////// aiImporterDesc
%immutable aiImporterDesc::mName;
%immutable aiImporterDesc::mAuthor;
%immutable aiImporterDesc::mMaintainer;
%immutable aiImporterDesc::mComments;
%immutable aiImporterDesc::mFileExtensions;

/////// aiImporterFlags
ENUM_FLAGS_DECL(aiImporterFlags);

/////// aiLight 
// Done

/////// aiLightSourceType 
// Done

/////// aiLogStream
%ignore aiLogStream::callback;

/////// aiMaterial 
ENUM_TYPEMAP(aiBlendMode);
ENUM_TYPEMAP(aiShadingMode);
ENUM_TYPEMAP(aiTextureMapping);
ENUM_TYPEMAP(aiTextureFlags);
ENUM_TYPEMAP(aiTextureMapMode);
ENUM_TYPEMAP(aiTextureOp);
ADD_UNMANAGED_OPTION(aiMaterial);
#pragma SWIG nowarn=305
%ignore aiMaterial::AddBinaryProperty;
%ignore aiMaterial::AddProperty;
%ignore aiMaterial::Get;
%ignore aiMaterial::GetTexture;
%ignore aiMaterial::RemoveProperty;
%ignore aiMaterial::mNumAllocated;
%ignore aiMaterial::mNumProperties;
%ignore aiMaterial::mProperties;
MATERIAL(0,         aiString,         Name,                  AI_MATKEY_NAME);
MATERIAL(0,         aiColor4D,        ColorDiffuse,          AI_MATKEY_COLOR_DIFFUSE);
MATERIAL(0,         aiColor4D,        ColorSpecular,         AI_MATKEY_COLOR_SPECULAR);
MATERIAL(0,         aiColor4D,        ColorAmbient,          AI_MATKEY_COLOR_AMBIENT);
MATERIAL(0,         aiColor4D,        ColorEmissive,         AI_MATKEY_COLOR_EMISSIVE);
MATERIAL(0,         aiColor4D,        ColorTransparent,      AI_MATKEY_COLOR_TRANSPARENT);
MATERIAL(0,         aiColor4D,        ColorReflective,       AI_MATKEY_COLOR_REFLECTIVE);        // TODO: Check if TYPE is correct !
MATERIAL(0,         float,            Opacity,               AI_MATKEY_OPACITY);
MATERIAL(0,         float,            Shininess,             AI_MATKEY_SHININESS);
MATERIAL(0,         float,            ShininessStrength,     AI_MATKEY_SHININESS_STRENGTH);
MATERIAL(0,         float,            Refraction,            AI_MATKEY_REFRACTI);
MATERIAL(0,         float,            Reflectivity,          AI_MATKEY_REFLECTIVITY);            // TODO: Check if TYPE is correct !
MATERIAL(0,         int,              TwoSided,              AI_MATKEY_TWOSIDED);
MATERIAL(0,         int,              EnableWireframe,       AI_MATKEY_ENABLE_WIREFRAME);
MATERIAL(0,         int,              BumpScaling,           AI_MATKEY_BUMPSCALING);             // TODO: Check if TYPE is correct !
MATERIAL(1,         aiBlendMode,      BlendFunction,         AI_MATKEY_BLEND_FUNC);
MATERIAL(1,         aiShadingMode,    ShadingModel,          AI_MATKEY_SHADING_MODEL);
MATERIAL(0,         aiString,         GlobalBackgroundImage, AI_MATKEY_GLOBAL_BACKGROUND_IMAGE);
MATERIAL_TEXTURE(0, aiString,         TexturePath,           AI_MATKEY_TEXTURE);
MATERIAL_TEXTURE(0, int,              UVWSource,             AI_MATKEY_UVWSRC);
MATERIAL_TEXTURE(1, aiTextureMapping, Mapping,               AI_MATKEY_MAPPING);
MATERIAL_TEXTURE(1, aiTextureFlags,   TextureFlags,          AI_MATKEY_TEXFLAGS);
MATERIAL_TEXTURE(1, aiTextureMapMode, MappingModeU,          AI_MATKEY_MAPPINGMODE_U);
MATERIAL_TEXTURE(1, aiTextureMapMode, MappingModeV,          AI_MATKEY_MAPPINGMODE_V);
MATERIAL_TEXTURE(1, aiTextureOp,      TextureOperation,      AI_MATKEY_TEXOP);
MATERIAL_TEXTURE(0, aiVector3D,       TextureMappingAxis,    AI_MATKEY_TEXMAP_AXIS);
MATERIAL_TEXTURE(0, float,            TextureBlend,          AI_MATKEY_TEXBLEND);
MATERIAL_TEXTURE(0, aiUVTransform,    UVTransform,           AI_MATKEY_UVTRANSFORM);             // TODO: Check if TYPE is correct !

/////// aiMatrix3x3 
// cf template section
ADD_UNMANAGED_OPTION_BASE(aiMatrix3x3, aiMatrix3x3t<float>);
%ignore aiMatrix3x3t<float>::operator[];

/////// aiMatrix4x4 
// cf template section
ADD_UNMANAGED_OPTION_BASE(aiMatrix4x4, aiMatrix4x4t<float>);
%ignore aiMatrix4x4t<float>::operator[];

/////// aiMesh 
DEF_ENUM(aiPrimitiveType, mPrimitiveTypes);
ADD_UNMANAGED_OPTION(aiMesh);
%ignore aiMesh::mNumVertices;
%ignore aiMesh::mVertices;
%ignore aiMesh::mNormals;
%ignore aiMesh::mTangents;
%ignore aiMesh::mBitangents;
%ignore aiMesh::mColors;
%ignore aiMesh::mTextureCoords;
%ignore aiMesh::mNumUVComponents;
%ignore aiMesh::mNumFaces;
%ignore aiMesh::mFaces;
%ignore aiMesh::mNumBones;
%ignore aiMesh::mBones;
%ignore aiMesh::mNumAnimMeshes;
%ignore aiMesh::mAnimMeshes;

/////// aiMeshAnim 
%ignore aiMeshAnim::mNumKeys;
%ignore aiMeshAnim::mKeys;

/////// aiMeshKey 
// Done

/////// aiMetadata
%ignore aiMetadata::mNumProperties;
%ignore aiMetadata::mKeys;
%ignore aiMetadata::mValues;

/////// aiNode 
ADD_UNMANAGED_OPTION(aiNode);
%ignore aiNode::mNumChildren;
%ignore aiNode::mChildren;
%ignore aiNode::mNumMeshes;
%ignore aiNode::mMeshes;
%ignore aiNode::FindNode(aiString const &);
%ignore aiNode::FindNode(char const *);

/////// aiNodeAnim 
%ignore aiNodeAnim::mNumPositionKeys;
%ignore aiNodeAnim::mPositionKeys;
%ignore aiNodeAnim::mNumRotationKeys;
%ignore aiNodeAnim::mRotationKeys;
%ignore aiNodeAnim::mNumScalingKeys;
%ignore aiNodeAnim::mScalingKeys;

/////// aiPlane 
// Done

/////// aiPostProcessSteps
ENUM_FLAGS_DECL(aiPostProcessSteps);
%typemap(cscode) aiPostProcessSteps %{, aiProcess_ConvertToLeftHanded = aiProcess_MakeLeftHanded | aiProcess_FlipUVs | aiProcess_FlipWindingOrder,
  aiProcessPreset_TargetRealtime_Fast = aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_SortByPType,
  aiProcessPreset_TargetRealtime_Quality = aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_LimitBoneWeights | aiProcess_RemoveRedundantMaterials | aiProcess_SplitLargeMeshes | aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_SortByPType | aiProcess_FindDegenerates | aiProcess_FindInvalidData,
  aiProcessPreset_TargetRealtime_MaxQuality = aiProcessPreset_TargetRealtime_Quality | aiProcess_FindInstances | aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes,
%}

/////// aiPrimitiveType
ENUM_FLAGS_DECL(aiPrimitiveType);

/////// aiQuaternion 
// cf template section
ADD_UNMANAGED_OPTION_BASE(aiQuaternion, aiQuaterniont<float>);

/////// aiQuatKey 
// Done

/////// aiRay 
// Done

/////// aiScene 
ADD_UNMANAGED_OPTION(aiScene);
%ignore aiScene::mNumAnimations;
%ignore aiScene::mAnimations;
%ignore aiScene::mNumCameras;
%ignore aiScene::mCameras;
%ignore aiScene::mNumLights;
%ignore aiScene::mLights;
%ignore aiScene::mNumMaterials;
%ignore aiScene::mMaterials;
%ignore aiScene::mNumMeshes;
%ignore aiScene::mMeshes;
%ignore aiScene::mNumTextures;
%ignore aiScene::mTextures;
%ignore aiScene::mPrivate;

/////// aiString 
ADD_UNMANAGED_OPTION(aiString);
%ignore aiString::operator=(std::string const &);
%ignore aiString::Set(std::string const &);
%rename(Data) aiString::data;
%rename(Length) aiString::length;
%typemap(cscode) aiString %{
  public override string ToString() {
    return Data;
  }

  // We need to define manually this method because swig seems to not accept multiple 'cscode' typemaps for the same class
  public aiString Unmanaged() {
    this.swigCMemOwn = false;
    return this;
  }
%}

/////// aiTexel 
%ignore aiTexel::operator aiColor4D() const;

/////// aiTexture 
ADD_UNMANAGED_OPTION(aiTexture);
%ignore aiTexture::pcData;
%extend aiTexture {
	void AllocData(unsigned int size) {
		if($self->pcData != NULL) {
			delete[] $self->pcData;
		}
		
		$self->pcData = (aiTexel*) new char[size];
		$self->mHeight = 0;
		$self->mWidth = size;
	}
}
%typemap(cscode) aiTexture %{
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Ansi)]
  private struct Texture {
    public uint mWidth;
    public uint mHeight;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=4)] public string achFormatHint;
    public IntPtr pcData;
  }

  public byte[] data {
    set {
      AllocData((uint) value.Length);
      
      Texture texture = (Texture) Marshal.PtrToStructure(swigCPtr.Handle, typeof(Texture));
      Marshal.Copy(value, 0, texture.pcData, value.Length);
    }
    get {
      byte[] data = null;
	
      if(mHeight == 0) {
        data = new byte[mWidth];

	    Texture texture = (Texture) Marshal.PtrToStructure(swigCPtr.Handle, typeof(Texture));
        Marshal.Copy(texture.pcData, data, 0, (int) texture.mWidth);
      }

      return data;
    }
  }

  // We need to define manually this method because swig seems to not accept multiple 'cscode' typemaps for the same class
  public aiTexture Unmanaged() {
    this.swigCMemOwn = false;
    return this;
  }
%}

/////// aiTextureFlags
ENUM_FLAGS_DECL(aiTextureFlags);

/////// aiUVTransform 
// Done

/////// aiVector2D 
// cf template section
ADD_UNMANAGED_OPTION_BASE(aiVector2D, aiVector2t<float>);
%ignore aiVector2t<float>::operator[](unsigned int);

/////// aiVector3D 
// cf template section
ADD_UNMANAGED_OPTION_BASE(aiVector3D, aiVector3t<float>);
%ignore aiVector3t<float>::operator[](unsigned int);

/////// aiVectorKey 
// Done

/////// aiVertexWeight 
// Done

/////// DefaultLogger
DEF_ENUM(aiDefaultLogStream, defStreams);
DEF_ENUM(Logger.ErrorSeverity, severity);

/////// Exporter
DEF_ENUM(aiPostProcessSteps, pPreprocessing);
%warnfilter(SWIGWARN_PARSE_NAMED_NESTED_CLASS) Assimp::Exporter::ExportFormatEntry;
%ignore Assimp::Exporter::RegisterExporter;
%ignore Assimp::Exporter::ExportToBlob(aiScene const *, std::string const &);
%ignore Assimp::Exporter::Export(aiScene const *, std::string const &, std::string const &);
%ignore Assimp::Exporter::Export(aiScene const *, std::string const &, std::string const &, unsigned int);
%ignore Assimp::Exporter::ExportToBlob(aiScene const *, std::string const &, unsigned int);

/////// Importer
DEF_ENUM(aiPostProcessSteps, pFlags);
%ignore Assimp::Importer::GetExtensionList;
%ignore Assimp::Importer::GetImporter;
%ignore Assimp::Importer::IsExtensionSupported(std::string const &) const;
%ignore Assimp::Importer::Pimpl;
%ignore Assimp::Importer::ReadFile(std::string const &, unsigned int);
%ignore Assimp::Importer::RegisterLoader;
%ignore Assimp::Importer::RegisterPPStep;
%ignore Assimp::Importer::SetPropertyBool(const char*, bool, bool*);
%ignore Assimp::Importer::SetPropertyFloat(const char*, float, bool*);
%ignore Assimp::Importer::SetPropertyInteger(const char*, int, bool*);
%ignore Assimp::Importer::SetPropertyString(const char*, const std::string&, bool*);
%ignore Assimp::Importer::SetPropertyMatrix(const char*, const aiMatrix4x4&, bool*);
%ignore Assimp::Importer::UnregisterLoader;
%ignore Assimp::Importer::UnregisterPPStep;
%extend Assimp::Importer {
  std::string GetExtensions() {
    std::string tmp;
    $self->GetExtensionList(tmp);
    return tmp;
  }
}

/////// IOStream
%ignore Assimp::IOStream::Read;
%ignore Assimp::IOStream::Write;

/////// IOSystem
%ignore Assimp::IOSystem::ComparePaths(std::string const &, std::string const &) const;
%ignore Assimp::IOSystem::Exists(char const *) const;
%ignore Assimp::IOSystem::Open(std::string const &);
%ignore Assimp::IOSystem::Open(std::string const &, std::string const &);

/////// Logger
%ignore Assimp::Logger::debug(std::string const &);
%ignore Assimp::Logger::error(std::string const &);
%ignore Assimp::Logger::info(std::string const &);
%ignore Assimp::Logger::warn(std::string const &);

/////// Globals
%ignore ::aiCopyScene;
%ignore ::aiGetMaterialProperty;
%ignore ::aiGetMaterialFloatArray;
%ignore ::aiGetMaterialFloat;
%ignore ::aiGetMaterialIntegerArray;
%ignore ::aiGetMaterialInteger;
%ignore ::aiGetMaterialColor;
%ignore ::aiGetMaterialString;
%ignore ::aiGetMaterialTextureCount;
%ignore ::aiGetMaterialTexture;

%include "..\..\include\assimp\defs.h"
%include "..\..\include\assimp\config.h"
%include "..\..\include\assimp\version.h"
%include "..\..\include\assimp\ai_assert.h"
%include "..\..\include\assimp\types.h"
%include "..\..\include\assimp\color4.h"
%include "..\..\include\assimp\vector2.h"
%include "..\..\include\assimp\vector3.h"
%include "..\..\include\assimp\quaternion.h"
%include "..\..\include\assimp\matrix3x3.h"
%include "..\..\include\assimp\matrix4x4.h"
%include "..\..\include\assimp\scene.h"
%include "..\..\include\assimp\mesh.h"
%include "..\..\include\assimp\anim.h"
%include "..\..\include\assimp\material.h"
%include "..\..\include\assimp\texture.h"
%include "..\..\include\assimp\camera.h"
%include "..\..\include\assimp\light.h"
%include "..\..\include\assimp\postprocess.h"
%include "..\..\include\assimp\metadata.h"
%include "..\..\include\assimp\Importer.hpp"
%include "..\..\include\assimp\cimport.h"
%include "..\..\include\assimp\importerdesc.h"
%include "..\..\include\assimp\Exporter.hpp"
%include "..\..\include\assimp\cexport.h"
%include "..\..\include\assimp\IOStream.hpp"
%include "..\..\include\assimp\IOSystem.hpp"
%include "..\..\include\assimp\cfileio.h"
%include "..\..\include\assimp\Logger.hpp"
%include "..\..\include\assimp\DefaultLogger.hpp"
%include "..\..\include\assimp\NullLogger.hpp"
%include "..\..\include\assimp\LogStream.hpp"
%include "..\..\include\assimp\ProgressHandler.hpp"

%template(aiColor4D) aiColor4t<float>;
%template(aiVector2D) aiVector2t<float>;
%template(aiVector3D) aiVector3t<float>;
%template(aiQuaternion) aiQuaterniont<float>;
%template(aiMatrix3x3) aiMatrix3x3t<float>;
%template(aiMatrix4x4) aiMatrix4x4t<float>;

ARRAY_DECL(aiUInt, unsigned int);

ARRAY_DECL(aiFace, aiFace);
ARRAY_DECL(aiString, aiString);
ARRAY_DECL(aiMeshKey, aiMeshKey);
ARRAY_DECL(aiQuatKey, aiQuatKey);
ARRAY_DECL(aiVectorKey, aiVectorKey);
ARRAY_DECL(aiVertexWeight, aiVertexWeight);

ARRAY_DECL(aiAnimation, aiAnimation*);
ARRAY_DECL(aiAnimMesh, aiAnimMesh*);
ARRAY_DECL(aiBone, aiBone*);
ARRAY_DECL(aiCamera, aiCamera*);
ARRAY_DECL(aiLight, aiLight*);
ARRAY_DECL(aiMaterial, aiMaterial*);
ARRAY_DECL(aiMesh, aiMesh*);
ARRAY_DECL(aiMeshAnim, aiMeshAnim*);
ARRAY_DECL(aiNode, aiNode*);
ARRAY_DECL(aiNodeAnim, aiNodeAnim*);
ARRAY_DECL(aiTexture, aiTexture*);

FIXED_ARRAY_DECL(aiUInt, unsigned int);

MULTI_ARRAY_DECL(aiVector3D, aiVector3D);
MULTI_ARRAY_DECL(aiColor4D, aiColor4D);
