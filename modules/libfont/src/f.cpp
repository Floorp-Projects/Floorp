/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
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
/* 
 * f.cpp (FontObject.cpp)
 *
 * C++ implementation of the (f) FontObject
 *
 * dp Suresh <dp@netscape.com>
 */


#include "f.h"
#include "nf.h"


FontObject::
FontObject(struct nff *f, struct nfrc *irc, const char *url)
		   : self(f), inGC(0), wfList(free_fh_store), iswebfont(0), state(NF_FONT_COMPLETE), shared(1)
{
	m_rcMajorType = nfrc_GetMajorType(irc, NULL);
	m_rcMinorType = nfrc_GetMinorType(irc, NULL);
	urlOfFont = CopyString(url);
	if (urlOfFont && *urlOfFont)
	{
		// Webfont
		iswebfont = 1;
		state = NF_FONT_INCOMPLETE;
	}
}

FontObject::
~FontObject()
{
	if (urlOfFont) delete (char *)urlOfFont;
}


jdouble * FontObject::
EnumerateSizes(struct nfrc *rc)
{
	wfListElement *tmp = head;
	jdouble *ret = NULL;
	int max_ret_len = 0;

    for (; tmp; tmp = tmp->next)
	  {
		struct fh_store *ele = (struct fh_store *)tmp->item;
		
		computeSizes(rc, ele);
		jdouble *sizes = ele->sizesList.getSizes();
		if (sizes)
		{
			MergeSizes(ret, max_ret_len, sizes);
		}
	}
	return (ret);
}

/*ARGSUSED*/
struct nfrf * FontObject::
GetRenderableFont(struct nfrc *rc, jdouble pointsize)
{
	struct nfrf * rf = NULL;
	wfListElement *tmp = head;
	int onlyOne = (head == tail);

	if (!onlyOne)
	{
		WF_TRACEMSG(("NF: More than one displayer handling font 0x%x.", self));
	}

	// Sanity check to make sure that the rc is of the same type
	if (!nfrc_IsEquivalent(rc, m_rcMajorType, m_rcMinorType, NULL))
	{
		// Invalid rc.
		return NULL;
	}

    for (; tmp; tmp = tmp->next)
	{
		struct fh_store *ele = (struct fh_store *)tmp->item;

		// Get any cached rf.
		rf = ele->sizesList.getRf(pointsize);
		if (rf)
		{
			// Increment the refcount of the cached rf.
			WF_TRACEMSG(("NF: Found rf %x (f = 0x%x, pointsize = %6.2f) in cache.",
				rf, self, pointsize));
			nfrf_addRef(rf, NULL);
			break;
		}

		// Try to get a new rf.
		// The right way to do this is to check with the list of enumerated
		// sizes to see if the displayer does support this size. We will
		// however, ask the displayer anyway if there was only one
		// displayer that handled this font.
		if (!onlyOne)
		{
			computeSizes(rc, ele);
			if (!ele->sizesList.supportsSize(pointsize))
			{
				// point size not supported by this displayer. Try next....
				continue;
			}
		}
		rf = ele->fppeer->CreateRenderableFont(rc, ele->fh, pointsize);
		if (!rf)
		{
			// That displayer lied about the sizes it can handle.
			// Remove the size from the sizes list.
			ele->sizesList.removeSize(pointsize);
		}
		else
		{
			WF_TRACEMSG(("NF: Created new rf %x (f = 0x%x, pointsize = %6.2f) from displayer %s.",
				rf, self, pointsize, ele->fppeer->name()));
			
			// Cache this rf.
			int ret = ele->sizesList.addRf(rf);
			//
			// if ret < 0, then we ran out of memory
			// This is very bad because we are now having the
			// fh refcount the nff. And the fh would need to know
			// exactly how many rf's were created with it. If an
			// rf was created but not added to the sizesList, then
			// the fh will be deleted one too soon causing access of
			// deleted object when the final RfDone() happens.
			break;
		}
	}
	return(rf);
}
struct nffmi *
FontObject::GetMatchInfo(struct nfrc *rc, jdouble pointsize)
{
	struct nffmi *fmi = NULL;
	wfListElement *tmp = head;

    for (; tmp; tmp = tmp->next)
	  {
		struct fh_store *ele = (struct fh_store *)tmp->item;
		computeSizes(rc, ele);

		if (ele->sizesList.supportsSize(pointsize))
		  {
			// Get the fh corresponding to this fontdisplayer
			fmi = ele->fppeer->GetMatchInfo(ele->fh);

			if (!fmi)
			  {
				// That displayer lied about the sizes it can handle.
				// Remove the size from the sizes list.
				ele->sizesList.removeSize(pointsize);
			  }
			else
			  {
				break;
			  }
		  }
	  }
	return (fmi);
}

jint
FontObject::GetRcMajorType()
{
	return (m_rcMajorType);
}
 
jint
FontObject::GetRcMinorType()
{
	return (m_rcMinorType);
}


int
FontObject::isShared(void)
{
	return (shared);
}

int
FontObject::setShared(int sharedState)
{
	shared = sharedState;
	return (0);
}


//
// FontBroker specific
//

int FontObject::
addFontHandle(FontDisplayerPeerObject *fppeer, void *fh)
{
	struct fh_store *ele = new fh_store;
	if (!ele) {
		// No memory.
		return (-1);
	}
	ele->fppeer = fppeer;
	ele->fh = fh;
	add((void *)ele);

	// Tell wffpPeer that we created a fh
	fppeer->FontHandleCreated(fh);

	//
	// We will have the fonthandle refcount the Font too.
	// A font should not be deleted until all its fonthandles
	// are deleted.
	//
	nff_addRef(self, NULL);

	// If the displayer is a non-native displayer
	// (The correct condition is if this is webfont)
	// then disable shared
	if (!fppeer->isNative())
	{
		setShared(0);
	}
	
	return (0);
}


int FontObject::
isRfExist(struct nfrf *rf)
{
	wfListElement *tmp = head;
	int nrf;

    for (; tmp; tmp = tmp->next)
	  {
		struct fh_store *ele = (struct fh_store *)tmp->item;
		nrf = ele->sizesList.isRfExist(rf);
		if (nrf > 0)
		  {
			break;
		  }
	  }
	return (nrf);
}

int
FontObject::releaseRf(struct nfrf *rf)
{
	wfListElement *tmp = head;
	int ret = 0;

    for (; tmp; tmp = tmp->next)
	  {
		struct fh_store *ele = (struct fh_store *)tmp->item;
		ret += ele->sizesList.removeRf(rf);
	  }

	return (ret);
}

int
FontObject::isWebFont()
{
	return (iswebfont);
}

const char *
FontObject::url()
{
	return (urlOfFont);
}


int
FontObject::GetState()
{
	return (state);
}


int
FontObject::setState(int completion_state)
{
	int old_state = state;
	state = completion_state;
	return (old_state);
}



#include "Pcf.h"
//
// The Font garbage Collector.
//
// A FontHandle can be released when there are no consumers using this
// FontObject and there are no rf's active. This is given by the expression
//		[referenceCount(FONT) - fonthandleCount(FONT)]
//				+ rfcount(FONT.sizesList)				== 0
// referenceCount(FONT) is got from looking into the nff* jmc object
// fonthandleCount(FONT) is got by count()
//
// A FontObject can be destroyed only when there are no fonthandles
// in the FontObject. This is given by the expression:
//		fonthandleCount(FONT) == 0
//
// This will return ZERO, if the Font can be deleted. If not, it
// will return NONZERO.
//
int FontObject::
GC()
{
	// Prevent the GC from calling itself. This will happen when fonthandles
	// are removed. Since each of the list element refcounts the Font, everytime
	// an element it removed, its freeItem() will Font::release().
	// The below will prevent the GC from stepping over itself.
	if (inGC)
	{
		return(-1);
	}
	inGC++;

	wfListElement *tmp = head;

	cfImplHeader* impl = (cfImplHeader*)cf2cfImpl(self);
	int nconsumers = impl->refcount - count();

	// At this point, we will increment the refcount by 1. This is because
	// removing an fh could cause the refcount to goto zero and that would
	// cause the FontObject that we are working with to be deleted from
	// underneath us. To prevent this, **AFTER COMPUTING nconsumers** we
	// increment the refcount.
	impl->refcount++;

    while(tmp)
	{
		struct fh_store *ele = (struct fh_store *)tmp->item;
		//
		// WARNING: we could be possibly removing this element.
		// Store the next element we should visit ahead of time.
		//
		tmp = tmp->next;

		if (nconsumers + ele->sizesList.getRfCount() == 0)
		{
			// This fonthandle is a candidate for deletion.
			remove(ele);
		}
	}

	// Undo the dummy incrementing of refcount we did to protect ourself.
	// NOTE: we cannot use nff_release() as that would go and delete the
	// object. Callers of the GC are smart enough to check the return value
	// of GC or look at the refcount and delete the object.
	impl->refcount--;

	inGC --;
	return (impl->refcount);
}

//
// Private method implementations.
//

void FontObject::
computeSizes(struct nfrc *rc, struct fh_store *ele)
{
#if defined(XP_WIN) && !defined(WIN32) && !defined(XP_OS2)
	/* Win 16 core dumps on  enumerating. HACK HACK HACK */
	return;
#else
	if (!ele->sizesList.initialized())
	  {
		// Sizes was never computed for this
		WF_TRACEMSG(("NF: Computing sizes supported by font 0x%x by displayer %s.\n",
			self, ele->fppeer->name()));
		jdouble *sizes = ele->fppeer->EnumerateSizes(rc, ele->fh);
		ele->sizesList.addSizes(sizes);
	  }
#endif
}

//
// Other functions
//
void
free_fh_store(wfList *object, void *item)
{
	// We know that the wfList is actually a FontObject
	// because FontObject is the superclass and wfList is the
	// base class.
	//
	// Hence this is ok.
	FontObject *fob = (FontObject *)object;
	struct fh_store *ele = (struct fh_store *)item;

	// Tell the wffpPeer that we are done with the the fonthandle
	// The wffpPeer will take decisions of unloading the displayer
	// DLM on this call.
	ele->fppeer->FontHandleDone(ele->fh);


	delete(ele);

	//
	// We will have the fonthandle refcount the Font too. So
	// deletion of a fontHandle will decrement the refcount of Font.
	// A font should not be deleted until all its fonthandles
	// are deleted.
	//
	nff_release(fob->self, NULL);
}

