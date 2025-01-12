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

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER

#include "3DXMLMaterial.h"

#include <assimp/Logger.hpp>
#include "3DXMLParser.h"
#include "HighResProfiler.h"

#include <cmath>

namespace Assimp {

	// ------------------------------------------------------------------------------------------------
	_3DXMLMaterial::_3DXMLMaterial(std::shared_ptr<ZipArchiveIOSystem> archive, const std::string& filename, aiMaterial* material, _3DXMLStructure::Dependencies& dependencies) : mReader(archive, filename), mMaterial(material), mDependencies(dependencies) {
		struct Params {
			_3DXMLMaterial* me;
			GlobalData data;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Feature element
			map.emplace_back("Feature", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){params.me->ReadFeature(&params.data);}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);


		params.me = this;
		params.data.mapping_type = NONE;
		params.data.mapping_operator = aiTextureMapping_UV;
		params.data.has_transform = false;
		params.data.ambient_coef = 1.0f;
		params.data.diffuse_coef = 1.0f;
		params.data.emissive_coef = 0.0f;

		// Parse the 3DRep file
		while(mReader.Next()) {
			if(mReader.IsElement("Osm")) {
				mReader.ParseElement(mapping, params);
			} else {
				mReader.SkipElement();
			}
		}

		mReader.Close();

		SetCoefficient(params.data.ambient_coef, AI_MATKEY_COLOR_AMBIENT);
		SetCoefficient(params.data.diffuse_coef, AI_MATKEY_COLOR_DIFFUSE);
		SetCoefficient(params.data.emissive_coef, AI_MATKEY_COLOR_EMISSIVE);

		switch(params.data.mapping_type) {
		case IMPLICIT_MAPPING:
			params.data.mapping_operator = aiTextureMapping_UV;
			mMaterial->AddProperty((int*) &params.data.mapping_operator, 1, AI_MATKEY_MAPPING_DIFFUSE(0));

			break;
		case OPERATOR_MAPPING:
			if(params.data.mapping_operator != aiTextureMapping_UV) {
				mMaterial->AddProperty((int*) &params.data.mapping_operator, 1, AI_MATKEY_MAPPING_DIFFUSE(0));
			} else {
				_3DXMLParser::LogMessage(Logger::Warn, "In Feature: Operator mapping defined but no operator provided.");

				params.data.mapping_operator = aiTextureMapping_UV;
				mMaterial->AddProperty((int*) &params.data.mapping_operator, 1, AI_MATKEY_MAPPING_DIFFUSE(0));
			}

			break;
		case ENVIRONMENT_MAPPING:
			_3DXMLParser::LogMessage(Logger::Err, "In Feature: Environment mapping not supported. Using UV coordinates instead.");

			params.data.mapping_operator = aiTextureMapping_UV;
			mMaterial->AddProperty((int*) &params.data.mapping_operator, 1, AI_MATKEY_MAPPING_DIFFUSE(0));

			break;
		default:
			break;
		}

		if(params.data.has_transform) {
			mMaterial->AddProperty(&params.data.transform, 1, AI_MATKEY_UVTRANSFORM_DIFFUSE(0));
		}

		// Default material options that have no equivalent in 3DXML
		aiBlendMode blend = aiBlendMode_Default;
		mMaterial->AddProperty((int*) &blend, 1, AI_MATKEY_BLEND_FUNC);

		//TODO: AI_MATKEY_UVWSRC(t, n)
		//TODO: AI_MATKEY_TEXMAP_AXIS(t, n)
	}

	// ------------------------------------------------------------------------------------------------
	_3DXMLMaterial::~_3DXMLMaterial() {

	}
	
	// ------------------------------------------------------------------------------------------------
	// Aborts the file reading with an exception
	void _3DXMLMaterial::ThrowException(const std::string& error) const {
		throw DeadlyImportError("3DXML: " + mReader.GetFilename() + " - " + error);
	}

	// ------------------------------------------------------------------------------------------------
	void _3DXMLMaterial::SetCoefficient(float coef, const char* key, unsigned int type, unsigned int index) {
		aiColor3D color;

		if(mMaterial->Get(key, type, index, color) == aiReturn_SUCCESS) {
			color = color * coef;

			mMaterial->RemoveProperty(key, type, index);
			mMaterial->AddProperty(&color, 1, key, type, index);
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	void _3DXMLMaterial::ReadFeature(GlobalData* data) {
		struct Params {
			_3DXMLMaterial* me;
			std::string value;
			GlobalData* data;
		} params;

		static const XMLParser::XSD::Sequence<Params> mapping(([](){
			XMLParser::XSD::Sequence<Params>::type map;

			// Parse Attr element
			map.emplace_back("Attr", XMLParser::XSD::Element<Params>([](const XMLParser* /*parser*/, Params& params){
				// We aggregate all the attributes of each Features into the output material
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

				static const std::map<std::string, std::function<void(Params&)>> attributes([]() {
					std::map<std::string, std::function<void(Params&)>> map;

					map.emplace("AmbientCoef", [](Params& params) {
						params.data->ambient_coef = params.me->ReadValue<float>(params.value);
					});
					
					map.emplace("AmbientColor", [](Params& params) {
						std::vector<float> values = params.me->ReadValues<float>(params.value);

						if(values.size() == 3) {
							aiColor3D color(values[0], values[1], values[2]);

							params.me->mMaterial->AddProperty(&color, 1, AI_MATKEY_COLOR_AMBIENT);
						} else {
							params.me->ThrowException("In attribute AmbientColor: invalid number of color components (" + params.me->mReader.ToString(values.size()) + " instead of 3).");
						}
					});
					
					map.emplace("DiffuseCoef", [](Params& params) {
						params.data->diffuse_coef = params.me->ReadValue<float>(params.value);
					});

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

					map.emplace("EmissiveCoef", [](Params& params) {
						params.data->emissive_coef = params.me->ReadValue<float>(params.value);
					});
					
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
					
					map.emplace("MappingType", [](Params& params) {
						int value = params.me->ReadValue<int>(params.value);

						switch(value) {
							case 0:
								params.data->mapping_operator = aiTextureMapping_PLANE;
								break;
							case 1:
								params.data->mapping_operator = aiTextureMapping_SPHERE; //TODO: Switch "aiTextureMapping_SPHERE" and "aiTextureMapping_BOX"?
								break;
							case 2:
								params.data->mapping_operator = aiTextureMapping_CYLINDER;
								break;
							default:
							case 3:
								params.data->mapping_operator = aiTextureMapping_BOX;
								break;
							case 4:
								params.data->mapping_operator = aiTextureMapping_OTHER;
								break;
						}
					});

					map.emplace("PreviewType", [](Params& params) {
						int value = params.me->ReadValue<int>(params.value);

						switch(value) {
							case 0:
								params.data->mapping_type = ENVIRONMENT_MAPPING;
								break;
							case 1:
								params.data->mapping_type = IMPLICIT_MAPPING;
								break;
							default:
							case 2:
								params.data->mapping_type = OPERATOR_MAPPING;
								break;
						}
					});

					map.emplace("TranslationU", [](Params& params) {
						params.data->transform.mTranslation.x = params.me->ReadValue<float>(params.value);
						params.data->has_transform = true;
					});

					map.emplace("TranslationV", [](Params& params) {
						params.data->transform.mTranslation.y = params.me->ReadValue<float>(params.value);
						params.data->has_transform = true;
					});

					map.emplace("Rotation", [](Params& params) {
						params.data->transform.mRotation = params.me->ReadValue<float>(params.value);
						params.data->has_transform = true;
					});

					map.emplace("ScaleU", [](Params& params) {
						params.data->transform.mScaling.x = params.me->ReadValue<float>(params.value);
						params.data->has_transform = true;
					});

					map.emplace("ScaleV", [](Params& params) {
						params.data->transform.mScaling.y = params.me->ReadValue<float>(params.value);
						params.data->has_transform = true;
					});

					map.emplace("WrappingModeU", [](Params& params) {
						int value = params.me->ReadValue<int>(params.value);

						aiTextureMapMode mode;

						switch(value) {
							case 0:
								mode = aiTextureMapMode_Clamp;
								break;
							default:
							case 1:
								mode = aiTextureMapMode_Wrap;
								break;
						}

						params.me->mMaterial->AddProperty((int*) &mode, 1, AI_MATKEY_MAPPINGMODE_U_DIFFUSE(0));
					});

					map.emplace("WrappingModeV", [](Params& params) {
						int value = params.me->ReadValue<int>(params.value);

						aiTextureMapMode mode;

						switch(value) {
							case 0:
								mode = aiTextureMapMode_Clamp;
								break;
							default:
							case 1:
								mode = aiTextureMapMode_Wrap;
								break;
						}

						params.me->mMaterial->AddProperty((int*) &mode, 1, AI_MATKEY_MAPPINGMODE_V_DIFFUSE(0));
					});

					map.emplace("AlphaTest", [](Params& params) {
						bool value;

						std::transform(params.value.begin(), params.value.end(), params.value.begin(), ::tolower);

						if(params.value.compare("true") == 0) {
							value = true;
						} else if(params.value.compare("false") == 0) {
							value = false;
						} else {
							value= params.me->ReadValue<bool>(params.value);
						}

						aiTextureFlags flags = (value ? aiTextureFlags_UseAlpha : aiTextureFlags_IgnoreAlpha);

						params.me->mMaterial->AddProperty((int*) &flags, 1, AI_MATKEY_TEXFLAGS_DIFFUSE(0));
					});

					//TODO: FlipU
					//TODO: FlipV
					//TODO: TextureDimension
					//TODO: TextureFunction
					//TODO: Filtering

					map.emplace("TextureImage", [](Params& params) {
						std::string value = params.me->ReadValue<std::string>(params.value);

						_3DXMLStructure::URI uri;

						_3DXMLParser::ParseURI(&params.me->mReader, value, uri);

						if(uri.id) {
							params.me->mDependencies.add(uri.filename);
						}

						aiString file(value);
						params.me->mMaterial->AddProperty(&file, AI_MATKEY_TEXTURE_DIFFUSE(0));

						float blend = 1.0;
						params.me->mMaterial->AddProperty(&blend, 1, AI_MATKEY_TEXBLEND_DIFFUSE(0));

						aiTextureOp tex_op = aiTextureOp_SmoothAdd;
						params.me->mMaterial->AddProperty((int*) &tex_op, 1, AI_MATKEY_TEXOP_DIFFUSE(0));
					});

					map.emplace("ReflectionImage", [](Params& params) {
						std::string value = params.me->ReadValue<std::string>(params.value);

						_3DXMLStructure::URI uri;

						_3DXMLParser::ParseURI(&params.me->mReader, value, uri);

						if(uri.id) {
							params.me->mDependencies.add(uri.filename);
						}

						aiString file(value);
						params.me->mMaterial->AddProperty(&file, AI_MATKEY_TEXTURE_REFLECTION(0));

						float blend = 1.0;
						params.me->mMaterial->AddProperty(&blend, 1, AI_MATKEY_TEXBLEND_REFLECTION(0));

						aiTextureOp tex_op = aiTextureOp_SmoothAdd;
						params.me->mMaterial->AddProperty((int*) &tex_op, 1, AI_MATKEY_TEXOP_REFLECTION(0));
					});
					
					return std::move(map);
				}());

				std::string name = *(params.me->mReader.GetAttribute<std::string>("Name", true));
				std::string type_str = *(params.me->mReader.GetAttribute<std::string>("Type", true));

				params.value = *(params.me->mReader.GetAttribute<std::string>("Value", true));

				/*
				std::map<std::string, Type>::const_iterator it = types.find(type_str);
				Type type;

				if(it != types.end()) {
					type = it->second;
				} else {
					_3DXMLParser::LogMessage(Logger::Warn, "Unsupported attribute type \"" + type_str + "\". Using string type instead.");

					type = STRING;
				}
				*/

				auto it = attributes.find(name);

				if(it != attributes.end()) {
					it->second(params);
				} else {
					//_3DXMLParser::LogMessage(Logger::Warn, "Unsupported attribute \"" + name + "\".");
				}
			}, 0, XMLParser::XSD::unbounded));
			
			return std::move(map);
		})(), 1, 1);

		params.me = this;
		params.data = data;

		mReader.GetAttribute<unsigned int>("Id", true);
		mReader.GetAttribute<std::string>("StartUp", true);
		mReader.GetAttribute<std::string>("Alias", true);
		//Optional<unsigned int> aggregating = mReader.GetAttribute<unsigned int>("Aggregating");

		mReader.ParseElement(mapping, params);
	}

} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
