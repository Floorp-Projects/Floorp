/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
** RCBase.cpp - Mixin class for NSPR C++ wrappers
*/

#include "rcbase.h"

RCBase::~RCBase() { }

PRSize RCBase::GetErrorTextLength() { return PR_GetErrorTextLength(); }
PRSize RCBase::CopyErrorText(char *text) { return PR_GetErrorText(text); }

void RCBase::SetError(PRErrorCode error, PRInt32 oserror)
    { PR_SetError(error, oserror); }

void RCBase::SetErrorText(PRSize text_length, const char *text)
    { PR_SetErrorText(text_length, text); }

/* rcbase.cpp */
