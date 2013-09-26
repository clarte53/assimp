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
$typemap(cstype, unsigned int);

%include "std_string.i"

%{
#include "..\..\include\assimp\defs.h"
#include "..\..\include\assimp\config.h"
#include "..\..\include\assimp\types.h"
#include "..\..\include\assimp\version.h"
#include "..\..\include\assimp\postprocess.h"
#include "..\..\include\assimp\vector2.h"
#include "..\..\include\assimp\vector3.h"
#include "..\..\include\assimp\color4.h"
#include "..\..\include\assimp\matrix3x3.h"
#include "..\..\include\assimp\matrix4x4.h"
#include "..\..\include\assimp\camera.h"
#include "..\..\include\assimp\light.h"
#include "..\..\include\assimp\anim.h"
#include "..\..\include\assimp\mesh.h"
#include "..\..\include\assimp\metadata.h"
#include "..\..\include\assimp\cfileio.h"
#include "..\..\include\assimp\material.h"
#include "..\..\include\assimp\quaternion.h"
#include "..\..\include\assimp\scene.h"
#include "..\..\include\assimp\texture.h"
#include "..\..\include\assimp\Importer.hpp"
#include "..\..\include\assimp\Exporter.hpp"
#include "..\..\include\assimp\cexport.h"
#include "..\..\include\assimp\IOSystem.hpp"
#include "..\..\include\assimp\IOStream.hpp"
#include "..\..\include\assimp\Logger.hpp"
#include "..\..\include\assimp\LogStream.hpp"
#include "..\..\include\assimp\NullLogger.hpp"
#include "..\..\include\assimp\ProgressHandler.hpp"
#include "..\..\include\assimp\Array.hpp"
%}

#define C_STRUCT
#define C_ENUM
#define ASSIMP_API
#define PACK_STRUCT
#define AI_FORCE_INLINE

%rename(__add__) operator+;
%rename(__addnset__) operator+=;
%rename(__sub__) operator-;
%rename(__subnset__) operator-=;
%rename(__mul__) operator*;
%rename(__mulnset__) operator*=;
%rename(__div__) operator/;
%rename(__divnset__) operator/=;
%rename(__equal__) operator==;
%rename(__nequal__) operator!=;
%rename(__idx__) operator[];
%rename(__set__) operator=;
%rename(__greater__) operator>;
%rename(__smaller__) operator<;

%rename(opNew) operator new;
%rename(opNewArray) operator new[];

%rename(opDelete) operator delete;
%rename(opDeleteArray) operator delete[];


%include "std_vector.i"

// Helper macros for wrapping the pointer-and-length arrays used in the
// Assimp API.

%define ASSIMP_ARRAY(CLASS, TYPE, NAME, LENGTH)
%csmethodmodifiers Get##NAME() "private";
%newobject CLASS::Get##NAME;
%extend CLASS {
  std::vector<TYPE > *Get##NAME() {
    std::vector<TYPE > *result = new std::vector<TYPE >;
    result->reserve(LENGTH);

    for (unsigned int i = 0; i < LENGTH; ++i) {
      result->push_back($self->NAME[i]);
    }

    return result;
  }
}
%ignore CLASS::NAME;
%enddef

%define ASSIMP_POINTER_ARRAY(CLASS, TYPE, NAME, LENGTH)
%csmethodmodifiers Get##NAME() "private";
%newobject CLASS::Get##NAME;
%extend CLASS {
  std::vector<TYPE *> *Get##NAME() {
    std::vector<TYPE *> *result = new std::vector<TYPE *>;
    result->reserve(LENGTH);
	 	
    TYPE *currentValue = (TYPE *)$self->NAME;
    TYPE *valueLimit = (TYPE *)$self->NAME + LENGTH;
    while (currentValue < valueLimit) {
      result->push_back(currentValue);
      ++currentValue;
    }
 	
    return result;
  }
}
%ignore CLASS::NAME;
%enddef 

%define ASSIMP_POINTER_POINTER(CLASS, TYPE, NAME, LENGTH)
%csmethodmodifiers Get##NAME() "private";
%newobject CLASS::Get##NAME;
%extend CLASS {
  std::vector<TYPE *> *Get##NAME() {
    std::vector<TYPE *> *result = new std::vector<TYPE *>;
    result->reserve(LENGTH);

    TYPE **currentValue = $self->NAME;
    TYPE **valueLimit = $self->NAME + LENGTH;
    while (currentValue < valueLimit) {
      result->push_back(*currentValue);
      ++currentValue;
    }

    return result;
  }
}
%ignore CLASS::NAME;
%enddef

%define ASSIMP_POINTER_ARRAY_ARRAY(CLASS, TYPE, NAME, OUTER_LENGTH, INNER_LENGTH)
%csmethodmodifiers Get##NAME() "private";
%newobject CLASS::Get##NAME;
%extend CLASS {
  std::vector< std::vector<TYPE*> > *Get##NAME() {
    std::vector< std::vector<TYPE*> > *result = new std::vector< std::vector<TYPE*> >;
    result->reserve(OUTER_LENGTH);

    for (unsigned int i = 0; i < OUTER_LENGTH; ++i) {
      std::vector<TYPE *> currentElements;

      if ($self->NAME[i] != 0) {
        currentElements.reserve(INNER_LENGTH);

        TYPE *currentValue = $self->NAME[i];
        TYPE *valueLimit = $self->NAME[i] + INNER_LENGTH;
        while (currentValue < valueLimit) {
          currentElements.push_back(currentValue);
          ++currentValue;
        }
      }

      result->push_back(currentElements);
    }

    return result;
  }
}
%ignore CLASS::NAME;
%enddef

%define ASSIMP_GETMATERIAL(XXX, KEY, TYPE, NAME)
%csmethodmodifiers Get##NAME() "private";
%newobject aiMaterial::Get##NAME;
%extend aiMaterial {
  bool Get##NAME(TYPE* INOUT) {
         return aiGetMaterial##XXX($self, KEY, INOUT) == AI_SUCCESS;
  }
}
%enddef


/////// aiAnimation 
ASSIMP_POINTER_POINTER(aiAnimation,aiNodeAnim,mChannels,$self->mNumChannels);
ASSIMP_POINTER_POINTER(aiAnimation,aiMeshAnim,mMeshChannels,$self->mNumMeshChannels);
%typemap(cscode) aiAnimation %{
  public aiNodeAnimVector mChannels { get { return GetmChannels(); } }
  public aiMeshAnimVector mMeshChannels { get { return GetmMeshChannels(); } }
%}

/////// aiAnimMesh 
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
// OK

/////// aiColor4D 
// OK

/////// aiFace 
%ignore aiFace::mNumIndices;
%ignore aiFace::mIndices;

/////// TODO: aiFile 
%ignore aiFile;
%ignore aiFile::FileSizeProc;
%ignore aiFile::FlushProc;
%ignore aiFile::ReadProc;
%ignore aiFile::SeekProc;
%ignore aiFile::TellProc;
%ignore aiFile::WriteProc;

/////// TODO: aiFileIO 
%ignore aiFileIO;
%ignore aiFileIO::CloseProc;
%ignore aiFileIO::OpenPrc;

/////// aiLight 
// Done

/////// aiLightSourceType 
// Done

/////// TODO: aiLogStream 
%ignore aiLogStream;
%ignore aiLogStream::callback;

/////// aiMaterial 
%ignore aiMaterial::Get;
%ignore aiMaterial::GetTexture;
%ignore aiMaterial::mNumAllocated;
%ignore aiMaterial::mNumProperties;
%ignore aiMaterial::mProperties;
ASSIMP_GETMATERIAL(Color,   AI_MATKEY_COLOR_DIFFUSE,        aiColor4D,  Diffuse);
ASSIMP_GETMATERIAL(Color,   AI_MATKEY_COLOR_SPECULAR,       aiColor4D,  Specular);
ASSIMP_GETMATERIAL(Color,   AI_MATKEY_COLOR_AMBIENT,        aiColor4D,  Ambient);
ASSIMP_GETMATERIAL(Color,   AI_MATKEY_COLOR_EMISSIVE,       aiColor4D,  Emissive);
ASSIMP_GETMATERIAL(Float,   AI_MATKEY_OPACITY,              float,      Opacity);
ASSIMP_GETMATERIAL(Float,   AI_MATKEY_SHININESS_STRENGTH,   float,      ShininessStrength);
ASSIMP_GETMATERIAL(Integer, AI_MATKEY_SHADING_MODEL,        int,        ShadingModel);
ASSIMP_GETMATERIAL(Integer, AI_MATKEY_TEXFLAGS_DIFFUSE(0),  int,        TexFlagsDiffuse0);
ASSIMP_GETMATERIAL(Integer, AI_MATKEY_MAPPINGMODE_U_DIFFUSE(0),int,     MappingModeUDiffuse0);
ASSIMP_GETMATERIAL(Integer, AI_MATKEY_MAPPINGMODE_V_DIFFUSE(0),int,     MappingModeVDiffuse0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_DIFFUSE(0),   aiString,   TextureDiffuse0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_SPECULAR(0),  aiString,   TextureSpecular0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_OPACITY(0),   aiString,   TextureOpacity0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_AMBIENT(0),   aiString,   TextureAmbient0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_EMISSIVE(0),  aiString,   TextureEmissive0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_SHININESS(0), aiString,   TextureShininess0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_LIGHTMAP(0),  aiString,   TextureLightmap0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_NORMALS(0),   aiString,   TextureNormals0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_TEXTURE_HEIGHT(0),    aiString,   TextureHeight0);
ASSIMP_GETMATERIAL(String,  AI_MATKEY_GLOBAL_BACKGROUND_IMAGE, aiString, GlobalBackgroundImage);
ASSIMP_GETMATERIAL(Integer, AI_MATKEY_TWOSIDED,             int,   TwoSided);
%typemap(cscode) aiMaterial %{
    public aiColor4D Diffuse { get { var v = new aiColor4D(); return GetDiffuse(v)?v:DefaultDiffuse; } }
    public aiColor4D Specular { get { var v = new aiColor4D(); return GetSpecular(v)?v:DefaultSpecular; } }
    public aiColor4D Ambient { get { var v = new aiColor4D(); return GetAmbient(v)?v:DefaultAmbient; } }
    public aiColor4D Emissive { get { var v = new aiColor4D(); return GetEmissive(v)?v:DefaultEmissive; } }
    public float Opacity { get { float v = 0; return GetOpacity(ref v)?v:DefaultOpacity; } }
    public float ShininessStrength { get { float v = 0; return GetShininessStrength(ref v)?v:DefaultShininessStrength; } }    
    public aiShadingMode ShadingModel { get { int v = 0; return GetShadingModel(ref v)?((aiShadingMode)v):DefaultShadingModel; } }
    public aiTextureFlags TexFlagsDiffuse0 { get { int v = 0; return GetTexFlagsDiffuse0(ref v)?((aiTextureFlags)v):DefaultTexFlagsDiffuse0; } }
    public aiTextureMapMode MappingModeUDiffuse0 { get { int v = 0; return GetMappingModeUDiffuse0(ref v)?((aiTextureMapMode)v):DefaultMappingModeUDiffuse0; } }
    public aiTextureMapMode MappingModeVDiffuse0 { get { int v = 0; return GetMappingModeVDiffuse0(ref v)?((aiTextureMapMode)v):DefaultMappingModeVDiffuse0; } }
    public string TextureDiffuse0 { get { var v = new aiString(); return GetTextureDiffuse0(v)?v.ToString():DefaultTextureDiffuse; } }
    public bool TwoSided { get { int v = 0; return GetTwoSided(ref v)?(v!=0):DefaultTwoSided; } }
    
    // These values are returned if the value material property isn't set
    // Override these if you don't want to check for null
    public static aiColor4D DefaultDiffuse = new aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    public static aiColor4D DefaultSpecular = new aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    public static aiColor4D DefaultAmbient = new aiColor4D(0.0f, 0.0f, 0.0f, 1.0f);
    public static aiColor4D DefaultEmissive = new aiColor4D(0.0f, 0.0f, 0.0f, 1.0f);
    public static float DefaultShininessStrength = 1.0f;
    public static float DefaultOpacity = 1.0f;
    public static aiShadingMode DefaultShadingModel = (aiShadingMode)0;
    public static aiTextureFlags DefaultTexFlagsDiffuse0 = (aiTextureFlags)0;
    public static aiTextureMapMode DefaultMappingModeUDiffuse0 = aiTextureMapMode.aiTextureMapMode_Wrap;
    public static aiTextureMapMode DefaultMappingModeVDiffuse0 = aiTextureMapMode.aiTextureMapMode_Wrap;
    public static string DefaultTextureDiffuse = null;
    public static bool DefaultTwoSided = false;
%}

/////// aiMatrix3x3 
%ignore aiMatrix3x3::operator!=;
%ignore aiMatrix3x3::operator*;
%ignore aiMatrix3x3::operator*=;
%ignore aiMatrix3x3::operator==;
%ignore aiMatrix3x3::operator[];

/////// aiMatrix4x4 
%ignore aiMatrix4x4::operator!=;
%ignore aiMatrix4x4::operator*;
%ignore aiMatrix4x4::operator*=;
%ignore aiMatrix4x4::operator==;
%ignore aiMatrix4x4::operator[];

/////// aiMesh 
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
%typemap(cstype)   unsigned int mPrimitiveTypes "aiPrimitiveType";
%typemap(csin)     unsigned int mPrimitiveTypes "(uint)$csinput";
%typemap(csvarout) unsigned int mPrimitiveTypes %{ get { return (aiPrimitiveType)$imcall; } %}

/////// aiMeshAnim 
ASSIMP_POINTER_ARRAY(aiMeshAnim,aiMeshKey,mKeys,$self->mNumKeys);
%typemap(cscode) aiMeshAnim %{
  public aiMeshKeyVector mKeys { get { return GetmKeys(); } }
%}

/////// aiMeshKey 
// Done

/////// aiNode 
%ignore aiNode::mNumChildren;
%ignore aiNode::mChildren;
%ignore aiNode::mNumMeshes;
%ignore aiNode::mMeshes;

/////// aiNodeAnim 
ASSIMP_POINTER_ARRAY(aiNodeAnim,aiVectorKey,mPositionKeys,$self->mNumPositionKeys);
ASSIMP_POINTER_ARRAY(aiNodeAnim,aiQuatKey,mRotationKeys,$self->mNumRotationKeys);
ASSIMP_POINTER_ARRAY(aiNodeAnim,aiVectorKey,mScalingKeys,$self->mNumScalingKeys);
%typemap(cscode) aiNodeAnim %{
  public aiVectorKeyVector mPositionKeys { get { return GetmPositionKeys(); } }
  public aiQuatKeyVector mRotationKeys { get { return GetmRotationKeys(); } }
  public aiVectorKeyVector mScalingKeys { get { return GetmScalingKeys(); } }
%}

/////// aiPlane 
// Done

/////// aiPostProcessSteps
%typemap(cscode) aiPostProcessSteps %{
	, aiProcess_ConvertToLeftHanded = aiProcess_MakeLeftHanded|aiProcess_FlipUVs|aiProcess_FlipWindingOrder,
%}

/////// aiQuaternion 
// Done

/////// aiQuatKey 
// Done

/////// aiRay 
// Done

/////// aiScene 
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

/////// aiString 
%ignore aiString::Append;
%ignore aiString::Clear;
%ignore aiString::Set;
%rename(Data) aiString::data;
%rename(Length) aiString::length;
%typemap(cscode) aiString %{
  public override string ToString() { return Data; } 
%}


/////// aiTexel 
// Done

/////// TODO: aiTexture 
%ignore aiString::achFormatHint;
%ignore aiString::pcData;

/////// aiUVTransform 
// Done

/////// aiVector2D 
// Done

/////// aiVector3D 
// Done

/////// aiVectorKey 
// Done

/////// aiVertexWeight 
// Done

/////// Assimp::*
%ignore Assimp::IOStream;
%ignore Assimp::IOSystem;
%ignore Assimp::Importer::ApplyPostProcessing;
%ignore Assimp::Importer::FindLoader;
%ignore Assimp::Importer::GetIOHandler;
%ignore Assimp::Importer::GetExtensionList(std::string&);
%ignore Assimp::Importer::GetExtensionList(aiString&);
%ignore Assimp::Importer::ReadFileFromMemory;
%ignore Assimp::Importer::RegisterLoader;
%ignore Assimp::Importer::RegisterPPStep;
%ignore Assimp::Importer::SetIOHandler;
%ignore Assimp::Importer::UnregisterLoader;
%ignore Assimp::Importer::UnregisterPPStep;
%extend Assimp::Importer {
  std::string GetExtensionList() {
    std::string tmp;
    $self->GetExtensionList(tmp);
    return tmp;
  }
}
%typemap(cstype)   unsigned int pFlags "aiPostProcessSteps";
%typemap(csin)     unsigned int pFlags "(uint)$csinput"
%typemap(csvarout) unsigned int pFlags %{ get { return (aiPostProcessSteps)$imcall; } %}
%ignore Assimp::Logger;
%ignore Assimp::NullLogger;

/////// Globals
%ignore ::aiImportFileEx;
%ignore ::aiImportFileEx;
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
%include "..\..\include\assimp\types.h"
%include "..\..\include\assimp\version.h"
%include "..\..\include\assimp\postprocess.h"
%include "..\..\include\assimp\vector2.h"
%include "..\..\include\assimp\vector3.h"
%include "..\..\include\assimp\color4.h"
%include "..\..\include\assimp\matrix3x3.h"
%include "..\..\include\assimp\matrix4x4.h"
%include "..\..\include\assimp\camera.h"
%include "..\..\include\assimp\light.h"
%include "..\..\include\assimp\anim.h"
%include "..\..\include\assimp\mesh.h"
%include "..\..\include\assimp\metadata.h"
%include "..\..\include\assimp\cfileio.h"
%include "..\..\include\assimp\material.h"
%include "..\..\include\assimp\quaternion.h"
%include "..\..\include\assimp\scene.h"
%include "..\..\include\assimp\texture.h"
%include "..\..\include\assimp\Importer.hpp"
%include "..\..\include\assimp\Exporter.hpp"
%include "..\..\include\assimp\cexport.h"
%include "..\..\include\assimp\ProgressHandler.hpp"
%include "..\..\include\assimp\Array.hpp"
//%include "..\..\include\IOSystem.h"
//%include "..\..\include\IOStream.h"
//%include "..\..\include\Logger.h"
//%include "..\..\include\LogStream.h"
//%include "..\..\include\NullLogger.h"


%template(aiColor4D) aiColor4t<float>;

%template(aiVector3D) aiVector3t<float>;
%template(aiVector2D) aiVector2t<float>;

%template(aiQuaternion) aiQuaterniont<float>;
%template(aiMatrix3x3) aiMatrix3x3t<float>;
%template(aiMatrix4x4) aiMatrix4x4t<float>;

%extend ArrayInterface {
%typemap(cscode) ArrayInterface %{
	public interface MultiArray<T> {
		uint Size();
		T Get(uint index);
	}
	
	public interface FixedArray<T> : MultiArray<T> {
		void Set(uint index, T value);
	}
	
	public interface Array<T> : FixedArray<T> {
		void Clear();
		void Reserve(uint size);
		bool Add(T value);
	}
%}
}

%define ARRAY_DECL(NAME, CTYPE)
%typemap(csinterfaces) Array<CTYPE> "IDisposable, ArrayInterface.Array<$typemap(cstype, CTYPE)>"
%ignore Array<CTYPE>::Array;
%template(NAME##Array) Array<CTYPE>;
%enddef

%define FIXED_ARRAY_DECL(NAME, CTYPE)
%typemap(csinterfaces) Array<CTYPE> "IDisposable, ArrayInterface.FixedArray<$typemap(cstype, CTYPE)>"
%ignore MultiArray<CTYPE>::FixedArray;
%template(NAME##FixedArray) FixedArray<CTYPE>;
%enddef

%define MULTI_ARRAY_DECL(NAME, CTYPE)
%typemap(csinterfaces) Array<CTYPE> "IDisposable, ArrayInterface.MultiArray<$typemap(cstype, CTYPE)>"
%ignore MultiArray<CTYPE>::MultiArray;
%ignore MultiArray<CTYPE>::Set;
ARRAY_DECL(NAME, CTYPE);
%template(NAME##MultiArray) MultiArray<CTYPE>;
%enddef

ARRAY_DECL(aiUInt, unsigned int);
ARRAY_DECL(aiFace, aiFace);
ARRAY_DECL(aiVertexWeighth, aiVertexWeight);
ARRAY_DECL(aiNode, aiNode*);
ARRAY_DECL(aiMesh, aiMesh*);
ARRAY_DECL(aiMaterial, aiMaterial*);
ARRAY_DECL(aiAnimation, aiAnimation*);
ARRAY_DECL(aiTexture, aiTexture*);
ARRAY_DECL(aiLight, aiLight*);
ARRAY_DECL(aiCamera, aiCamera*);
ARRAY_DECL(aiBone, aiBone*);
ARRAY_DECL(aiAnimMesh, aiAnimMesh*);

FIXED_ARRAY_DECL(aiUInt, unsigned int);

MULTI_ARRAY_DECL(aiVector3D, aiVector3D);
MULTI_ARRAY_DECL(aiColor4D, aiColor4D);

%template(FloatVector) std::vector<float>;
%template(aiMeshKeyVector) std::vector<aiMeshKey *>;
%template(aiNodeAnimVector) std::vector<aiNodeAnim *>;
%template(aiMeshAnimVector) std::vector<aiMeshAnim *>;
%template(aiQuatKeyVector) std::vector<aiQuatKey *>;
%template(aiVectorKeyVector) std::vector<aiVectorKey *>;
