/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *
 * Implements a link between the (cfmi) CFontMatchInfo Interface
 * implementation and its C++ implementation viz (fmi) FontMatchInfoObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _Pcfmi_H_
#define _Pcfmi_H_

#include "Mnffmi.h"
#include "Mcfmi.h"			/* Generated header */

struct cfmiImpl {
  cfmiImplHeader	header;
  void *object;				/* FontMatchInfoObject * */
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_cfmi_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_cfmi_finalize

/* fmi implements its own toString routine. This will be used
 * for catalog store in disk.
 */
#define OVERRIDE_cfmi_toString
#define OVERRIDE_cfmi_equals
#endif /* _Pcfmi_H_ */
