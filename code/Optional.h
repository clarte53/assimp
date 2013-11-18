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

/** @file Optional.h
 *  @brief Defines optional values.
 */

#ifndef AI_OPTIONAL_H_INC
#define AI_OPTIONAL_H_INC

#include <boost/noncopyable.hpp>

// Hack for Visual Studio whose support for standard C++ is sometime approximative
// TODO: check the version the day it will be supported to add this feature (useful to avoid hard to find bugs)
#if defined(_MSC_VER)
#	define EXPLICIT
#else
#	define EXPLICIT explicit
#endif

namespace Assimp {

	template<typename T>
	class Optional {

		protected:

			bool mDefined;

			T mValue;

		public:

			Optional() : mDefined(false), mValue() {}

			Optional(const Optional& other) : mDefined(other.mDefined), mValue(other.mValue) {}

			Optional(Optional&& other) : mDefined(other.mDefined), mValue(std::move(other.mValue)) {}

			Optional(const T& value) : mDefined(true), mValue(value) {}

			virtual ~Optional() {}

			inline Optional& operator=(const Optional& other) {mDefined = other.mDefined; mValue = other.mValue; return *this;}

			inline Optional& operator=(Optional&& other) {mDefined = other.mDefined; mValue = std::move(other.mValue); return *this;}

			inline EXPLICIT operator bool() const {return mDefined;}

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

			Optional() : mDefined(false), mValue(nullptr) {}
			
			Optional(Optional&& other) : mDefined(other.mDefined), mValue(other.mValue) {other.mValue = nullptr;}

			Optional(const T* value) : mDefined(true), mValue(value) {}

			virtual ~Optional() {if(mDefined && mValue != nullptr) {delete mValue;}}

			inline Optional& operator=(Optional&& other) {mDefined = other.mDefined; mValue = other.mValue; other.mValue = nullptr; return *this;}

			inline EXPLICIT operator bool() const {return mDefined;}

			inline const T* operator->() const {return mValue;}

			inline T* operator->() {return mValue;}

			inline const T& operator*() const {return *mValue;}

			inline T& operator*() {return *mValue;}

	}; // class Optional<T*>

} // end of namespace Assimp

#endif // AI_OPTIONAL_H_INC
