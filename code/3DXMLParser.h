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

#include "irrXMLWrapper.h"

namespace Assimp {

	class _3DXMLParser {

	public:

		/** Constructor from XML file */
		_3DXMLParser(const std::string& pFile);

		virtual ~_3DXMLParser();

	protected:

		/** Aborts the file reading with an exception */
		void ThrowException(const std::string& pError) const;

		/** Compares the current xml element name to the given string and returns true if equal */
		bool IsElement(const char* pName) const;

		/** Reads the text contents of an element, returns NULL if not given.
		    Skips leading whitespace. */
		const char* TestTextContent(std::string* pElementName = NULL);

		/** Reads the text contents of an element, throws an exception if not given. 
		    Skips leading whitespace. */
		const char* GetTextContent();

		void getMainFile(std::string& pFile) throw();

	protected:

		/** Filename, for a verbose error message */
		std::string mFileName;

		/** XML reader, member for everyday use */
		irr::io::IrrXMLReader* mReader;

	}; // end of class _3DXMLParser

	// ------------------------------------------------------------------------------------------------
	// Check for element match
	inline bool _3DXMLParser::IsElement(const char* pName) const {
		ai_assert(mReader->getNodeType() == irr::io::EXN_ELEMENT);

		return std::strcmp(mReader->getNodeName(), pName) == 0; 
	}

} // end of namespace Assimp

#endif // AI_3DXMLPARSER_H_INC
