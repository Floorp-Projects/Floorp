// The contents of this file are subject to the Mozilla Public License
// Version 1.1 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is James L. Nance.

#ifndef INTCNT_H
#define INTCNT_H

class IntCount
{
public:
    IntCount();
    ~IntCount();
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
