/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsTime_h__
#define nsTime_h__

#include "prtime.h"
#include "nsInt64.h"
#include "nscore.h"

/**
 * This class encapsulates full 64-bit time functionality and
 * provides simple arithmetic and conversion operations.
 */

// If you ever decide that you need to add a non-inline method to this
// class, be sure to change the class declaration to "class NS_BASE
// nsTime".

class nsTime
{
private:
    nsInt64 mValue;
    
public:
    /**
     * Construct the current time.
     */
    nsTime(void) : mValue(PR_Now()) {
    }

    /**
     * Construct the time from a string.
     */
    nsTime(const char* dateStr, PRBool defaultToGMT) {
        PRInt64 time;
        PRStatus status = PR_ParseTimeString(dateStr, defaultToGMT, &time);
        if (status == PR_SUCCESS)
            mValue = time;
        else
            mValue = LL_ZERO;
    }

    /**
     * Construct a time from a PRTime.
     */
    nsTime(const PRTime aTime) : mValue(aTime) {
    }

    /**
     * Construct a time from a 64-bit value.
     */
    nsTime(const nsInt64& aTime) : mValue(aTime) {
    }

    /**
     * Construct a time from another time.
     */
    nsTime(const nsTime& aTime) : mValue(aTime.mValue) {
    }

    // ~nsTime(void) -- XXX destructor unnecessary

    /**
     * Assign one time to another.
     */
    const nsTime& operator =(const nsTime& aTime) {
        mValue = aTime.mValue;
        return *this;
    }

    /**
     * Convert a nsTime object to a PRTime
     */
    operator PRTime(void) const {
        return mValue;
    }

    /**
     * Subtract a 64-bit interval from a time.
     */
    nsTime& operator -=(const nsInt64& aInterval) {
        mValue -= aInterval;
        return *this;
    }

    /**
     * Add a 64-bit interval to a time.
     */
    nsTime& operator +=(const nsInt64& aInterval) {
        mValue += aInterval;
        return *this;
    }

    // Arithmetic operators
    friend const nsTime operator +(const nsTime& aTime, const nsInt64& aInterval);
    friend const nsTime operator -(const nsTime& aTime, const nsInt64& aInterval);
    friend const nsInt64 operator -(const nsTime& aTime1, const nsTime& aTime2);

    // Comparison operators
    friend const PRBool operator ==(const nsTime& aTime1, const nsTime& aTime2);
    friend const PRBool operator !=(const nsTime& aTime1, const nsTime& aTime2);
    friend const PRBool operator <(const nsTime& aTime1, const nsTime& aTime2);
    friend const PRBool operator <=(const nsTime& aTime1, const nsTime& aTime2);
    friend const PRBool operator >(const nsTime& aTime1, const nsTime& aTime2);
    friend const PRBool operator >=(const nsTime& aTime1, const nsTime& aTime2);
};

/**
 * Binary addition to add a 64-bit interval to a time.
 */
inline const nsTime
operator +(const nsTime& aTime, const nsInt64& aInterval) {
    return nsTime(aTime.mValue + aInterval);
}

/**
 * Binary subtraction to subtract a 64-bit interval to a time.
 */
inline const nsTime
operator -(const nsTime& aTime, const nsInt64& aInterval) {
    return nsTime(aTime.mValue - aInterval);
}

/**
 * Binary subtraction to compute an interval from the difference of two times.
 */
inline const nsInt64
operator -(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue - aTime2.mValue;
}

/**
 * Determine if two times are equal
 */
inline const PRBool
operator ==(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue == aTime2.mValue;
}

/**
 * Determine if two times are different
 */
inline const PRBool
operator !=(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue != aTime2.mValue;
}

/**
 * Determine if one time is strictly less than another
 */
inline const PRBool
operator <(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue < aTime2.mValue;
}

/**
 * Determine if one time is less than or equal to another
 */
inline const PRBool
operator <=(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue <= aTime2.mValue;
}

/**
 * Determine if one time is strictly greater than another
 */
inline const PRBool
operator >(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue > aTime2.mValue;
}

/**
 * Determine if one time is greater than or equal to another
 */
inline const PRBool
operator >=(const nsTime& aTime1, const nsTime& aTime2) {
    return aTime1.mValue >= aTime2.mValue;
}

#endif // nsTime_h__
