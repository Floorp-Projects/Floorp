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
 * wfFCache.cpp (wfFontCache.cpp)
 *
 * C++ implementation of a Font Cache. This does Font to Fmi
 * mapping. Font <--> Fmi mapping is done by this.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "nf.h"
#include "libfont.h"

#include "wfFCache.h"

//
// Uses : FontObject
#include "Mcf.h"
#include "f.h"
#include "Pcf.h"

static void free_font_store(wfList *object, void *item);

void
/*ARGSUSED*/
free_font_store(wfList *object, void *item)
{
	struct font_store *ele = (struct font_store *) item;
	nffmi_release(ele->fmi, NULL);
	delete ele;
}

wfFontObjectCache::wfFontObjectCache()
: wfList(free_font_store)
{
}

wfFontObjectCache::~wfFontObjectCache()
{
}

// A match is a font object created for
//	1) the same rc type
//	2) the same fmi
//	3) the same accessor
// All fonts served by the native displayer can be shared
// irrespective of the accessor.
//
// So in the algorithm for a match of RcFmi2Font(), we will
// look for only sharable fonts.
//
struct nff *
wfFontObjectCache::RcFmi2Font(struct nfrc *rc, struct nffmi *fmi)
{
	struct nff *f = NULL;
	struct wfListElement *tmp = head;
    for (; tmp; tmp = tmp->next)
	  {
		struct font_store *ele = (struct font_store *) tmp->item;

		cfImpl *oimpl = nff2cfImpl(ele->f);
		FontObject *fob = (FontObject *)oimpl->object;
		if (!fob->isShared())
		{
			continue;
		}
		
		jint f_rcMajorType = nff_GetRcMajorType(ele->f, NULL);
		jint f_rcMinorType = nff_GetRcMinorType(ele->f, NULL);
		if (nfrc_IsEquivalent(rc, f_rcMajorType, f_rcMinorType, NULL)) {
			const char* eleFmiStr = nffmi_toString(ele->fmi, NULL);
			const char* fmiStr = nffmi_toString(fmi, NULL);
			if (eleFmiStr && fmiStr && !strcmp(eleFmiStr, fmiStr)) {
				f = ele->f;
				nff_addRef(f, NULL);
				break;
			}
		}
	  }
	return f;
}

#ifdef NOT_USED_ANYWHERE
struct nffmi *
wfFontObjectCache::Font2Fmi(struct nff *f)
{
	struct nffmi *fmi = NULL;
	struct wfListElement *tmp = head;
	for (; tmp; tmp = tmp->next)
	  {
		struct font_store *ele = (struct font_store *) tmp->item;
		if (ele->f == f)
		  {
			fmi = ele->fmi;
			nffmi_addRef(fmi, NULL);
			break;
		  }
	  }
	return fmi;
}
#endif /* NOT_USED_ANYWHERE */

//
// Find the font corresponding to an rf
//

struct nff *
wfFontObjectCache::
Rf2Font(struct nfrf *rf)
{
	struct nff *f = NULL;
	struct wfListElement *tmp = head;
    for (; tmp; tmp = tmp->next)
	  {
		struct font_store *ele = (struct font_store *) tmp->item;

		// We know that this Font object was created by us. So we can
		// snoop into its implementation to call implementation specific
		// methods.
		cfImpl *oimpl = cf2cfImpl(ele->f);
		FontObject *fob = (FontObject *)oimpl->object;
		if (fob->isRfExist(rf))
		  {
			f = ele->f;
			nff_addRef((struct nff *)f, NULL);
			break;
		  }
	  }
	return f;
}


int
wfFontObjectCache::add(struct nffmi *fmi, struct nff *f)
{
	struct font_store *ele = new font_store;
	wfList::ERROR_CODE err;
	int ret = 0;

	if (!ele)
	  {
		// No memory
		return -1;
	  }
	ele->fmi = fmi;
	ele->f = f;

	err = wfList::add(ele);

	if (err != wfList::SUCCESS)
	  {
		ret = -1;
	  }
	else
	  {
		if (fmi) nffmi_addRef(fmi, NULL);
		//
		// NOTE: The FontObject is refcounted by
		//			1. The actual references of FontObjects by consumers
		//			2. The fonthandle
		//		So even if all FontConsumers release a Font, the FontHandle
		//		that it contains will keep it from being destoyed. So we wont
		//		addRef the Font object. However, we need to be told when the
		//		FontObject is going away;
		//		hence, wfFontObjectCache::releaseFont()
		//
		//if (f) nff_addRef(f, NULL);
	  }
	return (ret);
}


//
// This gets called when a font object is going away.
// Remove all references to the font object in our font object cache.
int
wfFontObjectCache::releaseFont(struct nff *f)
{
	// The caller can specify either fmi or f
	struct wfListElement *tmp;

	wfList::ERROR_CODE err;
	int ret = 0;

	if (!f)
	{
		return (0);
	}

	tmp = head;
	while (tmp)
	{
		struct font_store *ele = (struct font_store *) tmp->item;
		
		// This could cause the element to be deleted. So move to the next
		// element before proceeding with the delete.
		tmp=tmp->next;
		
		if (f == ele->f)
		{
			WF_TRACEMSG( ("NF: Removing font (0x%x) created for fmi (%s).", this,
				nffmi_toString(ele->fmi, NULL)) );
			err = wfList::remove(ele);
			if (err != wfList::SUCCESS)
			{
				// Remove of this failed. Fail the overall remove.
				ret += -1;
			}
		}
	}

	return (ret);
}


int
wfFontObjectCache::releaseRf(struct nfrf *rf)
{
#ifdef DEBUG
	WF_TRACEMSG( ("NF: Deleting rf (0x%x).", rf) );
#endif /* DEBUG */

	struct wfListElement *tmp = head;
	int nrelease = 0;
    while(tmp)
	{
		struct font_store *ele = (struct font_store *) tmp->item;

		// We know that this Font object was created by us. So we can
		// snoop into its implementation to call implementation specific
		// methods.
		cfImpl *oimpl = cf2cfImpl(ele->f);
		FontObject *fob = (FontObject *)oimpl->object;

		//
		// WARNING: Before calling FontObject::releaseRf we need to get the
		// next item in the list. This is because the FontObject could turn
		// around and decide to remove itself from our list.
		//
		tmp = tmp->next;
		int n = fob->releaseRf(rf);
		if (n)
		{
			// Run the garbage collector since some rf was released. This
			// could even cause the font object to be deleted.
			if (!fob->GC())
			{
				struct nff *f_to_delete = (struct nff *) ele->f;

				// Delete the FontObject. Deleting the FontObject will turn
				// around and delete the element in the fontObjectCache list
				// by calling FontObjectCache::releaseFont().
				//
				// So DONT REMOVE THE ELEMENT. It will be removed.
				// wfList::remove(ele);

				nff_addRef(f_to_delete, NULL);
				nff_release(f_to_delete, NULL);
			}
		}
		nrelease += n;
	}

	if (nrelease)
	{
		// Some font had this rf. It is not possbile then for the same rf
		// exist with webfonts. Skip looking into the webfonts.
		return(nrelease);
	}
	return (nrelease);
}
