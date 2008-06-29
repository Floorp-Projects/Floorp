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
** C++ interval times (ref: prinrval.h)
**
**  An interval is a period of time. The start of the interval (epoch)
**  must be defined by the application. The unit of time of an interval
**  is platform dependent, therefore so is the maximum interval that is
**  representable. However, that interval is never less than ~6 hours.
*/
#if defined(_RCINTERVAL_H)
#else
#define _RCINTERVAL_H

#include "rcbase.h"
#include <prinrval.h>

class PR_IMPLEMENT(RCInterval): public RCBase
{
public:
    typedef enum {now, no_timeout, no_wait} RCReservedInterval;

    virtual ~RCInterval();

    RCInterval();

    RCInterval(PRIntervalTime interval);
    RCInterval(const RCInterval& copy);
    RCInterval(RCReservedInterval special);

    void SetToNow();

    void operator=(const RCInterval&);
    void operator=(PRIntervalTime interval);

    PRBool operator<(const RCInterval&);
    PRBool operator>(const RCInterval&);
    PRBool operator==(const RCInterval&);
    PRBool operator>=(const RCInterval&);
    PRBool operator<=(const RCInterval&);

    RCInterval operator+(const RCInterval&);
    RCInterval operator-(const RCInterval&);
    RCInterval& operator+=(const RCInterval&);
    RCInterval& operator-=(const RCInterval&);

    RCInterval operator/(PRUint32);
    RCInterval operator*(PRUint32);
    RCInterval& operator/=(PRUint32);
    RCInterval& operator*=(PRUint32);


    PRUint32 ToSeconds() const;
    PRUint32 ToMilliseconds() const;
    PRUint32 ToMicroseconds() const;
    operator PRIntervalTime() const;

    static PRIntervalTime FromSeconds(PRUint32 seconds);
    static PRIntervalTime FromMilliseconds(PRUint32 milli);
    static PRIntervalTime FromMicroseconds(PRUint32 micro);

    friend class RCCondition;

private:
    PRIntervalTime interval;
    
};  /* RCInterval */


inline RCInterval::RCInterval(): RCBase() { }

inline RCInterval::RCInterval(const RCInterval& his): RCBase()
    { interval = his.interval; }

inline RCInterval::RCInterval(PRIntervalTime ticks): RCBase()
    { interval = ticks; }

inline void RCInterval::SetToNow() { interval = PR_IntervalNow(); }

inline void RCInterval::operator=(const RCInterval& his)
    { interval = his.interval; }

inline void RCInterval::operator=(PRIntervalTime his)
    { interval = his; }

inline PRBool RCInterval::operator==(const RCInterval& his)
    { return (interval == his.interval) ? PR_TRUE : PR_FALSE; }
inline PRBool RCInterval::operator<(const RCInterval& his)
    { return (interval < his.interval)? PR_TRUE : PR_FALSE; }
inline PRBool RCInterval::operator>(const RCInterval& his)
    { return (interval > his.interval) ? PR_TRUE : PR_FALSE; }
inline PRBool RCInterval::operator<=(const RCInterval& his)
    { return (interval <= his.interval) ? PR_TRUE : PR_FALSE; }
inline PRBool RCInterval::operator>=(const RCInterval& his)
    { return (interval <= his.interval) ? PR_TRUE : PR_FALSE; }

inline RCInterval RCInterval::operator+(const RCInterval& his)
    { return RCInterval((PRIntervalTime)(interval + his.interval)); }
inline RCInterval RCInterval::operator-(const RCInterval& his)
    { return RCInterval((PRIntervalTime)(interval - his.interval)); }
inline RCInterval& RCInterval::operator+=(const RCInterval& his)
    { interval += his.interval; return *this; }
inline RCInterval& RCInterval::operator-=(const RCInterval& his)
    { interval -= his.interval; return *this; }

inline RCInterval RCInterval::operator/(PRUint32 him)
    { return RCInterval((PRIntervalTime)(interval / him)); }
inline RCInterval RCInterval::operator*(PRUint32 him)
    { return RCInterval((PRIntervalTime)(interval * him)); }

inline RCInterval& RCInterval::operator/=(PRUint32 him)
    { interval /= him; return *this; }

inline RCInterval& RCInterval::operator*=(PRUint32 him)
    { interval *= him; return *this; }

inline PRUint32 RCInterval::ToSeconds() const
    { return PR_IntervalToSeconds(interval); }
inline PRUint32 RCInterval::ToMilliseconds() const
    { return PR_IntervalToMilliseconds(interval); }
inline PRUint32 RCInterval::ToMicroseconds() const
    { return PR_IntervalToMicroseconds(interval); }
inline RCInterval::operator PRIntervalTime() const { return interval; }

inline PRIntervalTime RCInterval::FromSeconds(PRUint32 seconds)
    { return PR_SecondsToInterval(seconds); }
inline PRIntervalTime RCInterval::FromMilliseconds(PRUint32 milli)
    { return PR_MillisecondsToInterval(milli); }
inline PRIntervalTime RCInterval::FromMicroseconds(PRUint32 micro)
    { return PR_MicrosecondsToInterval(micro); }

#endif  /* defined(_RCINTERVAL_H) */

/* RCInterval.h */
