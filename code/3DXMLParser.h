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

/** @file 3DXMLParser.h
 *  @brief Defines the parser helper class for the 3DXML loader
 */

#ifndef AI_3DXMLPARSER_H_INC
#define AI_3DXMLPARSER_H_INC

#include "3DXMLStructure.h"
#include "XMLParser.h"

namespace Assimp {

	class _3DXMLParser {

		protected:

			/** The archive containing the 3DXML files */ 
			std::shared_ptr<Q3BSP::Q3BSPZipArchive> mArchive;

			/** Content of the 3DXML file */
			_3DXMLStructure mContent;

		public:

			/** Constructor from XML file */
			_3DXMLParser(const std::string& file, aiScene* scene);

			virtual ~_3DXMLParser();
			
			static void ParseURI(const XMLParser* parser, const std::string& uri, _3DXMLStructure::URI& result);

			static void ParseExtension(const std::string& filename, std::string& extension);

			static void ParseID(const std::string& data, unsigned int& id);

		protected:
			
			/** Aborts the file reading with an exception */
			static void ThrowException(const XMLParser* parser, const std::string& error);

			void BuildMaterials(const XMLParser* parser);

			void BuildMeshes(const XMLParser* parser, _3DXMLStructure::ReferenceRep& rep, unsigned int material_index);

			void BuildRoot(const XMLParser* parser, const std::string& main_file);

			void BuildStructure(const XMLParser* parser, _3DXMLStructure::Reference3D& ref, aiNode* node, Optional<unsigned int> material_index);

			void ReadManifest(const XMLParser* parser, std::string& main_file);

			void ReadFile(const XMLParser* parser);

			void ReadHeader(const XMLParser* parser);

			void ReadProductStructure(const XMLParser* parser);

			void ReadReference3D(const XMLParser* parser);

			void ReadInstance3D(const XMLParser* parser);

			void ReadReferenceRep(const XMLParser* parser);

			void ReadInstanceRep(const XMLParser* parser);

			void ReadCATMaterialRef(const XMLParser* parser);

			void ReadCATMatReference(const XMLParser* parser);

			void ReadMaterialDomain(const XMLParser* parser);

			void ReadMaterialDomainInstance(const XMLParser* parser);
			
			void ReadCATMaterial(const XMLParser* parser);

			void ReadCATMatConnection(const XMLParser* parser);

	}; // end of class _3DXMLParser

} // end of namespace Assimp

#endif // AI_3DXMLPARSER_H_INC
