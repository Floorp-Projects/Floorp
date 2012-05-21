/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INTCNT_H
#define INTCNT_H

class IntCount
{
public:
    IntCount();
    ~IntCount();
    void clear();
    int countAdd(int index, int increment=1);
    int countGet(int index);
    int getSize();
    int getCount(int pos);
    int getIndex(int pos);

private:
    IntCount(const IntCount&); // No copy constructor

    int    numInts;
    struct IntPair{int idx; int cnt;} *iPair;
};

#endif
