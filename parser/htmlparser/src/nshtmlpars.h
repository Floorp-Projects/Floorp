/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 */

#ifndef nshtmlpars_h___
#define nshtmlpars_h___

#include "nscore.h"
#include "nsError.h"

#ifdef _IMPL_NS_HTMLPARS
#define NS_HTMLPARS NS_EXPORT
#else
#define NS_HTMLPARS NS_IMPORT
#endif

#if defined(XP_MAC)
  #define CLASS_EXPORT_HTMLPARS NS_HTMLPARS class
#else
  #define CLASS_EXPORT_HTMLPARS class NS_HTMLPARS
#endif

#endif /* nshtmlpars_h___ */


