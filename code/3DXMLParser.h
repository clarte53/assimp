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

#include <functional>

namespace Assimp {

	namespace Q3BSP {

		class Q3BSPZipArchive;

	} // end of namespace Q3BSP

	class _3DXMLParser {

	protected:

		class XMLReader {

			public:

				XMLReader(Q3BSP::Q3BSPZipArchive* pArchive, const std::string& pFile);

				virtual ~XMLReader();

				void Open(const std::string& pFile);

				void Close();

				/** Go to the next element */
				bool Next();

				/** Test if the current xml element is an element */
				bool IsElement() const;

				/** Compares the current xml element name to the given string and returns true if equal */
				bool IsElement(const std::string& pName) const;
				
				/** Return the name of a node */
				std::string GetNodeName() const;

				/** Return the value of the attribute at a given index. Throw an exception
				    if the attribute is mandatory but not found */
				template <typename T>
				T GetAttribute(int pIndex, bool pMandatory = false) const;

				/** Return the value of the attribute with a given name. Throw an exception
				    if the attribute is mandatory but not found or if the value can not be
						converted into the appropriate type. */
				template <typename T>
				T GetAttribute(const std::string& pName, bool pMandatory = false) const;

				/** Reads the text contents of an element, throws an exception if not given.
					  Skips leading whitespace. */
				std::string GetTextContent();

				/** Aborts the file reading with an exception */
				void ThrowException(const std::string& pError);

			protected:

				/** Filename, for a verbose error message */
				std::string mFileName;

				/** Zip archive containing the data */
				Q3BSP::Q3BSPZipArchive* mArchive;

				/** Stream to the content of a file in the zip archive */
				IOStream* mStream;

				/** XML reader, member for everyday use */
				irr::io::IrrXMLReader* mReader;

		}; // end of class XMLReader

	public:

		/** Constructor from XML file */
		_3DXMLParser(const std::string& pFile);

		virtual ~_3DXMLParser();

	protected:

		/** Aborts the file reading with an exception */
		void ThrowException(const std::string& pError);

		void ReadManifest(XMLReader& pReader);

		void ReadModel(XMLReader& pReader);

		void ReadHeader(XMLReader& pReader);

		void ReadProductStructure(XMLReader& pReader);

		void ReadDefaultView(XMLReader& pReader);

		void ReadCATMaterial(XMLReader& pReader);

		void ReadCATMaterialRef(XMLReader& pReader);

	protected:

		/** Filename, for a verbose error message */
		std::string mFileName;

		/** Name of the root 3dxml file inside the archive */
		std::string mRootFileName;

		/** Mapping between the element names and the parsing functions */
		typedef std::map<std::string, std::function<void(XMLReader& pReader)> > FunctionMapType;
		FunctionMapType mFunctionMap;

	}; // end of class _3DXMLParser

	// ------------------------------------------------------------------------------------------------
	// Go to the next element
	inline bool _3DXMLParser::XMLReader::Next() {
		return mReader->read();
	}

	// ------------------------------------------------------------------------------------------------
	// Test if the current element is an element
	inline bool _3DXMLParser::XMLReader::IsElement() const {
		return mReader->getNodeType() == irr::io::EXN_ELEMENT;
	}

	// ------------------------------------------------------------------------------------------------
	// Check for element match
	inline bool _3DXMLParser::XMLReader::IsElement(const std::string& pName) const {
		return IsElement() && pName.compare(mReader->getNodeName()) == 0;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Return the name of a node
	inline std::string  _3DXMLParser::XMLReader::GetNodeName() const {
		return mReader->getNodeName();
	}

	// ------------------------------------------------------------------------------------------------
	template <typename T>
	T _3DXMLParser::XMLReader::GetAttribute(int pIndex, bool pMandatory) const {
		return GetAttribute<T>(mReader->getAttributeName(pIndex) , pMandatory);
	}

	// ------------------------------------------------------------------------------------------------
	template <typename T>
	T _3DXMLParser::XMLReader::GetAttribute(const std::string& pName, bool pMandatory) const {
		std::string ValueString = mReader->getAttributeValueSafe(pName);

		if(pMandatory && ValueString == "") {
			ThrowException("Attribute \"" + pName + "\" not found.");
		}

		std::istringstream Stream(ValueString);
		T Value;

		Stream >> Value;

		if(Stream.fail()) {
			ThrowException("Attribute \"" + pName + "\" can not be converted into the requested type.");
		}

		return Value;
	}

} // end of namespace Assimp

#endif // AI_3DXMLPARSER_H_INC
