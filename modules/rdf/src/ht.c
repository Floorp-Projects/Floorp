/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   This file implements the Hypertree model on top of the RDF graph model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "fs2rdf.h"
#include "glue.h"
#include "hist2rdf.h"
#include "ht.h"
#include "scook.h"
#include "rdfparse.h"


	/* globals */
HT_Icon			urlList = NULL;
HT_PaneStruct		*gHTTop = NULL;
RDF			gNCDB = NULL;
PRBool			gInited = PR_FALSE;
PRBool			gBatchUpdate = false, gAutoEditNewNode = false, gPaneDeletionMode = false;
XP_Bool			gMissionControlEnabled = false;
HT_MenuCommand		menuCommandsList = NULL;
_htmlElementPtr         htmlElementList = NULL;


	/* extern declarations */
extern RDFT		gRemoteStore;
extern char		*gRLForbiddenDomains;

void			FE_Print(const char *pUrl);	/* XXX this should be added to fe_proto.h */



void
HT_Startup()
{
	if (gInited == PR_FALSE)
	{
		gInited = PR_TRUE;
		gMissionControlEnabled = false;
		PREF_GetBoolPref(MISSION_CONTROL_RDF_PREF, &gMissionControlEnabled);
	}
}



void
HT_Shutdown()
{
	freeMenuCommandList();
	gInited = PR_FALSE;
	gMissionControlEnabled = false;
}



PR_PUBLIC_API(PRBool)
HT_IsLocalData (HT_Resource node)
{
	PRBool		isLocal = PR_FALSE;
	char		*origin;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		if ((origin = node->dataSource) != NULL)
		{
			if (startsWith("rdf:lfs", origin) ||
				startsWith("rdf:localStore", origin))
			{
				isLocal = PR_TRUE;
			}
		}
	}
	return (isLocal);
}



PR_PUBLIC_API(char *)
HT_DataSource (HT_Resource node)
{
	char		*dataSource = NULL;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		dataSource = node->dataSource;
	}
	return(dataSource);
}



HT_Resource
newHTEntry (HT_View view, RDF_Resource node)
{
	HT_Resource		existing, nr;

	XP_ASSERT(view != NULL);
	XP_ASSERT(node != NULL);

	if ((nr = (HT_Resource)getMem(sizeof(HT_ResourceStruct))) != NULL)
	{
		existing = PL_HashTableLookup(view->pane->hash, node);
		nr->view = view;
		if (existing != NULL)
		{
			existing->nextItem = nr;
		}
		else
		{
			PL_HashTableAdd(view->pane->hash, node, nr);
		}
		nr->node = node;
		if (containerp(node))
		{
			nr->flags |= HT_CONTAINER_FLAG;
		}
		else
		{
			nr->flags &= (~HT_CONTAINER_FLAG);
		}
	}
	return(nr);
}



void
addWorkspace(HT_Pane pane, RDF_Resource r, void *feData)
{
	HT_View			view;

	XP_ASSERT(pane != NULL);
	XP_ASSERT(r != NULL);

	/* if view has been deleted (false assertion), don't reassert it */

	if (!RDF_HasAssertion(gNCDB, r, gCoreVocab->RDF_parent, gNavCenter->RDF_Top, RDF_RESOURCE_TYPE, 0))
	{
		view = HT_NewView(r, pane, PR_TRUE, feData, PR_FALSE);
	}
}



void
deleteWorkspace(HT_Pane pane, RDF_Resource r)
{
	HT_Pane			paneList;
	HT_Resource		parent;
	HT_View			view, nextView;

	XP_ASSERT(pane != NULL);
	XP_ASSERT(r != NULL);

	/* find appropriate view(s) and delete them */

	paneList = gHTTop;
	while (paneList != NULL)
	{
		view = paneList->viewList;
		while (view != NULL)
		{
			nextView = view->next;
			if ((parent = HT_TopNode(view)) != NULL)
			{
				if (parent->node == r)
				{
					HT_DeleteView(view);
				}
			}
			view = nextView;
		}
		paneList = paneList->next;
	}
}



/*
	HyperTree pane notification function
*/

void
htrdfNotifFunc (RDF_Event ns, void* pdata)
{
	HT_Event		theEvent;
	HT_Pane			pane;
	HT_Resource		htr;
	PRHashTable		*hash;
	RDF_AssertEvent		aev;
	RDF_UnassertEvent	uev;
	RDF_Resource		vu;

	pane = (HT_Pane)pdata;
	XP_ASSERT(pane != NULL);
	hash = pane->hash;
	XP_ASSERT(hash != NULL);

	switch (ns->eventType) 
	{     
		case RDF_INSERT_NOTIFY:
		case RDF_ASSERT_NOTIFY:
		aev = &(ns->event.assert);
		if (aev->s == gCoreVocab->RDF_parent)
		{
			vu = (RDF_Resource)aev->v;
			if (vu == gNavCenter->RDF_Top)
			{
				if (aev->tv)
				{
					addWorkspace(pane, aev->u, NULL);
				}
				else
				{
					deleteWorkspace(pane, aev->u);
				}
			}
			else
			{
				htr = PR_HashTableLookup(hash, vu);
				while (htr != NULL)
				{
					if (HT_IsContainerOpen(htr))
					{
						if (aev->tv)
						{ 
							if (ns->eventType == RDF_INSERT_NOTIFY)
							{
								resynchContainer(htr);
							}
							else
							{
								addContainerItem(htr, aev->u);
							}
						}
						else
						{
							deleteContainerItem(htr, aev->u);
						}
					}
					htr = htr->nextItem;
				}
			}
		}
		else
		{
			if (aev->s == gNavCenter->RDF_HTMLURL)
			{
				theEvent = HT_EVENT_VIEW_HTML_ADD;
			}
			else
			{
				theEvent = HT_EVENT_NODE_VPROP_CHANGED;
			}
			htr = PR_HashTableLookup(hash, aev->u);
			while (htr != NULL)
			{
				resynchItem(htr, aev->s, aev->v, TRUE);
				sendNotification(htr, theEvent);
				htr = htr->nextItem;
			}
		}
		break;

		case RDF_DELETE_NOTIFY:
		uev = &(ns->event.unassert);
		if ((uev->s == gCoreVocab->RDF_parent))
		{
			if ((RDF_Resource)uev->v == gNavCenter->RDF_Top)
			{
				deleteWorkspace(pane, uev->u);
			}
			else
			{
				htr = PR_HashTableLookup(hash, (RDF_Resource)uev->v);
				while (htr != NULL)
				{
					if ((uev->s == gCoreVocab->RDF_parent) && (htr->child))
					{
						deleteContainerItem(htr, uev->u);
					}
					htr = htr->nextItem;
				}
			}
		}
		else
		{
			if (uev->s == gNavCenter->RDF_HTMLURL)
			{
				theEvent = HT_EVENT_VIEW_HTML_REMOVE;
			}
			else
			{
				theEvent = HT_EVENT_NODE_VPROP_CHANGED;
			}
			htr = PR_HashTableLookup(hash, (RDF_Resource)uev->u);
			while (htr != NULL)
			{
				resynchItem(htr, uev->s, uev->v, FALSE);
				sendNotification(htr, theEvent);
				htr = htr->nextItem;
			}      
		}
		break;
	}
}



/*
	Bookmark / Personal Toolbar pane notification function
	
	Note: only pass RDF_name property changes
*/

void
bmkNotifFunc (RDF_Event ns, void* pdata)
{
	HT_Pane			pane;
	HT_Resource		htr;
	PLHashTable		*hash;
	RDF_AssertEvent		aev = NULL;
	RDF_UnassertEvent	uev = NULL;

	pane = (HT_Pane)pdata;
	XP_ASSERT(pane != NULL);
	hash = pane->hash;
	XP_ASSERT(hash != NULL);

	switch (ns->eventType) 
	{
		case RDF_INSERT_NOTIFY :
		case RDF_ASSERT_NOTIFY :
		aev = &(ns->event.assert);
		if (aev->s == gCoreVocab->RDF_parent)
		{
			htr = PL_HashTableLookup(hash, (RDF_Resource)aev->v);
			while (htr != NULL)
			{
				/* Note: don't check if open... remove item even if closed */
				if (aev->tv)
				{
					if (ns->eventType == RDF_INSERT_NOTIFY)
					{
						resynchContainer(htr);
					}
					else
					{
						addContainerItem(htr, aev->u);
					}
				}
				else
				{
					deleteContainerItem(htr, aev->u);
				}
				htr = htr->nextItem;
			}
		}
		else if ((aev->s == gCoreVocab->RDF_name) || (aev->s == gNavCenter->RDF_smallIcon) ||
				 (aev->s == gNavCenter->RDF_largeIcon))
		{
			htr = PL_HashTableLookup(hash, aev->u);
			while (htr != NULL)
			{
				resynchItem(htr, aev->s, aev->v, TRUE);
				sendNotification(htr, HT_EVENT_NODE_VPROP_CHANGED);
				htr = htr->nextItem;
			}
		}
		break;
	      
		case RDF_DELETE_NOTIFY:
		uev = &(ns->event.unassert);
		if (uev->s == gCoreVocab->RDF_parent)
		{
			htr = PL_HashTableLookup(hash, (RDF_Resource)uev->v);
			while (htr != NULL)
			{
				if (htr->child)
				{
					deleteContainerItem(htr, uev->u);
				}
				htr = htr->nextItem;
			}
		}
		else if ((uev->s == gCoreVocab->RDF_name) || (uev->s == gNavCenter->RDF_smallIcon) ||
				 (uev->s == gNavCenter->RDF_largeIcon))
		{
			htr = PL_HashTableLookup(hash, (RDF_Resource)uev->u);
			while (htr != NULL)
			{
				resynchItem(htr, uev->s, uev->v, FALSE);
				sendNotification(htr, HT_EVENT_NODE_VPROP_CHANGED);
				htr = htr->nextItem;
			}
		}
		break;
	}
}



void
refreshItemListInt (HT_View view, HT_Resource node)
{
	uint32			bucketNum,count,offset;
	ldiv_t			cdiv;

	if (node != NULL)
	{
		node->view = view;
	}
	count = view->itemListCount;
	cdiv  = ldiv(count , ITEM_LIST_ELEMENT_SIZE);
	bucketNum = (uint32)cdiv.quot;
	offset = (uint32)cdiv.rem;
	if (*(view->itemList + bucketNum) == NULL) 
	{
		*(view->itemList + bucketNum) = (HT_Resource*)getMem(ITEM_LIST_ELEMENT_SIZE*4);
		if (*(view->itemList + bucketNum) == NULL)
		{
			return;
		}
	}
	*((*(view->itemList + bucketNum)) + offset) = node;
	node->itemListIndex = (uint32)count;
	view->itemListCount++;
}



int
compareStrings(char *s1, char *s2)
{
#ifdef	XP_WIN
	return(stricmp(s1,s2));		/* case insignificant string compare */
#endif

#ifdef	XP_MAC
	return(strcasecmp(s1,s2));
#endif

	return(strcmp(s1,s2));
}



int
nodeCompareRtn(HT_Resource *node1, HT_Resource *node2)
{
	PRBool		sortOnName = true, isSep1, isSep2;
	uint32		node1Index, node2Index;
	int     	retVal=0;
	void		*data1, *data2;
	void		*sortToken;
	uint32		sortTokenType;
	PRBool		descendingFlag;
	char		*node1Name = NULL, *node2Name = NULL;

	XP_ASSERT(node1 != NULL);
	XP_ASSERT(node2 != NULL);

	if (((*node1) == NULL) || ((*node2) == NULL))
	{
		if (((*node1) == NULL) && ((*node2) != NULL))		return(1);
		else if (((*node1) != NULL) && ((*node2) == NULL))	return(-1);
		return(0);
	}

	sortToken = (*node1)->view->sortToken;
	sortTokenType = (*node1)->view->sortTokenType;
	descendingFlag = (*node1)->view->descendingFlag;

	if (sortToken != NULL)
	{
		isSep1 = HT_IsSeparator(*node1);
		isSep2 = HT_IsSeparator(*node2);
		if (isSep1 && isSep2)	return(0);
		else if (isSep1)	return(1);
		else if (isSep2)	return(-1);

		HT_GetNodeData (*node1, sortToken, sortTokenType, &data1);
		HT_GetNodeData (*node2, sortToken, sortTokenType, &data2);

		switch(sortTokenType)
		{
		case	HT_COLUMN_STRING:
		case	HT_COLUMN_DATE_STRING:
			sortOnName = false;
			if (data1 && data2)
			{
				retVal = compareStrings(data1, data2);
				sortOnName = false;
			}
			else
			{
				descendingFlag = false;
				if (data1)		retVal=-1;
				else if (data2) 	retVal=1;
				else
				{
					descendingFlag = true;
					sortOnName = true;
					retVal=0;
				}
			}
			break;

		case	HT_COLUMN_INT:
		case	HT_COLUMN_DATE_INT:
			if ((long)data1 == (long)data2)
				retVal = 0;
			else if((long)data1 < (long)data2)
				retVal = -1;
			else	retVal = 1;
			sortOnName = false;
			break;
		}
		if (sortOnName == true)
		{
			node1Name = HT_GetNodeName(*node1);
			node2Name = HT_GetNodeName(*node2);
			if ((node1Name != NULL) && (node2Name != NULL))
			{
				retVal = compareStrings(node1Name, node2Name);
			}
			else
			{
				if (node1Name != NULL)		retVal=-1;
				else if (node2Name != NULL)	retVal=1;
				else				retVal=0;
			}
		}
		if ((descendingFlag == true) && (retVal != 0))
		{
			retVal = -retVal;
		}
	}
	else	/* natural order */
	{
		node1Index = (*node1)->unsortedIndex;
		node2Index = (*node2)->unsortedIndex;
		if (node1Index < node2Index)		retVal=-1;
		else if (node1Index > node2Index)	retVal=1;
		else					retVal=0;
	}
	return(retVal);
}



void
sortNodes(HT_View view, HT_Resource parent, HT_Resource *children, uint32 numChildren)
{
	RDF_BT			containerType;
	PRBool			descendingFlag, sortChanged = false;
	void			*sortToken;
	uint32			sortTokenType;

	XP_ASSERT(view != NULL);
	XP_ASSERT(parent != NULL);

	containerType = resourceType(parent->node);
	if ((view->sortToken == NULL) && (containerType != RDF_RT))
	{
		/* default: sort on name column */

		sortChanged = true;

		sortToken = view->sortToken;
		view->sortToken = gCoreVocab->RDF_name;

		sortTokenType = view->sortTokenType;
		view->sortTokenType = HT_COLUMN_STRING;

		descendingFlag = view->descendingFlag;
		view->descendingFlag = false;
	}
	if (numChildren>1)
	{
		XP_QSORT((void *)children, (size_t)numChildren,
		(size_t)sizeof(HT_Resource *), (void *)nodeCompareRtn);
	}
	if (sortChanged == true)
	{
		view->sortToken = sortToken;
		view->sortTokenType = sortTokenType;
		view->descendingFlag = descendingFlag;
	}
}



uint32
refreshItemList1(HT_View view, HT_Resource node)
{
	HT_Cursor		c;
	HT_Resource		*r;
	uint32			loop, numElements=0, numGrandChildren=0;

	XP_ASSERT(view != NULL);
	XP_ASSERT(node != NULL);

	if (node == NULL)	return(0);
	node->view = view;

	if ((c = HT_NewCursor(node)) != NULL)
	{
		if ((numElements = c->numElements) > 0)
		{
			if (node->children = (HT_Resource *)getMem(
						(numElements+1) * sizeof (HT_Resource *)))
			{
				loop=0;
				while (((node->children[loop]) = HT_GetNextItem(c)) != NULL)
				{
					++loop;
					if (loop >= numElements)	break;
				}
				numElements = loop;

				sortNodes(view, node, node->children, numElements);

				/* after a sort, update container's child list */

				r = &(node->child);
				for (loop=0; loop<numElements; loop++)
				{
					*r = node->children[loop];
					r = &((*r)->next);
				}
				*r = NULL;

				for (loop=0; loop<numElements; loop++)
				{
					refreshItemListInt(view, node->children[loop]);
					if (HT_IsContainer(node->children[loop]))
					{
						numGrandChildren += refreshItemList1(view,
									node->children[loop]);
					}
				}
				freeMem(node->children);
				node->children = NULL;
			}
		}
		HT_DeleteCursor(c);
		node->numChildren = numElements;
		node->numChildrenTotal = numElements + numGrandChildren;
		return(node->numChildrenTotal);
	}
	else
	{
		node->child = NULL;
		node->numChildren = 0;
		node->numChildrenTotal = 0;
		return(0);
	}
}



void
refreshItemList (HT_Resource node, HT_Event whatHappened)
{
	XP_ASSERT(node != NULL);
	XP_ASSERT(node->view != NULL);
	XP_ASSERT(node->view->top != NULL);

	if ((node == NULL) || (node->view == NULL) || (node->view->top == NULL))
	{
		return;
	}

	if (!node->view->refreshingItemListp)
	{
		node->view->refreshingItemListp = 1;
		node->view->itemListCount = 0;
		refreshItemList1(node->view,node->view->top);
		node->view->refreshingItemListp = 0;
		node->view->inited = PR_TRUE;
		if (whatHappened) sendNotification(node,  whatHappened);
	}
}



void
refreshPanes(PRBool onlyUpdateBookmarks, PRBool onlyUpdatePersonalToolbar, RDF_Resource newTopNode)
{
	HT_Pane		*paneList;

	paneList = &gHTTop;
	while ((*paneList) != NULL)
	{
		if (((onlyUpdateBookmarks == true) && ((*paneList)->bookmarkmenu == true)) ||
		((onlyUpdatePersonalToolbar == true) && ((*paneList)->personaltoolbar == true)))
		{
			if ((*paneList)->selectedView != NULL)
			{
				destroyViewInt((*paneList)->selectedView->top, FALSE);
				(*paneList)->selectedView->top->node = newTopNode;
				(*paneList)->selectedView->top->flags |= HT_OPEN_FLAG;
				(*paneList)->dirty = true;
			}
		}

		if ((*paneList)->dirty == true)
		{
			if ((*paneList)->selectedView != NULL)
			{
				refreshItemList (((*paneList)->selectedView)->top,
						HT_EVENT_VIEW_REFRESH);
			}
			(*paneList)->dirty = false;
		}
		paneList = &(*paneList)->next;
	}
}



PR_PUBLIC_API(uint32)
HT_GetCountVisibleChildren(HT_Resource node)
{
	uint32		count=0;

	XP_ASSERT(node != NULL);

	if (HT_IsContainer(node) && HT_IsContainerOpen(node))
	{
		count = node->numChildrenTotal;
	}
	return(count);
}



HT_Pane
paneFromResource(RDF_Resource resource, HT_Notification notify, PRBool autoFlushFlag, PRBool autoOpenFlag)
{
	HT_Pane			pane;
	HT_View			view;
        RDF_Event               ev;
	PRBool			err = false;

	do
	{
		if ((pane = (HT_Pane)getMem(sizeof(HT_PaneStruct))) == NULL)
		{
			err = true;
			break;
		}
		pane->special = true;
		pane->ns = notify;
		pane->mask = HT_EVENT_DEFAULT_NOTIFICATION_MASK ;
                if ((ev = (RDF_Event)getMem(sizeof(struct RDF_EventStruct))) == NULL) 
		{
			err = true; 
			break;
		}

		ev->eventType = HT_EVENT_DEFAULT_NOTIFICATION_MASK;

		pane->rns = RDF_AddNotifiable(gNCDB, bmkNotifFunc, ev, pane);
		freeMem(ev);

		if ((pane->hash = PL_NewHashTable(500, idenHash, PL_CompareValues,
						PL_CompareValues,  NULL, NULL)) == NULL)
		{
			err = true;
			break;
		}
		pane->db =  newNavCenterDB();
		pane->autoFlushFlag = autoFlushFlag;

		if ((view = HT_NewView(resource, pane, PR_FALSE, NULL, autoOpenFlag)) == NULL)
		{
			err = true;
			break;
		}
		pane->selectedView = view;

		pane->next = gHTTop;
		gHTTop = pane;

	} while (false);

	if (err == true)
	{
		if (pane != NULL)
		{
			htDeletePane(pane,PR_FALSE);
			pane = NULL;
		}
	}
	return(pane);
}



PR_PUBLIC_API(HT_Pane)
HT_PaneFromResource(RDF_Resource r, HT_Notification n, PRBool autoFlush)
{
	return paneFromResource(r, n, autoFlush, false);
}



PR_PUBLIC_API(HT_Pane)
HT_NewQuickFilePane(HT_Notification notify)
{
	HT_Pane			pane = NULL;
	RDF_Resource		qf;

	if ((qf = RDFUtil_GetQuickFileFolder()) != NULL)
	{
		if ((pane = paneFromResource( qf, notify, true, true)) != NULL)
		{
			pane->bookmarkmenu = true;
			RDFUtil_SetQuickFileFolder(pane->selectedView->top->node);
		}
	}
	return(pane);
}



PR_PUBLIC_API(HT_Pane)
HT_NewPersonalToolbarPane(HT_Notification notify)
{
	HT_Pane			pane = NULL;
	RDF_Resource		pt;


	if ((pt  = RDFUtil_GetPTFolder()) != NULL)
	{
		if ((pane = paneFromResource(pt, notify, true, true)) != NULL)
		{
			pane->personaltoolbar = true;
			RDFUtil_SetPTFolder(pane->selectedView->top->node);
		}
	}
	return(pane);
}



PR_PUBLIC_API(void)
HT_AddToContainer (HT_Resource container, char *url, char *optionalTitle)
{
	RDF_Resource            r;
	RDF             	db;

	XP_ASSERT(container != NULL);
	XP_ASSERT(container->view != NULL);
	XP_ASSERT(container->view->pane != NULL);
	XP_ASSERT(container->view->pane->db != NULL);
	XP_ASSERT(url != NULL);

	db = container->view->pane->db;

	if ((r = RDF_GetResource(db, url, 1)) != NULL)
	{
		if ((optionalTitle != NULL) && (optionalTitle[0] != '\0'))
		{
			RDF_Assert(db, r, gCoreVocab->RDF_name, 
				   optionalTitle, RDF_STRING_TYPE);
		}
		RDF_Assert(db, r, gCoreVocab->RDF_parent, container->node, RDF_RESOURCE_TYPE);
	}
}



PR_PUBLIC_API(void)
HT_AddBookmark (char *url, char *optionalTitle)
{
	RDF_Resource		nbFolder, r;

	XP_ASSERT(url != NULL);

	if ((nbFolder = RDFUtil_GetNewBookmarkFolder()) != NULL)
	{
		if ((r = RDF_GetResource(gNCDB, url, 1)) != NULL)
		{
			if ((optionalTitle != NULL) && (optionalTitle[0] != '\0'))
			{
				RDF_Assert(gNCDB, r, gCoreVocab->RDF_name, 
					   optionalTitle, RDF_STRING_TYPE);
			}
			RDF_Assert(gNCDB, r, gCoreVocab->RDF_parent, nbFolder, RDF_RESOURCE_TYPE);
		}
	}
}



RDF
newHTPaneDB()
{
	return gNCDB;
}



char *
gNavCenterDataSources1[15] = 
{
	"rdf:localStore", "rdf:remoteStore", "rdf:remoteStore", "rdf:history",
	 /* "rdf:ldap", */
	"rdf:esftp", "rdf:mail",

#ifdef	XP_MAC
	"rdf:appletalk",
#endif

	"rdf:lfs",  "rdf:ht",
	"rdf:columns", "rdf:CookieStore", NULL
};



RDF
HTRDF_GetDB()
{
	RDF		ans;
	char		*navCenterURL;

	PREF_SetDefaultCharPref("browser.NavCenter", "http://rdf.netscape.com/rdf/navcntr.rdf");
	PREF_CopyCharPref("browser.NavCenter", &navCenterURL);
	if (!strchr(navCenterURL, ':'))
	{
		navCenterURL = makeDBURL(navCenterURL);
	}
	else
	{
		copyString(navCenterURL);
	}
	*(gNavCenterDataSources1 + 1) = copyString(navCenterURL);
	ans = RDF_GetDB(gNavCenterDataSources1);
	freeMem(navCenterURL);
	return(ans);
}



PR_PUBLIC_API(HT_Pane)
HT_NewPane (HT_Notification notify)
{
	HT_Pane			pane;
	PRBool			err = false;
	RDF_Event               ev;


	do
	{
		if ((pane = (HT_Pane)getMem(sizeof(HT_PaneStruct))) == NULL)
		{
			err = true;
			break;
		}
		pane->ns = notify;
		pane->mask = HT_EVENT_DEFAULT_NOTIFICATION_MASK ;
		
		if ((ev = (RDF_Event)getMem(sizeof(struct RDF_EventStruct))) == NULL) 
		{
			err = true;
			break;
		}
		ev->eventType = HT_EVENT_DEFAULT_NOTIFICATION_MASK;
		pane->db = HTRDF_GetDB();
		pane->rns = RDF_AddNotifiable(pane->db, htrdfNotifFunc, ev, pane);
		freeMem(ev);

		if ((pane->hash = PL_NewHashTable(500, idenHash, PL_CompareValues,
						PL_CompareValues,  null, null)) == NULL)
		{
			err = true;
			break;
		}

		pane->next = gHTTop;
		gHTTop = pane;

		if ((err = initViews(pane)) == true)
		{
			break;
		}

/*
		pane->selectedView = pane->viewList;
*/
	} while (false);

	if (err == true)
	{
		if (pane != NULL)
		{
			htDeletePane(pane, PR_FALSE);
			pane = NULL;
		}
	}
	return(pane);
}



PRBool
initViews (HT_Pane pane)
{
	HT_View			view;
	RDF_Cursor		c;
	RDF_Resource		n;
	PRBool			err = false;

	XP_ASSERT(pane != NULL);

	if ((c = RDF_GetSources(pane->db, gNavCenter->RDF_Top, gCoreVocab->RDF_parent,
				RDF_RESOURCE_TYPE, 1)) != NULL)
	{
		while ((n = RDF_NextValue(c)) != NULL)
		{
			if ((view = HT_NewView(n, pane, PR_TRUE, NULL, PR_FALSE)) == NULL)
			{
				err = true;
				break;
			}
		}
		RDF_DisposeCursor(c);
	}
	return(err);
}



void
htNewWorkspace(HT_Pane pane, char *id, char *optionalTitle, uint32 workspacePos)
{
	RDF_Resource		r;

	XP_ASSERT(pane != NULL);
	XP_ASSERT(id != NULL);
	if (pane == NULL)	return;
	if (id == NULL)		return;

	if ((r = RDF_GetResource(pane->db, id, 0)) == NULL)
	{
		if ((r = RDF_GetResource(pane->db, id, 1)) != NULL)
		{
			setContainerp (r, true);
			setResourceType (r, RDF_RT);
		}
	}
	if (r != NULL)
	{
		if ((optionalTitle != NULL) && (optionalTitle[0] != '\0'))
		{
			RDF_Assert(pane->db, r, gCoreVocab->RDF_name, 
				   optionalTitle, RDF_STRING_TYPE);
		}
		if (workspacePos != 0)
		{
			RDF_Assert(pane->db, r, gNavCenter->RDF_WorkspacePos,
				(void *)&workspacePos, RDF_INT_TYPE);
		}
		RDFUtil_SetDefaultSelectedView(r);
		RDF_Assert(pane->db, r, gCoreVocab->RDF_parent,
			gNavCenter->RDF_Top, RDF_RESOURCE_TYPE);
	}
}



PR_PUBLIC_API(void)
HT_NewWorkspace(HT_Pane pane, char *id, char *optionalTitle)
{
	XP_ASSERT(pane != NULL);
	XP_ASSERT(id != NULL);
	if (pane == NULL)	return;
	if (id == NULL)		return;

	htNewWorkspace(pane, id, optionalTitle, 0);
}



HT_PaneStruct *
HT_GetHTPaneList ()
{
	return(gHTTop);
}



HT_PaneStruct *
HT_GetNextHTPane (HT_PaneStruct* pane)
{
	if (pane == NULL)
	{
		return(NULL);
	}
	return(pane->next);
}



void
htSetWorkspaceOrder(RDF_Resource src, RDF_Resource dest, PRBool afterDestFlag)
{
	HT_Pane			paneList;
	HT_View			*viewList, *prevViewList, srcView, *srcViewList;
	PRBool			foundSrc, foundDest;

	XP_ASSERT(src != NULL);
	XP_ASSERT(dest != NULL);
	if ((src == NULL) || (dest == NULL))	return;

	paneList = gHTTop;
	while (paneList != NULL)
	{
		foundSrc = foundDest = FALSE;
		srcView = NULL;
		prevViewList = srcViewList = NULL;
		viewList = &(paneList->viewList);
		while(*viewList)
		{
			if ((*viewList)->top->node == src)
			{
				srcView = (*viewList);
				srcViewList = viewList;
				foundSrc = TRUE;
			}
			if ((*viewList)->top->node == dest)
			{
				foundDest = TRUE;
			}
			if (foundDest == FALSE)
			{
				prevViewList = viewList;
			}
			if (foundSrc == TRUE && foundDest == TRUE)
			{
				break;
			}
			viewList = &(*viewList)->next;
		}
		if (foundSrc == TRUE && foundDest == TRUE)
		{
			if (htIsOpLocked(srcView->top,
				gNavCenter->RDF_WorkspacePosLock))
			{
				return;
			}
			if (afterDestFlag == TRUE)
			{
				/* move src workspace after dest workspace */
				
				if ((*prevViewList)->next != NULL)
				{
					if (htIsOpLocked(((*prevViewList)->next)->top,
						gNavCenter->RDF_WorkspacePosLock))
					{
						return;
					}
				}

				*srcViewList = (*srcViewList)->next;
				srcView->next = (*prevViewList)->next;
				(*prevViewList)->next = srcView;
			}
			else
			{
				/* move src workspace before dest workspace */

				if (prevViewList == NULL)
				{
					prevViewList = &(paneList->viewList);
				}
				else
				{
					prevViewList = &((*prevViewList)->next);
				}

				if ((*prevViewList) != NULL)
				{
					if (htIsOpLocked((*prevViewList)->top,
						gNavCenter->RDF_WorkspacePosLock))
					{
						return;
					}
				}
				*srcViewList = (*srcViewList)->next;
				srcView->next = *prevViewList;
				*prevViewList = srcView;
			}

			saveWorkspaceOrder(paneList);
			sendNotification(srcView->top, HT_EVENT_VIEW_WORKSPACE_REFRESH);
		}
		paneList = paneList->next;
	}
}



PR_PUBLIC_API(void)
HT_SetWorkspaceOrder(HT_View src, HT_View dest, PRBool afterDestFlag)
{
	XP_ASSERT(src != NULL);
	XP_ASSERT(src->pane != NULL);
	XP_ASSERT(dest != NULL);
	XP_ASSERT(dest->pane != NULL);
	XP_ASSERT(src->pane == dest->pane);

	if ((src == NULL) || (dest == NULL))	return;

	htSetWorkspaceOrder(src->top->node, dest->top->node, afterDestFlag);
}



HT_View
HT_NewView (RDF_Resource topNode, HT_Pane pane, PRBool useColumns, void *feData, PRBool autoOpen)
{
	HT_Column		*columnList, column, nextColumn;
	HT_View			view, *viewList;
	RDF_Cursor		cursor;
	RDF_Resource		r, selectedView;
	PRBool			err = false, workspaceAdded;
	uint32			workspacePos;

	do
	{
		if ((view = (HT_View)getMem(sizeof(HT_ViewStruct))) == NULL)
		{
			err = true;
			break;
		}
		view->pane = pane;
		if ((view->top = newHTEntry(view, topNode)) == NULL)
		{
			err = true;
			break;
		}

		HT_SetViewFEData(view, feData);

		if (autoOpen)	view->top->flags |= HT_OPEN_FLAG;
		view->top->depth = 0;

		view->itemListSize = ITEM_LIST_SIZE;
		if ((view->itemList = (HT_Resource **)getMem(ITEM_LIST_SIZE * 4)) == NULL)
		{
			err = true;
			break;
		}

		view->itemListCount = 0;
		if (autoOpen)
		{
			refreshItemList(view->top, 0);
		}

		if (useColumns)	{
		columnList = &view->columns;
		if ((cursor = RDF_GetTargets(pane->db, view->top->node, gNavCenter->RDF_Column, 
						RDF_RESOURCE_TYPE,  true)) != NULL)
		{
			while ((r = RDF_NextValue(cursor)) != NULL)
			{
				if (resourceID(r) == NULL)	break;

				column = (HT_Column)getMem(sizeof(HT_ColumnStruct));
				if (column == NULL)	{
					err = true;
					break;
					}

				column->token = r;
				column->tokenType = (uint32) RDF_GetSlotValue(pane->db, r,
								gNavCenter->RDF_ColumnDataType,
								RDF_INT_TYPE, false, true);
				column->width = (uint32) RDF_GetSlotValue(pane->db, r,
								gNavCenter->RDF_ColumnWidth,
								RDF_INT_TYPE, false, true);
				column->name = (char *) RDF_GetSlotValue(pane->db, r,
								gCoreVocab->RDF_name,
								RDF_STRING_TYPE, false, true);
				*columnList = column;
				columnList = &(column->next);
			}	
			RDF_DisposeCursor(cursor);
		}

		/* if no columns defined, force one (name) to exist */

		if (view->columns == NULL)
		{
			column = (HT_Column)getMem(sizeof(HT_ColumnStruct));
			if (column == NULL)	{
				err = true;
				break;
				}
			column->token = gCoreVocab->RDF_name;
			column->tokenType = HT_COLUMN_STRING;
			column->width = 10000;
			column->name = copyString(XP_GetString(RDF_DEFAULTCOLUMNNAME));

			view->columns = column;
		}
		}

		/*
			add into view list, taking into account any defined
			workspace positions, workspace lock status, etc...
		*/

		workspacePos = (uint32) RDF_GetSlotValue(gNCDB,
						view->top->node,
						gNavCenter->RDF_WorkspacePos,
						RDF_INT_TYPE, false, true);

		view->workspacePos = workspacePos;
		viewList = &(pane->viewList);
		if (htIsOpLocked(view->top, gNavCenter->RDF_WorkspacePosLock))
		{
			/* always insert locked workspaces BEFORE unlocked workspaces */
			workspaceAdded = false;
			while ((*viewList) != NULL)
			{
				if ((!htIsOpLocked((*viewList)->top, gNavCenter->RDF_WorkspacePosLock)) ||
					((*viewList)->workspacePos > workspacePos))
				{
					view->next = (*viewList);
					(*viewList) = view;
					workspaceAdded = true;
					break;
				}
				viewList = &((*viewList)->next);
			}
			if (workspaceAdded == false)
			{
				view->next = (*viewList);
				(*viewList) = view;
			}
		}
		else
		{
			/* always insert unlocked workspaces AFTER locked workspaces */
			if (workspacePos == 0)
			{
				/*
					if unlocked and position not specified,
					add onto end of view list
				*/
				while ((*viewList) != NULL)
				{
					viewList = &((*viewList)->next);
				}
				*viewList = view;
			}
			else
			{
				/*
					if unlocked and position specified,
					move past locked workspaces, and then
					add into list
				*/
				while (((*viewList) != NULL) && 
					(htIsOpLocked((*viewList)->top,
						gNavCenter->RDF_WorkspacePosLock)))
				{
					viewList = &((*viewList)->next);
				}
				while ((*viewList) != NULL)
				{
					if ((*viewList)->workspacePos > workspacePos)	break;
					viewList = &((*viewList)->next);
				}
				view->next = (*viewList);
				(*viewList) = view;
			}
		}
		++(pane->viewListCount);

		sendNotification(view->top, HT_EVENT_VIEW_ADDED);

		if (pane->special == false)
		{
			selectedView = RDFUtil_GetDefaultSelectedView();
			if ((selectedView == topNode) || (selectedView == NULL))
			{
				HT_SetSelectedView(pane, view);
			}
		}

	} while (false);

	if (err == true)
	{
		if (view != NULL)
		{
			if (view->top != NULL)
			{
				freeMem(view->top);
			}
			if (view->itemList != NULL)
			{
				freeMem(view->itemList);
				view->itemList = NULL;
			}
			column = view->columns;
			while (column != NULL)
			{
				nextColumn = column->next;
				freeMem(column);
				column = nextColumn;
			}
			freeMem(view);
			view = NULL;
		}
	}
	return(view);
}



void
sendNotification (HT_Resource node, HT_Event whatHappened)
{
	HT_Pane			pane;
	HT_Notification		ns;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->view != NULL);
	XP_ASSERT(node->view->pane != NULL);

	if (node->view == NULL)			return;
	if ((pane = node->view->pane) == NULL)	return;
	if ((ns = pane->ns) == NULL)		return;
	if (ns->notifyProc == NULL)		return;
  
	if (pane->mask & whatHappened) 
	{
		(*ns->notifyProc)(ns, node, whatHappened);
	}
	pane->dirty = TRUE;
}



void
deleteHTNode(HT_Resource node)
{
	HT_Value		value,nextValue;
	HT_View			view;
	HT_Resource		parent;
	ldiv_t			cdiv;
	uint32			bucketNum, itemListIndex, offset;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->view != NULL);

	/* HT_SetSelectedState(node, false); */
	sendNotification(node, (node->feData != NULL) ?
		HT_EVENT_NODE_DELETED_DATA : HT_EVENT_NODE_DELETED_NODATA);

	itemListIndex = node->itemListIndex;

	value = node->values;
	while (value != NULL)
	{
		nextValue = value->next;
		if (value->tokenType == HT_COLUMN_STRING ||
		    value->tokenType == HT_COLUMN_DATE_STRING)
		{
			if (value->data != NULL)
			{
				freeMem(value->data);
				value->data = NULL;
			}
		}
		freeMem(value);
		value = nextValue;
	}
	node->values = NULL;

	if (node->flags & HT_FREEICON_URL_FLAG)
	{
		if (node->url[0] != NULL)
		{
			freeMem(node->url[0]);
			node->url[0] = NULL;
		}
		if (node->url[1] != NULL)
		{
			freeMem(node->url[1]);
			node->url[1] = NULL;
		}
	}

	if ((!node->view->refreshingItemListp) && (node->view->itemList != NULL))
	{
		/* set node's associated entry in itemList to NULL */

		if (itemListIndex < node->view->itemListCount)
		{
			cdiv  = ldiv(itemListIndex , ITEM_LIST_ELEMENT_SIZE);
			bucketNum = (uint32)cdiv.quot;
			offset = (uint32)cdiv.rem;
			view = node->view;
			if (*(view->itemList + bucketNum) != NULL) 
			{
				*((*(view->itemList + bucketNum)) + offset) = NULL;
			}
		}

		/* update list count if list isn't busy */

		if ((gBatchUpdate != true) && (node->view->itemListCount > 0))
		{
			--(node->view->itemListCount);
		}
	}

	removeHTFromHash(node->view->pane, node);
	parent = node->parent;
	if (parent->numChildren > 0)	--(parent->numChildren);
	while (parent != NULL)
	{
		--(parent->numChildrenTotal);
		parent=parent->parent;
	}
	node->node = NULL;
	node->view = NULL;
	node->parent = NULL;
	node->child = NULL;
	node->children = NULL;	 
	freeMem(node);
}



void
destroyViewInt (HT_Resource r, PRBool saveOpenState)
{
	HT_Resource		child, tmp;
	PRBool			openState;

	child = r->child;
	while (child != NULL)
	{
		/* save container's open/closed state */
		if (HT_IsContainer(child) && (child->view->pane->special == PR_FALSE))
		{
			openState = nlocalStoreHasAssertion(gLocalStore, child->node,
                                                            gNavCenter->RDF_AutoOpen, "yes",
				RDF_STRING_TYPE, 1);
			if ((!openState) && (HT_IsContainerOpen(child)) &&
				(!startsWith("ftp:", resourceID(child->node))) &&
				(!startsWith("es:", resourceID(child->node))) &&
				(!startsWith("ldap:", resourceID(child->node))))
			{
				/* make assertion */
				nlocalStoreAssert(gLocalStore, child->node,
				gNavCenter->RDF_AutoOpen, "yes",
				RDF_STRING_TYPE, 1);
			}
			else if (openState && (!HT_IsContainerOpen(child)))
			{
				/* remove assertion */
				nlocalStoreUnassert(gLocalStore, child->node,
				gNavCenter->RDF_AutoOpen, "yes",
				RDF_STRING_TYPE);
			}
		}
		if (child->child)
		{
			tmp=child->next;
			destroyViewInt(child, saveOpenState);
			if(child != NULL)
			{
				deleteHTNode(child);
			}
			child = tmp;
		}
		else
		{
			tmp = child;
			child = child->next;
			deleteHTNode(tmp);
		}
	}
	r->child = NULL;
	r->numChildren = 0;
	r->numChildrenTotal = 0;
}



PR_PUBLIC_API(HT_Error)
HT_DeleteView (HT_View view)
{
	HT_Column		column, nextColumn;
	HT_View			*viewList;
	HT_Pane			pane;
	HT_Resource		**r;
	uint32			loop, viewIndex;

	XP_ASSERT(view != NULL);

	if (view->top != NULL)					/* delete nodes */
	{
		destroyViewInt(view->top, PR_TRUE);
	}
	if (view->itemList != NULL)				/* delete itemList */
	{
		for (loop=0; loop<view->itemListSize; loop++)
		{
			if ((r = (HT_Resource **)view->itemList[loop]) != NULL)
			{
				freeMem(r);
			}
		}
		freeMem(view->itemList);
		view->itemList = NULL;
	}

	column = view->columns;					/* delete columns */
	while (column != NULL)
	{
		nextColumn = column->next;
		if (column->name != NULL)
		{
			freeMem(column->name);
			column->name = NULL;
		}
		freeMem(column);
		column = nextColumn;
	}
	view->columns = NULL;

	pane = HT_GetPane(view);
	viewList = &(pane->viewList);
	while (*viewList)
	{
		if (*viewList == view)
		{
			viewIndex = HT_GetViewIndex(view);
			if (viewIndex > 0)	--viewIndex;

			*viewList = view->next;
			--(pane->viewListCount);

			/*
				if deleting the selected view,
				try and select another view
			*/

			if ((gPaneDeletionMode != true) && (pane->selectedView == view))
			{
				HT_SetSelectedView(pane, HT_GetNthView(pane, viewIndex));
			}
			break;
		}
		viewList = &((*viewList)->next);
	}

	sendNotification(view->top, HT_EVENT_VIEW_DELETED);
	if (gPaneDeletionMode != true)
	{
		sendNotification(view->top, HT_EVENT_VIEW_WORKSPACE_REFRESH);
	}
	
	if (view->top != NULL)
	{
		deleteHTNode(view->top);
		view->top = NULL;
	}

	freeMem(view);
	return (HT_NoErr);
}



PR_PUBLIC_API(char *)
HT_GetViewName(HT_View view)
{
	char		*name = NULL;

	XP_ASSERT(view != NULL);
	XP_ASSERT(view->top != NULL);

	if (view != NULL)
	{
		HT_GetNodeData (view->top, gCoreVocab->RDF_name, HT_COLUMN_STRING, &name);
	}
	return(name);
}



void
saveWorkspaceOrder(HT_Pane pane)
{
	HT_View			view;
	uint32			viewListIndex=1, workspacePos;

	XP_ASSERT(pane != NULL);
	if (pane == NULL)	return;

	view = pane->viewList;
	while (view != NULL)
	{
		workspacePos = (uint32) RDF_GetSlotValue(gNCDB,
					view->top->node,
					gNavCenter->RDF_WorkspacePos,
					RDF_INT_TYPE, false, true);
		RDF_Unassert(gNCDB, view->top->node,
					gNavCenter->RDF_WorkspacePos,
					(void *)&workspacePos, RDF_INT_TYPE);
		RDF_Assert(gNCDB, view->top->node,
					gNavCenter->RDF_WorkspacePos,
					(void *)&viewListIndex, RDF_INT_TYPE);
		++viewListIndex;
		view = view->next;
	}
}



void
htDeletePane(HT_Pane pane, PRBool saveWorkspaceOrderFlag)
{
	HT_Pane			*paneList;
	HT_View			view;

	XP_ASSERT(pane != NULL);
	if (pane == NULL)	return;

	if (saveWorkspaceOrderFlag == PR_TRUE)
	{
		saveWorkspaceOrder(pane);
	}
	gPaneDeletionMode = true;
	while ((view = pane->viewList) != NULL)
	{
		HT_DeleteView(view);
	}
	gPaneDeletionMode = false;
	if (pane->hash != NULL)
	{
		PL_HashTableDestroy(pane->hash);			/* delete hash table */
		pane->hash = NULL;
	}
	if (pane->rns != NULL)
	{
		RDF_DeleteNotifiable (pane->rns);
		pane->rns = NULL;
	}

	paneList = &gHTTop;
	while ((*paneList) != NULL)
	{
		if ((*paneList) == pane)
		{
			(*paneList) = pane->next;
			break;
		}
		paneList = &(*paneList)->next;
	}

	freeMem(pane);
}



PR_PUBLIC_API(HT_Error)
HT_DeletePane (HT_Pane pane)
{
	XP_ASSERT(pane != NULL);

	htDeletePane(pane, PR_TRUE);
	return (HT_NoErr);
}
	


PR_PUBLIC_API(HT_Resource)
HT_TopNode (HT_View view)
{
	HT_Resource		top = NULL;

	XP_ASSERT(view != NULL);

	if (view != NULL)
	{
		top = view->top;
	}

	return (top);
}



void
resynchItem (HT_Resource node, void *token, void *data, PRBool assertAction)
{
	HT_Value		*value, tempValue;

	XP_ASSERT(node != NULL);

	value = &(node->values);
	while ((*value) != NULL)
	{
		if ((*value)->token == token)
		{
			if ((*value)->tokenType == HT_COLUMN_STRING ||
			    (*value)->tokenType == HT_COLUMN_DATE_STRING)
			{
				
				if ((*value)->data)
				{
					freeMem((*value)->data);
					(*value)->data = NULL;
				}
			}

			if ((assertAction == false) || (data == NULL))
			{
				tempValue = (*value);
				(*value) = (*value)->next;
				freeMem(tempValue);
			}
			else if ((*value)->tokenType == HT_COLUMN_STRING ||
			    (*value)->tokenType == HT_COLUMN_DATE_STRING)
			{
				
				(*value)->data = copyString(data);
			}
			else
			{
				(*value)->data = data;
			}
			break;
		}
		value = &((*value)->next);
	}
}



void
resynchContainer (HT_Resource container)
{
	HT_Resource		parent, tc, nc = NULL;
	RDF_Cursor		c;
	RDF_Resource		next;
	PRBool			found;
	uint32			n = 0;

	if ((c = RDF_GetSources(container->view->pane->db, container->node,
			gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, 1)) != NULL)
	{
		while (next = RDF_NextValue(c))
		{
			tc = container->child;
			found = 0;
			while (tc)
			{
				if (tc->node == next)
				{
					tc->unsortedIndex = n;
					found = 1;
					break;
				}
				tc = tc->next;
			}
			if (!found)
			{
				if ((nc = newHTEntry(container->view, next)) != NULL)
				{
					nc->dataSource = RDF_ValueDataSource(c);
					nc->parent = container;
					nc->next = container->child;
					container->child = nc;
					nc->depth = container->depth + 1;
					++container->numChildren;
					nc->unsortedIndex = n;	/* container->numChildren; */
					parent = container;
					while (parent != NULL)
					{
						++(parent->numChildrenTotal);
						parent=parent->parent;
					}
				}
				break;
			}
			++n;
		}
		RDF_DisposeCursor(c);
	}
	if (nc != NULL)
	{
		refreshContainerIndexes(container);
		refreshItemList(nc, HT_EVENT_NODE_ADDED);
		if (gAutoEditNewNode == true)
		{
			gAutoEditNewNode = false;
			HT_SetSelection (nc);
			sendNotification(nc, HT_EVENT_NODE_EDIT);
		}
	}
	refreshItemList(container, HT_EVENT_VIEW_REFRESH);
}



void
refreshContainerIndexes(HT_Resource container)
{
	HT_Resource		child;
	RDF_Cursor		c;
	RDF_Resource		next;
	uint32			unsortedIndex=0;

	if (c = RDF_GetSources(container->view->pane->db, container->node,
  			gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, 1))
	{
		while (next = RDF_NextValue(c))
		{
			child = container->child;
			while (child)
			{
				if (child->node == next)
				{
					child->unsortedIndex = unsortedIndex;
					break;
				}
				child = child->next;
			}
			++unsortedIndex;
		} 
		RDF_DisposeCursor(c);
	}

}

  

HT_Resource
addContainerItem (HT_Resource container, RDF_Resource item)
{
	HT_Resource		nc, ch, parent;
	SBProvider		sb;

	if ((nc = newHTEntry(container->view, item)) != NULL)
	{
		ch = container->child;
		nc->view = container->view;
		nc->parent = container;
		nc->next = container->child;
		container->child = nc;
		nc->depth = container->depth + 1;
		++container->numChildren;

		nc->unsortedIndex = container->numChildren;
		refreshContainerIndexes(container);
	  
		parent = container;
		while (parent != NULL)
		{
			++(parent->numChildrenTotal);
			parent=parent->parent;
		}

		sb = SBProviderOfNode(nc);
		if (sb && sb->openp)
		{
			nc->flags |= HT_OPEN_FLAG;
		}
		else
		{
			nc->flags &= (~HT_OPEN_FLAG);
		}

		refreshItemList(nc, HT_EVENT_NODE_ADDED);
		if (gAutoEditNewNode == true)
		{
			gAutoEditNewNode = false;
			HT_SetSelection (nc);
			sendNotification(nc, HT_EVENT_NODE_EDIT);
		}
	}
	return(nc);
}



void
removeHTFromHash (HT_Pane pane, HT_Resource item)
{
	HT_Resource		htr, pr;
	PLHashTable		*hash;
  
	hash    = pane->hash;
	htr   = PL_HashTableLookup(hash, item->node);
	pr = htr;
	while (htr != NULL)
	{
		if (htr == item)
		{
			if (pr == htr)
			{
				if (htr->nextItem)
				{
					PL_HashTableAdd(hash, item->node,
						htr->nextItem);
				}
				else
				{
					PL_HashTableRemove(hash, item->node);
					RDF_ReleaseResource(pane->db, item->node);
				}
			}
			else
			{
				pr->nextItem = htr->nextItem;
			}
		}
	pr = htr;
	htr = htr->nextItem;
	}
}



void
deleteHTSubtree (HT_Resource subtree)
{
	HT_Resource		child, next;

	child = subtree->child;
	subtree->child = NULL;
	while (child)
	{
		next = child->next;
		if (HT_IsContainer(child))
		{
			deleteHTSubtree(child);
		}
		/* removeHTFromHash(child->view->pane, child); */
		deleteHTNode(child);
		child = next;
	}
}



void
deleteContainerItem (HT_Resource container, RDF_Resource item)
{
	HT_Resource		nc, pr, nx = NULL;

	nc = container->child;
	pr = nc;
	while (nc != NULL)
	{
		if (nc->node == item)
		{
			nx = nc;
			/* removeHTFromHash(container->view->pane, nx); */
			if (nc == pr)
			{
				container->child = nc->next;
			}
			else
			{
				pr->next = nc->next;
			}
			break;
		}
	pr = nc;
	nc = nc->next;
	}
	if (nx == NULL) return;
  
	if (HT_IsContainer(nx))
	{
		deleteHTSubtree(nx);
	}
	deleteHTNode(nx);

	if (gBatchUpdate != true)
	{
		refreshItemList(container, HT_EVENT_VIEW_REFRESH);
	}
}



char *
advertURLOfContainer (RDF r, RDF_Resource u)
{
	char			*url = NULL;

	url = RDF_GetSlotValue(r, u,  gNavCenter->RDF_HTMLURL,
				RDF_STRING_TYPE, 0, 1);
	return(url);
}



uint32
fillContainer (HT_Resource node)
{
	HT_Resource		pr = NULL, rn;
	RDF_Cursor		c;
	RDF_Resource		next;
	uint32			numElements=0;
	char			*advertURL;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->view != NULL);
	XP_ASSERT(node->view->pane != NULL);
	XP_ASSERT(node->view->pane->db != NULL);

#ifdef WIN32
	advertURL = RDF_GetSlotValue (node->view->pane->db, node->node, 
				       gNavCenter->RDF_HTMLURL, RDF_STRING_TYPE, 0, 1);
#endif

	if ((c = RDF_GetSources(node->view->pane->db, node->node, 
				gCoreVocab->RDF_parent,
				RDF_RESOURCE_TYPE,  true)) != NULL)
	{
		while (next = RDF_NextValue(c))
		{
			if ((rn = newHTEntry(node->view, next)) == NULL)
				break;
			rn->dataSource = RDF_ValueDataSource(c);
			rn->parent = node;
			rn->unsortedIndex = numElements++;
			rn->depth = node->depth+1;
			if (pr == NULL)
			{
				node->child = rn;
			}
			else
			{
				pr->next = rn;
			}
			pr = rn;
		}
#ifdef WIN32
		if (advertURL) XP_GetURLForView(node->view, advertURL);
#endif
		RDF_DisposeCursor(c);
	}
	return(numElements);
}



PR_PUBLIC_API(HT_Cursor)
HT_NewCursor (HT_Resource node)
{
	HT_Cursor		c;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->view != NULL);

	if ((node==NULL) || (!HT_IsContainer(node)))
	{
		return(NULL);
	}
	if (!HT_IsContainerOpen(node))
	{
		/* determine if container should be auto opened */

		if (node->view->pane->special == PR_TRUE)	return(NULL);

		if (node->flags & HT_INITED_FLAG)		return(NULL);
		node->flags |= HT_INITED_FLAG;
		if (RDF_HasAssertion(gNCDB, node->node, gNavCenter->RDF_AutoOpen,
			"yes", RDF_STRING_TYPE, 1))
		{
			node->flags |= HT_OPEN_FLAG;
		}
		else
		{
			return(NULL);
		}
	}

	if ((c = (HT_Cursor)getMem(sizeof(HT_CursorStruct))) != NULL)
	{
		c->container = node;
		if (node->child != NULL)
		{
			c->node = node->child;
			c->numElements = node->numChildren;
		}
		else
		{
			c->numElements = fillContainer(node);
			c->node = node->child;
		}
	}
	return(c);
}



PR_PUBLIC_API(HT_Cursor)
HT_NewColumnCursor (HT_View view)
{
	HT_Cursor		cursor;

	XP_ASSERT(view != NULL);

	if ((cursor = (HT_Cursor)getMem(sizeof(HT_CursorStruct))) != NULL)
	{
		cursor->columns = view->columns;
	}
	return(cursor);
}



PR_PUBLIC_API(PRBool)
HT_GetNextColumn(HT_Cursor cursor, char **colName, uint32 *colWidth,
		void **token, uint32 *tokenType)
{
	HT_Column		column;

	XP_ASSERT(cursor != NULL);

	if ((column = cursor->columns) == NULL)	return(false);

	if (colName != NULL)	*colName = column->name;
	if (colWidth != NULL)	*colWidth = column->width;
	if (token != NULL)	*token = column->token;
	if (tokenType != NULL)	*tokenType = column->tokenType;

	cursor->columns = column->next;
	return(true);
}



PR_PUBLIC_API(void)
HT_DeleteColumnCursor(HT_Cursor cursor)
{
	XP_ASSERT(cursor != NULL);

	freeMem(cursor);
}



PR_PUBLIC_API(void)
HT_SetColumnOrder(HT_View view, void *srcColToken, void *destColToken, PRBool afterDestFlag)
{
	HT_Column	*columnList, *srcCol = NULL, *destCol = NULL;

	XP_ASSERT(view != NULL);
	XP_ASSERT(srcColToken != NULL);
	XP_ASSERT(destColToken != NULL);

	if (columnList = &(view->columns))
	{
		while ((*columnList) != NULL)
		{
			if ((*columnList)->token == srcColToken)
			{
				srcCol = columnList;
			}
			else if ((*columnList)->token == destColToken)
			{
				destCol = columnList;
			}
			columnList = &((*columnList)->next);
		}
		if ((srcCol != NULL) && (destCol != NULL))
		{
			*srcCol = (*srcCol)->next;
			if (afterDestFlag == TRUE)
			{
				(*srcCol)->next = (*destCol)->next;
				(*destCol)->next = *srcCol;
			}
			else
			{
				(*srcCol)->next = *destCol;
				*destCol = *srcCol;
			}
		}
	}
}



PR_PUBLIC_API(void)
HT_SetSortColumn(HT_View view, void *token, uint32 tokenType, PRBool descendingFlag)
{
	XP_ASSERT(view != NULL);

	view->sortToken = token;
	view->sortTokenType = tokenType;
	view->descendingFlag = descendingFlag;
	refreshItemList(view->top, HT_EVENT_VIEW_SORTING_CHANGED);
}



PR_PUBLIC_API(PRBool)
HT_ContainerSupportsNaturalOrderSort(HT_Resource container)
{
	HT_Resource	parent;
	PRBool		naturalOrder = PR_FALSE;

	if (container != NULL && HT_IsContainer(container))
	{
		if (container->node == gNavCenter->RDF_BookmarkFolderCategory)
		{
			naturalOrder = PR_TRUE;
		}
		else
		{
			parent = container->parent;
			if (parent != NULL)
			{
				naturalOrder = nlocalStoreHasAssertion(gLocalStore, container->node,
					gCoreVocab->RDF_parent, parent->node, RDF_RESOURCE_TYPE, 1);
			}
		}
	}
	return (naturalOrder);
}



PR_PUBLIC_API(HT_Error)
HT_DeleteCursor (HT_Cursor cursor)
{
	XP_ASSERT(cursor != NULL);

	if (cursor != NULL)
	{
		freeMem(cursor);
	}
	return(HT_NoErr);
}



PR_PUBLIC_API(HT_Resource)
HT_GetNextItem (HT_Cursor cursor)
{
	HT_Resource		ans = NULL;

	XP_ASSERT(cursor != NULL);

	if (cursor != NULL)
	{
		if ((ans = cursor->node) != NULL)
		{
			cursor->node = ans->next;
		}
	}
	return(ans);
}



HT_MenuCmd
menus[] = {
	HT_CMD_OPEN_NEW_WIN,
	HT_CMD_OPEN_COMPOSER,
	HT_CMD_SEPARATOR,

	HT_CMD_OPEN,
	HT_CMD_OPEN_AS_WORKSPACE,
	HT_CMD_OPEN_FILE,
	HT_CMD_PRINT_FILE,
	HT_CMD_REVEAL_FILEFOLDER,
	HT_CMD_SEPARATOR,

	HT_CMD_NEW_BOOKMARK,
	HT_CMD_NEW_FOLDER,
	HT_CMD_NEW_SEPARATOR,
	HT_CMD_MAKE_ALIAS,
	HT_CMD_ADD_TO_BOOKMARKS,
	HT_CMD_SAVE_AS,	
	HT_CMD_CREATE_SHORTCUT,	
	HT_CMD_EXPORT,
	HT_CMD_EXPORTALL,
	HT_CMD_RENAME,

#ifdef	HT_PASSWORD_RTNS
	HT_CMD_SET_PASSWORD,
	HT_CMD_REMOVE_PASSWORD,
#endif

	HT_CMD_SEPARATOR,

	HT_CMD_SET_TOOLBAR_FOLDER,
	HT_CMD_SET_BOOKMARK_MENU,
	HT_CMD_REMOVE_BOOKMARK_MENU,
	HT_CMD_SET_BOOKMARK_FOLDER,
	HT_CMD_REMOVE_BOOKMARK_FOLDER,
	HT_CMD_SEPARATOR,

	HT_CMD_CUT,
	HT_CMD_COPY,
	HT_CMD_PASTE,
	HT_CMD_UNDO,
	HT_CMD_DELETE_FILE,
	HT_CMD_DELETE_FOLDER,
	HT_CMD_SEPARATOR,

	HT_CMD_REFRESH,	
	HT_CMD_PROPERTIES,
	HT_CMD_SEPARATOR,

	HT_CMD_NEW_WORKSPACE,
	HT_CMD_RENAME_WORKSPACE,
	HT_CMD_DELETE_WORKSPACE,
	HT_CMD_MOVE_WORKSPACE_UP,
	HT_CMD_MOVE_WORKSPACE_DOWN,

	HT_CMD_SEPARATOR,
	/* commands from the graph appear at the end */
	-1,
};



/* obsolete/old; use HT_NewContextualMenuCursor instead */

PR_PUBLIC_API(HT_Cursor)
HT_NewContextMenuCursor (HT_Resource node)
{
	HT_Cursor		cursor;

	XP_ASSERT(node != NULL);

	if ((cursor = (HT_Cursor)getMem(sizeof(HT_CursorStruct))) != NULL)
	{
		cursor->node = node;
		cursor->contextMenuIndex = 0;
		cursor->commandExtensions = PR_FALSE;
		if (node != NULL)
		{
			cursor->isWorkspaceFlag = ((node->parent == NULL) ? true:false);
		}
		else
		{
			cursor->isWorkspaceFlag = false;
		}
	}
	return(cursor);
}



PR_PUBLIC_API(HT_Cursor)
HT_NewContextualMenuCursor (HT_View view, PRBool workspaceMenuCmds, PRBool backgroundMenuCmds)
{
	HT_Cursor		cursor;

	if ((cursor = (HT_Cursor)getMem(sizeof(HT_CursorStruct))) != NULL)
	{
		cursor->node = (view != NULL) ? view->top : NULL;
		cursor->contextMenuIndex = 0;
		cursor->isWorkspaceFlag = workspaceMenuCmds;
		cursor->isBackgroundFlag = backgroundMenuCmds;
		cursor->commandExtensions = PR_FALSE;
	}
	return(cursor);
}



PRBool
htIsMenuCmdEnabled(HT_Pane pane, HT_MenuCmd menuCmd,
		PRBool isWorkspaceFlag, PRBool isBackgroundFlag)
{
	HT_Resource		node, nextNode;
	HT_View			*viewList;
	PRBool			multSelection = false, enableFlag = false;
	XP_Bool			mcEnabled = false;
	RDF_BT			type;
	RDF_Resource		bmFolder, nbFolder, ptFolder, rNode;
	int16			menuIndex;

	if (menuCmd == HT_CMD_SEPARATOR)	return(false);

	menuIndex=0;
	while (menus[menuIndex] >=0)
	{
		if (menus[menuIndex] == menuCmd)
		{
			break;
		}
		++menuIndex;
	}
	/*
	if (menus[menuIndex] < 0)	return(false);
	*/

	if (isWorkspaceFlag == true || isBackgroundFlag == true)
	{
		node = NULL;
		nextNode = NULL;
	}
	else
	{
		if ((node = HT_GetNextSelection(pane->selectedView, NULL)) == NULL)
		{
			return(false);
		}
		if ((nextNode = HT_GetNextSelection(pane->selectedView, node)) != NULL)
		{
			multSelection = true;
		}
	}

	do
	{
		if (node != NULL)
		{
			if ((rNode = node->node) == NULL)	return(false);

			if (HT_IsContainer(node))
			{
				type = resourceType(rNode);
			}
			else
			{
				type = resourceType(node->parent->node);
			}
		}

		switch(menus[menuIndex])
		{
			case	HT_CMD_OPEN:
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (node->view->pane->personaltoolbar == true)
								return(false);
			break;

			case	HT_CMD_OPEN_FILE:
			if (node == NULL)			return(false);
			if (HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != LFS_RT)	return(false);
			break;

			case	HT_CMD_PRINT_FILE:
			if (node == NULL)			return(false);
			if (HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != LFS_RT)	return(false);
			break;

			case	HT_CMD_OPEN_NEW_WIN:
			if (node == NULL)			return(false);
			if (HT_IsContainer(node))		return(false);
			if (HT_IsSeparator(node))		return(false);
			if (resourceType(node->node) == LFS_RT)	return(false);
			break;

			case	HT_CMD_OPEN_COMPOSER:
#ifdef EDITOR
			if (node == NULL)			return(false);
			if (HT_IsContainer(node))		return(false);
			if (HT_IsSeparator(node))		return(false);
			if (resourceType(node->node) == LFS_RT)	return(false);

			/*
				XXX to do
			
				Composer needs to expose a routine similar
				to FE_MakeNewWindow() for opening a URL in
				an editor window
			*/
			return(false);
#else
			return(false);
#endif /* EDITOR */
			break;

			case	HT_CMD_NEW_WORKSPACE:
			if (!isWorkspaceFlag)			return(false);
			break;

			case	HT_CMD_OPEN_AS_WORKSPACE:
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (htIsOpLocked(node, gNavCenter->RDF_CopyLock))
								return(false);
			break;

			case	HT_CMD_NEW_BOOKMARK:
			if (node == NULL)
			{
				if (pane == NULL)		return(false);
				node = HT_TopNode(pane->selectedView);
			}
			else
			{
				if (!HT_IsLocalData(node))	return(false);
			}
			if (node == NULL)			return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if (multSelection == true)		return(false);
			if (!HT_IsContainer(node))
			{
				if (node->view->pane->personaltoolbar == true)
								return(false);
			}
			if (htIsOpLocked(node, gNavCenter->RDF_AddLock))
								return(false);
			break;

			case	HT_CMD_NEW_FOLDER:
			if (node == NULL)
			{
				if (pane == NULL)		return(false);
				node = HT_TopNode(pane->selectedView);
			}
			else
			{
				if (!HT_IsLocalData(node))	return(false);
			}
			if (node == NULL)			return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if (multSelection == true)		return(false);
			if (!HT_IsContainer(node))
			{
				if (node->view->pane->personaltoolbar == true)
								return(false);
			}
			if (htIsOpLocked(node, gNavCenter->RDF_AddLock))
								return(false);
			break;

			case	HT_CMD_NEW_SEPARATOR:
			if (node == NULL)
			{
				if (pane == NULL)		return(false);
				node = HT_TopNode(pane->selectedView);
			}
			else
			{
				if (!HT_IsLocalData(node))	return(false);
			}
			if (node == NULL)			return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if (multSelection == true)		return(false);
			if (!HT_IsContainer(node))
			{
				if (node->view->pane->personaltoolbar == true)
								return(false);
			}
			if (htIsOpLocked(node, gNavCenter->RDF_AddLock))
								return(false);
			break;

			case	HT_CMD_MAKE_ALIAS:
			if (node == NULL)			return(false);
			if (resourceType(node->node) != LFS_RT)	return(false);
			break;

			case	HT_CMD_ADD_TO_BOOKMARKS:
			if (node == NULL)			return(false);
			if (multSelection == true)		return(false);
			if ((bmFolder = RDFUtil_GetQuickFileFolder()) == NULL)
								return(false);
			if (HT_IsContainer(node))
			{
				if (node->node == bmFolder)	return(false);
			}
			else
			{
				if (node->parent->node == bmFolder)
								return(false);
			}
			if (htIsOpLocked(node, gNavCenter->RDF_CopyLock))
								return(false);
			break;

			case	HT_CMD_SAVE_AS:
			if (node == NULL)			return(false);
			if (HT_IsContainer(node))		return(false);
			if (HT_IsSeparator(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			break;

			case	HT_CMD_CREATE_SHORTCUT:
			if (node == NULL)			return(false);
			if (HT_IsContainer(node))		return(false);
			if (HT_IsSeparator(node))		return(false);
			break;

			case	HT_CMD_SET_TOOLBAR_FOLDER:
			if (multSelection == true)		return(false);
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if ((ptFolder = RDFUtil_GetPTFolder()) == NULL)
								return(false);
			if (node->node == ptFolder)		return(false);
			if (node->view->pane->personaltoolbar == true)
								return(false);
			if (!HT_IsLocalData(node))		return(false);
			break;

			case	HT_CMD_SET_BOOKMARK_MENU:
			if (multSelection == true)		return(false);
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if ((bmFolder = RDFUtil_GetQuickFileFolder()) == NULL)
								return(false);
			if (node->node == bmFolder)		return(false);
			if (!HT_IsLocalData(node))		return(false);
			break;

			case	HT_CMD_REMOVE_BOOKMARK_MENU:
			if (multSelection == true)		return(false);
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if ((bmFolder = RDFUtil_GetQuickFileFolder()) == NULL)
								return(false);
			if (node->node != bmFolder)		return(false);
			break;

			case	HT_CMD_SET_BOOKMARK_FOLDER:
			if (multSelection == true)		return(false);
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if ((nbFolder = RDFUtil_GetNewBookmarkFolder()) == NULL)
								return(false);
			if (node->node == nbFolder)		return(false);
			if (!HT_IsLocalData(node))		return(false);
			break;

			case	HT_CMD_REMOVE_BOOKMARK_FOLDER:
			if (multSelection == true)		return(false);
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if ((nbFolder = RDFUtil_GetNewBookmarkFolder()) == NULL)
								return(false);
			if (node->node != nbFolder)		return(false);
			break;

			case	HT_CMD_CUT:
			if (node == NULL)			return(false);
			if (node->parent == NULL)		return(false);
			if (node->parent->node == NULL)		return(false);
			if ((resourceType(node->parent->node) != RDF_RT) &&
				(resourceType(node->parent->node) != HISTORY_RT))
								return(false);
			if (HT_IsContainer(node))
			{
				if (node->node == gNavCenter->RDF_HistoryBySite ||
				    node->node == gNavCenter->RDF_HistoryByDate)
				    				return(false);
				if ((ptFolder = RDFUtil_GetPTFolder()) != NULL)
				{
					if (node->node == ptFolder)
								return(false);
				}
				if ((bmFolder = RDFUtil_GetNewBookmarkFolder()) != NULL)
				{
					if (node->node == bmFolder)
								return(false);
				}
			}
			if (htIsOpLocked(node, gNavCenter->RDF_DeleteLock))
								return(false);
			break;

			case	HT_CMD_COPY:
			if (node == NULL)			return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if (htIsOpLocked(node, gNavCenter->RDF_CopyLock))
								return(false);
			break;

			case	HT_CMD_PASTE:
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) != RDF_RT)	return(false);
			if (htIsOpLocked(node, gNavCenter->RDF_AddLock))
								return(false);
			break;

			case	HT_CMD_UNDO:
			return(false);
			break;

			case	HT_CMD_RENAME:
			if (node == NULL)			return(false);
			if (isWorkspaceFlag == true)		return(false);
			if (multSelection == true)		return(false);
			if (!HT_IsContainer(node))
			{
				if (resourceType(node->parent->node) != RDF_RT)
								return(false);
			}
			else
			{
				if (resourceType(node->node) != RDF_RT)
								return(false);
			}
			if (HT_IsSeparator(node))		return(false);
			if (htIsOpLocked(node, gNavCenter->RDF_NameLock))
								return(false);
			break;

			case	HT_CMD_DELETE_FILE:
			if (node == NULL)			return(false);
			type = resourceType(node->parent->node);
			if (type != LFS_RT && type != ES_RT && type != FTP_RT)
								return(false);
			if (HT_IsContainer(node))		return(false);
			break;

			case	HT_CMD_DELETE_FOLDER:
			if (node == NULL)			return(false);
			type = resourceType(node->node);
			if (type != LFS_RT && type != ES_RT && type != FTP_RT)
								return(false);
			if (!HT_IsContainer(node))		return(false);
			break;

			case	HT_CMD_REVEAL_FILEFOLDER:
			if (node == NULL)			return(false);
#ifdef	XP_MAC
			if (resourceType(node->node) != LFS_RT)	return(false);
#else
			return(false);
#endif
			break;

			case	HT_CMD_PROPERTIES:
			if (multSelection == true)		return(false);
			if (node == NULL)
			{
				if (pane == NULL)		return(false);
				node = HT_TopNode(pane->selectedView);
			}
			if (node != NULL)
			{
				if (HT_IsSeparator(node))	return(false);
				if (HT_IsContainer(node))
				{
					if (type == HISTORY_RT)	return(false);
				}
			}
			break;

			case	HT_CMD_RENAME_WORKSPACE:
			if (!isWorkspaceFlag)			return(false);
			if (isBackgroundFlag == true)		return(false);
			if (pane == NULL)			return(false);
			if (pane->selectedView == NULL)		return(false);
			if (htIsOpLocked(pane->selectedView->top, gNavCenter->RDF_NameLock))
								return(false);
			break;

			case	HT_CMD_DELETE_WORKSPACE:
			if (!isWorkspaceFlag)			return(false);
			if (isBackgroundFlag == true)		return(false);
			if (pane == NULL)			return(false);
			if (pane->selectedView == NULL)		return(false);
			rNode = pane->selectedView->top->node;
			if (rNode == NULL)			return(false);
			if (rNode == gNavCenter->RDF_BookmarkFolderCategory ||
			    rNode == gNavCenter->RDF_LocalFiles ||
			    rNode == gNavCenter->RDF_History)
								return(false);
			if (htIsOpLocked(pane->selectedView->top, gNavCenter->RDF_DeleteLock))
								return(false);
			break;

			case	HT_CMD_MOVE_WORKSPACE_UP:
			if (isWorkspaceFlag == false)		return(false);
			if (isBackgroundFlag == true)		return(false);
			if (pane == NULL)			return(false);
			if (pane->selectedView == NULL)		return(false);
			if (pane->selectedView == pane->viewList)
								return(false);
			if (htIsOpLocked(pane->selectedView->top, gNavCenter->RDF_WorkspacePosLock))
								return(false);
			viewList = &(pane->viewList);
			while ((*viewList) != NULL)
			{
				if ((*viewList)->next != NULL)
				{
					if ((*viewList)->next == pane->selectedView)
					{
						if (htIsOpLocked((*viewList)->top,
							gNavCenter->RDF_WorkspacePosLock))
						{
							return(false);
						}
					}
				}
				viewList = &((*viewList)->next);
			}
			break;

			case	HT_CMD_MOVE_WORKSPACE_DOWN:
			if (isWorkspaceFlag == false)		return(false);
			if (isBackgroundFlag == true)		return(false);
			if (pane == NULL)			return(false);
			if (pane->selectedView == NULL)		return(false);
			if (pane->selectedView->next == NULL)	return(false);
			if (htIsOpLocked(pane->selectedView->top, gNavCenter->RDF_WorkspacePosLock))
								return(false);
			break;

			case	HT_CMD_REFRESH:
			if (node == NULL)
			{
				if (pane == NULL)		return(false);
				node = HT_TopNode(pane->selectedView);
			}
			if (node != NULL)
			{
				if (!HT_IsContainer(node))	return(false);
			}
			break;

			case	HT_CMD_EXPORT:
			if (isBackgroundFlag == true)		return(false);
			if (node == NULL)
			{
				if (pane == NULL)		return(false);
				if (pane->selectedView == NULL)	return(false);
				node = HT_TopNode(pane->selectedView);
			}
			if (node == NULL)			return(false);
			if (!HT_IsContainer(node))		return(false);
			if (resourceType(node->node) == LFS_RT)	return(false);
			if (htIsOpLocked(node, gNavCenter->RDF_CopyLock))
								return(false);
			break;

			case	HT_CMD_EXPORTALL:
			if (isWorkspaceFlag == false)			return(false);
			if (isBackgroundFlag == true)			return(false);
			if (!gMissionControlEnabled)			return(false);
			break;

#ifdef	HT_PASSWORD_RTNS

			case	HT_CMD_SET_PASSWORD:
			if (multSelection == true)			return(false);
			if (node == NULL)
			{
				if (pane == NULL)			return(false);
				node = HT_TopNode(pane->selectedView);
			}
			if (node != NULL)
			{
				if (!HT_IsContainer(node))		return(false);
			}
			break;

			case	HT_CMD_REMOVE_PASSWORD:
			if (multSelection == true)			return(false);
			if (node == NULL)
			{
				if (pane == NULL)			return(false);
				node = HT_TopNode(pane->selectedView);
			}
			if (node != NULL)
			{
				if (!HT_IsContainer(node))		return(false);
				if (ht_hasPassword(node) == PR_FALSE)	return(false);
			}
			break;

#endif

			default:
			/* process commands from the RDF graph last */
			return(true);
			break;
		}

		if (node != NULL)
		{
			
			node = nextNode;
			if (node == NULL)	break;
			if (pane == NULL)
			{
				node = NULL;
				break;
			}
			nextNode = HT_GetNextSelection(pane->selectedView, node);
		}
	} while (node != NULL);
	return(true);
}



PR_PUBLIC_API(PRBool)
HT_IsMenuCmdEnabled(HT_Pane pane, HT_MenuCmd menuCmd)
{
	return(htIsMenuCmdEnabled(pane, menuCmd, false, false));
}



PR_PUBLIC_API(PRBool)
HT_NextContextMenuItem (HT_Cursor cursor, HT_MenuCmd *menuCmd)
{
	HT_MenuCommand		*menuList;
	HT_Resource		node;
	RDF_Resource		graphCommand;
	PRBool			sepFlag = false, enableFlag = false;
	int16			menuIndex;

	XP_ASSERT(cursor != NULL);
	XP_ASSERT(menuCmd != NULL);

	if (cursor->commandExtensions == PR_FALSE)
	{
		/* process internal, well-known commands first */

		menuIndex = cursor->contextMenuIndex;
		while (menus[menuIndex] >= 0)
		{
			if (menus[menuIndex] == HT_CMD_SEPARATOR)
			{
				if (cursor->foundValidMenuItem == true)
				{
					sepFlag = true; 
				}
				++menuIndex;
				continue;
			}

			node = cursor->node;
			enableFlag = htIsMenuCmdEnabled(
					(node != NULL) ? node->view->pane : NULL,
					menus[menuIndex],
					cursor->isWorkspaceFlag,
					cursor->isBackgroundFlag);
			if (enableFlag == false)
			{
				++menuIndex;
				continue;
			}

			if (sepFlag == true)
			{
				*menuCmd = HT_CMD_SEPARATOR;
				cursor->contextMenuIndex = menuIndex;
			}
			else
			{
				*menuCmd = menus[menuIndex];
				cursor->contextMenuIndex = ++menuIndex;
				cursor->foundValidMenuItem = true;
			}
			break;
		}
		if ((menus[menuIndex] == -1) && (node != NULL))
		{
			cursor->commandExtensions = PR_TRUE;
			cursor->container = NULL;
			cursor->foundValidMenuItem = false;
			cursor->menuCmd = NUM_MENU_CMDS+1;

			/* process commands from the RDF graph last */
			freeMenuCommandList();
			while (TRUE)
			{
				if (cursor->cursor == NULL)
				{
					if ((cursor->container = HT_GetNextSelection(cursor->node->view, cursor->container)) != NULL)
					{
						cursor->cursor = RDF_GetSources(gNCDB, cursor->container->node,
							gNavCenter->RDF_Command, RDF_RESOURCE_TYPE, 1);
					}
					else
					{
						cursor->menuCommandList = menuCommandsList;
						break;
					}
				}
				if (cursor->cursor == NULL)	break;
				if ((graphCommand = RDF_NextValue(cursor->cursor)) != NULL)
				{
					/* add graph command at end of list, and only once */

					menuList = &menuCommandsList;
					while ((*menuList) != NULL)
					{
						if ((*menuList)->graphCommand == graphCommand)	break;
						menuList = &((*menuList)->next);
					}
					if ((*menuList) == NULL)
					{
						if (((*menuList) = (HT_MenuCommand)getMem(sizeof(_HT_MenuCommandStruct))) != NULL)
						{
							cursor->foundValidMenuItem = true;
							(*menuList)->menuCmd = cursor->menuCmd++;
							(*menuList)->graphCommand = graphCommand;
							(*menuList)->name = RDF_GetSlotValue(gNCDB, graphCommand,
									gCoreVocab->RDF_name, RDF_STRING_TYPE, false, true);
						}
					}
				}
				else
				{
					RDF_DisposeCursor(cursor->cursor);
					cursor->cursor = NULL;
				}
			}

			if (cursor->foundValidMenuItem == true)
			{
				cursor->foundValidMenuItem = false;
				*menuCmd = HT_CMD_SEPARATOR;
				enableFlag = true;
			}

		}
	}
	else
	{
		if (cursor->menuCommandList != NULL)
		{
			*menuCmd = cursor->menuCommandList->menuCmd;
			cursor->menuCommandList = cursor->menuCommandList->next;
			enableFlag = true;
		}

	}
	return(enableFlag);
}



PR_PUBLIC_API(void)
HT_DeleteContextMenuCursor(HT_Cursor cursor)
{
	XP_ASSERT(cursor != NULL);

	if (cursor != NULL)
	{
		freeMem(cursor);
	}
}



void
freeMenuCommandList()
{
	HT_MenuCommand		menuCommand;

	menuCommand = menuCommandsList;
	while (menuCommand != NULL)
	{
		if (menuCommand->name != NULL)	freeMem(menuCommand->name);
		menuCommand = menuCommand->next;
	}
	menuCommandsList = NULL;
}



PR_PUBLIC_API(char *)
HT_GetMenuCmdName(HT_MenuCmd menuCmd)
{
	HT_MenuCommand		menuCommand;
	char			*menuName = NULL;

	if (menuCmd < NUM_MENU_CMDS)
	{
		menuName = XP_GetString(RDF_CMD_0 + menuCmd);
	}
	else
	{
		menuCommand = menuCommandsList;
		while (menuCommand != NULL)
		{
			if (menuCommand->menuCmd == menuCmd)
			{
				menuName = menuCommand->name;
				break;
			}
			menuCommand = menuCommand->next;
		}
	}
	return(menuName);
}



void
exportCallbackWrite(PRFileDesc *fp, char *str)
{
	XP_ASSERT(fp != NULL);

	if (str != NULL)
	{
		PR_Write(fp, str, strlen(str));
	}
}



void
exportCallback(MWContext *context, char *filename, RDF_Resource node)
{
	PRFileDesc	*fp;
	char		*fileURL;

	XP_ASSERT(node != NULL);

	if (filename != NULL)
	{

#ifdef	XP_WIN
		fileURL = append2Strings("file:///", filename);
#else
		fileURL = append2Strings("file://", filename);
#endif

		if (fileURL != NULL)
		{
			if ((fp = CallPROpenUsingFileURL(fileURL,
				(PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE),
				0644)) != NULL)
			{
				outputRDFTree(gNCDB, fp, node);
				PR_Close(fp);
			}
			freeMem(fileURL);
		}
	}
}



PR_PUBLIC_API(HT_Error)
HT_DoMenuCmd(HT_Pane pane, HT_MenuCmd menuCmd)
{
	Chrome			chrome;
	HT_Error		error = 0;
	HT_MenuCommand		menuCommand;
	HT_Resource		node, topNode;
	HT_View			view, *viewList;
	RDF_Resource		rNode, newTopNode=NULL;
	URL_Struct		*urls;
	PRBool			needRefresh = false, openState, emptyClipboard = PR_TRUE, doAbout = PR_FALSE;
	PRBool			onlyUpdateBookmarks = false, onlyUpdatePersonalToolbar = false;
	uint32			uniqueCount=0;
	char			*title, *url;

#ifdef	HT_PASSWORD_RTNS
	char		*password1, *password2;
#endif

	XP_ASSERT(pane != NULL);
	view = HT_GetSelectedView(pane);
	XP_ASSERT(view != NULL);

	gBatchUpdate = true;
	switch(menuCmd)
	{
		case	HT_CMD_NEW_WORKSPACE:
		uniqueCount = 0;
		do
		{
			url = PR_smprintf("container%d.rdf",
				(int)++uniqueCount);
			rNode = RDF_GetResource(pane->db, url, false);
			if ((rNode != NULL) && (url != NULL))
			{
				XP_FREE(url);
				url = NULL;
			}
		} while (rNode != NULL);
		if ((url != NULL) && (title = FE_Prompt(((MWContext *)gRDFMWContext()),
				XP_GetString(RDF_NEWWORKSPACEPROMPT), "")) != NULL)
		{
			if (!strcmp(title, "about"))
			{
				XP_FREE(title);
				title = NULL;
				XP_FREE(url);
				url = PR_smprintf("http://people.netscape.com/rjc/about.rdf#root");
			}
			HT_NewWorkspace(pane, url, title);
		}
		if (title != NULL)
		{
			XP_FREE(title);
			title = NULL;
		}
		if (url != NULL)
		{
			XP_FREE(url);
			url = NULL;
		}
		break;

		case	HT_CMD_RENAME_WORKSPACE:
		if (view == NULL)	break;
		if ((topNode = HT_TopNode(view)) == NULL)	break;
		sendNotification(topNode, HT_EVENT_WORKSPACE_EDIT);
		break;

		case	HT_CMD_DELETE_WORKSPACE:
		if (view == NULL)	break;
		if ((topNode = HT_TopNode(view)) == NULL)	break;
		if (FE_Confirm(((MWContext *)gRDFMWContext()),
				PR_smprintf(XP_GetString(RDF_DELETEWORKSPACE),
				HT_GetViewName(topNode->view))))
		{
			RDF_AssertFalse(view->pane->db, topNode->node, gCoreVocab->RDF_parent,
				gNavCenter->RDF_Top, RDF_RESOURCE_TYPE);
		}
		break;

		case	HT_CMD_MOVE_WORKSPACE_UP:
		viewList = &(pane->viewList);
		while (*viewList)
		{
			if ((*viewList)->next == view)
			{
				HT_SetWorkspaceOrder(view, *viewList, FALSE);
				break;
			}
			viewList = &((*viewList)->next);
		}
		break;

		case	HT_CMD_MOVE_WORKSPACE_DOWN:
		if (view->next != NULL)
		{
			HT_SetWorkspaceOrder(view, view->next, TRUE);
		}
		break;

		default:
		node = HT_GetNextSelection(view, NULL);
		do
		{
			switch(menuCmd)
			{
				case	HT_CMD_OPEN:
				if (node == NULL)	break;
				HT_GetOpenState(node, &openState);
				error = HT_SetOpenState(node, !openState);
				break;

				case	HT_CMD_OPEN_NEW_WIN:
				if (node == NULL)	break;

				XP_MEMSET(&chrome, 0, sizeof(Chrome));
				chrome.show_button_bar = TRUE;
				chrome.show_url_bar = TRUE;
				chrome.show_directory_buttons = TRUE;
				chrome.show_bottom_status_bar = TRUE;
				chrome.show_menu = TRUE;
				chrome.show_security_bar = TRUE;
				chrome.allow_close = TRUE;
				chrome.allow_resize = TRUE;
				chrome.show_scrollbar = TRUE;

				urls = NET_CreateURLStruct(resourceID(node->node), NET_DONT_RELOAD);
				FE_MakeNewWindow((MWContext *)gRDFMWContext(), urls, NULL, &chrome);
				break;

				case	HT_CMD_OPEN_COMPOSER:
#ifdef EDITOR
				/*
					XXX to do
				
					Composer needs to expose a routine similar
					to FE_MakeNewWindow() for opening a URL in
					an editor window
				*/
#endif EDITOR
				break;

				case	HT_CMD_NEW_BOOKMARK:
				case	HT_CMD_NEW_FOLDER:
				case	HT_CMD_NEW_SEPARATOR:
				uniqueCount = 0;
				if (menuCmd == HT_CMD_NEW_BOOKMARK)
				{
					if ((url = FE_Prompt(((MWContext *)gRDFMWContext()),
							"URL:", "http://")) == NULL)	break;

					if (!strcmp(url, "about"))
					{
						XP_FREE(url);
						url = PR_smprintf("http://people.netscape.com/rjc/about.rdf#root");
						doAbout = PR_TRUE;
						menuCmd = HT_CMD_NEW_FOLDER;
					}
					/* for the moment, only allow direct addition of http URLs */
/*
					if (!startsWith("http://", url))
					{
						XP_FREE(url);
						url = NULL;
						break;
					}
*/
					gAutoEditNewNode = true;
				}
				else
				do
				{
					if (menuCmd == HT_CMD_NEW_FOLDER)
					{
						url = PR_smprintf("container%d.rdf",
							(int)++uniqueCount);
						gAutoEditNewNode = true;
					}
					else if (menuCmd == HT_CMD_NEW_SEPARATOR)
					{
						url = PR_smprintf("separator%d",
							(int)++uniqueCount);
					}
					rNode = RDF_GetResource(pane->db, url, false);
					if (rNode != NULL)
					{
						if (url != NULL)
						{
							XP_FREE(url);
							url = NULL;
						}
					}
				} while (rNode != NULL);

				if ((rNode = RDF_GetResource(pane->db, url, true)) != NULL)
				{
					setResourceType (rNode, RDF_RT);
					if (menuCmd == HT_CMD_NEW_BOOKMARK)
					{
						RDF_Assert(pane->db, rNode,
							gCoreVocab->RDF_name,
							XP_GetString(RDF_DATA_1), RDF_STRING_TYPE);
					}
					else if (menuCmd == HT_CMD_NEW_FOLDER)
					{
						setContainerp(rNode, true);
						if (doAbout == PR_TRUE)
						{
							gAutoEditNewNode = false;
						}
						else
						{
							RDF_Assert(pane->db, rNode,
								gCoreVocab->RDF_name,
								XP_GetString(RDF_DATA_2), RDF_STRING_TYPE);
						}
					}
					if (node != NULL)
					{
						if (HT_IsContainer(node))
						{
							RDF_Assert(pane->db, rNode,
								gCoreVocab->RDF_parent,
								node->node,
								RDF_RESOURCE_TYPE);
							error = HT_SetOpenState(node, TRUE);
						}
						else
						{
							RDF_Assert(pane->db, rNode,
								gCoreVocab->RDF_parent,
								node->parent->node,
								RDF_RESOURCE_TYPE);
						}
					}
					else
					{
						RDF_Assert(pane->db, rNode,
							gCoreVocab->RDF_parent,
							HT_TopNode(view)->node,
							RDF_RESOURCE_TYPE);
					}

					node = NULL;
				}
				if (url != NULL)
				{
					XP_FREE(url);
					url = NULL;
				}
				break;

				case	HT_CMD_ADD_TO_BOOKMARKS:
				if (node == NULL)	break;
				HT_AddBookmark (resourceID(node->node), HT_GetNodeName(node));
				break;

				case	HT_CMD_OPEN_AS_WORKSPACE:
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
#ifdef	HT_PASSWORD_RTNS
					if (ht_hasPassword(node) == PR_TRUE)
					{
						if (ht_checkPassword(node, false) == PR_FALSE)	break;
					}
#endif
				}
				HT_NewWorkspace(pane, resourceID(node->node), NULL);
				error = HT_NoErr;
				break;

				case	HT_CMD_PROPERTIES:
				if (node == NULL)
				{
					node = HT_TopNode(view);
				}
				if (!HT_IsSeparator(node))
				{
#ifdef	HT_PASSWORD_RTNS
					if (HT_IsContainer(node))
					{
						if (ht_hasPassword(node) == PR_TRUE)
						{
							if (ht_checkPassword(node, false) == PR_FALSE)	break;
						}
					}
#endif
					HT_Properties(node);
				}
				error = HT_NoErr;
				break;

				case	HT_CMD_REFRESH:
				if (node == NULL)
				{
					node = HT_TopNode(view);
				}
				if (HT_IsContainer(node))
				{
					destroyViewInt(node, PR_TRUE);
					RDF_Assert(node->view->pane->db, node->node,
						gNavCenter->RDF_Command, gNavCenter->RDF_Command_Refresh,
						RDF_RESOURCE_TYPE);
					refreshItemList (node, HT_EVENT_VIEW_REFRESH);
				}
				break;

				case	HT_CMD_CUT:
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
#ifdef	HT_PASSWORD_RTNS
					if (ht_hasPassword(node) == PR_TRUE)
					{
						if (ht_checkPassword(node, false) == PR_FALSE)	break;
					}
#endif
				}
				if (node->parent)
				{
					htRemoveChild(node->parent, node, true);
					needRefresh = true;
				}
				break;

				case	HT_CMD_COPY:
				if (node == NULL)	break;
				htCopyReference(node->node, gNavCenter->RDF_Clipboard, emptyClipboard);
				emptyClipboard = PR_FALSE;
				break;

				case	HT_CMD_PASTE:
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
					htCopyReference(gNavCenter->RDF_Clipboard, node->node, PR_FALSE);
				}
				break;

				case	HT_CMD_UNDO:
				break;

				case	HT_CMD_RENAME:
				if (node == NULL)	break;
				sendNotification(node, HT_EVENT_NODE_EDIT);
				break;

			        case   HT_CMD_PRINT_FILE:
				if (node == NULL)	break;
#ifdef WIN32
				if ((resourceType(node->parent->node) == LFS_RT) &&
					(!HT_IsContainer(node)))
				{
					FE_Print(resourceID(node->node));
				}
#endif
				break;

				case	HT_CMD_DELETE_FILE:
				if (node == NULL)		break;
				if (HT_IsContainer(node))	break;
				if (htRemoveChild(node->parent, node, false))
				{
					node = NULL;
					break;
				}
				needRefresh = true;
				break;

				case	HT_CMD_DELETE_FOLDER:
				if (node == NULL)		break;
				if (!HT_IsContainer(node))	break;
				if (htRemoveChild(node->parent, node, false))
				{
					node = NULL;
					break;
				}
				needRefresh = true;
				break;

				case	HT_CMD_EXPORT:
				if (node == NULL)
				{
					if (pane == NULL)		break;
					if (pane->selectedView == NULL)	break;
					node = HT_TopNode(pane->selectedView);
				}
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
#ifdef	HT_PASSWORD_RTNS
					if (ht_hasPassword(node) == PR_TRUE)
					{
						if (ht_checkPassword(node, false) == PR_FALSE)	break;
					}
#endif
					FE_PromptForFileName(((MWContext *)gRDFMWContext()),
						"", NULL, false, false,
						(ReadFileNameCallbackFunction)exportCallback,
						(void *)node->node);
				}
				break;

				case	HT_CMD_EXPORTALL:
				FE_PromptForFileName(((MWContext *)gRDFMWContext()),
					NULL, NETSCAPE_RDF_FILENAME, false, false,
					(ReadFileNameCallbackFunction)exportCallback,
					(void *)gNavCenter->RDF_Top);
				break;

				case	HT_CMD_SET_TOOLBAR_FOLDER:
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
#ifdef	HT_PASSWORD_RTNS
					if (ht_hasPassword(node) == PR_TRUE)
					{
						if (ht_checkPassword(node, false) == PR_FALSE)	break;
					}
#endif
					if ((newTopNode = node->node) != NULL)
					{
						RDFUtil_SetPTFolder(newTopNode);
						onlyUpdateBookmarks = false;
						onlyUpdatePersonalToolbar = true;
						needRefresh = true;
					}
				}
				break;

				case	HT_CMD_SET_BOOKMARK_MENU:
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
#ifdef	HT_PASSWORD_RTNS
					if (ht_hasPassword(node) == PR_TRUE)
					{
						if (ht_checkPassword(node, false) == PR_FALSE)	break;
					}
#endif
					if ((newTopNode = node->node) != NULL)
					{
						RDFUtil_SetQuickFileFolder(newTopNode);
						onlyUpdateBookmarks = true;
						onlyUpdatePersonalToolbar = (newTopNode == RDFUtil_GetPTFolder()) ? true:false;
						needRefresh = true;
					}
				}
				break;

				case	HT_CMD_REMOVE_BOOKMARK_MENU:
				if (node == NULL)	break;
				if ((topNode = HT_TopNode(view)) == NULL)	break;
				if ((newTopNode = topNode->node) != NULL)
				{
					RDFUtil_SetQuickFileFolder(newTopNode);
					onlyUpdateBookmarks = true;
					onlyUpdatePersonalToolbar = (newTopNode == RDFUtil_GetPTFolder()) ? true:false;
					needRefresh = true;
				}
				break;

				case	HT_CMD_SET_BOOKMARK_FOLDER:
				if (node == NULL)	break;
				if (HT_IsContainer(node))
				{
					if ((newTopNode = node->node) != NULL)
					{
						RDFUtil_SetNewBookmarkFolder(newTopNode);
						onlyUpdateBookmarks = true;
						onlyUpdatePersonalToolbar = false;
						needRefresh = true;
					}
				}
				break;

				case	HT_CMD_REMOVE_BOOKMARK_FOLDER:
				if (node == NULL)	break;
				if ((topNode = HT_TopNode(view)) == NULL)	break;
				if ((newTopNode = topNode->node) != NULL)
				{
					RDFUtil_SetNewBookmarkFolder(newTopNode);
					onlyUpdateBookmarks = true;
					onlyUpdatePersonalToolbar = false;
					needRefresh = true;
				}
				break;

#ifdef	HT_PASSWORD_RTNS

				case	HT_CMD_SET_PASSWORD:
				if (ht_hasPassword(node) == PR_TRUE)
				{
					if (ht_checkPassword(node, true) == PR_FALSE)	break;
				}
				if ((password1 = FE_PromptPassword(((MWContext *)gRDFMWContext()),
								XP_GetString(RDF_NEWPASSWORD))) == NULL)	break;
				if ((password2 = FE_PromptPassword(((MWContext *)gRDFMWContext()),
								XP_GetString(RDF_CONFIRMPASSWORD))) == NULL)	break;
				if (strcmp(password1, password2))
				{
					FE_Alert(((MWContext *)gRDFMWContext()), XP_GetString(RDF_MISMATCHPASSWORD));
					break;
				}
				ht_SetPassword(node, password1);
				break;

				case	HT_CMD_REMOVE_PASSWORD:
				if (ht_hasPassword(node) == PR_TRUE)
				{
					if (ht_checkPassword(node, true) == PR_FALSE)	break;
				}
				ht_SetPassword(node, NULL);
				break;
#endif

				default:
				if (node == NULL)	break;

				/* handle commands from the RDF graph */
				if (menuCmd >= NUM_MENU_CMDS)
				{
					menuCommand = menuCommandsList;
					while (menuCommand != NULL)
					{
						if (menuCommand->menuCmd == menuCmd)
						{
							RDF_Assert(gNCDB, node->node, gNavCenter->RDF_Command, 
								menuCommand->graphCommand, RDF_RESOURCE_TYPE);
							break;
						}
						menuCommand = menuCommand->next;
					}
				}
				break;
			}
			if (node != NULL)
			{
				node = HT_GetNextSelection(view, node);
			}
		} while (node != NULL);
		break;
	}

	freeMenuCommandList();

	if (needRefresh == true)
	{
		refreshPanes(onlyUpdateBookmarks, onlyUpdatePersonalToolbar, newTopNode);
	}
	gBatchUpdate = false;

	return(error);
}



void
htEmptyClipboard(RDF_Resource parent)
{
	RDF_Cursor		c;
	RDF_Resource		node;

	XP_ASSERT(parent != NULL);

	if (parent != NULL)
	{
		if ((c = RDF_GetSources(gNCDB, parent, gCoreVocab->RDF_parent,
					RDF_RESOURCE_TYPE,  true)) != NULL)
		{
			while (node = RDF_NextValue(c))
			{
				RDF_Unassert(gNCDB, node, gCoreVocab->RDF_parent,
					parent, RDF_RESOURCE_TYPE);
			}
			RDF_DisposeCursor(c);
		}
	}
}



void
htCopyReference(RDF_Resource original, RDF_Resource newParent, PRBool empty)
{
	RDF_Cursor		c;
	RDF_Resource		node;

	XP_ASSERT(original != NULL);
	XP_ASSERT(newParent != NULL);

	if (empty == PR_TRUE)
	{
		htEmptyClipboard(newParent);
	}
	
	if ((original != NULL) && (newParent != NULL))
	{
		if (original == gNavCenter->RDF_Clipboard)
		{
			if ((c = RDF_GetSources(gNCDB, original, gCoreVocab->RDF_parent,
						RDF_RESOURCE_TYPE,  true)) != NULL)
			{
				while (node = RDF_NextValue(c))
				{
					RDF_Assert(gNCDB, node, gCoreVocab->RDF_parent,
						newParent, RDF_RESOURCE_TYPE);
				}
				RDF_DisposeCursor(c);
			}
		}
		else
		{
			RDF_Assert(gNCDB, original, gCoreVocab->RDF_parent,
				newParent, RDF_RESOURCE_TYPE);
		}
	}
}



PR_PUBLIC_API(HT_Resource)
HT_GetNthItem (HT_View view, uint32 theIndex)
{
  uint32 count = theIndex;
  ldiv_t  cdiv  = ldiv(count , ITEM_LIST_ELEMENT_SIZE);
  uint32 bucketNum = (uint32)cdiv.quot;
  uint32 offset = (uint32)cdiv.rem;
  if (theIndex >= view->itemListCount) return NULL;
  if (*(view->itemList + bucketNum) == NULL) return NULL;
  return   *((*(view->itemList + bucketNum)) + offset);
}



PR_PUBLIC_API(uint32)
HT_GetNodeIndex(HT_View view, HT_Resource node)
{
	uint32		itemListIndex = 0;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		itemListIndex = node->itemListIndex;
	}
	return(itemListIndex);
}



PR_PUBLIC_API(uint32)
HT_GetViewIndex(HT_View view)
{
	HT_View		viewList = NULL;
	uint32		viewListIndex = 0;

	XP_ASSERT(view != NULL);

	if (view != NULL)
	{
		viewList = (view->pane->viewList);
		while (viewList != NULL)
		{
			if (viewList == view)	break;
			++viewListIndex;
			viewList = viewList->next;
		}
	}
	if (viewList != view)	viewListIndex = 0;
	return(viewListIndex);
}



PR_PUBLIC_API(PRBool)
HT_ItemHasForwardSibling(HT_Resource r)
{
	XP_ASSERT(r != NULL);

	return ((r != NULL) && (r->next) ? true : false);
}



PR_PUBLIC_API(PRBool)
HT_ItemHasBackwardSibling(HT_Resource r)
{
	PRBool		flag = false;

	XP_ASSERT(r != NULL);

	if ((r != NULL) && r->parent && (r->parent->child != r))
	{
		flag = true;
	}
	return (flag);
}



PR_PUBLIC_API(uint16)
HT_GetItemIndentation (HT_Resource r)
{
	XP_ASSERT(r != NULL);

	return (r->depth);
}



PR_PUBLIC_API(uint32)
HT_GetItemListCount (HT_View v)
{
	XP_ASSERT(v != NULL);

	return (v->itemListCount);
}



PR_PUBLIC_API(uint32)
HT_GetViewListCount (HT_Pane p)
{
	uint32		count = 0;

	XP_ASSERT(p != NULL);

	if (p != NULL)
	{
		count = p->viewListCount;
	}
	return (count);
}



PR_PUBLIC_API(HT_View)
HT_GetNthView (HT_Pane pane, uint32 theIndex)
{
	HT_View		view = NULL;

	XP_ASSERT(pane != NULL);

	if (pane != NULL)
	{
		view = pane->viewList;
		while ((theIndex > 0) && (view != NULL))
		{
			view = view->next;
			--theIndex;
		}
	}
	return(view);
}


 
PR_PUBLIC_API(HT_Resource)
HT_GetParent (HT_Resource node)
{
	XP_ASSERT(node != NULL);

	if (!node) return NULL;
	return (node->parent);
}



PR_PUBLIC_API(HT_View)
HT_GetView (HT_Resource node)
{
	HT_View			view = NULL;

	if (node != NULL)
	{
		view = node->view;
	}
	return (view);
}



PR_PUBLIC_API(PRBool)
HT_IsSelected (HT_Resource node)
{
	PRBool			isSelected = PR_FALSE;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		if (node->flags & HT_SELECTED_FLAG)	isSelected = PR_TRUE;
	}
	return (isSelected);
}



PR_PUBLIC_API(PRBool)
HT_IsContainer (HT_Resource node)
{
	PRBool			isContainer = PR_FALSE;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		if (containerp(node->node))	isContainer = PR_TRUE;
	}
	return (isContainer);
}



PR_PUBLIC_API(PRBool)
HT_IsContainerOpen (HT_Resource node)
{
	PRBool			isOpen = PR_FALSE;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		if (node->flags & HT_OPEN_FLAG)	isOpen = PR_TRUE;
	}
	return (isOpen);
}



PR_PUBLIC_API(PRBool)
HT_IsSeparator (HT_Resource node)
{
	PRBool			isSep = PR_FALSE;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->node != NULL);

	if ((node != NULL) && (node->node != NULL))
	{
		if (isSeparator(node->node))	isSep = PR_TRUE;
	}
	return(isSep);
}



PRBool
ht_isURLReal(HT_Resource node)
{
	RDF_BT			type;
	PRBool			validFlag = true;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->node != NULL);

	type = resourceType(node->node);
/*
	if (type == LFS_RT)
	{
		validFlag = false;
	}
	else
*/
	if (HT_IsContainer(node))
	{
		if (type == RDF_RT || type == HISTORY_RT || type == SEARCH_RT || type == PM_RT || type == IM_RT || type == ATALKVIRTUAL_RT)
		{
			validFlag = false;
		}
	}
	else if (HT_IsSeparator(node))
	{
		validFlag = false;
	}
	return(validFlag);
}



PR_PUBLIC_API(PRBool)
HT_GetNodeData (HT_Resource node, void *token, uint32 tokenType, void **nodeData)
{
	HT_Value		values;
	PRBool			foundData = false;
	void			*data = NULL;

	XP_ASSERT(node != NULL);

	if (token != NULL && nodeData != NULL)
	{
		*nodeData = NULL;

		if (values = node->values)
		{
			while (values != NULL)
			{
				if ((values->tokenType == tokenType) &&
				     (values->token == token))
				{
					*nodeData = values->data;
					return(true);
				}
				values = values->next;
			}
		}

		if ((token == gWebData->RDF_URL) && (ht_isURLReal(node) == false))
		{
			/* don't return RDF container or separator URLs */
		}
		else if (tokenType == HT_COLUMN_INT || tokenType == HT_COLUMN_DATE_INT)
		{
			data = RDF_GetSlotValue(node->view->pane->db, node->node, token,
						RDF_INT_TYPE, false, true);
			foundData = true;
		}
		else if (tokenType == HT_COLUMN_STRING || tokenType == HT_COLUMN_DATE_STRING)
		{
			if (token == gWebData->RDF_URL)
			{
				data = resourceID(node->node);
			}
			else
			{
				data = RDF_GetSlotValue(node->view->pane->db, node->node,
						token, RDF_STRING_TYPE, false, true);
			}
			if (data != NULL)
			{
				foundData = true;
			}
		}

		if (values = (HT_Value)getMem(sizeof(HT_ValueStruct)))
		{
			values->tokenType = tokenType;
			values->token = token;
			values->data = data;

			values->next = node->values;
			node->values = values;
		}
		*nodeData = data;
	}
	return(foundData);
}



PR_PUBLIC_API(HT_Error)
HT_SetNodeData (HT_Resource node, void *token, uint32 tokenType, void *data)
{
	HT_Error		error = HT_Err;
	void			*oldData = NULL;
	PRBool			dirty = PR_TRUE;

	XP_ASSERT(node != NULL);
	XP_ASSERT(token != NULL);

	if (HT_IsNodeDataEditable(node, token, tokenType))
	{
		if (HT_GetNodeData(node, token, tokenType, &oldData))
		{
			switch(tokenType)
			{
			case	HT_COLUMN_STRING:
				if (data != NULL && oldData != NULL)
				{
					if (!strcmp(data, oldData))
					{
						dirty = PR_FALSE;
					}
				}
				if (dirty == PR_TRUE)
				{
					if (oldData != NULL)
					{
						RDF_Unassert(node->view->pane->db, node->node,
							token, oldData, RDF_STRING_TYPE);
					}
					if ((data != NULL) && (((char *)data)[0] != '\0'))
					{
						RDF_Assert(node->view->pane->db, node->node,
							token, data, RDF_STRING_TYPE);
					}
					if (token == gNavCenter->RDF_smallIcon ||
					    token == gNavCenter->RDF_largeIcon)
					{
						if (node->flags & HT_FREEICON_URL_FLAG)
						{
							if (node->url[0] != NULL)
							{
								freeMem(node->url[0]);
								node->url[0] = NULL;
							}
							if (node->url[1] != NULL)
							{
								freeMem(node->url[1]);
								node->url[1] = NULL;
							}
						}
					}
				}
				error = HT_NoErr;
			break;
			}
		}
	}
	return(error);
}



PR_PUBLIC_API(HT_Error)
HT_SetNodeName(HT_Resource node, void *data)
{
	XP_ASSERT(node != NULL);

	return(HT_SetNodeData(node, gCoreVocab->RDF_name, HT_COLUMN_STRING, data));
}



PR_PUBLIC_API(PRBool)
HT_IsNodeDataEditable(HT_Resource node, void *token, uint32 tokenType)
{
	RDF_BT			type;
	PRBool			canEditFlag = false;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->node != NULL);
	XP_ASSERT(token != NULL);

	if (!HT_IsSeparator(node))
	{
		if (node->parent != NULL)
		{
			type = resourceType(node->parent->node);
		}
		else
		{
			type = resourceType(node->node);
		}

		if (((token == gCoreVocab->RDF_name) && (!htIsOpLocked(node, gNavCenter->RDF_NameLock))) ||
		    ((token == gNavCenter->RDF_largeIcon) && (!htIsOpLocked(node, gNavCenter->RDF_IconLock))) ||
		    ((token == gNavCenter->RDF_smallIcon) && (!htIsOpLocked(node, gNavCenter->RDF_IconLock))) ||
		    (token == gWebData->RDF_description) || (token == gNavCenter->RDF_URLShortcut) ||
		    (token == gNavCenter->RDF_HTMLURL) || (token == gNavCenter->RDF_HTMLHeight) ||
		    (token == gNavCenter->treeFGColor) || (token == gNavCenter->treeBGColor) ||
		    (token == gNavCenter->treeBGURL) || (token == gNavCenter->showTreeConnections) ||
		    (token == gNavCenter->treeConnectionFGColor) || (token == gNavCenter->treeOpenTriggerIconURL) ||
		    (token == gNavCenter->treeClosedTriggerIconURL) || (token == gNavCenter->selectionFGColor) ||
		    (token == gNavCenter->selectionBGColor) || (token == gNavCenter->columnHeaderFGColor) ||
		    (token == gNavCenter->columnHeaderBGColor) || (token == gNavCenter->columnHeaderBGURL) ||
		    (token == gNavCenter->showColumnHeaders) || (token == gNavCenter->showColumnHeaderDividers) ||
		    (token == gNavCenter->sortColumnFGColor) || (token == gNavCenter->sortColumnBGColor) ||
		    (token == gNavCenter->titleBarFGColor) || (token == gNavCenter->titleBarBGColor) ||
		    (token == gNavCenter->titleBarBGURL) || (token == gNavCenter->dividerColor) ||
		    (token == gNavCenter->showDivider) || (token == gNavCenter->selectedColumnHeaderFGColor) ||
		    (token == gNavCenter->selectedColumnHeaderBGColor) || (token == gNavCenter->showColumnHilite) ||
		    (token == gNavCenter->triggerPlacement) ||
		/*  ((token == gWebData->RDF_URL) && ht_isURLReal(node)) || */
#ifdef	HT_PASSWORD_RTNS
			(token == gNavCenter->RDF_Password) ||
#endif
			((gMissionControlEnabled == true) && 
			((token == gNavCenter->RDF_AddLock) ||
			(token == gNavCenter->RDF_DeleteLock) ||
			(token == gNavCenter->RDF_IconLock) ||
			(token == gNavCenter->RDF_NameLock) ||
			(token == gNavCenter->RDF_CopyLock) ||
			(token == gNavCenter->RDF_MoveLock) ||
			(token == gNavCenter->RDF_WorkspacePosLock))))
		{
			if (tokenType == HT_COLUMN_STRING)
			{
				if (type == RDF_RT)
				{
					canEditFlag = true;
				}
				else if (type == LFS_RT)
				{
					if (node->node == gNavCenter->RDF_LocalFiles)
					{
						canEditFlag = true;
					}
				}
				else if (type == HISTORY_RT)
				{
					if (node->parent == NULL)
					{
						canEditFlag = true;
					}
				}
			}
		}
	}
	return(canEditFlag);
}



/* XXX HT_NodeDisplayString is obsolete! Don't use. */

PR_PUBLIC_API(HT_Error)
HT_NodeDisplayString (HT_Resource node, char *buffer, int bufferLen)
{
	char *name = HT_GetNodeName(node);
	memset(buffer, '\0', bufferLen);
	if (name != NULL)
	{
	  	if (strlen(name) < (unsigned)bufferLen)
	  	{
	  		strcpy(buffer, name);
	  	}
	  	else
	  	{
			strncpy(buffer, name, bufferLen-2);
			name[bufferLen-1]='\0';
		}
	}  
	return HT_NoErr;
}



/* XXX HT_ViewDisplayString is obsolete! Don't use. */

PR_PUBLIC_API(HT_Error)
HT_ViewDisplayString (HT_View view, char *buffer, int bufferLen)
{
	char *name = HT_GetViewName(view);
	memset(buffer, '\0', bufferLen);
	if (name != NULL)
	{
		if (strlen(name) < (unsigned)bufferLen)
		{
			strcpy(buffer, name);
		}
		else
		{
			strncpy(buffer, name, bufferLen-2);
			name[bufferLen-1]='\0';
		}
	}  
	return HT_NoErr;
}



char *
buildInternalIconURL(HT_Resource node, PRBool *volatileURLFlag,
			PRBool largeIconFlag, PRBool workspaceFlag)
{
	HT_Icon			theURL, *urlEntry;
	RDF_BT			targetType;
	char			buffer[128], *object="", *objectType="", *objectInfo="";

	XP_ASSERT(node != NULL);
	XP_ASSERT(volatileURLFlag != NULL);

	*volatileURLFlag = false;
	targetType = resourceType(node->node);
	/* if (targetType == ES_RT) targetType = LFS_RT; */
	if (HT_IsContainer(node))
	{
		if (workspaceFlag)
		{
			object = "workspace";
			if (node->node == gNavCenter->RDF_Sitemaps)
			{
				objectType = ",sitemap";
			}
/*
			else if (node->node == gNavCenter->RDF_Channels)
			{
				objectType = ",channels";
			}
*/
			else if (targetType == LDAP_RT)
			{
				objectType = ",ldap";
			}
			else if (targetType == LFS_RT)
			{
				objectType = ",file";
			}
			else
			{
				objectType = ",personal";
			}
		}
		else
		{
			object = "folder";

			/* URL is state-based, so may change */
			*volatileURLFlag = true;

			if (targetType == LFS_RT)
			{
				objectInfo = (HT_IsContainerOpen(node)) ? "/local,open" : "/local,closed";
			}
			else if (targetType == FTP_RT || targetType == ES_RT ||
				targetType == LDAP_RT)
			{
				objectInfo = (HT_IsContainerOpen(node)) ? "/remote,open" : "/remote,closed";
			}
			else
			{
				objectInfo = (HT_IsContainerOpen(node)) ? "/open" : "/closed";
			}
		}
	}
	else
	{
		*volatileURLFlag = false;

		if (targetType == LFS_RT)
		{
			object = "file";
			objectInfo = "/local";
		}
		else if (targetType == FTP_RT || targetType == ES_RT)
		{
		  object = "file";
		     objectInfo = "/remote";
		}
		else
		{
			object = "url";
			if (HT_IsSeparator(node))
			{
				object = "url/separator";
			}
			else if ((node->node != NULL) && ( resourceID(node->node) != NULL))
			{
				if (startsWith("file:",  resourceID(node->node)))
				{
					object = "file/local";
				}
				else if (startsWith("mailbox:",  resourceID(node->node)))
				{
					object = "mailbox";
				}
				else if (startsWith("mail:", resourceID(node->node)))
				{
					object = "mail";
				}
				else if (startsWith("news:", resourceID(node->node)))
				{
					object = "news";
				}
			}
		}
	}

	switch(targetType)
	{
		case	FTP_RT:		objectType=",ftp";		break;
		case	ES_RT:		objectType=",server";		break;
		case	SEARCH_RT:	objectType=",search";		break;
		case	HISTORY_RT:	objectType=",history";		break;
		case	LDAP_RT:	objectType=",ldap";		break;
/*
		case    SITEMAP_RT:	objectType=",sitemap";		break;

*/
		default:
			if (object[0] == '\0')
			{
				object="unknown";
				objectInfo="";
				objectType="";
			}
		break;
	}

	sprintf(buffer,"icon/%s:%s%s%s", ((largeIconFlag) ? "large":"small"),
		object, objectType, objectInfo);

	/* maintain a cache of icon types so that a lot of memory isn't wasted
	 * giving each node its own copy of a relatively small number of types
         */

	urlEntry = &urlList;
	while (*urlEntry != NULL)
	{
		if ((*urlEntry)->name)
		{
			if (!strcmp((*urlEntry)->name, buffer))
			{
				return((*urlEntry)->name);
			}
		}
		urlEntry = &((*urlEntry)->next);
	}
	if ((theURL = (HT_Icon)getMem(sizeof(_HT_Icon))) != NULL)
	{
		if (theURL->name = copyString(buffer))	{
			theURL->next = urlList;
			urlList = theURL;
			return(theURL->name);
			}
		freeMem(theURL);
	}
	return(NULL);
}



char *
getIconURL( HT_Resource node, PRBool largeIconFlag, PRBool workspaceFlag)
{

	RDF_Resource	res;
	PRBool		volatileURLFlag;
	int		iconIndex;

	iconIndex = (largeIconFlag) ? 0:1;
	res = (largeIconFlag) ? gNavCenter->RDF_largeIcon : gNavCenter->RDF_smallIcon;

	/* if volatile URL, flush if needed and re-create */

	if (node->flags & HT_VOLATILE_URL_FLAG)
	{
		node->url[0] = NULL;
		node->url[1] = NULL;
	}

	if (node->url[iconIndex] == NULL)
	{
		if ((node->url[iconIndex] = (char*)RDF_GetSlotValue(node->view->pane->db,
				node->node, res, RDF_STRING_TYPE, false, true)) != NULL)
		{
			node->flags &= (~HT_VOLATILE_URL_FLAG);
			node->flags |= HT_FREEICON_URL_FLAG;
		}
		else
		{
			node->url[iconIndex] = buildInternalIconURL(node,
				&volatileURLFlag, largeIconFlag, workspaceFlag);
			if (volatileURLFlag)
			{
				node->flags |= HT_VOLATILE_URL_FLAG;
			}
			else
			{
				node->flags &= (~HT_VOLATILE_URL_FLAG);
			}
		}
	}
	return(node->url[iconIndex]);
}



PR_PUBLIC_API(char *)
HT_GetNodeLargeIconURL (HT_Resource r)
{
	XP_ASSERT(r != NULL);
	XP_ASSERT(r->node != NULL);

	return (getIconURL( r, true, false));
}



PR_PUBLIC_API(char *)
HT_GetNodeSmallIconURL (HT_Resource r)
{
	XP_ASSERT(r != NULL);
	XP_ASSERT(r->node != NULL);

	return (getIconURL( r, false, false));
}



PR_PUBLIC_API(char *)
HT_GetWorkspaceLargeIconURL (HT_View view)
{
	XP_ASSERT(view != NULL);

	return (getIconURL( view->top, true, true));
}



PR_PUBLIC_API(char *)
HT_GetWorkspaceSmallIconURL (HT_View view)
{
	XP_ASSERT(view != NULL);

	return (getIconURL( view->top, false, true));
}



/* obsolete! */
PR_PUBLIC_API(char *)
HT_GetLargeIconURL (HT_Resource r)
{
	return(NULL);
}



/* obsolete! */
PR_PUBLIC_API(char *)
HT_GetSmallIconURL (HT_Resource r)
{
	return(NULL);
}



static PRBool
rdfColorProcDialogHandler(XPDialogState *dlgstate, char **argv, int argc, unsigned int button)
{
	PRBool			dirty, retVal = PR_TRUE;
	_htmlElementPtr         htmlElement;
	void			*data = NULL;
	int			loop;

	switch(button)
	{
		case    XP_DIALOG_OK_BUTTON:
		for (loop=0; loop<argc; loop+=2)
		{
			htmlElement = htmlElementList;
			while (htmlElement != NULL)
			{
				if (strcmp( resourceID(htmlElement->token), argv[loop]))
				{
					htmlElement = htmlElement->next;
					continue;
				}
				if (HT_IsNodeDataEditable(htmlElement->node,
					htmlElement->token, htmlElement->tokenType))
				{
					HT_GetNodeData (htmlElement->node,
						htmlElement->token,
						htmlElement->tokenType, &data);

					dirty = FALSE;
					switch(htmlElement->tokenType)
					{
						case    HT_COLUMN_STRING:
						if (data == NULL)
						{
							if (argv[loop+1][0] != '\0')
							{
								dirty = TRUE;
							}
						}
						else if (strcmp(data,argv[loop+1]))
						{
							dirty = TRUE;
						}
						break;
					}
					if (dirty == TRUE)
					{
						HT_SetNodeData (htmlElement->node,
								htmlElement->token,
								htmlElement->tokenType,
								argv[loop+1]);
					}
				}
				break;
			}
		}
		retVal = PR_FALSE;
		break;

		case    XP_DIALOG_CANCEL_BUTTON:
		retVal = PR_FALSE;
		break;
	}

	if (retVal == PR_FALSE)
	{
		freeHtmlElement(gNavCenter->treeFGColor);
		freeHtmlElement(gNavCenter->treeBGColor);
		freeHtmlElement(gNavCenter->treeBGURL);
		freeHtmlElement(gNavCenter->showTreeConnections);
		freeHtmlElement(gNavCenter->treeConnectionFGColor);
		freeHtmlElement(gNavCenter->treeOpenTriggerIconURL);
		freeHtmlElement(gNavCenter->treeClosedTriggerIconURL);
		freeHtmlElement(gNavCenter->selectionFGColor);
		freeHtmlElement(gNavCenter->selectionBGColor);
		freeHtmlElement(gNavCenter->columnHeaderFGColor);
		freeHtmlElement(gNavCenter->columnHeaderBGColor);
		freeHtmlElement(gNavCenter->columnHeaderBGURL);
		freeHtmlElement(gNavCenter->showColumnHeaders);
		freeHtmlElement(gNavCenter->showColumnHeaderDividers);
		freeHtmlElement(gNavCenter->sortColumnFGColor);
		freeHtmlElement(gNavCenter->sortColumnBGColor);
		freeHtmlElement(gNavCenter->titleBarFGColor);
		freeHtmlElement(gNavCenter->titleBarBGColor);
		freeHtmlElement(gNavCenter->titleBarBGURL);
		freeHtmlElement(gNavCenter->dividerColor);
		freeHtmlElement(gNavCenter->showDivider);
		freeHtmlElement(gNavCenter->selectedColumnHeaderFGColor);
		freeHtmlElement(gNavCenter->selectedColumnHeaderBGColor);
		freeHtmlElement(gNavCenter->showColumnHilite);
		freeHtmlElement(gNavCenter->triggerPlacement);
	}

	return(retVal);
}



static XPDialogInfo rdfColorPropDialogInfo = {
	(XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON),
	rdfColorProcDialogHandler,
	500, 400
};



static PRBool
rdfProcDialogHandler(XPDialogState *dlgstate, char **argv, int argc, unsigned int button)
{
	HT_Resource		node = NULL;
	PRBool			dirty, retVal = PR_TRUE;
	XPDialogStrings		*strings = NULL;
	_htmlElementPtr         htmlElement;
	int			loop;
	void			*data = NULL;
	char			*dynStr = NULL, *preHTMLdynStr = NULL, *postHTMLdynStr = NULL;
	PRInt32			val;

	switch(button)
	{
		case    XP_DIALOG_OK_BUTTON:
		node = htmlElementList->node;

		if (gMissionControlEnabled == true)
		{
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_AddLock,
					RDF_RESOURCE_TYPE);
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_DeleteLock,
					RDF_RESOURCE_TYPE);
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_IconLock,
					RDF_RESOURCE_TYPE);
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_NameLock,
					RDF_RESOURCE_TYPE);
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_CopyLock,
					RDF_RESOURCE_TYPE);
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_MoveLock,
					RDF_RESOURCE_TYPE);
			RDF_Unassert(node->view->pane->db, node->node,
					gNavCenter->RDF_Locks, gNavCenter->RDF_WorkspacePosLock,
					RDF_RESOURCE_TYPE);
		}
		for (loop=0; loop<argc; loop+=2)
		{
			htmlElement = htmlElementList;
			while (htmlElement != NULL)
			{
				if (strcmp( resourceID(htmlElement->token), argv[loop]))
				{
					htmlElement = htmlElement->next;
					continue;
				}

				if ((gMissionControlEnabled == true) &&
				   ((htmlElement->token == gNavCenter->RDF_AddLock) ||
				    (htmlElement->token == gNavCenter->RDF_DeleteLock) ||
				    (htmlElement->token == gNavCenter->RDF_IconLock) ||
				    (htmlElement->token == gNavCenter->RDF_NameLock) ||
				    (htmlElement->token == gNavCenter->RDF_CopyLock) ||
				    (htmlElement->token == gNavCenter->RDF_MoveLock) ||
				    (htmlElement->token == gNavCenter->RDF_WorkspacePosLock)))
				{
					RDF_Assert(htmlElement->node->view->pane->db,
						node->node, gNavCenter->RDF_Locks,
						htmlElement->token, RDF_RESOURCE_TYPE);
				}
				else if (HT_IsNodeDataEditable(htmlElement->node,
					htmlElement->token, htmlElement->tokenType))
				{
					HT_GetNodeData (htmlElement->node,
						htmlElement->token,
						htmlElement->tokenType, &data);

					dirty = FALSE;
					switch(htmlElement->tokenType)
					{
						case    HT_COLUMN_STRING:
						if (data == NULL)
						{
							if (argv[loop+1][0] != '\0')
							{
								dirty = TRUE;
							}
						}
						else if (strcmp(data,argv[loop+1]))
						{
							dirty = TRUE;
						}
						if (dirty == TRUE)
						{
							HT_SetNodeData (htmlElement->node,
									htmlElement->token,
									htmlElement->tokenType,
									argv[loop+1]);
						}
						break;

						case	HT_COLUMN_INT:
						val = atol(argv[loop+1]);
						if (dirty == TRUE)
						{
							HT_SetNodeData (htmlElement->node,
									htmlElement->token,
									htmlElement->tokenType,
									&val);
						}
						break;

					}
				}
				break;
			}
		}
		retVal = PR_FALSE;
		break;

		case    XP_DIALOG_CANCEL_BUTTON:
		retVal = PR_FALSE;
		break;

		case	XP_DIALOG_MOREINFO_BUTTON:
		node = htmlElementList->node;

		preHTMLdynStr = constructHTMLTagData(preHTMLdynStr, RDF_SETCOLOR_JS, NULL);

		dynStr = constructHTMLTagData(dynStr, RDF_HTML_INFOHEADER_STR, XP_GetString(RDF_TREE_COLORS_TITLE));
		dynStr = constructHTML(dynStr, node, gNavCenter->treeFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->treeBGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->treeBGURL, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->showTreeConnections, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->treeConnectionFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->treeOpenTriggerIconURL, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->treeClosedTriggerIconURL, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->triggerPlacement, HT_COLUMN_STRING);

		dynStr = constructHTMLTagData(dynStr, RDF_HTML_EMPTYHEADER_STR, "");
		dynStr = constructHTMLTagData(dynStr, RDF_HTML_INFOHEADER_STR, XP_GetString(RDF_TITLEBAR_COLORS_TITLE));
		dynStr = constructHTML(dynStr, node, gNavCenter->titleBarFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->titleBarBGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->titleBarBGURL, HT_COLUMN_STRING);

		dynStr = constructHTMLTagData(dynStr, RDF_HTML_EMPTYHEADER_STR, "");
		dynStr = constructHTMLTagData(dynStr, RDF_HTML_INFOHEADER_STR, XP_GetString(RDF_SELECTION_COLORS_TITLE));
		dynStr = constructHTML(dynStr, node, gNavCenter->selectionFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->selectionBGColor, HT_COLUMN_STRING);

		dynStr = constructHTMLTagData(dynStr, RDF_HTML_EMPTYHEADER_STR, "");
		dynStr = constructHTMLTagData(dynStr, RDF_HTML_INFOHEADER_STR, XP_GetString(RDF_COLUMN_COLORS_TITLE));
		dynStr = constructHTML(dynStr, node, gNavCenter->columnHeaderFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->columnHeaderBGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->columnHeaderBGURL, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->showColumnHeaders, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->showColumnHeaderDividers, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->selectedColumnHeaderFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->selectedColumnHeaderBGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->sortColumnFGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->sortColumnBGColor, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->showColumnHilite, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->showDivider, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, gNavCenter->dividerColor, HT_COLUMN_STRING);

		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->treeFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->treeFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->treeBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->treeBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->treeConnectionFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->treeConnectionFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->titleBarFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->titleBarFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->titleBarBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->titleBarBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->selectionFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->selectionFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->selectionBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->selectionBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->columnHeaderFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->columnHeaderFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->columnHeaderBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->columnHeaderBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->selectedColumnHeaderFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->selectedColumnHeaderFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->selectedColumnHeaderBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->selectedColumnHeaderBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->sortColumnFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->sortColumnFGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->sortColumnBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->sortColumnBGColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_COLOR_LAYER, resourceID(gNavCenter->dividerColor));
		postHTMLdynStr = constructHTMLTagData(postHTMLdynStr, RDF_DEFAULTCOLOR_JS, resourceID(gNavCenter->dividerColor));

		strings = XP_GetDialogStrings(RDF_HTMLCOLOR_STR);
		if ((strings != NULL) && (dynStr != NULL))
		{
			if (preHTMLdynStr != NULL)	XP_CopyDialogString(strings, 0, preHTMLdynStr);
			XP_CopyDialogString(strings, 1, dynStr);
			if (postHTMLdynStr != NULL)	XP_CopyDialogString(strings, 2, postHTMLdynStr);
			XP_MakeHTMLDialog(NULL, &rdfColorPropDialogInfo, RDF_COLOR_TITLE,
				strings, node, PR_FALSE);
		}
		if (dynStr != NULL)	XP_FREE(dynStr);
		if (strings != NULL)	XP_FreeDialogStrings(strings);
		break;
	}

	if (retVal == PR_FALSE)
	{
		freeHtmlElementList();
	}
	return(retVal);
}



static XPDialogInfo rdfPropDialogInfo = {
	(XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON),
	rdfProcDialogHandler,
	500, 400
};
static XPDialogInfo rdfWorkspacePropDialogInfo = {
	(XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON | XP_DIALOG_MOREINFO_BUTTON),
	rdfProcDialogHandler,
	500, 400
};



void
addHtmlElement(HT_Resource node, RDF_Resource token, uint32 tokenType)
{
	_htmlElementPtr         htmlElement;

	if (htmlElement = (_htmlElementPtr)getMem(sizeof(_htmlElement)))
	{
		htmlElement->node = node;
		htmlElement->token = token;
		htmlElement->tokenType = tokenType;

		htmlElement->next = htmlElementList;
		htmlElementList = htmlElement;
	}
}



void
freeHtmlElementList()
{
	_htmlElementPtr		htmlElement,nextElement;

	htmlElement = htmlElementList;
	while (htmlElement != NULL)
	{
		nextElement = htmlElement->next;
		free(htmlElement);
		htmlElement = nextElement;
	}
	htmlElementList = NULL;
}



_htmlElementPtr
findHtmlElement(void *token)
{
	_htmlElementPtr		htmlElement;

	htmlElement = htmlElementList;
	while(htmlElement != NULL)
	{
		if (htmlElement->token == token)	break;
		htmlElement = htmlElement->next;
	}
	return(htmlElement);
}



void
freeHtmlElement(void *token)
{
	_htmlElementPtr		*htmlElement, nextElement;

	htmlElement = &htmlElementList;
	while((*htmlElement) != NULL)
	{
		if ((*htmlElement)->token == token)
		{
			nextElement = (*htmlElement)->next;
			free(*htmlElement);
			*htmlElement = nextElement;
			break;
		}
		htmlElement = &((*htmlElement)->next);
	}
}



char *
constructHTMLTagData(char *dynStr, int strID, char *data)
{
	char			*html, *temp1, *temp2;

	if ((html = XP_GetString(strID)) != NULL)
	{
		/* yikes... need to find a better solution */
		if (data == NULL)	data="";
		temp1 = PR_smprintf(html, data, data, data, data, data, data, data, data);
		if (temp1 != NULL)
		{
			if (dynStr != NULL)
			{
				temp2 = PR_smprintf("%s%s",dynStr, temp1);
				XP_FREE(temp1);
				XP_FREE(dynStr);
				dynStr = temp2;
			}
			else
			{
				dynStr = temp1;
			}
		}
	}
	return(dynStr);
}



char *
constructHTML(char *dynStr, HT_Resource node, void *token, uint32 tokenType)
{
	struct tm		*time;
	time_t			dateVal;
	PRBool			isEditable;
	char			*html = NULL, *temp1 = NULL, *temp2;
	char			*data = NULL, *tokenName;
	char			buffer[128];

	XP_ASSERT(node != NULL);
	XP_ASSERT(token != NULL);

	if (findHtmlElement(token) != NULL)	return(dynStr);

	isEditable = HT_IsNodeDataEditable(node, token, tokenType);
	if ((!HT_GetNodeData (node, token, tokenType, &data)) || (data == NULL))
	{
		data = "";
	}
	else
	{
		switch(tokenType)
		{
			case	HT_COLUMN_RESOURCE:
			data =  resourceID((RDF_Resource)token);
			break;

			case	HT_COLUMN_STRING:
			break;

			case	HT_COLUMN_DATE_STRING:
			if ((dateVal = (time_t)atol((char *)data)) == 0)        break;
			if ((time = localtime(&dateVal)) == NULL)       break;
#ifdef	XP_WIN
			strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#else
			strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#endif
			data = buffer;
			break;

			case	HT_COLUMN_DATE_INT:
			if ((time = localtime((time_t *) &data)) == NULL)       break;
#ifdef	XP_WIN
			strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_WINDATE),time);
#else
			strftime(buffer,sizeof(buffer),XP_GetString(RDF_HTML_MACDATE),time);
#endif
			data = buffer;
			break;

			case	HT_COLUMN_INT:
			sprintf(buffer,"%d",(int)data);
			data = buffer;
			break;

			case	HT_COLUMN_UNKNOWN:
			default:
			data = NULL;
			return(dynStr);
			break;
		}
	}
	
	addHtmlElement(node, token, tokenType);

	tokenName = (char *) RDF_GetSlotValue(gNCDB, token,
					gCoreVocab->RDF_name,
					RDF_STRING_TYPE, false, true);
	if (tokenName == NULL)
	{
		tokenName = resourceID((RDF_Resource)token);
		if (tokenName != NULL)
		{
			tokenName = copyString(tokenName);
		}
	}

	if (isEditable)
	{

#ifdef	HT_PASSWORD_RTNS
		if (token == gNavCenter->RDF_Password)
		{
			html = XP_GetString(RDF_HTML_STR_2);
		}
		else
#endif

		if (token == gWebData->RDF_description)
		{
			html = XP_GetString(RDF_HTML_STR_5);
		}
		else if ((token == gNavCenter->treeFGColor) || (token == gNavCenter->treeBGColor) ||
			 (token == gNavCenter->selectionFGColor) || (token == gNavCenter->selectionBGColor) ||
			 (token == gNavCenter->columnHeaderFGColor) || (token == gNavCenter->columnHeaderBGColor) ||
			 (token == gNavCenter->sortColumnFGColor) || (token == gNavCenter->sortColumnBGColor) ||
			 (token == gNavCenter->titleBarFGColor) || (token == gNavCenter->titleBarBGColor) ||
			 (token == gNavCenter->selectedColumnHeaderFGColor) || (token == gNavCenter->selectedColumnHeaderBGColor) ||
			 (token == gNavCenter->treeConnectionFGColor) || (token == gNavCenter->dividerColor))
		{
			/* yikes... need to find a better solution */
			html = XP_GetString(RDF_HTML_COLOR_STR);
			temp1 = PR_smprintf(html, tokenName, resourceID((RDF_Resource)token),
					(data) ? data:"", resourceID((RDF_Resource)token),
					resourceID((RDF_Resource)token));
		}
		else if (token == gNavCenter->RDF_HTMLHeight)
		{
			html = XP_GetString(RDF_HTML_STR_NUMBER);
		}
		else
		{
			html = XP_GetString(RDF_HTML_STR_1);
		}
		if (temp1 == NULL)
		{
			temp1 = PR_smprintf(html, tokenName, resourceID((RDF_Resource)token), (data) ? data:"");
		}
	}
	else if ((data != NULL) && ((*data) != '\0'))
	{
		html=XP_GetString(RDF_HTML_STR_3);
		temp1 = PR_smprintf(html, tokenName, (data) ? data:"");
	}

	if (tokenName != NULL)
	{
		freeMem(tokenName);
	}

	if (temp1 != NULL)
	{
		if (dynStr != NULL)
		{
			temp2 = PR_smprintf("%s%s",dynStr, temp1);
			XP_FREE(temp1);
			XP_FREE(dynStr);
			dynStr = temp2;
		}
		else
		{
			dynStr = temp1;
		}
	}
	return(dynStr);
}



char *
constructHTMLPermission(char *dynStr, HT_Resource node, RDF_Resource token, char *text)
{
	char			*html, *temp1, *temp2;
	PRBool			enabledFlag = false;

	XP_ASSERT(node != NULL);
	XP_ASSERT(token != NULL);
	XP_ASSERT(text != NULL);

	html=XP_GetString(RDF_HTML_STR_4);
	if (html != NULL)
	{
		enabledFlag = htIsOpLocked(node, token);

		temp1 = PR_smprintf(html, resourceID((RDF_Resource)token),
					resourceID((RDF_Resource)token),
					(enabledFlag) ? "CHECKED":"", text);
		if (dynStr != NULL)
		{
			temp2 = PR_smprintf("%s%s",dynStr, temp1);
			XP_FREE(temp1);
			dynStr = temp2;
		}
		else
		{
			dynStr = temp1;
		}
		addHtmlElement(node, token, HT_COLUMN_RESOURCE);
	}
	return(dynStr);
}



PRBool
htIsOpLocked(HT_Resource node, RDF_Resource token)
{
	PRBool			lockedFlag = false;

	XP_ASSERT(node != NULL);
	XP_ASSERT(token != NULL);

	if ((node != NULL) && (node->node != NULL) && (token != NULL))
	{
		lockedFlag = RDF_HasAssertion(gNCDB, node->node,
				gNavCenter->RDF_Locks, token,
				RDF_RESOURCE_TYPE, 1);
	}
	return(lockedFlag);
}



PR_PUBLIC_API(void)
HT_Properties (HT_Resource node)
{
	RDF_BT			type;
	RDF_Cursor		c;
	RDF_Resource		r;
	PRBool			isContainer, showPermissions = false;
	XP_Bool			mcEnabled = false;
	XPDialogStrings		*strings = NULL;
	char			*dynStr = NULL, *postHTMLdynStr = NULL, *title;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->node != NULL);

	type = resourceType(node->node);
	if (!(isContainer=HT_IsContainer(node)))
	{
		if (node->parent != NULL)
		{
			type = resourceType(node->parent->node);
		}
	}

	if (gMissionControlEnabled == true)
	{
		showPermissions = true;
	}

	title = HT_GetNodeName(node);
	if (title == NULL)
	{
		title = HT_GetNodeURL(node);
	}
	dynStr = constructHTMLTagData(dynStr, RDF_HTML_MAININFOHEADER_STR, title );

	switch(type)
	{
		case    RDF_RT:
		if (HT_IsContainer(node))
		{
			dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_description, HT_COLUMN_STRING);
#ifdef	HT_PASSWORD_RTNS
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_Password, HT_COLUMN_STRING);
#endif
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_bookmarkAddDate, HT_COLUMN_DATE_STRING);
#ifdef	HT_LARGE_ICON_SUPPORT
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_largeIcon, HT_COLUMN_STRING);
#endif
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_smallIcon, HT_COLUMN_STRING);
			if (node->parent == NULL)
			{
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_HTMLURL, HT_COLUMN_STRING);
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_HTMLHeight, HT_COLUMN_STRING);
			}
		}
		else
		{
			dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_URL, HT_COLUMN_STRING);
			if (HT_IsLocalData(node))
			{
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_URLShortcut, HT_COLUMN_STRING);
			}
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_description, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_bookmarkAddDate, HT_COLUMN_DATE_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_lastVisitDate, HT_COLUMN_DATE_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_lastModifiedDate, HT_COLUMN_DATE_STRING);
#ifdef	HT_LARGE_ICON_SUPPORT
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_largeIcon, HT_COLUMN_STRING);
#endif
			dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_smallIcon, HT_COLUMN_STRING);
		}
		break;

		case	LFS_RT:
		showPermissions = false;
		if (HT_IsContainer(node))
		{
			if (node->parent == NULL)
			{
				showPermissions = true;
			}
			dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
			if (node->node != gNavCenter->RDF_LocalFiles)
			{
				dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_URL, HT_COLUMN_STRING);
#ifdef	NSPR20
				dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_creationDate, HT_COLUMN_DATE_INT);
#endif
				dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_lastModifiedDate, HT_COLUMN_DATE_INT);
			}
			else
			{
#ifdef	HT_LARGE_ICON_SUPPORT
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_largeIcon, HT_COLUMN_STRING);
#endif
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_smallIcon, HT_COLUMN_STRING);
			}
		}
		else
		{
			dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_URL, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_size, HT_COLUMN_INT);
#ifdef	NSPR20
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_creationDate, HT_COLUMN_DATE_INT);
#endif
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_lastModifiedDate, HT_COLUMN_DATE_INT);
		}
		break;

		case	HISTORY_RT:
		showPermissions = false;
		if (HT_IsContainer(node))
		{
			if (node->parent == NULL)
			{
				showPermissions = true;
				dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
#ifdef	HT_LARGE_ICON_SUPPORT
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_largeIcon, HT_COLUMN_STRING);
#endif
				dynStr = constructHTML(dynStr, node, (void *)gNavCenter->RDF_smallIcon, HT_COLUMN_STRING);
			}
		}
		else
		{
			dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_URL, HT_COLUMN_STRING);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_firstVisitDate, HT_COLUMN_DATE_INT);
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_lastVisitDate, HT_COLUMN_DATE_INT);
			/* XXX should also show calculated expiration date */
			dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_numAccesses, HT_COLUMN_INT);
		}
		break;

		case    FTP_RT:
		case    ES_RT:
		showPermissions = false;
		dynStr = constructHTML(dynStr, node, (void *)gCoreVocab->RDF_name, HT_COLUMN_STRING);
		dynStr = constructHTML(dynStr, node, (void *)gWebData->RDF_URL, HT_COLUMN_STRING);
		break;

		case    SEARCH_RT:
		case    LDAP_RT:
		case    PM_RT:
		case    RDM_RT:
		default:
			dynStr = NULL;
		break;
	}

	if (showPermissions == true)
	{

		dynStr = constructHTMLTagData(dynStr, RDF_HTML_EMPTYHEADER_STR, "");
		dynStr = constructHTMLTagData(dynStr, RDF_HTML_INFOHEADER_STR, XP_GetString(RDF_MISSION_CONTROL_TITLE));

		if (HT_IsContainer(node) && (resourceType(node->node) == RDF_RT))
		{
			postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_AddLock, XP_GetString(RDF_ADDITIONS_ALLOWED));
		}
		postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_DeleteLock, XP_GetString(RDF_DELETION_ALLOWED));
		postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_IconLock, XP_GetString(RDF_ICON_URL_LOCKED));
		postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_NameLock, XP_GetString(RDF_NAME_LOCKED));
		postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_CopyLock, XP_GetString(RDF_COPY_ALLOWED));
		postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_MoveLock, XP_GetString(RDF_MOVE_ALLOWED));
		if (HT_IsContainer(node) && (node->parent == NULL))
		{
			postHTMLdynStr = constructHTMLPermission(postHTMLdynStr, node, gNavCenter->RDF_WorkspacePosLock, XP_GetString(RDF_WORKSPACE_POS_LOCKED));
		}
	}

	if (dynStr != NULL)
	{

		/* dynamic property lookup */

		if ((c = RDF_GetTargets(node->view->pane->db, node->node, gCoreVocab->RDF_slotsHere,
					RDF_RESOURCE_TYPE, 1)) != NULL)
		{
			while ((r = RDF_NextValue(c)) != NULL)
			{
				dynStr = constructHTML(dynStr, node, (void *)r, HT_COLUMN_STRING);
			}
			RDF_DisposeCursor(c);
		}
	}
	
	strings = XP_GetDialogStrings(RDF_HTML_STR);
	if (strings != NULL && dynStr != NULL)
	{
		XP_CopyDialogString(strings, 0, dynStr);
		if (postHTMLdynStr != NULL)
		{
			XP_CopyDialogString(strings, 1, postHTMLdynStr);
		}
		if (node->parent == NULL)
		{
			XP_MakeHTMLDialog(NULL, &rdfWorkspacePropDialogInfo, RDF_MAIN_TITLE,
				strings, node, PR_FALSE);
		}
		else
		{
			XP_MakeHTMLDialog(NULL, &rdfPropDialogInfo, RDF_MAIN_TITLE,
				strings, node, PR_FALSE);
		}
	}
	else
	{
		freeHtmlElementList();
#ifdef	DEBUG
		XP_MakeHTMLAlert(NULL, "Properties not yet implemented for this node type.");
#endif
	}
	if (dynStr != NULL)	XP_FREE(dynStr);
	if (strings != NULL)	XP_FreeDialogStrings(strings);
}



PR_PUBLIC_API(RDF_Resource)
HT_GetRDFResource (HT_Resource node)
{
	RDF_Resource		r = NULL;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		r = node->node;
	}
	return (r);
}



PR_PUBLIC_API(char*)
HT_GetNodeURL(HT_Resource node)
{
	XP_ASSERT(node != NULL);
	XP_ASSERT(node->node != NULL);

	return  resourceID(node->node);
}



PR_PUBLIC_API(char*)
HT_GetNodeName(HT_Resource node)
{
	char		*name = NULL;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		HT_GetNodeData (node, gCoreVocab->RDF_name, HT_COLUMN_STRING, &name);
	}
	return(name);
}



void
setHiddenState (HT_Resource node)
{
	HT_Resource	child;
	HT_View		view;
	HT_Pane		pane;

	if ((child = node->child) == NULL)	return;
	if ((view = child->view) == NULL)	return;
	if ((pane = view->pane) == NULL)	return;

	if (!HT_IsContainerOpen(node) && pane->autoFlushFlag == true)
	{
		/* destroy all interior HT nodes */

		destroyViewInt(node, PR_FALSE);
	}
	else
	{
		/* hide all interior HT nodes */

		while (child != NULL)
		{
			if ((node->flags & HT_HIDDEN_FLAG) || (!HT_IsContainerOpen(node)))
			{
				child->flags |= HT_HIDDEN_FLAG;
			}
			else
			{
				child->flags &= (~HT_HIDDEN_FLAG);
			}
			if (HT_IsContainer(child))
			{
				setHiddenState(child);
			}
			child->flags &= (~HT_SELECTED_FLAG);
			child = child->next;
		}
	}
}



PR_PUBLIC_API(HT_Error)
HT_SetOpenState (HT_Resource containerNode, PRBool isOpen)
{
	SBProvider		sb;

	XP_ASSERT(containerNode != NULL);

	if ((isOpen && (!HT_IsContainerOpen(containerNode))) ||
	   ((!isOpen) && (HT_IsContainerOpen(containerNode))))
	{

#ifdef	HT_PASSWORD_RTNS
		if (isOpen && (ht_hasPassword(containerNode)))
		{
			if (ht_checkPassword(containerNode, false) == PR_FALSE)
			{
				return(HT_Err);
			}
		}
#endif

		if ((containerNode->view->inited == false)
#ifdef	XP_MAC
			/* Mac bookmark menu kludge */
		   || (containerNode->view->pane->special == true)
#endif
		)
		{
			if (startsWith("ftp:", resourceID(containerNode->node)) ||
			    startsWith("ldap:", resourceID(containerNode->node)))
			{
				return(HT_NoErr);
			}
		}

		sendNotification(containerNode,  HT_EVENT_NODE_OPENCLOSE_CHANGING);
		if (isOpen)
		{
			containerNode->flags |= HT_OPEN_FLAG;
		}
		else
		{
			containerNode->flags &= (~HT_OPEN_FLAG);
		}
		setHiddenState(containerNode);
		refreshItemList(containerNode, HT_EVENT_NODE_OPENCLOSE_CHANGED);
	}

	sb = SBProviderOfNode(containerNode);
	if (sb) sb->openp = isOpen;

	return (HT_NoErr);
}



PR_PUBLIC_API(HT_Error)
HT_SetSelectedState (HT_Resource node, PRBool isSelected)
{
	XP_ASSERT(node != NULL);

	if ((isSelected && (!HT_IsSelected(node))) ||
	   ((!isSelected) && (HT_IsSelected(node))))
	{
		if (isSelected)
		{
			node->flags |= HT_SELECTED_FLAG;
		}
		else
		{
			node->flags &= (~HT_SELECTED_FLAG);
		}
		sendNotification(node,  HT_EVENT_NODE_SELECTION_CHANGED);
	}
	return (HT_NoErr);
}



PR_PUBLIC_API(HT_Error)
HT_SetNotificationMask (HT_Pane pane, HT_NotificationMask mask)
{
	XP_ASSERT(pane != NULL);

	pane->mask = mask;
	return (HT_NoErr);
}



PR_PUBLIC_API(HT_Error)
HT_GetOpenState (HT_Resource containerNode, PRBool *openState)
{
	XP_ASSERT(containerNode != NULL);
	XP_ASSERT(openState != NULL);

	*openState = (containerNode->flags & HT_OPEN_FLAG) ? PR_TRUE:PR_FALSE;
	return (HT_NoErr);
}



PR_PUBLIC_API(HT_Error)
HT_GetSelectedState (HT_Resource node, PRBool *selectedState)
{
	XP_ASSERT(node != NULL);
	XP_ASSERT(selectedState != NULL);

	if ((node != NULL) && (selectedState != NULL))
	{
		*selectedState = (node->flags & HT_SELECTED_FLAG) ? PR_TRUE:PR_FALSE;
	}
	return (HT_NoErr);
}



/*
	HT_SetSelection:
	Select a given node in a view, deselecting all other nodes
*/

PR_PUBLIC_API(HT_Error)
HT_SetSelection (HT_Resource node)
{
	HT_Resource	res;
	HT_View		view;
	uint32		itemListCount,theIndex;

	XP_ASSERT(node != NULL);
	view = HT_GetView(node);
	XP_ASSERT(view != NULL);

	itemListCount = HT_GetItemListCount(view);
	for (theIndex=0; theIndex<itemListCount; theIndex++)
	{
		if ((res=HT_GetNthItem (view, theIndex)) != NULL)
		{
			HT_SetSelectedState (res, (res==node) ? true:false);
		}
	}
	return (HT_NoErr);
}




/*
	HT_SetSelectionAll:
	Select/Deselect all nodes for a given view
*/

PR_PUBLIC_API(HT_Error)
HT_SetSelectionAll (HT_View view, PRBool selectedState)
{
	HT_Resource	res;
	uint32		itemListCount,theIndex;

	XP_ASSERT(view != NULL);

	itemListCount = HT_GetItemListCount(view);
	for (theIndex=0; theIndex<itemListCount; theIndex++)
	{
		if ((res=HT_GetNthItem (view, theIndex)) != NULL)
		{
			HT_SetSelectedState (res, selectedState);
		}
	}
	return HT_NoErr;
}



/*
	HT_SetSelectionRange:
	Select any/all nodes inbetween nodes "node1" and "node2", deselecting all other nodes

	Note: both "node1" and "node2" should be in the same view
*/

PR_PUBLIC_API(HT_Error)
HT_SetSelectionRange (HT_Resource node1, HT_Resource node2)
{
	HT_Resource	res;
	HT_View		view,view2;
	uint32		itemListCount,theIndex;
	PRBool		selectionState = false, invertState;

	XP_ASSERT(node1 != NULL);
	XP_ASSERT(node2 != NULL);
	view = HT_GetView(node1);
	view2 = HT_GetView(node2);
	XP_ASSERT(view != NULL);
	XP_ASSERT(view2 != NULL);
	XP_ASSERT(view == view2);

	itemListCount = HT_GetItemListCount(view);
	for (theIndex=0; theIndex<itemListCount; theIndex++)
	{
		if ((res=HT_GetNthItem (view, theIndex)) != NULL)
		{
			/* note: can't just invert selectionState as 
				 need to be inclusive on ending node */

			invertState = ((res == node1) || (res == node2));
			if (invertState && selectionState == false)
			{
				selectionState = true;
				invertState = false;
			}
			HT_SetSelectedState (res, selectionState);
			if (invertState && selectionState == true)
			{
				selectionState = false;
			}
		}
	}
	return HT_NoErr;
}



/*
	HT_GetNextSelection:
	if "startingNode" is NULL, get the first selected item (returns NULL if none);
	otherwise, find the first selected node AFTER "startingNode" (returns NULL if none)
*/

PR_PUBLIC_API(HT_Resource)
HT_GetNextSelection(HT_View view, HT_Resource startingNode)
{
	HT_Resource	node, nextNode = NULL;
	uint32		itemListCount,theIndex;
	PRBool		selectedState;

	XP_ASSERT(view != NULL);

	itemListCount = HT_GetItemListCount(view);

	if ((startingNode != NULL) && (view->selectedNodeHint < itemListCount) &&
		HT_GetNthItem(view, view->selectedNodeHint) == startingNode)
	{
		theIndex = view->selectedNodeHint + 1;
	}
	else
	{
		theIndex = view->selectedNodeHint = 0;
	}

	for (; theIndex < itemListCount; theIndex++)
	{
		if ((node=HT_GetNthItem (view, theIndex)) != NULL)
		{
			if (node == startingNode)	continue;
			HT_GetSelectedState (node, &selectedState);
			if (selectedState == true)
			{
				view->selectedNodeHint = (uint32)theIndex;
				nextNode = node;
				break;
			}
		}
	}
	return (nextNode);
}



/*
	HT_ToggleSelection:
	for a given node, unselect it if selected, otherwise select it
*/

PR_PUBLIC_API(void)
HT_ToggleSelection(HT_Resource node)
{
	PRBool		selectedState;

	XP_ASSERT(node != NULL);

	HT_GetSelectedState (node, &selectedState);
	HT_SetSelectedState (node, !selectedState);
}
 


PR_PUBLIC_API(PRBool)
HT_Launch(HT_Resource node, MWContext *context)
{
	PRBool		retVal = PR_FALSE;

	XP_ASSERT(node != NULL);

	if (node != NULL)
	{
		if ( (!HT_IsContainer(node)) && (!HT_IsSeparator(node)) )
		{
			if (RDF_HasAssertion(node->view->pane->db, node->node,
				gNavCenter->RDF_Command, gNavCenter->RDF_Command_Launch,
				RDF_RESOURCE_TYPE, 1))
			{
				retVal = PR_TRUE;

				RDF_Assert(node->view->pane->db, node->node,
					gNavCenter->RDF_Command, gNavCenter->RDF_Command_Launch,
					RDF_RESOURCE_TYPE);
			}
		}
	}
	return(retVal);
}



PR_PUBLIC_API(HT_Error)
HT_GetNotificationMask (HT_Pane pane, HT_NotificationMask *mask)
{
	XP_ASSERT(pane != NULL);
	XP_ASSERT(mask != NULL);

	*mask = pane->mask;
	return (HT_NoErr);
}



PRBool
mutableContainerp (RDF_Resource node)
{
	XP_ASSERT(node != NULL);

	return (containerp(node) && (resourceType(node) == RDF_RT));
}



PR_PUBLIC_API(HT_Resource)
HT_MakeNewContainer(HT_Resource parent, char* name)
{
	HT_Resource		hnc = NULL;
	RDF			db;
	RDF_Resource		nc;
	char			*id = NULL;

#ifdef	NSPR20
	PRTime			tm;
#else
	int64			tm;
#endif

#ifndef	HAVE_LONG_LONG
	double			doubletm;
#endif

	if (mutableContainerp(parent->node))
	{
		db = parent->view->pane->db;

#ifdef NSPR20
		tm = PR_Now();
#else
		tm = PR_LocalTime();
#endif

#ifdef HAVE_LONG_LONG
		id = PR_smprintf("Topic%d", (double)tm);
#else
		LL_L2D(doubletm, tm);
		id = PR_smprintf("Topic%d.rdf", doubletm);
#endif
		nc = RDF_GetResource(db, id, 1);
		setContainerp(nc, 1);

		RDF_Assert(db, nc, gCoreVocab->RDF_name, name, RDF_STRING_TYPE);
		RDF_Assert(db, nc, gCoreVocab->RDF_parent, parent->node, RDF_RESOURCE_TYPE);
		RDF_Assert(db, nc, gCoreVocab->RDF_name, name, RDF_STRING_TYPE); 
		hnc = newHTEntry(parent->view, nc);

		refreshItemList(parent, HT_EVENT_NODE_ADDED);
	}
	return (hnc);
}



PR_PUBLIC_API(HT_DropAction)
HT_CanDropHTROn(HT_Resource dropTarget, HT_Resource obj)
{
	return dropOn (dropTarget, obj, 1) ;
}



PR_PUBLIC_API(HT_DropAction)
HT_CanDropURLOn(HT_Resource dropTarget, char* url)
{
	return dropURLOn(dropTarget, url, NULL, 1);
}



PR_PUBLIC_API(HT_DropAction)
HT_DropHTROn(HT_Resource dropTarget, HT_Resource obj)
{
	HT_DropAction		ac;

	ac = dropOn(dropTarget, obj, 0);				    
	resynchContainer (dropTarget);
	refreshItemList(dropTarget, HT_EVENT_VIEW_REFRESH);
	return (ac);
}



PR_PUBLIC_API(HT_DropAction)
HT_DropURLOn(HT_Resource dropTarget, char* url)
{
	HT_DropAction		ac;

	ac = dropURLOn(dropTarget, url, NULL, 0);
	resynchContainer (dropTarget);
	refreshItemList(dropTarget, HT_EVENT_VIEW_REFRESH);
	return (ac);
}



void
possiblyCleanUpTitle (char* title)
{
	int16 n = charSearch(-51, title);
	if (n > -1) title[n] = '\0';
}



PR_PUBLIC_API(HT_DropAction)
HT_DropURLAndTitleOn(HT_Resource dropTarget, char* url, char *title)
{
	HT_DropAction		ac;

	possiblyCleanUpTitle(title);
	ac = dropURLOn(dropTarget, url, title, 0);
	resynchContainer (dropTarget);
	refreshItemList(dropTarget, HT_EVENT_VIEW_REFRESH);
	return (ac);
}



PR_PUBLIC_API(HT_DropAction)
HT_CanDropHTRAtPos(HT_Resource dropTarget, HT_Resource obj, PRBool before)
{
	HT_Resource		dropParent;
	HT_View			viewList;
	HT_Pane			pane;
	HT_DropAction		action = DROP_NOT_ALLOWED;

	XP_ASSERT(dropTarget != NULL);
	XP_ASSERT(obj != NULL);
	if (dropTarget == NULL)	return(action);
	if (obj == NULL)	return(action);
	
	dropParent = dropTarget->parent;

	if ((dropParent == NULL) && (obj->parent == NULL))
	{
		/* workspace reordering */

		if (!htIsOpLocked(obj->parent, gNavCenter->RDF_WorkspacePosLock))
		{
			if (before)
			{
				if (!htIsOpLocked(dropTarget, gNavCenter->RDF_WorkspacePosLock))
				{
					action = COPY_MOVE_CONTENT;
				}
			}
			else
			{
				if ((pane = HT_GetPane(HT_GetView(dropTarget))) != NULL)
				{
					viewList = pane->viewList;
					while (viewList != NULL)
					{
						if (HT_TopNode(viewList) == dropTarget)
						{
							viewList = viewList->next;
							break;
						}
						viewList = viewList->next;
					}
					if (viewList != NULL)
					{
						if (!htIsOpLocked(HT_TopNode(viewList), gNavCenter->RDF_WorkspacePosLock))
						{
							action = COPY_MOVE_CONTENT;
						}
					}
				}
			}
		}
	}
	else
	{	
		action = dropOn(dropParent, obj, 1);
		if (action && nlocalStoreHasAssertion(gLocalStore, dropTarget->node,
			gCoreVocab->RDF_parent, dropParent->node, RDF_RESOURCE_TYPE, 1))
		{
		}
		else
		{
			action = DROP_NOT_ALLOWED;
		}
	}
	return (action);
}
 
	
  
PR_PUBLIC_API(HT_DropAction)
HT_CanDropURLAtPos(HT_Resource dropTarget, char *url, PRBool before)
{
	HT_Resource		dropParent;
	HT_DropAction		action = DROP_NOT_ALLOWED;

	XP_ASSERT(dropTarget != NULL);
	XP_ASSERT(url != NULL);
	if (dropTarget == NULL)	return(DROP_NOT_ALLOWED);
	if (url == NULL)	return(DROP_NOT_ALLOWED);

	dropParent = dropTarget->parent;
	action = dropURLOn(dropParent, url, NULL, 1);
	if (action &&  nlocalStoreHasAssertion(gLocalStore, dropTarget->node,
		gCoreVocab->RDF_parent, dropParent->node, RDF_RESOURCE_TYPE, 1))
	{
	}
	else
	{
		action = DROP_NOT_ALLOWED;
	}
	return (action);
}


 
PR_PUBLIC_API(HT_DropAction)
HT_DropHTRAtPos(HT_Resource dropTarget, HT_Resource obj, PRBool before)
{
	HT_DropAction		action = DROP_NOT_ALLOWED;
	uint32			workspacePos;

	XP_ASSERT(dropTarget != NULL);
	XP_ASSERT(obj != NULL);
	if (dropTarget == NULL)	return(DROP_NOT_ALLOWED);
	if (obj == NULL)	return(DROP_NOT_ALLOWED);

	if (dropTarget->parent == NULL)
	{
		if (obj->parent == NULL)
		{
			/* workspace reordering */

			htSetWorkspaceOrder(obj->node, dropTarget->node, !before);
			action = COPY_MOVE_CONTENT;
		}
		else
		{
			/* new workspace, specifying position */

			workspacePos = (HT_GetView(dropTarget))->workspacePos;
			if (before && (workspacePos>0))	--workspacePos;
			htNewWorkspace(HT_GetPane(HT_GetView(dropTarget)),
				resourceID(HT_GetRDFResource(obj)), NULL, workspacePos);
		}
	}
	else
	{
		action = copyMoveRDFLinkAtPos(dropTarget, obj, before);
		resynchContainer (dropTarget->parent);
		refreshItemList(dropTarget, HT_EVENT_VIEW_REFRESH);
	}
	return (action);
}



PR_PUBLIC_API(HT_DropAction)
HT_DropURLAtPos(HT_Resource dropTarget, char* url, PRBool before)
{
	HT_DropAction		ac;

	ac = copyRDFLinkURLAt(dropTarget, url, NULL, before);
	resynchContainer (dropTarget->parent);
	refreshItemList(dropTarget, HT_EVENT_VIEW_REFRESH);
	return (ac);
}



PR_PUBLIC_API(HT_DropAction)
HT_DropURLAndTitleAtPos(HT_Resource dropTarget, char* url, char *title, PRBool before)
{
	HT_DropAction		ac;

	possiblyCleanUpTitle(title);
	ac = copyRDFLinkURLAt(dropTarget, url, title, before);
	resynchContainer (dropTarget->parent);
	refreshItemList(dropTarget, HT_EVENT_VIEW_REFRESH);
	return (ac);
}



PRBool
htRemoveChild(HT_Resource parent, HT_Resource child, PRBool moveToTrash)
{
	RDF_Resource		r;
	RDF_BT			type;

	/* disallow various nodes from being removed */

	if (child->node == gNavCenter->RDF_HistoryBySite ||
	    child->node == gNavCenter->RDF_HistoryByDate)
	{
		return(true);
	}
	if ((r = RDFUtil_GetPTFolder()) != NULL)
	{
		if (child->node == r)	return(true);
	}
	if ((r = RDFUtil_GetNewBookmarkFolder()) != NULL)
	{
		if (child->node == r)	return(true);
	}
	if (htIsOpLocked(child, gNavCenter->RDF_DeleteLock))
	{
		return(true);
	}

	type = resourceType(child->node);
	if ((type == LFS_RT) || (type == ES_RT) || (type == FTP_RT))
	{
		if (moveToTrash == false)
		{
			if (HT_IsContainer(child))
			{
				if (!FE_Confirm(((MWContext *)gRDFMWContext()),
					PR_smprintf(XP_GetString(RDF_DELETEFOLDER),
					resourceID(child->node))))	return(true);
				if (fsRemoveDir( resourceID(child->node), true) == true)
				{
					FE_Alert(((MWContext *)gRDFMWContext()),
						PR_smprintf(XP_GetString(RDF_UNABLETODELETEFOLDER),
						resourceID(child->node)));
					return(true);
				}
			}
			else
			{
				if (!FE_Confirm(((MWContext *)gRDFMWContext()),
					PR_smprintf(XP_GetString(RDF_DELETEFILE),
					 resourceID(child->node))))	return(true);
				if (CallPRWriteAccessFileUsingFileURL(resourceID(child->node)))
				{
					FE_Alert(((MWContext *)gRDFMWContext()),
						PR_smprintf(XP_GetString(RDF_UNABLETODELETEFILE),
						resourceID(child->node)));
					return(true);
				}
			}
		}
		else
		{
			/*
				hack type so that the object can be unasserted
				without the actual file/folder being deleted
			*/
			nlocalStoreUnassert (gLocalStore, child->node,
				gCoreVocab->RDF_parent, parent->node, RDF_RESOURCE_TYPE);
			return(false);
		}
	}

	/* assert into trash? */

	if (moveToTrash == true)
	{
		RDF_Assert(parent->view->pane->db, child->node, gCoreVocab->RDF_parent,
			gNavCenter->RDF_Trash, RDF_RESOURCE_TYPE);
	}
	RDF_Unassert(parent->view->pane->db, child->node, gCoreVocab->RDF_parent,
		parent->node, RDF_RESOURCE_TYPE);
	return(false);
}



PR_PUBLIC_API(PRBool)
HT_RemoveChild (HT_Resource parent, HT_Resource child)
{
	PRBool			retVal;

	retVal = htRemoveChild(parent, child, true);
	refreshPanes(false, false, NULL);
	return(retVal);
}



PR_PUBLIC_API(void*)
HT_GetViewFEData (HT_View view)
{
	return view->pdata;
}



PR_PUBLIC_API(void)
HT_SetViewFEData(HT_View view, void* data)
{
	view->pdata = data;
}



PR_PUBLIC_API(void*)
HT_GetPaneFEData (HT_Pane pane)
{
	return pane->pdata;
}



PR_PUBLIC_API(void)
HT_SetPaneFEData (HT_Pane pane, void* data)
{
	pane->pdata = data;
}



PR_PUBLIC_API(void*)
HT_GetNodeFEData (HT_Resource node)
{
	return node->feData;
}



PR_PUBLIC_API(void)
HT_SetNodeFEData (HT_Resource node, void* data)
{
	node->feData = data;
}



#ifdef	HT_PASSWORD_RTNS



void
ht_SetPassword(HT_Resource node, char *newPassword)
{
	char		*activePassword;

	XP_ASSERT(node != NULL);
	XP_ASSERT(newPassword != NULL);

	if ((activePassword = RDF_GetSlotValue(node->view->pane->db, node->node,
			gNavCenter->RDF_Password, RDF_STRING_TYPE, false, true)) != NULL)
	{
		RDF_Unassert(node->view->pane->db, node->node,
			gNavCenter->RDF_Password, activePassword, RDF_STRING_TYPE);
		freeMem(activePassword);
	}
	if ((newPassword != NULL) && (((char *)newPassword)[0] != '\0'))
	{
		RDF_Assert(node->view->pane->db, node->node, gNavCenter->RDF_Password,
			newPassword, RDF_STRING_TYPE);
	}
	else
	{
		node->flags &= (~HT_PASSWORDOK_FLAG);
	}
}



PRBool
ht_hasPassword(HT_Resource node)
{
	char		*activePassword;
	PRBool		hasPasswordFlag = PR_FALSE;

	if ((activePassword = RDF_GetSlotValue(node->view->pane->db, node->node,
			gNavCenter->RDF_Password, RDF_STRING_TYPE, false, true)) != NULL)
	{
		if ((activePassword != NULL) && (activePassword[0] != '\0'))
		{
			hasPasswordFlag = PR_TRUE;
		}
		freeMem(activePassword);
	}
	return(hasPasswordFlag);
}



PRBool
ht_checkPassword(HT_Resource node, PRBool alwaysCheck)
{
	char		*activePassword, *userPassword, *name;
	PRBool		granted = PR_TRUE;

	XP_ASSERT(node != NULL);
	XP_ASSERT(node->view != NULL);
	XP_ASSERT(node->view->pane != NULL);
	XP_ASSERT(node->view->pane->db != NULL);
	XP_ASSERT(node->node != NULL);


	if ((alwaysCheck == false) && (node->flags & HT_PASSWORDOK_FLAG))	return(granted);

	if ((activePassword = RDF_GetSlotValue(node->view->pane->db, node->node,
			gNavCenter->RDF_Password, RDF_STRING_TYPE, false, true)) != NULL)
	{
		if ((activePassword != NULL) && (activePassword[0] != '\0'))
		{
			granted = PR_FALSE;

			if (!(name = HT_GetNodeName(node)))
			{
				name = 	 resourceID(node->node);
			}

			if ((userPassword = FE_PromptPassword(((MWContext *)gRDFMWContext()),
						PR_smprintf(XP_GetString(RDF_ENTERPASSWORD),
						name))) != NULL)
			{
				if (!strcmp(activePassword, userPassword))
				{
					node->flags |= HT_PASSWORDOK_FLAG;
					granted = PR_TRUE;
				}
				else
				{
					FE_Alert(((MWContext *)gRDFMWContext()), XP_GetString(RDF_MISMATCHPASSWORD));
				}
			}
		}
		freeMem(activePassword);
	}
	return(granted);
}



#endif



PR_PUBLIC_API(HT_Error)
HT_SetSelectedView (HT_Pane pane, HT_View view)
{
	HT_Notification		ns;
	char			*advertURL;

	XP_ASSERT(pane != NULL);
	if (pane == NULL)	return(HT_NoErr);
	XP_ASSERT(pane->db != NULL);

	if (pane->selectedView != view)
	{
#ifdef	HT_PASSWORD_RTNS
		if (view != NULL)
		{
			if (ht_hasPassword(view->top))
			{
				if (ht_checkPassword(view->top, false) == PR_FALSE)	return(HT_Err);
			}
		}
#endif
		pane->selectedView = view;
		if (view != NULL)
		{
			if (!HT_IsContainer(view->top))	return(HT_Err);
			RDFUtil_SetDefaultSelectedView(view->top->node);
			if (view->itemListCount == 0)
			{
				HT_SetOpenState(view->top, PR_TRUE);
#ifdef WIN32
				advertURL = advertURLOfContainer(pane->db, view->top->node);
				if (advertURL != NULL)
				{
					XP_GetURLForView(view, advertURL);
					freeMem(advertURL);
				}
#endif
			} 
			sendNotification(view->top, HT_EVENT_VIEW_SELECTED);
		}
		else
		{
			RDFUtil_SetDefaultSelectedView(NULL);

			/*
			   sendNotification() doesn't accept a NULL node,
			   so send the event directly from here
			*/

			if (pane->mask & HT_EVENT_VIEW_SELECTED)
			{
				if ((ns = pane->ns) != NULL)
				{
					if (ns->notifyProc != NULL)
					{
						(*ns->notifyProc)(ns, NULL,
							HT_EVENT_VIEW_SELECTED);
					}
				}
			}
		}
	}
	return(HT_NoErr);
}



PR_PUBLIC_API(HT_View)
HT_GetSelectedView (HT_Pane pane)
{
	HT_View		view = NULL;

	XP_ASSERT(pane != NULL);

	if (pane != NULL)
	{
		view = pane->selectedView;
	}
	return(view);
}



PR_PUBLIC_API(HT_View)
HT_GetViewType (HT_Pane pane, HT_ViewType viewType)
{
	HT_Resource	node;
	HT_View		view = NULL;
	RDF_Resource	resToFind = NULL;

	XP_ASSERT(pane != NULL);

	switch(viewType)
	{
		case	HT_VIEW_BOOKMARK:
		resToFind = gNavCenter->RDF_BookmarkFolderCategory;
		break;

		case	HT_VIEW_HISTORY:
		resToFind = gNavCenter->RDF_History;
		break;

		case	HT_VIEW_SITEMAP:
		resToFind = gNavCenter->RDF_Sitemaps;
		break;

		default:
		resToFind = NULL;
		break;
	}
	if (resToFind != NULL)
	{
		view = pane->viewList;
		while(view != NULL)
		{
			if ((node = HT_TopNode(view)) != NULL)
			{
				if (node->node == resToFind)
				{
					break;
				}
			}
			view = view->next;
		}
	}
	return(view);
}



PR_PUBLIC_API(HT_Pane)
HT_GetPane (HT_View view)
{
	HT_Pane		pane = NULL;

	XP_ASSERT(view != NULL);
	
	if (view != NULL)
	{
		pane = view->pane;
	}
	return(pane);
}



/*******************************************************************************/
/*                             Drag and Drop Stuff                             */
/*******************************************************************************/



HT_DropAction
dropOn (HT_Resource dropTarget, HT_Resource dropObject, PRBool justAction)
{
	HT_Resource		elders, parent;
	RDF_BT			targetType;
	RDF_BT			objType;

	if (dropTarget == NULL)	return(DROP_NOT_ALLOWED);
	if (dropObject == NULL)	return(DROP_NOT_ALLOWED);

	targetType  = resourceType(dropTarget->node);
	objType     = resourceType(dropObject->node);

	if (!containerp(dropTarget->node))	return DROP_NOT_ALLOWED;
	if (objType == HISTORY_RT)		return DROP_NOT_ALLOWED;

	/* disallow dropping a parent folder into itself or a child folder */
	elders = dropTarget;
	while (elders != NULL)
	{
		if (elders == dropObject)  return DROP_NOT_ALLOWED;
		elders = elders->parent;
	}

	switch (targetType) 
	{
		case SEARCH_RT:
		case HISTORY_RT:
		return DROP_NOT_ALLOWED;
		break;

		case LFS_RT:
#ifdef XP_WIN32
		if (objType == LFS_RT)
		{
			if (justAction)
			{
				return COPY_MOVE_CONTENT;
			}
			else
			{
				parent = dropObject->parent;
				Win32FileCopyMove(dropTarget, dropObject);
				destroyViewInt(dropTarget, PR_FALSE);
				refreshItemList (dropTarget, HT_EVENT_VIEW_REFRESH);
				destroyViewInt(parent, PR_FALSE);
				refreshItemList (parent, HT_EVENT_VIEW_REFRESH);
				return COPY_MOVE_CONTENT;
			} 
		}
		else
		return DROP_NOT_ALLOWED;
#else
		return DROP_NOT_ALLOWED;
#endif
		break;
  
		case FTP_RT:
		case ES_RT:
		if ((objType == ES_RT) || (objType == FTP_RT))
		{
			if (justAction)
			{
				return COPY_MOVE_CONTENT;
			}
			else
			{
				return esfsCopyMoveContent(dropTarget, dropObject);
			}
		}
		else if (containerp(dropObject->node) && (objType == RDF_RT))
		{
			if (justAction)
			{
				return UPLOAD_RDF;
			}
			else
			{
				return uploadRDFFile(dropTarget, dropObject);
			}
		}
		else if (objType == LFS_RT)
		{
			if (justAction)
			{
				return UPLOAD_LFS;
			}
			else
			{
				return uploadLFS(dropTarget, dropObject);
			}
		}
		return DROP_NOT_ALLOWED;
		break;

		case LDAP_RT:
		case RDF_RT :
		if (justAction)
		{
			return COPY_MOVE_LINK;
		}
		else
		{
			return copyMoveRDFLink(dropTarget, dropObject);
		}
	}
	return DROP_NOT_ALLOWED;
}



#ifdef XP_WIN32

/* We forgot we were not supposed to implement this */

#include "windows.h"
#include "xp_file.h"
#include "shellapi.h"

void
Win32FileCopyMove(HT_Resource dropTarget, HT_Resource dropObject)
{
	char		*from =  resourceID(dropObject->node);
	char		*to =  resourceID(dropTarget->node);
	char		*lffrom;
	char		*lfto;
	char		*finalFrom;
	char		*finalTo;
	size_t		fl,ft;
	SHFILEOPSTRUCT	action;

	memset(&action, 0, sizeof(action));
	XP_ConvertUrlToLocalFile(from, &lffrom) ;
	XP_ConvertUrlToLocalFile(to, &lfto);

	fl = strlen(lffrom);
	ft = strlen(lfto);

	finalFrom = (char *)getMem(fl+2);
	finalTo = (char *)getMem(ft+2);

	strcpy(finalFrom, lffrom);
	strcpy(finalTo, lfto);

	finalFrom[fl+1] = '\0';
	finalTo[ft+1] = '\0';

	action.hwnd = NULL;
	action.wFunc = (from[8] == to[8] ? FO_MOVE : FO_COPY);
	action.pFrom = finalFrom;
	action.pTo = finalTo;
	action.fFlags = FOF_ALLOWUNDO;

	SHFileOperation(&action);

	freeMem(finalFrom);
	freeMem(finalTo);

}
#endif



HT_DropAction
copyMoveRDFLink (HT_Resource dropTarget, HT_Resource dropObject)
{
	HT_Resource		origin;
	RDF			db;
	RDF_Resource		old,new,obj;
	PRBool			moveAction;
	char			*name;

	XP_ASSERT(dropTarget != NULL);
	XP_ASSERT(dropObject != NULL);
	if ((dropTarget == NULL) || (dropObject == NULL))	return(DROP_NOT_ALLOWED);

	origin = dropObject->parent;
	db = dropTarget->view->pane->db;
	moveAction = ((origin != NULL) && 
			(origin->view->top->node == dropTarget->view->top->node));
	old = (origin == NULL ? NULL : origin->node);
	new = dropTarget->node;
	obj = dropObject->node;
	name = RDF_GetSlotValue(db, obj, gCoreVocab->RDF_name, RDF_STRING_TYPE, 0, 1);
	if (name != NULL)
	{
		RDF_Assert(db, obj, gCoreVocab->RDF_name, name, RDF_STRING_TYPE);
		freeMem(name);
	}

	/* drag&drops between personal toolbar and bookmark view should be a move action */
	if ((!moveAction) && (origin != NULL))
	{
		if ((origin->view->pane->personaltoolbar == true && 
			dropTarget->view->top->node == gNavCenter->RDF_BookmarkFolderCategory) || 
			((dropTarget->view->pane->personaltoolbar == true && 
			origin->view->top->node == gNavCenter->RDF_BookmarkFolderCategory)))
		{
			moveAction = PR_TRUE;
		}
	}

	
	/* ensure object can copy/move, and target accepts additions */
	if (htIsOpLocked(dropObject, ((moveAction) ? 
		gNavCenter->RDF_MoveLock : gNavCenter->RDF_CopyLock)))
	{
		return(DROP_NOT_ALLOWED);
	}
	if (htIsOpLocked(dropTarget, gNavCenter->RDF_AddLock))
	{
		return(DROP_NOT_ALLOWED);
	}

	if (moveAction && old)
	{
		RDF_Unassert(db, obj, gCoreVocab->RDF_parent, old, RDF_RESOURCE_TYPE);
	}
	RDF_Assert(db, obj, gCoreVocab->RDF_parent, new, RDF_RESOURCE_TYPE);
	return COPY_MOVE_LINK;      
}



HT_DropAction
copyMoveRDFLinkAtPos (HT_Resource dropx, HT_Resource dropObject, PRBool before)
{
	HT_Resource		origin;
	HT_Resource		dropTarget;
	RDF			db;
	RDF_Resource		old;
	RDF_Resource		parent;
	RDF_Resource		obj;
	PRBool			moveAction;
	char			*name;

	if (dropx == NULL)	return(DROP_NOT_ALLOWED);
	if (dropObject == NULL)	return(DROP_NOT_ALLOWED);

	origin = dropObject->parent;
	dropTarget = dropx->parent;
	db = dropTarget->view->pane->db;
	moveAction = ((origin != NULL) && 
			(origin->view->top->node == dropTarget->view->top->node));
	old = (origin == NULL ? NULL : origin->node);
  	parent = dropTarget->node;
	obj = dropObject->node;
	name = RDF_GetSlotValue(db, obj, gCoreVocab->RDF_name, RDF_STRING_TYPE, 0, 1);
	if (name != NULL)
	{
		RDF_Assert(db, obj, gCoreVocab->RDF_name, copyString(name), RDF_STRING_TYPE);
		freeMem(name);
	}

	/* drag&drops between personal toolbar and bookmark view should be a move action */
	if ((!moveAction) && (origin != NULL))
	{
		if ((origin->view->pane->personaltoolbar == true && 
			dropTarget->view->top->node == gNavCenter->RDF_BookmarkFolderCategory) || 
			((dropTarget->view->pane->personaltoolbar == true && 
			origin->view->top->node == gNavCenter->RDF_BookmarkFolderCategory)))
		{
			moveAction = PR_TRUE;
		}
	}

	
	/* ensure object can copy/move, and target accepts additions */
	if (htIsOpLocked(dropObject, ((moveAction) ? 
		gNavCenter->RDF_MoveLock : gNavCenter->RDF_CopyLock)))
	{
		return(DROP_NOT_ALLOWED);
	}
	if (htIsOpLocked(dropTarget, gNavCenter->RDF_AddLock))
	{
		return(DROP_NOT_ALLOWED);
	}

	if (moveAction && old)
	{
		RDF_Unassert(db, obj, gCoreVocab->RDF_parent, old, RDF_RESOURCE_TYPE);
	}
	nlocalStoreUnassert(gLocalStore, obj, gCoreVocab->RDF_parent, parent, RDF_RESOURCE_TYPE);
	nlocalStoreAddChildAt(gLocalStore, parent, dropx->node, obj, before);
	return COPY_MOVE_LINK;      
}



HT_DropAction
uploadLFS (HT_Resource dropTarget, HT_Resource dropObject)
{
	XP_ASSERT(dropTarget != NULL);
	XP_ASSERT(dropObject != NULL);

	if ((dropTarget != NULL) && (dropObject != NULL))
	{
		RDF_Assert(gNCDB, dropObject->node, gCoreVocab->RDF_parent,
			dropTarget->node, RDF_RESOURCE_TYPE);
	}
	return 0;
}



HT_DropAction
uploadRDFFile (HT_Resource dropTarget, HT_Resource dropObject)
{
	return 0;
} 



HT_DropAction
esfsCopyMoveContent (HT_Resource dropTarget, HT_Resource dropObject)
{
	return 0;
}



RDF_BT
urlResourceType (char* url)
{
	if (startsWith("file:", url)) return LFS_RT;
	return RDF_RT;
}



HT_DropAction
dropURLOn (HT_Resource dropTarget, char* objURL, char *objTitle, PRBool justAction)
{
	RDF_BT			targetType;
	RDF_BT			objType;

	if (dropTarget == NULL)	return(DROP_NOT_ALLOWED);
	if (objURL == NULL)	return(DROP_NOT_ALLOWED);

	targetType  = resourceType(dropTarget->node);
	objType     = urlResourceType(objURL);

	if (!containerp(dropTarget->node)) return DROP_NOT_ALLOWED;
	if (objType == HISTORY_RT) return DROP_NOT_ALLOWED;

	switch (targetType) 
	{
		case LFS_RT:
		case SEARCH_RT:
		case HISTORY_RT:
		return 0;
		break;
  
		case FTP_RT:
		case ES_RT:
		if ((objType == ES_RT) || (objType == FTP_RT))
		{
			if (justAction)
			{
				return COPY_MOVE_CONTENT;
			}
			else
			{
				return esfsCopyMoveContentURL(dropTarget, objURL);
			}
		}
		else if (0 /*containerp(objURL) can never be true */ && (objType == RDF_RT))
		{
			if (justAction)
			{
				return UPLOAD_RDF;
			}
			else
			{
				return uploadRDFFileURL(dropTarget, objURL);
			}
		}
		else if (objType == LFS_RT)
		{
			if (justAction)
			{
				return UPLOAD_LFS;
			}
			else
			{
				return uploadLFSURL(dropTarget, objURL);
			}
		}
		return DROP_NOT_ALLOWED;
		break;

		case LDAP_RT:
		case RDF_RT :
		if (justAction)
		{
			return COPY_MOVE_LINK;
		}
		else
		{
			return copyRDFLinkURL(dropTarget, objURL, objTitle);
		}
	}
	return DROP_NOT_ALLOWED;
}



void
replacePipeWithColon(char* url)
{
	size_t			n=0, size;

	XP_ASSERT(url != NULL);

	size = strlen(url);
	n = 0;
	while (n < size)
	{
		if (url[n] == '|')
		{
			url[n] = ':';
		}
		n++;
	}
}



HT_DropAction
copyRDFLinkURL (HT_Resource dropTarget, char* objURL, char *objTitle)
{
	RDF_Resource		new, obj;
	RDF			db;

	new = dropTarget->node;
	db = dropTarget->view->pane->db;
	replacePipeWithColon(objURL);
	obj = RDF_GetResource(db, objURL, 1);

	if (objTitle != NULL)
	{
		RDF_Assert(db, obj, gCoreVocab->RDF_name, objTitle, RDF_STRING_TYPE);
	}
	RDF_Assert(db, obj, gCoreVocab->RDF_parent, new, RDF_RESOURCE_TYPE);
	return COPY_MOVE_LINK;      
}



HT_DropAction
copyRDFLinkURLAt (HT_Resource dropx, char* objURL, char *objTitle, PRBool before)
{
	HT_Resource		dropTarget;
	RDF			db;
	RDF_Resource		parent,obj;

	dropTarget = dropx->parent;
	db = dropTarget->view->pane->db;
	parent = dropTarget->node;
	replacePipeWithColon(objURL);
	obj = RDF_GetResource(db, objURL, 1);

	if (objTitle != NULL)
	{
		RDF_Assert(db, obj, gCoreVocab->RDF_name, objTitle, RDF_STRING_TYPE);
	}
	nlocalStoreUnassert(gLocalStore, obj, gCoreVocab->RDF_parent, parent, RDF_RESOURCE_TYPE);
	nlocalStoreAddChildAt(gLocalStore, parent, dropx->node, obj, before);
	return COPY_MOVE_LINK;      
}



HT_DropAction
uploadLFSURL (HT_Resource dropTarget, char* objURL)
{
	RDF_Resource		r;

	XP_ASSERT(dropTarget != NULL);
	XP_ASSERT(objURL != NULL);

	if ((dropTarget != NULL) && (objURL != NULL))
	{
		if ((r = RDF_GetResource(gNCDB, objURL, 1)) != NULL)
		{
			RDF_Assert(gNCDB, r, gCoreVocab->RDF_parent,
				dropTarget->node, RDF_RESOURCE_TYPE);
		}
	}
	return 0;
}



HT_DropAction
uploadRDFFileURL (HT_Resource dropTarget, char* objURL)
{
	return 0;
}



HT_DropAction
esfsCopyMoveContentURL (HT_Resource dropTarget, char* objURL)
{
	return 0;
}



PR_PUBLIC_API(RDF)
RDF_GetNavCenterDB()
{
	return gNCDB;
}


#define RDF_SITEMAP 1
#define RDF_RELATED_LINKS 2
#define FROM_PAGE 1
#define GUESS_FROM_PREVIOUS_PAGE 2
#define HTADD remoteStoreAdd
#define HTDEL remoteStoreRemove

HT_URLSiteMapAssoc *
makeNewSMP (HT_Pane htPane, char* pUrl, char* sitemapUrl)
{
	HT_URLSiteMapAssoc	*nm;
        HT_URLSiteMapAssoc *nsmp = htPane->smp;
	while (nsmp != NULL) {
       if (stringEquals(nsmp->url, pUrl) && stringEquals(nsmp->url, sitemapUrl)) return nsmp;
	   nsmp = nsmp->next;
	}

	if ((nm = (HT_URLSiteMapAssoc*)getMem(sizeof(HT_URLSiteMapAssoc))) != NULL)
	{
		nm->sitemapUrl = copyString(sitemapUrl);
		nm->url = copyString(pUrl);

	}
	return(nm);
}



PR_PUBLIC_API(void)
HT_AddSitemapFor(HT_Pane htPane, char *pUrl, char *pSitemapUrl, char* name)
{
	HT_URLSiteMapAssoc		*nsmp;
	RDF_Resource			nu;
	RDFT				sp;
	char				*nm;

	
	sp = RDFTNamed(htPane->db, "rdf:ht");
	if (sp == NULL) return;
	nu = RDF_GetResource(htPane->db, pSitemapUrl, 1);
	nsmp = makeNewSMP(htPane, pUrl, pSitemapUrl);
        nsmp->sitemap = nu;
        nsmp->next = htPane->smp;
        htPane->smp = nsmp;
	if (name != NULL) {
		nm = copyString(name);
	}
	else {
		nm = copyString(XP_GetString(RDF_SITEMAPNAME));
	}
	nsmp->siteToolType = RDF_SITEMAP;
    if (name) nsmp->name = copyString(name);
    nsmp->sitemapUrl = copyString(pSitemapUrl);
	
	setContainerp(nu, 1);
    
    nsmp->origin =  FROM_PAGE;
    nsmp->onDisplayp = 1;
	HTADD(sp, nu, gCoreVocab->RDF_name, nm,  RDF_STRING_TYPE, 1);
	HTADD(sp, nu, gCoreVocab->RDF_parent, gNavCenter->RDF_Top, 
                     RDF_RESOURCE_TYPE, 1);
	 

}



void
RetainOldSitemaps (HT_Pane htPane, char *pUrl)
{
  HT_URLSiteMapAssoc	*nsmp;
  RDFT			sp;
  
  sp = RDFTNamed(htPane->db, "rdf:ht");
  if (sp == NULL) return;
  nsmp = htPane->smp;
  while (nsmp != NULL) {
    if ((nsmp->siteToolType == RDF_SITEMAP)) {
		if (startsWith(nsmp->url, pUrl) && 
			(!startsWith("file:", nsmp->url))) 
        { 
          if (!nsmp->onDisplayp) {	
            RDF_Resource nu = RDF_GetResource(htPane->db, nsmp->sitemapUrl, 1);
            nsmp->sitemap = nu;
            nsmp->origin =  GUESS_FROM_PREVIOUS_PAGE;
            nsmp->onDisplayp = 1;
            HTADD(sp, nu, gCoreVocab->RDF_name, copyString(nsmp->name),  
                         RDF_STRING_TYPE, 1);
            HTADD(sp, nu, gCoreVocab->RDF_parent, 
                         gNavCenter->RDF_Top, RDF_RESOURCE_TYPE, 1);
          }
        } else if (nsmp->onDisplayp) {
          HTDEL(sp, nsmp->sitemap, gCoreVocab->RDF_parent,
                        gNavCenter->RDF_Top, RDF_RESOURCE_TYPE);
          nsmp->onDisplayp = 0;
        }
    }
    nsmp = nsmp->next;
  }
}



PR_PUBLIC_API(void)
HT_ExitPage(HT_Pane htPane, char *pUrl)
{
  HT_URLSiteMapAssoc	*nsmp;
  RDFT			sp;
  
  sp = RDFTNamed(htPane->db, "rdf:ht");
  nsmp = htPane->sbp;
  while (nsmp != NULL) {    
    HT_URLSiteMapAssoc *next;
    HTDEL(sp, nsmp->sitemap, gCoreVocab->RDF_parent,
                  gNavCenter->RDF_Sitemaps, RDF_RESOURCE_TYPE);      
    next = nsmp->next;
    freeMem(nsmp->url);
    freeMem(nsmp->sitemapUrl);
    freeMem(nsmp);
    nsmp = next;
  }

  freeMem(htPane->windowURL);
  htPane->windowURL = NULL;
  htPane->sbp = NULL;
}



void
populateSBProviders (HT_Pane htPane)
{
	RDF_Cursor			c; 
	RDF_Resource			prov;
	RDF				db;
	SBProvider			sb;

	XP_ASSERT(htPane != NULL);

	db = htPane->db;
	c =  RDF_GetSources(htPane->db,  gNavCenter->RDF_SBProviders,
				gCoreVocab->RDF_parent,	RDF_RESOURCE_TYPE, 1);
	while (prov = RDF_NextValue(c))
	{
		sb = (SBProvider)getMem(sizeof(SBProviderStruct));
		sb->name = RDF_GetResourceName(db, prov);
		sb->url  = copyString(resourceID(prov));
		sb->containerp = (!RDF_HasAssertion(db, prov, gNavCenter->RDF_resultType, 
		              "TEXT/HTML", RDF_STRING_TYPE, 1));
		sb->next = htPane->smartBrowsingProviders;
		htPane->smartBrowsingProviders = sb;
	}
	RDF_DisposeCursor(c);
}



SBProvider
SBProviderOfNode (HT_Resource node)
{
	HT_Pane			htPane;
	SBProvider		prov;

	XP_ASSERT(node != NULL);

	htPane = node->view->pane;
	prov = htPane->smartBrowsingProviders;
	while (prov)
	{
		if (startsWith(prov->url, resourceID(node->node)))
		{
			return prov;
		}
		prov = prov->next;
	}
	return NULL;
}



PRBool
implicitDomainURL (char* url)
{
	uint16			n = 7, size;

	XP_ASSERT(url != NULL);

	size = strlen(url);
	while (n < size)
	{
		if (url[n] == '/') return 1;
		if (url[n] == '.') return 0;
		n++;
	}
	return 1;
}



PRBool
domainMatches (char *dom, char *url)
{
	size_t n = 0, m = 0, ns, ms;

	XP_ASSERT(dom != NULL);
	XP_ASSERT(url != NULL);

	ns = strlen(dom);
	ms = strlen(url);
	while ((n < ns) && (m < ms))
	{
		if (dom[n] == '*')
		{
			n++;
			while (m < ms)
			{
				if (url[m] == '.') break; 
				m++;
			}
		}
		else if (url[m] == '/')
		{
			return 0;
		}
		else if (url[m] != dom[n])
		{
			return 0;
		}
		else
		{
			m++;
			n++;
		}
	}
	return 1;
}



void
nextDomain (char* dom, size_t *n)
{
	PRBool			somethingSeenp = 0;
	size_t			m = 0;
	uint16			ns;
	char			c;

	ns = strlen(gRLForbiddenDomains);
	memset(dom, '\0', 100);
	while (*n < ns)
	{
		c = gRLForbiddenDomains[*n];
		if (c != ' ') somethingSeenp = 1;
		if (c == ',')
		{
			*n= (*n)+1;
			return;
		}
		if (somethingSeenp) dom[m++] = c;
		*n = (*n)+1;
	}
}



PRBool
relatedLinksEnabledURL (char* url)
{
	size_t			n = 0, ns;
	char			dom[100];

	if (url == NULL) return 0;
	if (strlen(url) > 100) return 0; 
	if (!startsWith("http:", url))
	{
		return 0;
	}
	else if (implicitDomainURL(url))
	{
		return 0;
	}
	else if (gRLForbiddenDomains != NULL)
	{
		ns = strlen(gRLForbiddenDomains);
		while (n < ns)
		{
			nextDomain(dom, &n);
			if (dom[0] != '\0' && domainMatches(dom, &url[7]))
			{
				return 0;
			}
		}
		return 1;
	}
	else return 1;
}



PR_PUBLIC_API(void)
HT_AddRelatedLinksFor(HT_Pane htPane, char *pUrl)
{
	HT_URLSiteMapAssoc	*nsmp;
	RDF_Resource		nu;
	RDFT			sp;
	SBProvider		prov;
	char			*buffer;


	sp = RDFTNamed(htPane->db, "rdf:ht");
	if (!htPane->smartBrowsingProviders) populateSBProviders(htPane);
	if (!relatedLinksEnabledURL(pUrl)) return;
	prov = htPane->smartBrowsingProviders;
	while (prov)
	{
		buffer = getMem(strlen(prov->url) + strlen(pUrl));
		sprintf(buffer, "%s%s", prov->url,  &pUrl[7]);
		nu = RDF_GetResource(htPane->db, buffer, 1);
		setContainerp(nu, prov->containerp);
                setResourceType(nu, RDF_RT);
		nsmp = makeNewSMP(htPane, pUrl, buffer);
                nsmp->sitemap = nu;
                nsmp->next = htPane->sbp;
                htPane->sbp = nsmp;
		nsmp->siteToolType = RDF_RELATED_LINKS;
		HTADD(sp, nu, gCoreVocab->RDF_name, copyString(prov->name), RDF_STRING_TYPE, 1);
		HTADD(sp, nu, gCoreVocab->RDF_parent, gNavCenter->RDF_Sitemaps, RDF_RESOURCE_TYPE, 1);
		prov = prov->next;
		freeMem(buffer);
	}
        htPane->windowURL = copyString(pUrl);
	RetainOldSitemaps(htPane, pUrl);

}




PR_PUBLIC_API(PRBool)
HT_HasHTMLPane(HT_View htView)
{
	HT_Resource		top;
	PRBool			hasHTML = PR_FALSE;
	char			*url = NULL;
  
	XP_ASSERT(htView != NULL);

	if (htView != NULL)
	{
		if ((top = HT_TopNode(htView)) != NULL)
		{
			HT_GetNodeData (top, gNavCenter->RDF_HTMLURL, HT_COLUMN_STRING, (void *)&url);
			if (url != NULL)
			{
				hasHTML = PR_TRUE;
			}
		}
	}
	return(hasHTML);
}



PR_PUBLIC_API(char *)
HT_HTMLPaneHeight(HT_View htView)
{
	HT_Resource		top;
	PRBool			hasHTML = PR_FALSE;
	char			*paneHeightStr = NULL;
  
	XP_ASSERT(htView != NULL);

	if (htView != NULL)
	{
		if (HT_HasHTMLPane(htView))
		{
			if ((top = HT_TopNode(htView)) != NULL)
			{
				HT_GetNodeData (top, gNavCenter->RDF_HTMLHeight, HT_COLUMN_STRING, (void *)&paneHeightStr);
			}
		}
	}
	return(paneHeightStr);
}
