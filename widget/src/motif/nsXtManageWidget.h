/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/* 
   nsXtManageWidget.h --- defines a subclass of XmManager
   Lifted from xfe code 9/10/98. Modified to support resize.
 */

#ifndef _FE_NEWMANAGE_H_
#define _FE_NEWMANAGE_H_

#include <Xm/Xm.h>

extern WidgetClass newManageClass;
typedef struct _NewManageClassRec *NewManageClass;
typedef struct _NewManageRec      *NewManage;

#endif /* _FE_NEWMANAGE_H_ */
