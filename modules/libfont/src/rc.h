/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 * rc.h (RenderingContextObject)
 *
 * C++ implementation of the (rc) RenderingContextObject
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _rc_H_
#define _rc_H_

#include "libfont.h"

#include "nf.h"

// Uses :

class RenderingContextObject {
public:
  struct rc_data rcData;

public:
  // Constructor and Destructor
  RenderingContextObject(jint majorType, jint minorType, void **arg, jsize nargs);
  ~RenderingContextObject();

  jint GetMajorType(void);
  jint GetMinorType(void);
  jint IsEquivalent(jint majorType, jint minorType);

  //
  // This will be used only by the Displayer
  //
  struct rc_data GetPlatformData(void);
  jint SetPlatformData(struct rc_data *newRcData);
};
#endif /* _rc_H_ */
