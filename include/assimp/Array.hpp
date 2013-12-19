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
// Warning: When used, think of updating SceneCombiner to copy the array
#ifndef __ARRAY_HPP_INC__
#define __ARRAY_HPP_INC__

#ifdef __cplusplus

#include <stdexcept> 


class Interface {

};

template<typename T>
class Array {

	protected:
		
		void Update(unsigned int size) {
			if(mData != NULL) {
				if((*mData) != mLastReference) {
					mReservedMemory = (*mSize);
					mLastReference = (*mData);
				}

				Reserve(size);
			}
		}
		
	public:
		
		Array(T** data, unsigned int* size) {
			Create(data, size);
		}
		
		virtual ~Array() {
			// We do not deallocate the memory as assimp classes already do it.
		}
		
		void Create(T** data, unsigned int* size) {
			mData = data;
			mLastReference = (*data);
			mSize = size;
			mReservedMemory = (*size);
		}
		
		void Clear() {
			if((*mData) != NULL) {
				delete[] (*mData);
			}
			
			(*mData) = NULL;
			mLastReference = NULL;
			(*mSize) = 0;
			mReservedMemory = 0;
		}
		
		void Reset() {
			// Save the size to restore it in case it is shared between different arrays
			unsigned int size = (*mSize);

			Clear();

			mData = NULL;
			(*mSize) = size;
			mSize = NULL;
		}

		void Reserve(unsigned int size) {
			unsigned int previous_size = ((*mSize) <= mReservedMemory ? (*mSize) : mReservedMemory);

			if(size > previous_size) {
				if(size > mReservedMemory) {
					while(size > mReservedMemory) {
						if(mReservedMemory == 0) {
							mReservedMemory = mDefaultMemory;
						} else {
							mReservedMemory *= 2;
						}
					}

					T* data = new T[mReservedMemory];

					for(unsigned int i = 0; i < previous_size; i++) {
						data[i] = (*mData)[i];
					}
					for(unsigned int i = previous_size; i < mReservedMemory; i++) {
						data[i] = T();
					}

					delete[] (*mData);
					(*mData) = data;
					mLastReference = data;
				}

				(*mSize) = ((*mSize) >= size ? (*mSize) : size);
			}
		}

		inline unsigned int Size() const {
			return (*mSize);
		}
		
		inline T Get(unsigned int index) {
			#ifdef ASSIMP_BUILD_DEBUG
				if(mData == NULL || (*mData) == NULL || index >= Size()) {
					throw std::out_of_range("Invalid index to unallocated memory");
				}
			#endif
			
			Update(Size());
			
			return (*mData)[index];
		}
		
		// Warning: copy the value into the data array
		inline void Set(unsigned int index, const T& value) {
			if(mData != NULL) {
				Update(index + 1);

				(*mData)[index] = value;
			}
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
	
		void Update(unsigned int size) {
			if(mData != NULL) {
				if((*mData) != mLastReference) {
					mReservedMemory = (*mSize);
					mLastReference = (*mData);
				}

				Reserve(size);
			}
		}

	public:
	
		Array(T*** data, unsigned int* size) {
			Create(data, size);
		}
		
		virtual ~Array() {
			// We do not deallocate the memory as assimp classes already do it.
		}
		
		void Create(T*** data, unsigned int* size) {
			mData = data;
			mLastReference = (*data);
			mSize = size;
			mReservedMemory = (*size);
		}
		
		void Clear() {
			if((*mData) != NULL) {
				for(unsigned int i = 0; i < Size(); i++) {
					delete (*mData)[i];
					(*mData)[i] = NULL;
				}
				
				delete[] (*mData);
			}
			
			(*mData) = NULL;
			mLastReference = NULL;
			(*mSize) = 0;
			mReservedMemory = 0;
		}
		
		void Reset() {
			// Save the size to restore it in case it is shared between different arrays
			unsigned int size = (*mSize);

			Clear();

			mData = NULL;
			(*mSize) = size;
			mSize = NULL;
		}

		void Reserve(unsigned int size) {
			unsigned int previous_size = ((*mSize) <= mReservedMemory ? (*mSize) : mReservedMemory);

			if(size > previous_size) {
				if(size > mReservedMemory) {
					while(size > mReservedMemory) {
						if(mReservedMemory == 0) {
							mReservedMemory = mDefaultMemory;
						} else {
							mReservedMemory *= 2;
						}
					}

					T** data = new T*[mReservedMemory];

					for(unsigned int i = 0; i < previous_size; i++) {
						data[i] = (*mData)[i];
					}
					for(unsigned int i = previous_size; i < mReservedMemory; i++) {
						data[i] = NULL;
					}

					delete[] (*mData);
					(*mData) = data;
					mLastReference = data;
				}

				(*mSize) = ((*mSize) >= size ? (*mSize) : size);
			}
		}

		inline unsigned int Size() const {
			return (*mSize);
		}
		
		inline T& Get(unsigned int index) {
			#ifdef ASSIMP_BUILD_DEBUG
				if(mData == NULL || (*mData) == NULL || index >= Size()) {
					throw std::out_of_range("Invalid index to unallocated memory");
				}
			#endif
			
			Update(Size());
			
			return *((*mData)[index]);
		}
		
		// Warning: copy the pointer into the data array but does not take ownership of it (you still have to free the associated memory)
		inline void Set(unsigned int index, T* value) {
			if(mData != NULL) {
				Update(index + 1);

				(*mData)[index] = value;
			}
		}
		
	protected:
	
		T*** mData;
		
		T** mLastReference;
		
		unsigned int* mSize;
		
		unsigned int mReservedMemory;
		
		static const unsigned int mDefaultMemory = 128;
		
}; // class Array<T*>

template<typename T>
class MultiArray {

	public:
	
		MultiArray(unsigned int size) : mData(NULL) {
			Create(size, false);
		}
		
		virtual ~MultiArray() {
			Clear();
		}

		void Create(unsigned int size, bool dealloc = true) {
			if(dealloc) {
				Clear();
			}

			mSize = size;

			mData = new Array<T>*[mSize];
			
			for(unsigned int i = 0; i < mSize; i++) {
				mData[i] = NULL;
			}
		}

		void Clear() {
			if(mData != NULL) {
				for(unsigned int i = 0; i < mSize; i++) {
					if(mData[i] != NULL) {
						delete mData[i];
						mData[i] = NULL;
					}
				}
		
				delete[] mData;
				mData = NULL;
				mSize = 0;
			}
		}
		
		inline unsigned int Size() const {
			return mSize;
		}
		
		inline Array<T>& Get(unsigned int index) const {
			#ifdef ASSIMP_BUILD_DEBUG
				if(mData == NULL || index >= Size()) {
					throw std::out_of_range("Invalid index to unallocated memory");
				}
			#endif
			
			return *(mData[index]);
		}
		
		inline void Set(unsigned int index, Array<T>* value, bool dealloc = true) const {
			#ifdef ASSIMP_BUILD_DEBUG
				if(mData == NULL || index >= Size()) {
					throw std::out_of_range("Invalid index to unallocated memory");
				}
			#endif

			if(dealloc && mData[index] != NULL) {
				delete mData[index];
			}
			
			mData[index] = value;
		}

	protected:

		unsigned int mSize;
		
		Array<T>** mData;
	
}; // class MultiArray<T>

template<typename T>
class FixedArray {

	public:
	
		FixedArray(T* data, unsigned int size) : mData(NULL) {
			Create(data, size);
		}
		
		virtual ~FixedArray() {
		
		}

		void Create(T* data, unsigned int size) {
			mData = data;
			mSize = size;
		}
		
		inline unsigned int Size() const {
			return mSize;
		}
		
		inline T Get(unsigned int index) const {
			#ifdef ASSIMP_BUILD_DEBUG
				if(mData == NULL || index >= Size()) {
					throw std::out_of_range("Invalid index to unallocated memory");
				}
			#endif
			
			return mData[index];
		}
		
		inline void Set(unsigned int index, const T& value) const {
			#ifdef ASSIMP_BUILD_DEBUG
				if(mData == NULL || index >= Size()) {
					throw std::out_of_range("Invalid index to unallocated memory");
				}
			#endif
			
			mData[index] = value;
		}
		
	protected:
	
		T* mData;
	
		unsigned int mSize;

}; // class FixedArray<T>

#endif //__cplusplus

#endif // __ARRAY_HPP_INC__
