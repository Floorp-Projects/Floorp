/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsxpfc_h___
#define nsxpfc_h___

#include "nscore.h"

#ifdef XP_PC
#pragma warning( disable : 4275 4251 )  // Disable warning messages
#endif

#ifdef _IMPL_NS_XPFC
#define NS_XPFC NS_EXPORT
#else
#define NS_XPFC NS_IMPORT
#endif

#if defined(XP_MAC)
  #define CLASS_EXPORT_XPFC NS_XPFC class
#else
  #define CLASS_EXPORT_XPFC class NS_XPFC
#endif

#endif /* nsxpfc_h___ */
