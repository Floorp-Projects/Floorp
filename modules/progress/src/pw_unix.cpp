/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* pw_unix.cpp
 * Unix implementation of progress interface
 */
#ifndef _WINDOWS

#include "pw_public.h"
 
pw_ptr PW_Create( MWContext * parent, 		/* Parent window, can be NULL */
				PW_WindowType type			/* What kind of window ? Modality, etc */
				)
{
return NULL;
}

void PW_SetCancelCallback(pw_ptr pw,
							PW_CancelCallback cancelcb,
							void * cancelClosure)
{}


void PW_Show(pw_ptr pw)
{}

void PW_Hide(pw_ptr pw)
{}

void PW_Destroy(pw_ptr pw)
{}
 
void PW_SetWindowTitle(pw_ptr pw, const char * title)
{}

void PW_SetLine1(pw_ptr pw, const char * text)
{}

void PW_SetLine2(pw_ptr pw, const char * text)
{}

void PW_SetProgressText(pw_ptr pw, const char * text)
{}

void PW_SetProgressRange(pw_ptr pw, int32 minimum, int32 maximum)
{}

void PW_SetProgressValue(pw_ptr pw, int32 value)
{}

#endif

