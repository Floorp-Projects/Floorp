/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Class implementation for calendar time routines (ref: prtime.h)
*/

#include "rctime.h"

RCTime::~RCTime() { }

RCTime::RCTime(PRTime time): RCBase() {
    gmt = time;
}
RCTime::RCTime(const RCTime& his): RCBase() {
    gmt = his.gmt;
}
RCTime::RCTime(RCTime::Current): RCBase() {
    gmt = PR_Now();
}
RCTime::RCTime(const PRExplodedTime& time): RCBase()
{
    gmt = PR_ImplodeTime(&time);
}

void RCTime::operator=(const PRExplodedTime& time)
{
    gmt = PR_ImplodeTime(&time);
}

RCTime RCTime::operator+(const RCTime& his)
{
    RCTime sum(gmt + his.gmt);
    return sum;
}

RCTime RCTime::operator-(const RCTime& his)
{
    RCTime difference(gmt - his.gmt);
    return difference;
}

RCTime RCTime::operator/(PRUint64 his)
{
    RCTime quotient(gmt / gmt);
    return quotient;
}

RCTime RCTime::operator*(PRUint64 his)
{
    RCTime product(gmt * his);
    return product;
}

