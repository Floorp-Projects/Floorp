/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
