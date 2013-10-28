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
#include "irrXMLWrapper.h"

#include <boost/noncopyable.hpp>
#include <functional>
#include <memory>

namespace Assimp {

	namespace Q3BSP {

		class Q3BSPZipArchive;

	} // end of namespace Q3BSP

	class _3DXMLParser {

		public:

			class XMLReader {

				public:

					template<typename T>
					class Optional {

						protected:

							bool mDefined;

							T mValue;

						public:

							Optional() : mDefined(false), mValue() {}

							Optional(const Optional& other) : mDefined(other.mDefined), mValue(other.mValue) {}

							Optional(const T& value) : mDefined(true), mValue(value) {}

							inline operator bool() const {return mDefined;}

							inline const T* operator->() const {return &mValue;}

							inline T* operator->() {return &mValue;}

							inline const T& operator*() const {return mValue;}

							inline T& operator*() {return mValue;}

					}; // class Optional<T>

					template<typename T>
					class Optional<T*> : public boost::noncopyable {

						protected:

							bool mDefined;

							T* mValue;

						public:

							Optional() : mDefined(false), mValue(NULL) {}

							Optional(const T* value) : mDefined(true), mValue(value) {}

							virtual ~Optional() {if(mDefined && mValue != NULL) {delete mValue;}}

							inline operator bool() const {return mDefined;}

							inline const T* operator->() const {return mValue;}

							inline T* operator->() {return mValue;}

							inline const T& operator*() const {return *mValue;}

							inline T& operator*() {return *mValue;}

					}; // class Optional<T*>

					template<typename T>
					struct ParsingMap {
						typedef std::map<std::string, std::function<void(T&)>> type;
					}; // struct ParsingMap
			
				protected:

					/** Filename, for a verbose error message */
					std::string mFileName;

					/** Zip archive containing the data */
					std::shared_ptr<Q3BSP::Q3BSPZipArchive> mArchive;

					/** Stream to the content of a file in the zip archive */
					ScopeGuard<IOStream> mStream;

					/** XML reader, member for everyday use */
					ScopeGuard<irr::io::IrrXMLReader> mReader;

				public:

					XMLReader(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& file);

					virtual ~XMLReader();

					void Open(const std::string& file);

					void Close();
					
					/** Aborts the file reading with an exception */
					void ThrowException(const std::string& error) const;
					
					const std::string& GetFilename() const;

					/** Go to the next element */
					bool Next() const;

					/** Test if this element has sub elements */
					bool HasElements() const;

					/** Compares the current xml element name to the given string and returns true if equal */
					bool IsElement(const std::string& name) const;

					void SkipElement() const;

					void SkipUntilEnd(const std::string& name) const;

					template<typename T>
					void ParseNode(const typename ParsingMap<T>::type& map, T& params) const;

					/** Return the name of a node */
					std::string GetNodeName() const;

					/** Return the value of the attribute at a given index. Throw an exception
						if the attribute is mandatory but not found */
					template<typename T>
					Optional<T> GetAttribute(int pIndex, bool mandatory = false) const;

					/** Return the value of the attribute with a given name. Throw an exception
						if the attribute is mandatory but not found or if the value can not be
							converted into the appropriate type. */
					template<typename T>
					Optional<T> GetAttribute(const std::string& name, bool mandatory = false) const;

					/** Reads the text contents of an element, throws an exception if not given.
						  Skips leading whitespace. */
					template<typename T>
					Optional<T> GetContent(bool mandatory = false) const;

					template<typename T>
					std::string ToString(const T& value) const;

					template<>
					std::string ToString(const std::string& value) const;

					template<typename T>
					T FromString(const std::string& string) const;

					template<>
					std::string FromString(const std::string& string) const;

			}; // end of class XMLReader
		
		protected:

			/** The archive containing the 3DXML files */ 
			std::shared_ptr<Q3BSP::Q3BSPZipArchive> mArchive;

			/** Current xml reader */
			ScopeGuard<XMLReader> mReader;

			/** Content of the 3DXML file */
			_3DXMLStructure mContent;

		public:

			/** Constructor from XML file */
			_3DXMLParser(const std::string& file, aiScene* scene);

			virtual ~_3DXMLParser();

		protected:
			
			/** Aborts the file reading with an exception */
			void ThrowException(const std::string& error) const;

			void ParseFile();

			void ParseURI(const std::string& uri, _3DXMLStructure::URI& result) const;

			static void ParseExtension(const std::string& filename, std::string& extension);

			static void ParseID(const std::string& data, unsigned int& id);

			void BuildStructure(_3DXMLStructure::Reference3D& ref, aiNode* node) const;

			void ReadManifest(std::string& main_file);

			void ReadModel_3dxml();

			void ReadHeader();

			void ReadProductStructure();

			void ReadCATMaterialRef();

			void ReadCATMaterial();
			
			void ReadReference3D();

			void ReadInstance3D();

			void ReadReferenceRep();

			void ReadInstanceRep();

			void Read3DRep(const std::string& filename, std::list<aiMesh*>& meshes);

	}; // end of class _3DXMLParser

	// ------------------------------------------------------------------------------------------------
	// Return the name of the file currently parsed
	inline const std::string& _3DXMLParser::XMLReader::GetFilename() const {
		return mFileName;
	}

	// ------------------------------------------------------------------------------------------------
	// Go to the next element
	inline bool _3DXMLParser::XMLReader::Next() const {
		return mReader->read();
	}

	// ------------------------------------------------------------------------------------------------
	// Test if the element has sub elements
	inline bool _3DXMLParser::XMLReader::HasElements() const {
		return ! mReader->isEmptyElement();
	}

	// ------------------------------------------------------------------------------------------------
	// Check for element match
	inline bool _3DXMLParser::XMLReader::IsElement(const std::string& name) const {
		return mReader->getNodeType() == irr::io::EXN_ELEMENT && name.compare(mReader->getNodeName()) == 0;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Parse a node
	template<typename T>
	void _3DXMLParser::XMLReader::ParseNode(const typename ParsingMap<T>::type& map, T& params) const {
		irr::io::EXML_NODE node_type;
		std::string node_name;

		// Test if it's not an <element />
		if(! mReader->isEmptyElement()) {
			// Save the current node name for checking closing of elements
			std::string name = mReader->getNodeName();

			while(mReader->read()) {
				node_type = mReader->getNodeType();
				node_name = mReader->getNodeName();

				// Test if we have an opening element
				if(node_type == irr::io::EXN_ELEMENT) {
					typename ParsingMap<T>::type::const_iterator it = map.find(node_name);

					// Is the element mapped?
					if(it != map.end()) {
						(it->second)(params);

						SkipUntilEnd(it->first);
					} else {
						// Ignore elements that are not mapped
						SkipElement();
					}
				} else if(node_type == irr::io::EXN_ELEMENT_END) {
					if(name.compare(node_name) == 0) {
						// Ok, we can stop the loop
						break;
					} else {
						ThrowException("Expected end of <" + name + "> element.");
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Return the name of a node
	inline std::string  _3DXMLParser::XMLReader::GetNodeName() const {
		return mReader->getNodeName();
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	_3DXMLParser::XMLReader::Optional<T> _3DXMLParser::XMLReader::GetAttribute(int pIndex, bool mandatory) const {
		return GetAttribute<T>(mReader->getAttributeName(pIndex) , mandatory);
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	_3DXMLParser::XMLReader::Optional<T> _3DXMLParser::XMLReader::GetAttribute(const std::string& name, bool mandatory) const {
		std::string value = mReader->getAttributeValueSafe(name.c_str());

		if(value == "") {
			if(mandatory) {
				ThrowException("Attribute \"" + name + "\" not found.");
			} else {
				return _3DXMLParser::XMLReader::Optional<T>();
			}
		}

		return _3DXMLParser::XMLReader::Optional<T>(FromString<T>(value));
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	_3DXMLParser::XMLReader::Optional<T> _3DXMLParser::XMLReader::GetContent(bool mandatory) const {
		// present node should be the beginning of an element
		if(mReader->getNodeType() != irr::io::EXN_ELEMENT) {
			ThrowException("The current node is not an xml element.");
		}

		// present node should not be empty
		if(mReader->isEmptyElement()) {
			if(mandatory) {
				ThrowException("Can not get content of the empty element \"" + std::string(mReader->getNodeName()) + "\".");
			} else {
				return _3DXMLParser::XMLReader::Optional<T>();
			}
		}

		// read contents of the element
		if(! mReader->read() || mReader->getNodeType() != irr::io::EXN_TEXT) {
			ThrowException("The content of the element \"" + std::string(mReader->getNodeName()) + "\" is not composed of text.");
		}

		// skip leading whitespace
		const char* text = mReader->getNodeData();
		SkipSpacesAndLineEnd(&text);

		if(text == NULL) {
			if(mandatory) {
				ThrowException("Invalid content in element \"" + std::string(mReader->getNodeName()) + "\".");
			} else {
				return _3DXMLParser::XMLReader::Optional<T>();
			}
		}

		return _3DXMLParser::XMLReader::Optional<T>(FromString<T>(text));
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	std::string _3DXMLParser::XMLReader::ToString(const T& value) const {
		std::stringstream stream;

		stream << value;

		if(stream.fail()) {
			ThrowException("The type \"" + std::string(typeid(T).name()) + "\" can not be converted into string.");
		}

		return stream.str();
	}

	template<>
	std::string _3DXMLParser::XMLReader::ToString(const std::string& value) const {
		return value;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	T _3DXMLParser::XMLReader::FromString(const std::string& string) const {
		std::istringstream stream(string);
		T value;

		stream >> value;

		if(stream.fail()) {
			ThrowException("The value \"" + string + "\" can not be converted into \"" + std::string(typeid(T).name()) + "\".");
		}

		return value;
	}

	template<>
	std::string _3DXMLParser::XMLReader::FromString(const std::string& string) const {
		return string;
	}

} // end of namespace Assimp

#endif // AI_3DXMLPARSER_H_INC
