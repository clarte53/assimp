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
#include "HighResProfiler.h"

#include <cmath>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLMaterial::_3DXMLMaterial(std::shared_ptr<Q3BSP::Q3BSPZipArchive> archive, const std::string& filename, aiMaterial* material) : mReader(archive, filename), mMaterial(material) { PROFILER;
		struct Params {
			_3DXMLMaterial* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Feature element
			map.emplace_back("Feature", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){params.me->ReadFeature();}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);


		params.me = this;

		// Parse the 3DRep file
		while(mReader.Next()) {
			if(mReader.IsElement("Osm")) {
				mReader.ParseElement(mapping, params);
			} else {
				mReader.SkipElement();
			}
		}

		mReader.Close();
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLMaterial::~_3DXMLMaterial() { PROFILER;

	}
	
	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLMaterial::ThrowException(const std::string& error) const { PROFILER;
		throw DeadlyImportError(boost::str(boost::format("3DXML: %s - %s") % mReader.GetFilename() % error));
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLMaterial::ReadFeature() { PROFILER;
		struct Params {
			_3DXMLMaterial* me;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Attr element
			map.emplace_back("Attr", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				// We aggregate all the attributes of each Features into the output material
				params.me->ReadAttribute();
			}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;
		//unsigned int id = *(mReader.GetAttribute<unsigned int>("Id", true));
		std::string start_up = *(mReader.GetAttribute<std::string>("StartUp", true));
		std::string alias = *(mReader.GetAttribute<std::string>("Alias", true));
		Optional<unsigned int> aggregating = mReader.GetAttribute<unsigned int>("Aggregating");

		mReader.ParseElement(mapping, params);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLMaterial::ReadAttribute() { PROFILER;
		/*
		enum Type {EXTERNAL, SPECOBJECT, COMPONENT, STRING, INT, DOUBLE, OCTET, BOOLEAN};

		static const std::map<std::string, Type> types([]() {
			std::map<std::string, Type> map;

			map.emplace("external", EXTERNAL);
			map.emplace("specobject", SPECOBJECT);
			map.emplace("component", COMPONENT);
			map.emplace("string", STRING);
			map.emplace("int", INT);
			map.emplace("double", DOUBLE);
			map.emplace("octet", OCTET);
			map.emplace("boolean", BOOLEAN);

			return std::move(map);
		}());
		*/

		struct Params {
			_3DXMLMaterial* me;
			std::string value;
		} params;

		static const std::map<std::string, std::function<void(Params&)>> attributes([]() {
			std::map<std::string, std::function<void(Params&)>> map;

			//TODO: AmbientCoef

			map.emplace("AmbientColor", [](Params& params) {
				std::vector<float> values = params.me->ReadValues<float>(params.value);

				if(values.size() == 3) {
					aiColor3D color(values[0], values[1], values[2]);

					params.me->mMaterial->AddProperty(&color, 1, AI_MATKEY_COLOR_AMBIENT);
				} else {
					params.me->ThrowException("In attribute AmbientColor: invalid number of color components (" + params.me->mReader.ToString(values.size()) + " instead of 3).");
				}
			});

			//TODO: DiffuseCoef

			map.emplace("DiffuseColor", [](Params& params) {
				std::vector<float> values = params.me->ReadValues<float>(params.value);

				if(values.size() == 3) {
					aiColor3D color(values[0], values[1], values[2]);

					params.me->mMaterial->AddProperty(&color, 1, AI_MATKEY_COLOR_DIFFUSE);
				} else {
					params.me->ThrowException("In attribute DiffuseColor: invalid number of color components (" + params.me->mReader.ToString(values.size()) + " instead of 3).");
				}
			});
			
			map.emplace("SpecularCoef", [](Params& params) {
				float value = params.me->ReadValue<float>(params.value);

				params.me->mMaterial->AddProperty(&value, 1, AI_MATKEY_SHININESS_STRENGTH);
			});
			
			map.emplace("SpecularColor", [](Params& params) {
				std::vector<float> values = params.me->ReadValues<float>(params.value);

				if(values.size() == 3) {
					aiColor3D color(values[0], values[1], values[2]);

					params.me->mMaterial->AddProperty(&color, 1, AI_MATKEY_COLOR_SPECULAR);
				} else {
					params.me->ThrowException("In attribute SpecularColor: invalid number of color components (" + params.me->mReader.ToString(values.size()) + " instead of 3).");
				}
			});

			map.emplace("SpecularExponent", [](Params& params) {
				long double value = params.me->ReadValue<long double>(params.value);

				if(1.0 - value >= 0.0) {
					value = 127.0 * std::pow(1.0 - value, 1.0 / 3.0) + 1.0;
				} else {
					params.me->ThrowException("In attribute SpecularExponent: can not compute the cubic root of negative value \"" + params.me->mReader.ToString(1.0 - value) + "\".");
				}

				float result = (float) value; // Cast to the appropriate type, after all computation are done to minimize the rounding errors

				params.me->mMaterial->AddProperty(&result, 1, AI_MATKEY_SHININESS);

				aiShadingMode shading = aiShadingMode_Blinn;
				params.me->mMaterial->AddProperty((int*) &shading, 1, AI_MATKEY_SHADING_MODEL);
			});

			//TODO: EmissiveCoeff
			
			map.emplace("EmissiveColor", [](Params& params) {
				std::vector<float> values = params.me->ReadValues<float>(params.value);

				if(values.size() == 3) {
					aiColor3D color(values[0], values[1], values[2]);

					params.me->mMaterial->AddProperty(&color, 1, AI_MATKEY_COLOR_EMISSIVE);
				} else {
					params.me->ThrowException("In attribute EmissiveColor: invalid number of color components (" + params.me->mReader.ToString(values.size()) + " instead of 3).");
				}
			});

			//TODO: BlendColor

			map.emplace("Transparency", [](Params& params) {
				float value = params.me->ReadValue<float>(params.value);

				value = 1.0f - value;

				params.me->mMaterial->AddProperty(&value, 1, AI_MATKEY_OPACITY);
			});

			map.emplace("Reflectivity", [](Params& params) {
				float value = params.me->ReadValue<float>(params.value);

				params.me->mMaterial->AddProperty(&value, 1, AI_MATKEY_REFLECTIVITY);
			});

			map.emplace("Refraction", [](Params& params) {
				float value = params.me->ReadValue<float>(params.value);

				params.me->mMaterial->AddProperty(&value, 1, AI_MATKEY_REFRACTI);
			});

			//TODO: MappingType
			//TODO: PreviewType
			//TODO: TranslationU
			//TODO: TranslationV
			//TODO: Rotation
			//TODO: ScaleU
			//TODO: ScaleV
			//TODO: FlipU
			//TODO: FlipV
			//TODO: TextureDimension
			//TODO: TextureFunction
			//TODO: WrappingModeU
			//TODO: WrappingModeV
			//TODO: Filtering
			//TODO: AlphaTest

			map.emplace("TextureImage", [](Params& params) {
				aiString file(params.me->ReadValue<std::string>(params.value));

				params.me->mMaterial->AddProperty(&file, AI_MATKEY_TEXTURE_DIFFUSE(0));
			});

			return std::move(map);
		}());

		std::string name = *(mReader.GetAttribute<std::string>("Name", true));
		std::string type_str = *(mReader.GetAttribute<std::string>("Type", true));
		params.value = *(mReader.GetAttribute<std::string>("Value", true));
		params.me = this;

		/*
		std::map<std::string, Type>::const_iterator it = types.find(type_str);
		Type type;

		if(it != types.end()) {
			type = it->second;
		} else {
			DefaultLogger::get()->warn("Unsupported attribute type \"" + type_str + "\". Using string type instead.");

			type = STRING;
		}
		*/

		auto it = attributes.find(name);

		if(it != attributes.end()) {
			it->second(params);
		} else {
			//DefaultLogger::get()->warn("Unsupported attribute \"" + name + "\".");
		}
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_Q3BSP_IMPORTER
#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
