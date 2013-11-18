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

/** @file HighResProfiler.cpp
 *  @brief Implementation of an high resolution profiler
 */

#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_3DXML_IMPORTER

#include "HighResProfiler.h"

#include <fstream>

namespace Assimp {

	namespace Profiling {

		HighResProfiler::HighResProfiler() {
		
		}

		HighResProfiler::~HighResProfiler() {
			save("Profiler.csv");
		}

		HighResProfiler& HighResProfiler::get() {
			static HighResProfiler instance;

			return instance;
		}

		void HighResProfiler::add(const std::string& function, const std::chrono::microseconds& duration) {
			auto it = mStats.find(function);

			if(it == mStats.end()) {
				auto insert = mStats.emplace(function, std::list<std::chrono::microseconds>());

				if(insert.second) {
					it = insert.first;
				} else {
					throw std::runtime_error("Impossible to add the element " + function + " to the map of profiling statistics.");
				}
			}

			it->second.emplace_back(duration);
		}

		void HighResProfiler::save(const std::string& filename) {
			if(! mStats.empty()) {
				std::ofstream file(filename);

				if(file.is_open()) {
					file << "Location;Average time (seconds);Total time (microseconds);Min time (microseconds);Max time (microseconds);Number of iterations;" << std::endl;

					for(auto it(mStats.begin()), end(mStats.end()); it != end; ++it) {
						if(! it->second.empty()) {
							std::chrono::microseconds min(it->second.front());
							std::chrono::microseconds max(it->second.front());
							std::chrono::microseconds total(0);

							for(const std::chrono::microseconds& value: it->second) {
								if(value < min) {
									min = value;
								} else if(value > max) {
									max = value;
								}

								total += value;
							}

							double average = (((double) total.count()) / ((double) it->second.size())) * 1e-6;

							file.precision(100);
							file << '"' << it->first << '"' << ';'
									<< average << ';'
									<< total.count() << ';'
									<< min.count() << ';'
									<< max.count() << ';'
									<< it->second.size() << ';' << std::endl;
						}
					}

					file.close();

					mStats.clear();
				}
			}
		}

		HighResProfilerCall::HighResProfilerCall(const std::string& function) : mFunction(function) {
			mStart = std::chrono::high_resolution_clock::now();
		}

		HighResProfilerCall::~HighResProfilerCall() {
			HighResProfiler::get().add(mFunction, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - mStart));
		}
		
	} // Namespace Profiling
	
} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
