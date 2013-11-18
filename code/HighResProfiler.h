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

/** @file HighResProfiler.h
 *  @brief Defines an high resolution profiler
 */

#ifndef AI_HIGHRESPROFILER_H_INC
#define AI_HIGHRESPROFILER_H_INC

#if defined _DEBUG || ! defined(NDEBUG)
#	define _TO_STRING(x) #x
#	define TO_STRING(x) _TO_STRING(x)
#	define _CONCAT(x,y) x##y
#	define CONCAT(x,y) _CONCAT(x, y)
#	define PROFILER Assimp::Profiling::HighResProfilerCall CONCAT(__profiler,__COUNTER__)(std::string(__FILE__) + ": " + __PRETTY_FUNCTION__ + " (" + TO_STRING(__LINE__) + ")")
#else
#	define PROFILER
#endif

#include <chrono>

namespace Assimp {

	namespace Profiling {

		class HighResProfiler {

			protected:

				std::map<std::string, std::list<std::chrono::microseconds>> mStats;

				HighResProfiler();

				HighResProfiler(const HighResProfiler&) = delete;

				HighResProfiler(HighResProfiler&&) = delete;

				virtual ~HighResProfiler();

			public:

				static HighResProfiler& get();

				void add(const std::string& function, const std::chrono::microseconds& duration);

				void save(const std::string& filename);

		}; // class HighResProfiler

		class HighResProfilerCall {

			protected:

				std::string mFunction;

				std::chrono::time_point<std::chrono::high_resolution_clock> mStart;

			public:

				HighResProfilerCall(const std::string& function);

				virtual ~HighResProfilerCall();

		}; // class HighResProfilerCall
	
	} // end of namespace Profiling
	
} // end of namespace Assimp

#endif // AI_HIGHRESPROFILER_H_INC
