/* 
* Simplistic approach to shared_ptr (in the lack of C++11) 
* Note: non thread-safe implementation
*/

#ifndef _SHARED_PTR_
#define _SHARED_PTR_

#include <stdio.h>
#include <stdlib.h>

template<class T>
class Shared_ptr
{
public:
	Shared_ptr(T* in_ptr)
	{
		ptr = in_ptr;
		ptr_clients = new long;
		*ptr_clients = 1;
	}

	Shared_ptr(int size=1) // Should be an explicit constructor,
						   // but we don't have C++11.
	{
		ptr = new T[size];
		ptr_clients = new long;
		*ptr_clients = 1;
	}

	Shared_ptr(const Shared_ptr &other)
	{
		this->ptr_clients = other.ptr_clients;
		*ptr_clients = *ptr_clients + 1;
		this->ptr = other.ptr;
	}

	~Shared_ptr()
	{
		*ptr_clients = *ptr_clients - 1;
		if(*ptr_clients == 0)
		{
			delete[] ptr;
			delete ptr_clients;
		}
	}
	
	Shared_ptr& operator= (const Shared_ptr& other)
	{
		this->ptr_clients = other.ptr_clients;
		*ptr_clients = *ptr_clients + 1;
		this->ptr = other.ptr;

		return *this;
	}

	Shared_ptr& operator= (T* in_ptr)
	{
		if(ptr)
			delete[] ptr;

		*ptr_clients = 1;
		ptr = in_ptr;

		return *this;
	}

	T& operator[] (int index) const
	{
		return ptr[index];
	}

	T* release_ptr() const
	{
		return ptr;
	}

	T* realloc(int size)
	{
		if(ptr)
			delete[] ptr;

		*ptr_clients = 1;

		ptr = new T[size];

		return ptr;
	}

private:
	T* ptr;

	// Counts how many clients are
	// currently using ptr
	long* ptr_clients;
};

#endif // _SHARED_PTR_
