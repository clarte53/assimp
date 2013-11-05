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

/** @file XMLParser.h
 *  @brief Defines XML parser helper class
 */

#ifndef AI_XMLPARSER_H_INC
#define AI_XMLPARSER_H_INC

#include "irrXMLWrapper.h"
#include "Optional.h"
#include "ParsingUtils.h"

#include <functional>

namespace Assimp {

	namespace Q3BSP {

		class Q3BSPZipArchive;

	} // end of namespace Q3BSP

	class XMLParser {

		public:

			struct XSD {

				static const unsigned int unbounded;

				template<typename T>
				class Element {

					public:

						typedef std::function<void(T&)> parser;

					protected:

						parser mParser;

						unsigned int mMinOccurs;

						unsigned int mMaxOccurs;

					public:

						Element(const parser& p, unsigned int min = 1, unsigned int max = 1);

						Element(parser&& p, unsigned int min = 1, unsigned int max = 1);

						const typename Element<T>::parser& GetParser() const;

						unsigned int GetMin() const;

						unsigned int GetMax() const;

				}; // class Element
				
				template<typename T>
				class Container {

					public:

						typedef T type;

					protected:
					
						type mMap;

						unsigned int mMinOccurs;

						unsigned int mMaxOccurs;

					public:

						Container(typename Container<T>::type&& map, unsigned int min = 1, unsigned int max = 1);
						
						const typename Container<T>::type& GetMap() const;

						unsigned int GetMin() const;

						unsigned int GetMax() const;

						virtual typename Container<T>::type::const_iterator find(const typename Container<T>::type& map, typename Container<T>::type::const_iterator position, const std::string& name) const = 0;

				}; // class Container

				template<typename T>
				class Choice : public Container<std::map<std::string, Element<T>>> {
					
					public:

						typedef typename Container<std::map<std::string, Element<T>>>::type type;

						Choice(typename Choice<T>::type&& map, unsigned int min = 1, unsigned int max = 1);

						virtual typename Choice<T>::type::const_iterator find(const typename Choice<T>::type& map, typename Choice<T>::type::const_iterator position, const std::string& name) const;

				}; // class Choice

				template<typename T>
				class Sequence : public Container<std::list<std::pair<std::string, Element<T>>>> {

					public:

						typedef typename Container<std::list<std::pair<std::string, Element<T>>>>::type type;

						Sequence(typename Sequence<T>::type&& map, unsigned int min = 1, unsigned int max = 1);

						virtual typename Sequence<T>::type::const_iterator find(const typename Sequence<T>::type& map, typename Sequence<T>::type::const_iterator position, const std::string& name) const;

				}; // class Sequence
				
			}; // struct XSD
			
		protected:

			/** Filename, for a verbose error message */
			std::string mFileName;

			/** Zip archive containing the data */
			std::shared_ptr<Q3BSP::Q3BSPZipArchive> mArchive;

			/** Stream to the content of a file in the zip archive */
			IOStream* mStream;

			/** XML reader, member for everyday use */
			irr::io::IrrXMLReader* mReader;

		public:

			XMLParser(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& file);

			virtual ~XMLParser();

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

			template<typename T, typename U>
			void ParseElements(const XSD::Container<T>* schema, U& params) const;

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

			template<typename T>
			T FromString(const std::string& string) const;

	}; // end of class XMLParser
	
	template<>
	std::string XMLParser::ToString(const std::string& value) const;
			
	template<>
	std::string XMLParser::FromString(const std::string& string) const;

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Element<T>::Element(const typename XMLParser::XSD::Element<T>::parser& parser, unsigned int min, unsigned int max) : mParser(parser), mMinOccurs(min), mMaxOccurs(max) {
	
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Element<T>::Element(typename XMLParser::XSD::Element<T>::parser&& parser, unsigned int min, unsigned int max) : mParser(parser), mMinOccurs(min), mMaxOccurs(max) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	inline const typename XMLParser::XSD::Element<T>::parser& XMLParser::XSD::Element<T>::GetParser() const {
		return mParser;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	inline unsigned int XMLParser::XSD::Element<T>::GetMin() const {
		return mMinOccurs;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	inline unsigned int XMLParser::XSD::Element<T>::GetMax() const {
		return mMaxOccurs;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Container<T>::Container(typename XMLParser::XSD::Container<T>::type&& map, unsigned int min, unsigned int max) : mMap(map), mMinOccurs(min), mMaxOccurs(max) {
	
	}
						
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	inline const typename XMLParser::XSD::Container<T>::type& XMLParser::XSD::Container<T>::GetMap() const {
		return mMap;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	inline unsigned int XMLParser::XSD::Container<T>::GetMin() const {
		return mMinOccurs;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	inline unsigned int XMLParser::XSD::Container<T>::GetMax() const {
		return mMaxOccurs;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Choice<T>::Choice(typename XMLParser::XSD::Choice<T>::type&& map, unsigned int min, unsigned int max) : Container<std::map<std::string, Element<T>>>(std::move(map), min, max) {
	
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	typename XMLParser::XSD::Choice<T>::type::const_iterator XMLParser::XSD::Choice<T>::find(const typename XMLParser::XSD::Choice<T>::type& map, typename XMLParser::XSD::Choice<T>::type::const_iterator /*position*/, const std::string& name) const {
		return map.find(name);
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Sequence<T>::Sequence(typename XMLParser::XSD::Sequence<T>::type&& map, unsigned int min, unsigned int max) : Container<std::list<std::pair<std::string, Element<T>>>>(std::move(map), min, max) {
	
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	typename XMLParser::XSD::Sequence<T>::type::const_iterator XMLParser::XSD::Sequence<T>::find(const typename XMLParser::XSD::Sequence<T>::type& map, typename XMLParser::XSD::Sequence<T>::type::const_iterator position, const std::string& name) const {
		// Move to the position of the current element in the sequence
		while(position != map.end() && position->first.compare(name) != 0) {
			++position;
		}

		return position;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Return the name of the file currently parsed
	inline const std::string& XMLParser::GetFilename() const {
		return mFileName;
	}

	// ------------------------------------------------------------------------------------------------
	// Go to the next element
	inline bool XMLParser::Next() const {
		return mReader->read();
	}

	// ------------------------------------------------------------------------------------------------
	// Test if the element has sub elements
	inline bool XMLParser::HasElements() const {
		return ! mReader->isEmptyElement();
	}

	// ------------------------------------------------------------------------------------------------
	// Check for element match
	inline bool XMLParser::IsElement(const std::string& name) const {
		return mReader->getNodeType() == irr::io::EXN_ELEMENT && name.compare(mReader->getNodeName()) == 0;
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T, typename U>
	void XMLParser::ParseElements(const XSD::Container<T>* schema, U& params) const {
		irr::io::EXML_NODE node_type;
		std::string node_name;

		// Test if it's not an <element />
		if(! mReader->isEmptyElement()) {
			// Save the current node name for checking closing of elements
			std::string name = mReader->getNodeName();
			
			// Get a reference to the map
			const typename XSD::Container<T>::type& map = schema->GetMap();

			// Save the count for each type of element
			std::map<std::string, unsigned int> check;

			// Initialize all the counters to 0
			for(typename XSD::Container<T>::type::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
				check[it->first] = 0;
			}

			typename XSD::Container<T>::type::const_iterator position = map.begin();

			while(mReader->read()) {
				node_type = mReader->getNodeType();
				node_name = mReader->getNodeName();

				// Test if we have an opening element
				if(node_type == irr::io::EXN_ELEMENT) {
					// Get the position of the current element
					typename XSD::Container<T>::type::const_iterator it = schema->find(map, position, node_name);
					
					// Is the element mapped?
					if(it != map.end()) {
						(it->second.GetParser())(params);

						SkipUntilEnd(it->first);

						// Increment the counter for this type of element
						(check[it->first])++;

						// Save the new position in the map
						position = it;
					} else {
						// Ignore elements that are not mapped
						SkipElement();
						
						DefaultLogger::get()->warn("Skipping parsing of element \"" + node_name + "\".");
					}
				} else if(node_type == irr::io::EXN_ELEMENT_END) {
					if(name.compare(node_name) == 0) {
						// check if the minOccurs & maxOccurs conditions where satisfied
						for(typename XSD::Container<T>::type::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
							unsigned int occurs = check[it->first];

							//TODO: FIXME
							if(occurs < it->second.GetMin()) {
								ThrowException("The element \"" + it->first + "\" is not present enough times (" + ToString(occurs) + " times instead of min. " + ToString(it->second.GetMin()) + ") in element \"" + name + "\" to validate the schema.");
							} else if(occurs > it->second.GetMax()) {
								ThrowException("The element \"" + it->first + "\" is present too many times (" + ToString(occurs) + " times instead of max. " + ToString(it->second.GetMax()) + ") in element \"" + name + "\" to validate the schema.");
							}
						}

						// Ok, we can stop the loop
						break;
					} else {
						ThrowException("Expected end of \"" + name + "\" element.");
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Return the name of a node
	inline std::string  XMLParser::GetNodeName() const {
		return mReader->getNodeName();
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	Optional<T> XMLParser::GetAttribute(int pIndex, bool mandatory) const {
		return GetAttribute<T>(mReader->getAttributeName(pIndex) , mandatory);
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	Optional<T> XMLParser::GetAttribute(const std::string& name, bool mandatory) const {
		std::string value = mReader->getAttributeValueSafe(name.c_str());

		if(value == "") {
			if(mandatory) {
				ThrowException("Attribute \"" + name + "\" not found.");
			} else {
				return Optional<T>();
			}
		}

		return Optional<T>(FromString<T>(value));
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	Optional<T> XMLParser::GetContent(bool mandatory) const {
		// present node should be the beginning of an element
		if(mReader->getNodeType() != irr::io::EXN_ELEMENT) {
			ThrowException("The current node is not an xml element.");
		}

		// present node should not be empty
		if(mReader->isEmptyElement()) {
			if(mandatory) {
				ThrowException("Can not get content of the empty element \"" + std::string(mReader->getNodeName()) + "\".");
			} else {
				return Optional<T>();
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
				return Optional<T>();
			}
		}

		return Optional<T>(FromString<T>(text));
	}
	
	// ------------------------------------------------------------------------------------------------
	template<typename T>
	std::string XMLParser::ToString(const T& value) const {
		std::stringstream stream;

		stream << value;

		if(stream.fail()) {
			ThrowException("The type \"" + std::string(typeid(T).name()) + "\" can not be converted into string.");
		}

		return stream.str();
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	T XMLParser::FromString(const std::string& string) const {
		std::istringstream stream(string);
		T value;

		stream >> value;

		if(stream.fail()) {
			ThrowException("The value \"" + string + "\" can not be converted into \"" + std::string(typeid(T).name()) + "\".");
		}

		return value;
	}

} // end of namespace Assimp

#endif // AI_XMLPARSER_H_INC
