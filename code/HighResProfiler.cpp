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

		void HighResProfiler::add(const std::string& file, const std::string& function, const std::size_t& line, const std::chrono::microseconds& duration) {
			mProgram.files[file].functions[function].blocs[line].durations.emplace_back(duration);
			mProgram.files[file].functions[function].blocs[line].total += duration;
			mProgram.files[file].functions[function].total += duration;
			mProgram.files[file].total += duration;
			mProgram.total += duration;
		}

		void HighResProfiler::save(const std::string& filename) {
			if(! mProgram.files.empty()) {
				std::ofstream file(filename);

				if(file.is_open()) {
					file << "File;Function;Line;File of program (%);Function of file (%);Bloc of function (%);Average time (seconds);Total time (microseconds);Min time (microseconds);Max time (microseconds);Number of iterations;" << std::endl;

					for(auto it_file(mProgram.files.begin()), end_file(mProgram.files.end()); it_file != end_file; ++it_file) {
						for(auto it_func(it_file->second.functions.begin()), end_func(it_file->second.functions.end()); it_func != end_func; ++it_func) {
							for(auto it_line(it_func->second.blocs.begin()), end_line(it_func->second.blocs.end()); it_line != end_line; ++it_line) {
								if(! it_line->second.durations.empty()) {
									std::chrono::microseconds min(it_line->second.durations.front());
									std::chrono::microseconds max(it_line->second.durations.front());

									for(const std::chrono::microseconds& value: it_line->second.durations) {
										if(value < min) {
											min = value;
										} else if(value > max) {
											max = value;
										}
									}

									file.precision(100);
									file << '"' << it_file->first << '"' << ';'
											<< '"' << it_func->first << '"' << ';'
											<< it_line->first << ';'
											<< (mProgram.total.count() != 0 ? ((double) it_file->second.total.count()) / ((double) mProgram.total.count()) : 0) << ';'
											<< (it_file->second.total.count() != 0 ? ((double) it_func->second.total.count()) / ((double) it_file->second.total.count()) : 0) << ';'
											<< (it_func->second.total.count() != 0 ? ((double) it_line->second.total.count()) / ((double) it_func->second.total.count()) : 0) << ';'
											<< (it_line->second.durations.size() != 0 ? (((double) it_line->second.total.count()) / ((double) it_line->second.durations.size())) * 1e-6 : 0) << ';'
											<< it_line->second.total.count() << ';'
											<< min.count() << ';'
											<< max.count() << ';'
											<< it_line->second.durations.size() << ';' << std::endl;
								}

								it_line->second.durations.clear();
								it_line->second.total = std::chrono::microseconds(0);
							}

							it_func->second.blocs.clear();
							it_func->second.total = std::chrono::microseconds(0);
						}

						it_file->second.functions.clear();
						it_file->second.total = std::chrono::microseconds(0);
					}

					mProgram.files.clear();
					mProgram.total = std::chrono::microseconds(0);

					file.close();
				}
			}
		}

		HighResProfilerCall::HighResProfilerCall(const std::string& file, const std::string& function, const std::size_t& line) : mFile(file), mFunction(function), mLine(line) {
			mStart = std::chrono::high_resolution_clock::now();
		}

		HighResProfilerCall::~HighResProfilerCall() {
			HighResProfiler::get().add(mFile, mFunction, mLine, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - mStart));
		}
		
	} // Namespace Profiling
	
} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_3DXML_IMPORTER
