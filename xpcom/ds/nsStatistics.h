/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Original Author:
 *   Chris Waterson <waterson@netscape.com>
 */

/*

  An object that collects distribution data

*/
#ifndef nsStatistics_h__
#define nsStatistics_h__

#include <stdio.h>
#include "nscore.h"
#include "plhash.h"

/**
 * An object that collects distribution data.
 */
class NS_COM nsStatistics
{
public:
    nsStatistics(const char* aTopic);
    ~nsStatistics();

    /**
     * Add a value to the distribution
     */
    void Record(PRInt32 aValue);

    /**
     * Clear the information collected so far
     */
    void Clear();

    /**
     * Print the mean, standard deviation, and distribution
     * of values reported so far
     */
    void Print(FILE* aStream);

protected:
    const char* mTopic;
    PLHashTable* mDistribution;
    PRInt32 mCount;
    PRInt32 mMinimum;
    PRInt32 mMaximum;
    double mSum;
    double mSumOfSquares;

    static PLHashNumber PR_CALLBACK HashPRInt32(const void* aKey);
};


#endif
