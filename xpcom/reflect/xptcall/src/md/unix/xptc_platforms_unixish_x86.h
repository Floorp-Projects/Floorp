/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Platform specific #defines to be shared by the various platforms sharing 
*  the unixish_86 code
*/

/*
*  The goal here is to clearly define the binary compatibility parameters for
*  the platforms that will use this code. Rather than switch at compile time
*  based on the compiler that happens to be in use we are forcing implementors
*  to make a conscious decision.
*
*  For some of these platforms the community may choose to have more than one
*  binary model in effect. In that case I suggest that there be explicit sub
*  defines for that platform specifying 'TYPE1', 'TYPE2', etc. The decision on
*  which 'TYPE' to use would be triggered by a setting passed through from the 
*  config system.
*
*  For example we might end up with something like:
*
* #elif defined(NTO) 
* #  if defined(TYPE1)
* #    define CFRONT_STYLE_THIS_ADJUST
* #  elif defined(TYPE1) 
* #    define THUNK_BASED_THIS_ADJUST
* #  else
* #    error "need TYPE1 or TYPE2 for NTO"
* #  endif
* #elif defined(__BEOS__) 
*
*  and so on....
*
*/

#if defined(LINUX)
#define THUNK_BASED_THIS_ADJUST

#elif defined(__FreeBSD__) 
#define THUNK_BASED_THIS_ADJUST

#elif defined(__NetBSD__) 
#define THUNK_BASED_THIS_ADJUST

#elif defined(__OpenBSD__) 
#define THUNK_BASED_THIS_ADJUST

#elif defined(__bsdi__) 
#define CFRONT_STYLE_THIS_ADJUST

#elif defined(NTO) 
#define CFRONT_STYLE_THIS_ADJUST

#elif defined(__BEOS__) 
#define CFRONT_STYLE_THIS_ADJUST

#elif defined(__sun__)
#define THUNK_BASED_THIS_ADJUST

#else
#error "need a platform define if using unixish x86 code"
#endif

/***************************************************************************/

#if !defined(THUNK_BASED_THIS_ADJUST) && !defined(CFRONT_STYLE_THIS_ADJUST)
#error "need to define 'this' adjust scheme"    
#endif

#if defined(THUNK_BASED_THIS_ADJUST) && defined(CFRONT_STYLE_THIS_ADJUST)
#error "need to define only ONE 'this' adjust scheme"    
#endif
