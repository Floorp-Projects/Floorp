/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   This file implements (Mac) Appletalk support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "atalk.h"

	/* externs */
void	startAsyncCursors();
void	stopAsyncCursors();


	/* globals */
static PRHashTable	*atalk2rdfHash;
static PRHashTable	*invatalk2rdfHash;
static RDFT		gRDFDB = NULL;



void
getZones()
{
	XPPParamBlock		xBlock;
	OSErr			err = noErr;

	startAsyncCursors();

	XP_BZERO(&xBlock, sizeof(xBlock));
	xBlock.XCALL.xppTimeout = 3;
	xBlock.XCALL.xppRetry = 4;
	xBlock.XCALL.zipBuffPtr = getMem(578);
	xBlock.XCALL.zipLastFlag = 0;
	xBlock.XCALL.zipInfoField[0] = 0;
	xBlock.XCALL.zipInfoField[1] = 0;
	while ((err == noErr) && (xBlock.XCALL.zipBuffPtr != NULL) && (!xBlock.XCALL.zipLastFlag))
	{
		err = GetZoneList(&xBlock, FALSE);
		if (!err)
		{
			processZones(xBlock.XCALL.zipBuffPtr, xBlock.XCALL.zipNumZones);
		}
	}
	if (xBlock.XCALL.zipBuffPtr != NULL)
	{
		freeMem(xBlock.XCALL.zipBuffPtr);
		xBlock.XCALL.zipBuffPtr = NULL;
	}

	stopAsyncCursors();
}



void
processZones(char *zones, uint16 numZones)
{
	RDF_Resource		r, parent;
	char			*escapedURL, url[1024], virtualURL[1024], *p;
	uint16			loop;

	XP_ASSERT(zones != NULL);
	if (zones == NULL)	return;

	for (loop=0; loop<numZones; loop++)
	{
		parent = gNavCenter->RDF_Appletalk;

		XP_SPRINTF(url, zones+1, (unsigned)zones[0]);
		url[(unsigned)zones[0]] = '\0';

		p = url;
		while (p != NULL)
		{
			p = XP_STRCHR(p, ' ');
			if (p != NULL)	*p = '\0';
			escapedURL = NET_Escape(url, URL_XALPHAS);	/* URL_PATH */
			if (escapedURL == NULL)	return;
			if (p != NULL)	*p++ = ' ';

			virtualURL[0] = '\0';
			if (p != NULL)	XP_STRCAT(virtualURL, "virtual");
			XP_STRCAT(virtualURL, "zone://");
			XP_STRCAT(virtualURL, escapedURL);
			XP_FREE(escapedURL);

			if ((r = RDF_GetResource(NULL, virtualURL, PR_TRUE)) != NULL)
			{
				if (startsWith("zone://", virtualURL))
				{
					setResourceType(r, ATALK_RT);
				}
				else
				{
					setResourceType(r, ATALKVIRTUAL_RT);
				}
			}
			if (r == NULL)	break;

			setContainerp(r, PR_TRUE);
			AtalkAdd(gRDFDB, r, gCoreVocab->RDF_parent,
				parent, RDF_RESOURCE_TYPE);
			parent = r;
		}

#if 0		XP_SPRINTF(url, "zone://");
		XP_STRNCAT(url, zones+1, (unsigned)zones[0]);
		p = &url[7];

		parent = gNavCenter->RDF_Appletalk;
		while (p != NULL)
		{
			r = NULL;
			if ((q = XP_STRCHR(p,' ')))
			{
				*q = '\0';
				escapedURL =  NET_Escape(url + strlen("zone://"), URL_XALPHAS);	/* URL_PATH */
				if (escapedURL != NULL)
				{
					strcpy(virtualUrl, "virtualzone://");
					strcat(virtualUrl, escapedURL);
					if ((r = RDF_GetResource(NULL, escapedURL, PR_TRUE)) != NULL)
					{
						setResourceType(r, ATALKVIRTUAL_RT);
					}
					XP_FREE(escapedURL);
				}
			}
			else
			{
				escapedURL = NET_Escape(url + strlen("zone://"), URL_XALPHAS);  /* URL_PATH */
				if (escapedURL != NULL)
				{
					strcpy(virtualUrl, "zone://");
					strcat(virtualUrl, escapedURL);
					if ((r = RDF_GetResource(NULL, virtualUrl, PR_TRUE)) != NULL)
					{
						setResourceType(r, ATALK_RT);
					}
					XP_FREE(escapedURL);
				}
				q = NULL;
			}			
			if (r == NULL)	break;

			setContainerp(r, PR_TRUE);
			AtalkAdd(gRDFDB, r, gCoreVocab->RDF_parent,
				parent, RDF_RESOURCE_TYPE);
			parent = r;
			
			if (q == NULL)	break;
			*q++ = ' ';
			p=q;
		}
#endif

		zones += (1 + (unsigned)zones[0]);
	}
}



void
checkServerLookup (MPPParamBlock *nbp)
{
	AddrBlock		address;
	EntityName		entity;
	RDF_Resource		r, parent;
	char			*escapedURL, afpUrl[128], url[128];
	uint16			loop;
	OSErr			err;

	if (nbp != NULL)
	{
		switch(nbp->MPPioResult)
		{
			case	1:
			/* poll once a second */
			FE_SetTimeout((void *)checkServerLookup, (void *)nbp, 1000);
			break;

			case	0:
			for (loop=0; loop<nbp->NBPnumGotten; loop++)
			{
				err = NBPExtract(nbp->NBPretBuffPtr, nbp->NBPnumGotten, loop, &entity, &address);
				if (!err)
				{
					strncpy(url, &((char *)&entity)[1], ((char *)&entity)[0]);
					url[((char *)&entity)[0]] = '\0';
					escapedURL = NET_Escape(url, URL_XALPHAS);  /* URL_PATH */
					if (escapedURL != NULL)
					{
						strcpy(afpUrl, "afp://");
						strcat(afpUrl, escapedURL);
						strcat(afpUrl, ":AFPServer@");
						strcat(afpUrl, ((char *)nbp->NBP.userData) + strlen("zone://"));

						if ((parent = RDF_GetResource(NULL, (char *)(nbp->NBP.userData), PR_TRUE)) != NULL)
						{
							if ((r = RDF_GetResource(NULL, afpUrl, PR_TRUE)) != NULL)
							{
								setResourceType(r, ATALK_RT);
								AtalkAdd(gRDFDB, r, gCoreVocab->RDF_parent,
									parent, RDF_RESOURCE_TYPE);
							}
						}
						XP_FREE(escapedURL);
					}
				}
			}
			/* note: no "break" here! If operation finished (success or failure), free up memory */

			default:
			if (nbp->NBPretBuffPtr != NULL)	freeMem(nbp->NBPretBuffPtr);
			if (nbp->NBPentityPtr != NULL)	freeMem(nbp->NBPentityPtr);
			if (nbp->NBP.userData != NULL)	freeMem((char *)nbp->NBP.userData);
			freeMem(nbp);
			break;
		}
	}
}



void
getServers(RDF_Resource parent)
{
	EntityName		*entity;
	MPPParamBlock		*nbp;
	OSErr			err = noErr;
	char			*buffer, *parentID, *zone, url[128];

	if (!startsWith("zone://", resourceID(parent)))	return;

	nbp = (MPPParamBlock *)getMem(sizeof(MPPParamBlock));
	entity = (EntityName *)getMem(sizeof(EntityName));
	buffer = getMem(10240);
	zone = unescapeURL(resourceID(parent)+strlen("zone://"));
	parentID = copyString(resourceID(parent));

	if ((nbp != NULL) && (entity != NULL) && (buffer != NULL) && (zone != NULL) && (parentID != NULL))
	{
		url[0] = strlen(zone);
		strcpy(&url[1], zone);
		NBPSetEntity((char *)entity, "\p=", "\pAFPServer", (void *)url);
		nbp->MPPioCompletion = NULL;
		nbp->NBPinterval = 5;
		nbp->NBPcount = 3;
		nbp->NBPentityPtr = (char *)entity;
		nbp->NBPretBuffPtr = buffer;
		nbp->NBPretBuffSize = 10240;
		nbp->NBPmaxToGet = 256;
		nbp->NBP.userData = (long)parentID;
		err = PLookupName(nbp, TRUE);

		/* poll once a second */
		FE_SetTimeout((void *)checkServerLookup, (void *)nbp, 1000);
	}
	if (err < 0)
	{
		if (nbp != NULL)	freeMem(nbp);
		if (zone != NULL)	freeMem(zone);
		if (buffer != NULL)	freeMem(buffer);
		if (parentID != NULL)	freeMem(parentID);
	}
}



Assertion
atalkarg1 (RDF_Resource u)
{
	return (Assertion) PR_HashTableLookup(atalk2rdfHash, u);
}



Assertion
setatalkarg1 (RDF_Resource u, Assertion as)
{
	return (Assertion) PR_HashTableAdd(atalk2rdfHash, u, as);
}



Assertion
atalkarg2 (RDF_Resource u)
{
	return (Assertion) PR_HashTableLookup(invatalk2rdfHash, u);
}



Assertion
setatalkarg2 (RDF_Resource u, Assertion as)
{
	return (Assertion) PR_HashTableAdd(invatalk2rdfHash, u, as);
}



PRBool
AtalkHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type, PRBool tv)
{
	Assertion		nextAs;

	if (startsWith("zone://", resourceID(u)))
	{
		nextAs = atalkarg1(u);
		while (nextAs != null)
		{
			if (asEqual(nextAs, u, s, v, type) && (nextAs->tv == tv)) return true;
			nextAs = nextAs->next;
		}

		if (s == gCoreVocab->RDF_parent || s == gCoreVocab->RDF_child)
		{
			if (atalkarg1(u) == NULL)
			{
				getServers(u);
			}
		}

		nextAs = atalkarg1(u);
		while (nextAs != null)
		{
			if (asEqual(nextAs, u, s, v, type) && (nextAs->tv == tv)) return true;
			nextAs = nextAs->next;
		}
	}
	return(PR_FALSE);
}



void *
AtalkGetSlotValue(RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,
		PRBool inversep, PRBool tv)
{
	void			*val = NULL;
	char			*url;

	if (u == NULL)	return(NULL);

	if ((s == gCoreVocab->RDF_name) && ((resourceType(u) == ATALK_RT) ||
		(resourceType(u) == ATALKVIRTUAL_RT)) && (type == RDF_STRING_TYPE) && (tv))
	{
		if (u == gNavCenter->RDF_Appletalk)
		{
			/* XXX localization */

			val = copyString("Appletalk Zones and File Servers");
		}
		else if (startsWith("virtualzone://", resourceID(u)))
		{
			if ((url = unescapeURL(resourceID(u) + strlen("virtualzone://"))) != NULL)
			{
				if ((val = XP_STRRCHR(url, ' ')) != NULL)
				{
					val = copyString(((char *)val) + 1);
				}
				else
				{
					val = copyString(url);
				}
				freeMem(url);
			}
		}
		else if (startsWith("zone://", resourceID(u)))
		{
			if ((url = unescapeURL(resourceID(u) + strlen("zone://"))) != NULL)
			{
				if ((val = XP_STRRCHR(url, ' ')) != NULL)
				{
					val = copyString(((char *)val) + 1);
				}
				else
				{
					val = copyString(url);
				}
				freeMem(url);
			}
		}
		else if (startsWith("afp://", resourceID(u)))
		{
			if ((url = unescapeURL(resourceID(u) + strlen("afp://"))) != NULL)
			{
				if ((val = XP_STRRCHR(url, ':')) != NULL)
				{
					*(char *)val = '\0';
				}
				val = copyString(url);
				freeMem(url);
			}
		}
	}
	return(val);
}



PRBool
AtalkAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
	Assertion		nextAs, prevAs, newAs;

	if ((s == gCoreVocab->RDF_instanceOf) && (v == gWebData->RDF_Container))
	{
		setContainerp(u, true);
		return 1;
	}
	nextAs = prevAs = u->rarg1;
	while (nextAs != null)
	{
		if (asEqual(nextAs, u, s, v, type)) return 1;
		prevAs = nextAs;
		nextAs = nextAs->next;
	}
	newAs = makeNewAssertion(u, s, v, type, 1);
	if (prevAs == null)
	{
		u->rarg1 = newAs;
	}
	else
	{
		prevAs->next = newAs;
	}
	if (type == RDF_RESOURCE_TYPE)
	{
		nextAs = prevAs = ((RDF_Resource)v)->rarg2;
		while (nextAs != null)
		{
			prevAs = nextAs;
			nextAs = nextAs->invNext;
		}
		if (prevAs == null)
		{
			((RDF_Resource)v)->rarg2 = newAs;
		}
		else
		{
			prevAs->invNext = newAs;
		}
	}
	sendNotifications2(rdf, RDF_INSERT_NOTIFY, u, s, v, type, 1);
	return true;
}



void *
AtalkNextValue (RDFT rdf, RDF_Cursor c)
{
	Assertion		as;

	XP_ASSERT(c != NULL);
	if (c == NULL)		return(NULL);

	while (c->pdata != null)
	{
		as = (Assertion) c->pdata;
		if ((as->s == c->s) && (as->tv == c->tv) && (c->type == as->type))
		{
			c->value = (c->inversep ? as->u : as->value);
			c->pdata = (c->inversep ? as->invNext : as->next);
			return c->value;
		}
		c->pdata = (c->inversep ? as->invNext : as->next);
	}
	return(NULL);
}



RDF_Cursor
AtalkGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type,  PRBool inversep, PRBool tv)
{
	Assertion		as;
	RDF_Cursor		c = NULL;

	as  = (inversep ? atalkarg2(u) : atalkarg1(u));
	if (as == null)
	{
		getServers(u);

		as  = (inversep ? atalkarg2(u) : atalkarg1(u));
		if (as == null)
			return null;
	}

	if ((c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct))) != NULL)
	{
		c->u = u;
		c->s = s;
		c->value = NULL;
		c->type = type;
		c->inversep = inversep;
		c->tv = tv;
		c->count = 0;
	}
	return(c);
}



RDF_Error
AtalkDisposeCursor (RDFT rdf, RDF_Cursor c)
{
	if (c != NULL)
	{
		freeMem(c);
	} 
	return(0);
}



RDF_Error
AtalkDestroy (RDFT r)
{
	/* XXX to do - kill of any outstanding NBP lookups */
}



RDFT
MakeAtalkStore (char* url)
{
	RDFT		ntr = NULL;

	if (gRDFDB != NULL)	return(gRDFDB);

	if (strstr(url, "rdf:appletalk"))
	{
		if ((ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct))) != NULL)
		{
			ntr->assert = NULL;
			ntr->unassert = NULL;
			ntr->hasAssertion = AtalkHasAssertion;
			ntr->getSlotValue = AtalkGetSlotValue;
			ntr->getSlotValues = AtalkGetSlotValues;
			ntr->nextValue = AtalkNextValue;
			ntr->disposeCursor = AtalkDisposeCursor;
			ntr->destroy = AtalkDestroy;
			ntr->url = copyString(url);

			atalk2rdfHash = PR_NewHashTable(500, idenHash, idenEqual, idenEqual, null, null);
			invatalk2rdfHash = PR_NewHashTable(500, idenHash, idenEqual, idenEqual, null, null);
			gRDFDB  = ntr;

			getZones();
		}
	       
	}
	return(ntr);
}
