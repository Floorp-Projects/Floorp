/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// Not putting a ns prefix for the fileName as this is a
// temporary hack to turn off the compilation of the
// profile code.

// PLEASE NOTE THAT THIS FILE ALWAYS HAS TO BE INCLUDED
// AFTER nsIProfile.h. This is because nsIProfile.h also
// allows to turn on the compilation of the profile code.
// As this is already being used, we do not want to force
// people to always use this file to include profiles. An
// ugly hack for now.
#ifndef profileSwitch_h__
#define profileSwitch_h__


// Comment out the line below to perform the compilation
// of the profile code. In this state it causes the
// profile code compilation to be skipped.
#undef NS_USING_PROFILES 

#endif /* profileSwitch_h__ */

