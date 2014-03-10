/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

/*
 * Wrapper - Helper class for wrapper objects.
 *
 * This helps to construct a shared_ptr object which wraps access to an underlying handle.
 * (The handle could be a pointer to some low-level type, a conventional C handle, an int ID, a GUID, etc.)
 *
 * Usage:
 *   To obtain a FooPtr from a foo_handle_t, call FooPtr Foo::wrap(foo_handle_t);
 *
 * To implement Foo using Wrapper, Foo needs to include this macro in its class definition:
 *   CSF_DECLARE_WRAP(Foo, foo_handle_t);
 * It also needs to include this in the cpp file, to provide the wrap() implementation and define the static Wrapper.
 *   CSF_IMPLEMENT_WRAP(Foo, foo_handle_t);
 * These are all declared in common/Wrapper.h - Foo.h needs to include this too.
 * The client needs to declare Foo(foo_handle_t) as private, and provide a suitable implementation, as well as
 *   implementing wrappers for any other functions to be exposed.
 * The client needs to implement ~Foo() to perform any cleanup as usual.
 *
 * wrap() will always return the same FooPtr for a given foo_handle_t, it will not construct additional objects
 *        if a suitable one already exists.
 * changeHandle() is used in rare cases where the underlying handle is changed, but the wrapper object is intended
 *                to remain.  This is the case for the "fake" CC_DPCall generated on CC_DPLine::CreateCall(), where
 *                the correct IDPCall* is provided later.
 * reset() is a cleanup step to wipe the handle map and allow memory to be reclaimed.
 *
 * Future enhancements:
 * - For now, objects remain in the map forever.  Better would be to add a releaseHandle() function which would
 *   allow the map to be emptied as underlying handles expired.  While we can't force the client to give up its
 *   shared_ptr<Foo> objects, we can remove our own copy, for instance on a call ended event.
 */

#include <map>
#include "prlock.h"
#include "mozilla/Assertions.h"

/*
 * Wrapper has its own autolock class because the instances are declared
 * statically and mozilla::Mutex will not work properly when instantiated
 * in a static constructor.
 */

class LockNSPR {
public:
  LockNSPR() : lock_(nullptr) {
    lock_ = PR_NewLock();
    MOZ_ASSERT(lock_);
  }
  ~LockNSPR() {
    PR_DestroyLock(lock_);
  }

  void Acquire() {
    PR_Lock(lock_);
  }

  void Release() {
    PR_Unlock(lock_);
  }

private:
  PRLock *lock_;
};

class AutoLockNSPR {
public:
  AutoLockNSPR(LockNSPR& lock) : lock_(lock) {
    lock_.Acquire();
  }
  ~AutoLockNSPR() {
    lock_.Release();
  }

private:
  LockNSPR& lock_;
};

template <class T>
class Wrapper
{
private:
    typedef std::map<typename T::Handle, typename T::Ptr>      	HandleMapType;
    HandleMapType handleMap;
    LockNSPR handleMapMutex;

public:
	Wrapper() {}

	typename T::Ptr wrap(typename T::Handle handle)
	{
		AutoLockNSPR lock(handleMapMutex);
		typename HandleMapType::iterator it = handleMap.find(handle);
		if(it != handleMap.end())
		{
			return it->second;
		}
		else
		{
			typename T::Ptr p(new T(handle));
			handleMap[handle] = p;
			return p;
		}
	}

	bool changeHandle(typename T::Handle oldHandle, typename T::Handle newHandle)
	{
		AutoLockNSPR lock(handleMapMutex);
		typename HandleMapType::iterator it = handleMap.find(oldHandle);
		if(it != handleMap.end())
		{
			typename T::Ptr p = it->second;
			handleMap.erase(it);
			handleMap[newHandle] = p;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool release(typename T::Handle handle)
	{
		AutoLockNSPR lock(handleMapMutex);
		typename HandleMapType::iterator it = handleMap.find(handle);
		if(it != handleMap.end())
		{
			handleMap.erase(it);
			return true;
		}
		else
		{
			return false;
		}
	}

	void reset()
	{
		AutoLockNSPR lock(handleMapMutex);
		handleMap.clear();
	}
};

#define CSF_DECLARE_WRAP(classname, handletype) \
	public: \
		static classname ## Ptr wrap(handletype handle); \
		static void reset(); \
                static void release(handletype handle); \
	private: \
		friend class Wrapper<classname>; \
		typedef classname ## Ptr Ptr; \
		typedef handletype Handle; \
		static Wrapper<classname>& getWrapper() { \
			static Wrapper<classname> wrapper; \
			return wrapper; \
		}

#define CSF_IMPLEMENT_WRAP(classname, handletype) \
	classname ## Ptr classname::wrap(handletype handle) \
	{ \
		return getWrapper().wrap(handle); \
	} \
	void classname::reset() \
	{ \
		getWrapper().reset(); \
	} \
        void classname::release(handletype handle) \
        { \
                getWrapper().release(handle); \
        }
