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

/** @file 3DXMLImporter.cpp
 *  @brief Implementation of the 3DXML loader
 */

#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER
#ifndef ASSIMP_BUILD_NO_Q3BSP_IMPORTER

#include "3DXMLImporter.h"
#include "3DXMLParser.h"

static const aiImporterDesc desc = {
	"3DXML Importer",
	"Leo Terziman",
	"",
	"http://3ds.com/3dxml",
	aiImporterFlags_SupportCompressedFlavour | aiImporterFlags_LimitedSupport | aiImporterFlags_Experimental,
	0,
	0,
	0,
	0,
	"3dxml"
};

namespace Assimp {

	_3DXMLImporter::_3DXMLImporter() : mUseNodeMaterials(false) {

	}

	_3DXMLImporter::~_3DXMLImporter() {

	}

	// ------------------------------------------------------------------------------------------------
	//	Returns true, if the loader can read this.
	bool _3DXMLImporter::CanRead(const std::string& pFile, IOSystem* /*pIOHandler*/, bool checkSig) const {
		bool canRead = false;
		
		if(! checkSig) {
			canRead = SimpleExtensionCheck(pFile, "3dxml");
		}

		return canRead;
	}

	// ------------------------------------------------------------------------------------------------
	//	Update importer configuration
	void _3DXMLImporter::SetupProperties(const Importer* pImp)
	{
		mUseNodeMaterials = pImp->GetPropertyInteger(AI_CONFIG_IMPORT_3DXML_USE_NODE_MATERIALS, 0) != 0;
	}

	// ------------------------------------------------------------------------------------------------
	//	Adds extensions.
	const aiImporterDesc* _3DXMLImporter::GetInfo () const {
		return &desc;
	}

	// ------------------------------------------------------------------------------------------------
	//	Import method.
	void _3DXMLImporter::InternReadFile(const std::string& pFile, aiScene* pScene, IOSystem* pIOHandler) {
		_3DXMLParser fileParser(pIOHandler, pFile, pScene, mUseNodeMaterials);
	}

} // end of namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
