/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
