/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1997                                                 *
*   (C) Copyright International Business Machines Corporation,  1997                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*/
//------------------------------------------------------------------------------
// File:     mutex.h
//
// Simple mutex class which allows platform-independent thread safety.  The
// burden is on the library caller to provide the actual locking implementation,
// if they need thread-safety.  Clients who don't care don't need to do anything.
//
// Author:   Alan Liu  1/31/97
// History:
// 06/04/97   helena         Updated setImplementation as per feedback from 5/21 drop.
//
//------------------------------------------------------------------------------
#ifndef _NLSMUTEX_
#define _NLSMUTEX_
#include "ptypes.h"

#ifndef _MUTEXIMP
#include "muteximp.h"
#endif

//------------------------------------------------------------------------------
// Code within this library which accesses protected data
// should instantiate a Mutex object while doing so.  Notice that there is only
// one coarse-grained lock which applies to this entire library, so keep locking
// short and sweet.

// For example:

// void Function(int arg1, int arg2)
// {
//    static Object* foo; // Shared read-write object
//    Mutex mutex();
//    foo->Method();
//    // When 'mutex' goes out of scope and gets destroyed here, the lock is released
// }

#ifdef NLS_MAC
#pragma export on
#endif

class NLSCNVAPI_PUBLIC_CLASS NLSMutex
{
public:
	NLSMutex();
	~NLSMutex();

	// Clients must call this method in order to establish the mutex
	// implementation on the local platform.  Note that this call itself
	// is not thread-safe.  The caller is responsible for making this call
	// in a thread-safe way.  Clients who don't care about thread safety
	// don't have to call this method.
	static void SetImplementation(const MutexImplementation* aliased_implementation);

	//
	// Clients can call this method to release the mutex implementation
	// on the local platform.  This call is not thread-safe, and the
	// one called, no other operation will be thread safe.  This should
	// only be called as a part of the application termination routine,
	// and is provided primarily to help clients using memory analysis
	// software obtain a clean report.
	static void ReleaseImplementation(void);

private:
	static MutexImplementation* fgImplementation;
};

#ifdef NLS_MAC
#pragma export off
#endif


#endif //_MUTEX_
//eof
