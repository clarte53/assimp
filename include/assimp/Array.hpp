/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------

Copyright (c) 2006-2012, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the following 
conditions are met:

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
---------------------------------------------------------------------------
*/

/** @file array.hpp
 *  @brief Defines a class to access and modify the arrays of pointers of the assimp types.
 */
#ifndef __ARRAY_HPP_INC__
#define __ARRAY_HPP_INC__

#ifdef __cplusplus

#include <stdexcept> 


class ArrayType {

};

template<typename T>
class Array {

	protected:
		
		void Create(T** data, unsigned int* size) {
			mData = data;
			mLastReference = (*data);
			mSize = size;
			mReservedMemory = 0;
		}
		
		void Reserve(unsigned int size) {
			if(size > Size()) {
				if((*mData) != mLastReference) {
					mReservedMemory = 0;
				}
				
				if(size > mReservedMemory) {
					while(size > mReservedMemory) {
						if(mReservedMemory == 0) {
							mReservedMemory = mDefaultMemory;
						} else {
							mReservedMemory *= 2;
						}
					}
				
					T* data = new T[mReservedMemory];
				
					for(unsigned int i = 0; i < Size(); i++) {
						data[i] = (*mData)[i];
					}
					for(unsigned int i = Size(); i < mReservedMemory; i++) {
						data[i] = T();
					}
				
					delete[] (*mData);
					(*mData) = data;
					mLastReference = data;
				}
			}
		}
		
	public:
		
		Array(T** data, unsigned int* size) {
			Create(data, size);
		}
		
		virtual ~Array() {
			
		}
		
		unsigned int Size() const {
			return (*mSize);
		}
		
		T Get(unsigned int index) const {
			if(index >= Size()) {
				throw std::out_of_range("Invalid index to unallocated memory");
			}
			
			return (*mData)[index];
		}
		
		// Warning: copy the value into the data array without regard for ownership
		void Set(unsigned int index, const T& value) {
			if(index >= Size()) {
				throw std::out_of_range("Invalid index to unallocated memory");
			}
			
			(*mData)[index] = value;
		}
		
		// Warning: copy the value into the data array without regard for ownership
		void Add(const T& value) {
			Reserve(Size() + 1);
			
			(*mData)[(*mSize)++] = value;
		}
		
	protected:
	
		T** mData;
		
		T* mLastReference;
		
		unsigned int* mSize;
		
		unsigned int mReservedMemory;
		
		static const unsigned int mDefaultMemory = 128;

}; // class Array<T>

template<typename T>
class Array<T*> {

	protected:
	
		void Create(T*** data, unsigned int* size) {
			mData = data;
			mLastReference = (*data);
			mSize = size;
			mReservedMemory = 0;
		}

		void Reserve(unsigned int size) {
			if(size > Size()) {
				if((*mData) != mLastReference) {
					mReservedMemory = 0;
				}
				
				if(size > mReservedMemory) {
					while(size > mReservedMemory) {
						if(mReservedMemory == 0) {
							mReservedMemory = mDefaultMemory;
						} else {
							mReservedMemory *= 2;
						}
					}
				
					T** data = new T*[mReservedMemory];
				
					for(unsigned int i = 0; i < Size(); i++) {
						data[i] = (*mData)[i];
					}
					for(unsigned int i = Size(); i < mReservedMemory; i++) {
						data[i] = NULL;
					}
				
					delete[] (*mData);
					(*mData) = data;
					mLastReference = data;
				}
			}
		}
		
	public:
	
		Array(T*** data, unsigned int* size) {
			Create(data, size);
		}
		
		virtual ~Array() {
			
		}
		
		unsigned int Size() const {
			return (*mSize);
		}
		
		T& Get(unsigned int index) const {
			if(index >= Size()) {
				throw std::out_of_range("Invalid index to unallocated memory");
			}
			
			return *((*mData)[index]);
		}
		
		// Warning: copy the value into the data array without regard for ownership
		void Set(unsigned int index, const T& value) {
			if(index >= Size()) {
				throw std::out_of_range("Invalid index to unallocated memory");
			}
			
			*((*mData)[index]) = value;
		}
		
		// Warning: copy the value into the data array without regard for ownership
		void Add(const T& value) {
			Reserve(Size() + 1);
			
			*((*mData)[(*mSize)++]) = value;
		}
		
	protected:
	
		T*** mData;
		
		T** mLastReference;
		
		unsigned int* mSize;
		
		unsigned int mReservedMemory;
		
		static const unsigned int mDefaultMemory = 128;
		
}; // class Array<T*>

#endif //__cplusplus

#endif // __ARRAY_HPP_INC__
