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
** RCBase.h - Mixin class for NSPR C++ wrappers
*/

#if defined(_RCRUNTIME_H)
#else
#define _RCRUNTIME_H

#include <prerror.h>

/*
** Class: RCBase (mixin)
**
** Generally mixed into every base class. The functions in this class are all
** static. Therefore this entire class is just syntatic sugar. It gives the
** illusion that errors (in particular) are retrieved via the same object
** that just reported a failure. It also (unfortunately) might lead one to
** believe that the errors are persistent in that object. They're not.
*/

class PR_IMPLEMENT(RCBase)
{
public:
    virtual ~RCBase();

    static void AbortSelf();

    static PRErrorCode GetError();
    static PRInt32 GetOSError();

    static PRSize GetErrorTextLength();
    static PRSize CopyErrorText(char *text);

    static void SetError(PRErrorCode error, PRInt32 oserror);
    static void SetErrorText(PRSize textLength, const char *text);

protected:
    RCBase() { }
};  /* RCObject */

inline PRErrorCode RCBase::GetError() { return PR_GetError(); }
inline PRInt32 RCBase::GetOSError() { return PR_GetOSError(); }

#endif  /* defined(_RCRUNTIME_H) */

/* rcbase.h */
