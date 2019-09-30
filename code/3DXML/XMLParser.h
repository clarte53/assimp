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

#include <assimp/irrXMLWrapper.h>
#include <assimp/ParsingUtils.h>
#include "HighResProfiler.h"
#include "Optional.h"

#include <functional>
#include <memory>
#include <sstream>

namespace Assimp {

	class ZipArchiveIOSystem;

	class XMLParser {

		public:

			struct XSD {

				static const unsigned int unbounded;

				template<typename T>
				class Element {

					public:

						typedef std::function<void(const XMLParser*, T&)> parser;

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
				
				template<typename T, typename U>
				class Container : public Element<T> {

					public:

						typedef U type;

					protected:
					
						type mMap;

					public:

						Container(typename Container<T, U>::type&& map, unsigned int min = 1, unsigned int max = 1);
						
						const typename Container<T, U>::type& GetMap() const;

				}; // class Container

				template<typename T>
				class Choice : public Container<T, std::map<std::string, Element<T>>> {
					
					public:

						typedef typename Container<T, std::map<std::string, Element<T>>>::type type;

						Choice(typename Choice<T>::type&& map, unsigned int min = 1, unsigned int max = 1);

				}; // class Choice

				template<typename T>
				class Sequence : public Container<T, std::list<std::pair<std::string, Element<T>>>> {

					public:

						typedef typename Container<T, std::list<std::pair<std::string, Element<T>>>>::type type;

						Sequence(typename Sequence<T>::type&& map, unsigned int min = 1, unsigned int max = 1);

				}; // class Sequence
				
			}; // struct XSD
			
		protected:

			/** Filename, for a verbose error message */
			std::string mFileName;

			/** Zip archive containing the data */
			std::shared_ptr<ZipArchiveIOSystem> mArchive;

			/** Stream to the content of a file in the zip archive */
			IOStream* mStream;

			/** XML reader, member for everyday use */
			irr::io::IrrXMLReader* mReader;

			void OpenInArchive(const std::string& file);

		public:

			XMLParser(std::shared_ptr<ZipArchiveIOSystem> archive, const std::string& file);

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

			template<typename T>
			void ParseElement(const XSD::Element<T>& schema, T& params) const;

			template<typename T>
			void ParseElement(const XSD::Choice<T>& schema, T& params) const;

			template<typename T>
			void ParseElement(const XSD::Sequence<T>& schema, T& params) const;

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

			template<typename T>
			T FromString(std::istringstream& stream) const;

	}; // end of class XMLParser
	
	template<>
	std::string XMLParser::ToString(const std::string& value) const;
			
	template<>
	std::string XMLParser::FromString(const std::string& string) const;

	template<>
	std::string XMLParser::FromString(std::istringstream& stream) const;

	template<>
	float XMLParser::FromString(const std::string& string) const;

	template<>
	float XMLParser::FromString(std::istringstream& stream) const;

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
	template<typename T, typename U>
	XMLParser::XSD::Container<T, U>::Container(typename XMLParser::XSD::Container<T, U>::type&& map, unsigned int min, unsigned int max) : Element<T>([this](const XMLParser* parser, T& params){parser->ParseElement(*this, params);}, min, max), mMap(map) {
	
	}
						
	// ------------------------------------------------------------------------------------------------
	template<typename T, typename U>
	inline const typename XMLParser::XSD::Container<T, U>::type& XMLParser::XSD::Container<T, U>::GetMap() const {
		return mMap;
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Choice<T>::Choice(typename XMLParser::XSD::Choice<T>::type&& map, unsigned int min, unsigned int max) : Container<T, std::map<std::string, Element<T>>>(std::move(map), min, max) {
	
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	XMLParser::XSD::Sequence<T>::Sequence(typename XMLParser::XSD::Sequence<T>::type&& map, unsigned int min, unsigned int max) : Container<T, std::list<std::pair<std::string, Element<T>>>>(std::move(map), min, max) {
	
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
	template<typename T>
	void XMLParser::ParseElement(const XSD::Element<T>& schema, T& params) const {
		(schema.GetParser())(this, params);
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	void XMLParser::ParseElement(const XSD::Choice<T>& schema, T& params) const {
		irr::io::EXML_NODE node_type;
		std::string node_name;

		// Test if it's not an <element />
		if(! mReader->isEmptyElement()) {
			// Save the current node name for checking closing of elements
			std::string name = mReader->getNodeName();

			// Get a reference to the map
			const typename XSD::Choice<T>::type& map = schema.GetMap();

			// Save the count for each type of element
			std::map<std::string, unsigned int> check;
			unsigned int total_check = 0;

			// Initialize all the counters to 0
			for(typename XSD::Choice<T>::type::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
				check[it->first] = 0;
			}

			while(mReader->read()) {
				node_type = mReader->getNodeType();
				node_name = mReader->getNodeName();

				// Test if we have an opening element
				if(node_type == irr::io::EXN_ELEMENT) {
					// Get the position of the current element
					typename XSD::Choice<T>::type::const_iterator it = map.find(node_name);

					// Is the element mapped?
					if(it != map.end()) {
						ParseElement(it->second, params);

						SkipUntilEnd(it->first);

						// Increment the counter for this type of element
						(check[it->first])++;
						total_check++;
					} else {
						// Ignore elements that are not mapped
						SkipElement();

						//DefaultLogger::get()->warn("Skipping parsing of element \"" + node_name + "\".");
					}
				} else if(node_type == irr::io::EXN_ELEMENT_END) {
					if(name.compare(node_name) == 0) {
						// Check if the minOccurs & maxOccurs conditions where satisfied for the choice
						if(total_check < schema.GetMin()) {
							ThrowException("The element \"" + name + "\" does not contain enough sub elements (" + ToString(total_check) + " elements instead of min. " + ToString(schema.GetMin()) + ") to validate the schema.");
						} else if(total_check > schema.GetMax()) {
							ThrowException("The element \"" + name + "\" contains too many sub elements (" + ToString(total_check) + " elements instead of max. " + ToString(schema.GetMax()) + ") to validate the schema.");
						}

						// Check if the minOccurs & maxOccurs conditions where satisfied for each sub elements
						for(typename XSD::Choice<T>::type::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
							unsigned int occurs = check[it->first];

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
	template<typename T>
	void XMLParser::ParseElement(const XSD::Sequence<T>& schema, T& params) const {
		irr::io::EXML_NODE node_type;
		std::string node_name;

		// Test if it's not an <element />
		if(! mReader->isEmptyElement()) {
			// Save the current node name for checking closing of elements
			std::string name = mReader->getNodeName();
			
			// Get a reference to the map
			const typename XSD::Sequence<T>::type& map = schema.GetMap();

			// Save the count for each type of element
			std::map<std::string, unsigned int> check;
			unsigned int total_check = 1;

			// Initialize all the counters to 0
			for(typename XSD::Sequence<T>::type::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
				check[it->first] = 0;
			}

			typename XSD::Sequence<T>::type::const_iterator position = map.begin();

			while(mReader->read()) {
				node_type = mReader->getNodeType();
				node_name = mReader->getNodeName();

				// Test if we have an opening element
				if(node_type == irr::io::EXN_ELEMENT) {
					// Get the position of the current element
					typename XSD::Sequence<T>::type::const_iterator it = position;
					bool looped = false;
					bool found = false;

					// Move to the position of the current element in the sequence
					do {
						if(it->first.compare(node_name) == 0) {
							position = it;
							found = true;
						} else {
							++it;

							if(it == map.end()) {
								it = map.begin();

								looped = true;
							}
						}
					} while(! found && it != position);

					// Is the element mapped?
					if(found) {
						ParseElement(position->second, params);

						SkipUntilEnd(position->first);

						// Increment the counter for this type of element
						(check[position->first])++;
						if(looped) {
							// Update the global counter for this sequence
							total_check++;

							// Check if the minOccurs & maxOccurs conditions where satisfied for each sub elements for this iteration of the sequence
							for(typename XSD::Sequence<T>::type::const_iterator it(map.begin()), end(map.end()); it != end; ++it) {
								unsigned int occurs = check[it->first];

								if(occurs < it->second.GetMin()) {
									ThrowException("The element \"" + it->first + "\" is not present enough times (" + ToString(occurs) + " times instead of min. " + ToString(it->second.GetMin()) + ") in element \"" + name + "\" to validate the schema.");
								} else if(occurs > it->second.GetMax()) {
									ThrowException("The element \"" + it->first + "\" is present too many times (" + ToString(occurs) + " times instead of max. " + ToString(it->second.GetMax()) + ") in element \"" + name + "\" to validate the schema.");
								}

								// Reset the counters for each element
								check[it->first] = 0;
							}
						}

						// Save the new position in the map
						position = it;
					} else {
						// Ignore elements that are not mapped
						SkipElement();
						
						//DefaultLogger::get()->warn("Skipping parsing of element \"" + node_name + "\".");
					}
				} else if(node_type == irr::io::EXN_ELEMENT_END) {
					if(name.compare(node_name) == 0) {
						// Check if the minOccurs & maxOccurs conditions where satisfied for the sequence
						if(total_check < schema.GetMin()) {
							ThrowException("The sequence \"" + name + "\" is not repeated enough times (" + ToString(total_check) + " times instead of min. " + ToString(schema.GetMin()) + ") to validate the schema.");
						} else if(total_check > schema.GetMax()) {
							ThrowException("The sequence \"" + name + "\" is repeated too many times (" + ToString(total_check) + " times instead of max. " + ToString(schema.GetMax()) + ") to validate the schema.");
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
			if(mandatory) {
				ThrowException("The content of the element \"" + std::string(mReader->getNodeName()) + "\" is not composed of text.");
			} else {
				return Optional<T>();
			}
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

		// We're using C++11, therefore the copied value of the stream will be automatically moved by the compiler
		return stream.str();
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	T XMLParser::FromString(const std::string& string) const {
		std::istringstream stream(string);
		
		return FromString<T>(stream);
	}

	// ------------------------------------------------------------------------------------------------
	template<typename T>
	T XMLParser::FromString(std::istringstream& stream) const {
		T value;

		stream >> value;

		if(stream.fail()) {
			ThrowException("The value \"" + stream.str() + "\" can not be converted into \"" + std::string(typeid(T).name()) + "\".");
		}

		// We're using C++11, therefore the value will be automatically moved by the compiler
		return value;
	}

} // end of namespace Assimp

#endif // AI_XMLPARSER_H_INC
