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
** Class definitions for calendar time routines (ref: prtime.h)
*/

#if defined(_RCTIME_H)
#else
#define _RCTIME_H

#include "rcbase.h"

#include <prtime.h>

/*
** Class: RCTime (ref: prtime.h)
**
** RCTimes are objects that are intended to be used to represent calendar
** times. They maintain units internally as microseconds since the defined
** epoch (midnight, January 1, 1970, GMT). Conversions to and from external
** formats (PRExplodedTime) are available.
**
** In general, NCTimes possess normal algebretic capabilities.
*/

class PR_IMPLEMENT(RCTime): public RCBase
{
public:
    typedef enum {now} Current;

    RCTime();                       /* leaves the object unitialized */
    RCTime(Current);                /* initializes to current system time */
    RCTime(const RCTime&);          /* copy constructor */
    RCTime(const PRExplodedTime&);  /* construction from exploded representation */

    virtual ~RCTime();

    /* assignment operators */
    void operator=(const RCTime&); 
    void operator=(const PRExplodedTime&);

    /* comparitive operators */
    PRBool operator<(const RCTime&);
    PRBool operator>(const RCTime&);
    PRBool operator<=(const RCTime&);
    PRBool operator>=(const RCTime&);
    PRBool operator==(const RCTime&);

    /* associative operators */
    RCTime operator+(const RCTime&);
    RCTime operator-(const RCTime&);
    RCTime& operator+=(const RCTime&);
    RCTime& operator-=(const RCTime&);

    /* multiply and divide operators */
    RCTime operator/(PRUint64);
    RCTime operator*(PRUint64);
    RCTime& operator/=(PRUint64);
    RCTime& operator*=(PRUint64);

    void Now();                     /* assign current time to object */

private:
    PRTime gmt;

public:

    RCTime(PRTime);                 /* construct from raw PRTime */
    void operator=(PRTime);         /* assign from raw PRTime */
    operator PRTime() const;        /* extract internal representation */
};  /* RCTime */

inline RCTime::RCTime(): RCBase() { }

inline void RCTime::Now() { gmt = PR_Now(); }
inline RCTime::operator PRTime() const { return gmt; }

inline void RCTime::operator=(PRTime his) { gmt = his; }
inline void RCTime::operator=(const RCTime& his) { gmt = his.gmt; }

inline PRBool RCTime::operator<(const RCTime& his)
    { return (gmt < his.gmt) ? PR_TRUE : PR_FALSE; }
inline PRBool RCTime::operator>(const RCTime& his)
    { return (gmt > his.gmt) ? PR_TRUE : PR_FALSE; }
inline PRBool RCTime::operator<=(const RCTime& his)
    { return (gmt <= his.gmt) ? PR_TRUE : PR_FALSE; }
inline PRBool RCTime::operator>=(const RCTime& his)
    { return (gmt >= his.gmt) ? PR_TRUE : PR_FALSE; }
inline PRBool RCTime::operator==(const RCTime& his)
    { return (gmt == his.gmt) ? PR_TRUE : PR_FALSE; }

inline RCTime& RCTime::operator+=(const RCTime& his)
    { gmt += his.gmt; return *this; }
inline RCTime& RCTime::operator-=(const RCTime& his)
    { gmt -= his.gmt; return *this; }
inline RCTime& RCTime::operator/=(PRUint64 his)
    { gmt /= his; return *this; }
inline RCTime& RCTime::operator*=(PRUint64 his)
    { gmt *= his; return *this; }

#endif /* defined(_RCTIME_H) */

/* RCTime.h */
