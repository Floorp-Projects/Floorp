/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsISupports_h___
#define nsISupports_h___

#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsIID.h"
#include "nsError.h"
#include "nsISupportsUtils.h"

#if defined(NS_MT_SUPPORTED)
#include "prcmon.h"
#endif  /* NS_MT_SUPPORTED */

#if defined(XPIDL_JS_STUBS)
struct JSObject;
struct JSContext;
#endif

/*@{*/


////////////////////////////////////////////////////////////////////////////////

/**
 * IID for the nsISupports interface
 * {00000000-0000-0000-c000-000000000046}
 *
 * NOTE: NEVER EVER EVER EVER EVER change this IID. Never. Not once.
 * No. Don't do it. Stop!
 */
#define NS_ISUPPORTS_IID      \
{ 0x00000000, 0x0000, 0x0000, \
  {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }

/**
 * Reference count values
 */
typedef PRUint32 nsrefcnt;

/**
 * Base class for all XPCOM objects to use. This macro forces the C++
 * compiler to use a compatible vtable layout for all XPCOM objects.
 */
#ifdef XP_MAC
#define XPCOM_OBJECT : public __comobject
#else
#define XPCOM_OBJECT
#endif

/**
 * Basic component object model interface. Objects which implement
 * this interface support runtime interface discovery (QueryInterface)
 * and a reference counted memory model (AddRef/Release). This is
 * modelled after the win32 IUnknown API.
 */
class nsISupports XPCOM_OBJECT {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISUPPORTS_IID; return iid; }
  /**
   * @name Methods
   */

  //@{
  /**
   * A run time mechanism for interface discovery.
   * @param aIID [in] A requested interface IID
   * @param aInstancePtr [out] A pointer to an interface pointer to
   * receive the result.
   * @return <b>NS_OK</b> if the interface is supported by the associated
   * instance, <b>NS_NOINTERFACE</b> if it is not. 
   * <b>NS_ERROR_INVALID_POINTER</b> if <i>aInstancePtr</i> is <b>NULL</b>.
   */
  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr) = 0;
  /**
   * Increases the reference count for this interface.
   * The associated instance will not be deleted unless
   * the reference count is returned to zero.
   *
   * @return The resulting reference count.
   */
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;

  /**
   * Decreases the reference count for this interface.
   * Generally, if the reference count returns to zero,
   * the associated instance is deleted.
   *
   * @return The resulting reference count.
   */
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;

#if XPIDL_JS_STUBS
  // XXX Scriptability hack...
  static NS_EXPORT_(JSObject*) InitJSClass(JSContext* cx) {
    return 0;
  }

  static NS_EXPORT_(JSObject*) GetJSObject(JSContext* cx, nsISupports* priv) {
    NS_NOTYETIMPLEMENTED("nsISupports isn't XPIDL scriptable yet");
    return 0;
  }
#endif

  //@}
};

////////////////////////////////////////////////////////////////////////////////


/*@}*/

#endif /* nsISupports_h___ */
