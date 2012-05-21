/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "intcnt.h"

IntCount::IntCount() : numInts(0), iPair(0) { }
IntCount::~IntCount() { delete [] iPair;}
int IntCount::getSize() {return numInts;}
int IntCount::getCount(int pos) {return iPair[pos].cnt;}
int IntCount::getIndex(int pos) {return iPair[pos].idx;}

void IntCount::clear()
{
    delete[] iPair;
    iPair = new IntPair[0];
    numInts = 0;
}

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
