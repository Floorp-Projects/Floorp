/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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


