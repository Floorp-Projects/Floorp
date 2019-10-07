/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** RCBase.cpp - Mixin class for NSPR C++ wrappers
*/

#include "rcbase.h"

RCBase::~RCBase() { }

PRSize RCBase::GetErrorTextLength() {
    return PR_GetErrorTextLength();
}
PRSize RCBase::CopyErrorText(char *text) {
    return PR_GetErrorText(text);
}

void RCBase::SetError(PRErrorCode error, PRInt32 oserror)
{
    PR_SetError(error, oserror);
}

void RCBase::SetErrorText(PRSize text_length, const char *text)
{
    PR_SetErrorText(text_length, text);
}

/* rcbase.cpp */
