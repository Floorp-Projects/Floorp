/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "SharedPtr.h"
#include "base/lock.h"

template <class T>
class Wrapper
{
private:
    typedef std::map<typename T::Handle, typename T::Ptr>      	HandleMapType;
	HandleMapType 	handleMap;
	Lock 		handleMapMutex;

public:
	typename T::Ptr wrap(typename T::Handle handle)
	{
		AutoLock lock(handleMapMutex);
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
		AutoLock lock(handleMapMutex);
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
		AutoLock lock(handleMapMutex);
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
		AutoLock lock(handleMapMutex);
		handleMap.clear();
	}
};

#define CSF_DECLARE_WRAP(classname, handletype) \
	public: \
		static classname ## Ptr wrap(handletype handle); \
		static void reset(); \
	private: \
		friend class Wrapper<classname>; \
		typedef classname ## Ptr Ptr; \
		typedef handletype Handle; \
		static Wrapper<classname> wrapper;

#define CSF_IMPLEMENT_WRAP(classname, handletype) \
	Wrapper<classname> classname::wrapper; \
	classname ## Ptr classname::wrap(handletype handle) \
	{ \
		return wrapper.wrap(handle); \
	} \
	void classname::reset() \
	{ \
		wrapper.reset(); \
	}

