/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPCOMCIDInternal_h__
#define nsXPCOMCIDInternal_h__

#include "nsXPCOMCID.h"

/**
 * A hashtable-based property bag component.
 * @implements nsIWritablePropertyBag, nsIWritablePropertyBag2
 */
#define NS_HASH_PROPERTY_BAG_CID \
{ 0x678c50b8, 0x6bcb, 0x4ad0, \
{ 0xb9, 0xb8, 0xc8, 0x11, 0x75, 0x95, 0x51, 0x99 } }
#define NS_HASH_PROPERTY_BAG_CONTRACTID "@mozilla.org/hash-property-bag;1"

/**
 * Factory for creating nsIUnicharInputStream
 * @implements nsIUnicharInputStreamFactory
 * @note nsIUnicharInputStream instances cannot be created via
 *       createInstance. Code must use one of the custom factory methods.
 */
#define NS_SIMPLE_UNICHAR_STREAM_FACTORY_CONTRACTID \
  "@mozilla.org/xpcom/simple-unichar-stream-factory;1"

/**
 * The global thread manager service.  This component is a singleton.
 * @implements nsIThreadManager
 */
#define NS_THREADMANAGER_CONTRACTID "@mozilla.org/thread-manager;1"

/**
 * A thread pool component.
 * @implements nsIThreadPool
 */
#define NS_THREADPOOL_CONTRACTID "@mozilla.org/thread-pool;1"

/**
 * The contract id for the nsIXULAppInfo service.
 */
#define XULAPPINFO_SERVICE_CONTRACTID \
  "@mozilla.org/xre/app-info;1"

/**
 * The contract id for the nsIXULRuntime service.
 */
#define XULRUNTIME_SERVICE_CONTRACTID \
  "@mozilla.org/xre/runtime;1"

#endif  // nsXPCOMCIDInternal_h__
