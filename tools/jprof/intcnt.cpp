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

#include "intcnt.h"

IntCount::IntCount() : numInts(0), iPair(0) { }
IntCount::~IntCount() { delete [] iPair;}
int IntCount::getSize() {return numInts;}
int IntCount::getCount(int pos) {return iPair[pos].cnt;}
int IntCount::getIndex(int pos) {return iPair[pos].idx;}

int IntCount::countAdd(int index, int increment)
{
    if(numInts) {
	// Do a binary search to find the element
	int divPoint = 0;

	if(index>iPair[numInts-1].idx) {
	    divPoint = numInts;
	} else if(index<iPair[0].idx) {
	    divPoint = 0;
	} else {
	    int low=0, high=numInts-1;
	    int mid = (low+high)/2;
	    while(1) {
		mid = (low+high)/2;

		if(index<iPair[mid].idx) {
		    high = mid;
		} else if(index>iPair[mid].idx) {
		    if(mid<numInts-1 && index<iPair[mid+1].idx) {
			divPoint = mid+1;
			break;
		    } else {
			low = mid+1;
		    }
		} else if(index==iPair[mid].idx) {
		    return iPair[mid].cnt += increment;
		}
	    }
	}

	int i;
	IntPair *tpair = new IntPair[numInts+1];
	for(i=0; i<divPoint; i++) tpair[i] = iPair[i];
	for(i=divPoint; i<numInts; i++) tpair[i+1] = iPair[i];
	++numInts;
	delete [] iPair;
	iPair = tpair;
	iPair[divPoint].idx = index;
	iPair[divPoint].cnt = increment;
	return increment;
    } else {
        iPair = new IntPair[1];
	numInts = 1;
	iPair[0].idx = index;
	return iPair[0].cnt = increment;
    }
}
