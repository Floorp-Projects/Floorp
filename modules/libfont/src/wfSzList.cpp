/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 * wfSzList.cpp (FontObject.cpp) 
 *
 * Object local to libfont. This is used to track the association between
 * rc, sizes[] and rf[] for a particular fh in a Font Object.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "wfSzList.h"

wfSizesList::wfSizesList() : sizesLen(-1), sizes(NULL), rfs(NULL), rfcount(0),
  maxrfcount(0), rfAllocStep(4)
{
}

wfSizesList::~wfSizesList()
{
	freeSizes();

	if (rfs)
	{
		WF_FREE(rfs);
	}
}

int
wfSizesList::initialized()
{
	return(sizesLen >= 0 );
}


jdouble *
wfSizesList::getSizes()
{
	jdouble *ret = NULL;
	if (sizesLen)
	  {
		ret = sizes;
	  }
	return (ret);
}

int
wfSizesList::addSizes(jdouble *s)
{
	if (sizesLen)
	  {
		// A list of sizes already exists for this. We will just free
		// it and use the current sizes list.
		freeSizes();
	  }
	sizes = s;
	for (sizesLen=0; sizes && sizes[sizesLen]>=0; sizesLen++);
	return 0;
}

int
wfSizesList::removeSize(jdouble size)
{
	int ret = -1;

	for(int i=0; i<sizesLen; i++)
	  {
		if (sizes[i] == size)
		  {
			// Remove the size from the array
			// by moving the last element to this spot
			sizes[i] = sizes[sizesLen-1];
			sizesLen--;
			sizes[sizesLen] = -1;
			ret = 0;
		  }
	  }
	return (ret);
}

int wfSizesList::supportsSize(jdouble size)
{
#if defined(XP_WIN) && !defined(WIN32)
	/* for win16, don't use the size list, */
	return(1);
#else
	int found = 0;

	if (sizes)
	  {
		for (int i=0; i<sizesLen; i++)
		  {
			if (sizes[i] == size || sizes[i] == 0)
			  {
				found = 1;
				break;
			  }
		  }
	  }
	return (found);
#endif
	
}

int
/*ARGSUSED*/
wfSizesList::addRf(struct nfrf *rf)
{
	// We are changing logic here. All we are going to do is to
	// add the rf in the list of rf's that were created here.
	// size is not used.
	if (rfcount + 1 > maxrfcount)
	{
		// need more memory
		if (maxrfcount == 0)
		{
			rfs = (struct nfrf **)WF_ALLOC(rfAllocStep * sizeof (*rfs));
		}
		else
		{
			rfs = (struct nfrf **) WF_REALLOC(rfs,
				(maxrfcount+rfAllocStep) * sizeof(*rfs));
		}
		if (!rfs)
		{
			// XXX no more memory. things are about to go horribly
			// wrong.  The garbage collection code is banking on this
			// to be able to keep track of the rfs that were created.
			return -1;
		}
		maxrfcount += rfAllocStep;
	}
	rfs[rfcount] = rf;
	// NOTE: we should not increment the reference count here
	//			This was done so that the rfs will release normally.
	//			If we did increment the refcount, then this rf will
	//			never be released. That is why we have nffbp::RfDone()
	//			to notify us that this rf is going away.
	// nfrf_addRef((struct nfrf *)rf, NULL);
	rfcount++;

	return (0);
}


int
wfSizesList::removeRf(struct nfrf *rf)
{
	int ret = 0;
	int i = 0;
	while (i < rfcount)
	  {
		if (rfs[i] == rf)
		  {
			// NOTE: we dont have to decrement the reference count here
			//			as we are not incrementing the reference count for
			//			the rf's stored int he rfs[]. This was done so that
			//			the rfs will release normally. If we did increment
			//			the refcount, then this rf will never be released.
			//			That is why we have nffbp::RfDone() to notify us
			//			that this rf is going away.
			rfs[i] = rfs[rfcount-1];
			rfcount--;
			ret ++;
		  }
		else
		{
			i++;
		}
	  }
	return (ret);
}


int
wfSizesList::isRfExist(struct nfrf *rf)
{
	int ret = 0;
	for (int i = 0; i < rfcount; i++)
	{
		if (rfs[i] == rf)
		{
			ret++;
		}
	  }
	return (ret);
}

int
wfSizesList::getRfCount()
{
	return (rfcount);
}


struct nfrf *
wfSizesList::getRf(jdouble pointsize)
{
	struct nfrf *rf = NULL;
	for (int i = 0; i < rfcount; i++)
	  {
		if (nfrf_GetPointSize(rfs[i], NULL) == pointsize)
		  {
			rf = rfs[i];
			break;
		  }
	  }
	return (rf);
}

//
// Private method implementations
//

void
wfSizesList::freeSizes()
{
	if (sizes)
	  {
		// XXX Possible alloc/free mismatch
		WF_FREE(sizes);
		sizes = NULL;
	  }
}
