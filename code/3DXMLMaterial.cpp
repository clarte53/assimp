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

/** @file 3DXMLMaterial.cpp
 *  @brief Implementation of the 3DXML material parser helper
 */

#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER
#ifndef ASSIMP_BUILD_NO_Q3BSP_IMPORTER

#include "3DXMLMaterial.h"

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLMaterial::_3DXMLMaterial(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& filename, aiMaterial* material) : mReader(archive, filename), mMaterial(material) {
		struct Params {
			_3DXMLMaterial* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Feature element
			map.emplace_back("Feature", XMLParser::XSD::Element<Params>([](Params& params){

			}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);


		params.me = this;

		// Parse the main 3DXML file
		while(mReader.Next()) {
			if(mReader.IsElement("Osm")) {
				mReader.ParseElements(&mapping, params);
			} else {
				mReader.SkipElement();
			}
		}

		mReader.Close();
	}

	_3DXMLMaterial::~_3DXMLMaterial() {

	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
