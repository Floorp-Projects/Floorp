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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
