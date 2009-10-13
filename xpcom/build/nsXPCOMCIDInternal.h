/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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
 * The global proxy object manager.  This component is a singleton.
 * @implement nsIProxyObjectManager
 */
#define NS_XPCOMPROXY_CONTRACTID "@mozilla.org/xpcomproxy;1"

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
