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
			setAtalkResourceName(r);
			remoteStoreAdd(gRDFDB, r, gCoreVocab->RDF_parent,
				parent, RDF_RESOURCE_TYPE, PR_TRUE);
			parent = r;
		}
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
								setAtalkResourceName(r);
								remoteStoreAdd(gRDFDB, r, gCoreVocab->RDF_parent,
									parent, RDF_RESOURCE_TYPE, PR_TRUE);
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



void
setAtalkResourceName(RDF_Resource u)
{
	void			*val = NULL;
	char			*url;

	if (u == NULL)	return;

	if ((resourceType(u) == ATALK_RT) || (resourceType(u) == ATALKVIRTUAL_RT))
	{
		if (u == gNavCenter->RDF_Appletalk)
		{
			val = copyString(XP_GetString(RDF_APPLETALK_TOP_NAME));
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
	if (val != NULL)
	{
		remoteStoreAdd(gRDFDB, u, gCoreVocab->RDF_name,
			val, RDF_STRING_TYPE, PR_TRUE);
	}
}



void
AtalkPossible(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
	char		*id;

	if (((resourceType(u) == ATALK_RT) || (resourceType(u) == ATALKVIRTUAL_RT)) &&
		(s == gCoreVocab->RDF_parent) && (containerp(u)))
	{
		id = resourceID(u);
		getServers(u);
	}
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
			ntr->hasAssertion = remoteStoreHasAssertion;
			ntr->getSlotValue = remoteStoreGetSlotValue;
			ntr->getSlotValues = remoteStoreGetSlotValues;
			ntr->nextValue = remoteStoreNextValue;
			ntr->disposeCursor = remoteStoreDisposeCursor;
			ntr->possiblyAccessFile =  AtalkPossible;
			ntr->destroy = AtalkDestroy;
			ntr->url = copyString(url);
			gRDFDB  = ntr;

			atalk2rdfHash = PR_NewHashTable(500, idenHash, idenEqual, idenEqual, null, null);
			invatalk2rdfHash = PR_NewHashTable(500, idenHash, idenEqual, idenEqual, null, null);

			setAtalkResourceName(gNavCenter->RDF_Appletalk);
			getZones();
		}
	       
	}
	return(ntr);
}
