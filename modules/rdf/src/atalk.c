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
   This file implements (Mac) Appletalk support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

/* docs at 'http://developer.apple.com/technotes/tn/tn1111.html' */

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
			XP_STRCAT(virtualURL, "at://");
			XP_STRCAT(virtualURL, escapedURL);
			XP_FREE(escapedURL);

			if ((r = RDF_GetResource(NULL, virtualURL, PR_TRUE)) != NULL)
			{
				if (startsWith("at://", virtualURL))
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
						strcpy(afpUrl, "afp:/at/");
						strcat(afpUrl, escapedURL);
						/* strcat(afpUrl, ":AFPServer@"); */
						strcat(afpUrl, ":");
						strcat(afpUrl, ((char *)nbp->NBP.userData) + strlen("at://"));

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

	if (!startsWith("at://", resourceID(parent)))	return;

	nbp = (MPPParamBlock *)getMem(sizeof(MPPParamBlock));
	entity = (EntityName *)getMem(sizeof(EntityName));
	buffer = getMem(10240);
	zone = unescapeURL(resourceID(parent)+strlen("at://"));
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

	if (u == gNavCenter->RDF_Appletalk)
	{
		val = copyString(XP_GetString(RDF_APPLETALK_TOP_NAME));
	}
	else if (startsWith("virtualat://", resourceID(u)))
	{
		if ((url = unescapeURL(resourceID(u) + strlen("virtualat://"))) != NULL)
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
	else if (startsWith("at://", resourceID(u)))
	{
		if ((url = unescapeURL(resourceID(u) + strlen("at://"))) != NULL)
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
	else if (startsWith("afp:/at/", resourceID(u)))
	{
		if ((url = unescapeURL(resourceID(u) + strlen("afp:/at/"))) != NULL)
		{
			if ((val = XP_STRRCHR(url, ':')) != NULL)
			{
				*(char *)val = '\0';
			}
			val = copyString(url);
			freeMem(url);
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

	if (((startsWith("at://", resourceID(u))) || (startsWith("virtualat://", resourceID(u)))) &&
		(s == gCoreVocab->RDF_parent) && (containerp(u)))
	{
		id = resourceID(u);
		getServers(u);
	}
}



RDF_Error
AtalkDestroy (RDFT r)
{
	/* XXX to do - kill off any outstanding NBP lookups */
}



PRBool
AtalkAssert (RDFT mcf, RDF_Resource u, RDF_Resource s, void *v, RDF_ValueType type, PRBool tv)
{
	AFPVolMountInfo		afpInfo;
	AFPXVolMountInfo	afpXInfo;
	Bool			response;
	OSErr			err;
	ParamBlockRec		pBlock;
	PRBool			retVal = false, useTCP = false;
	char			*url = NULL, *at, *colon, *slash, *msg = NULL;
	char			*volume = NULL, *volPassword = NULL;
	char			*server = NULL, *zone = NULL;
	char			*user = NULL, *password = NULL;
	short			len, pBlockSize = 0;
	long			result;
	char			errorMsg[256], errorNum[64];
	PRHostEnt		hpbuf;
	char			dbbuf[PR_NETDB_BUF_SIZE];

	if (s == gNavCenter->RDF_Command)
	{
		if ((startsWith("afp:/", resourceID(u))) && (v == gNavCenter->RDF_Command_Launch))
		{
			/* indicate that we handle mounting of AFP URLs */

			retVal = true;

			/* parse AFP urls:  afp:/protocol/[id[:password]]@server[:zone]  */

			if (startsWith("afp:/at/", resourceID(u)))
			{
				url = unescapeURL(resourceID(u) + strlen("afp:/at/"));
			}
			else if (startsWith("afp:/tcp/", resourceID(u)))
			{
				/* if AFP URL indicates TCP, AppleShare Client 3.7 or later is needed */
				url = unescapeURL(resourceID(u) + strlen("afp:/tcp/"));
				if ((err = Gestalt(kAppleShareVerGestalt, &result)))	return(retVal);
				if ((result & 0x0000FFFF) < kAppleShareVer_3_7)
				{
					/* XXX localization */
					FE_Alert(NULL, "Please install AppleShare Client version 3.7 or later.");
					return(retVal);
				}
				useTCP = true;
			}
			if (url == NULL)	return(retVal);

			server = url;
			if ((at = XP_STRCHR(url, '@')) != NULL)
			{
				user = server;
				*at = '\0';
				server = ++at;
			}
			else
			{
				if ((slash = XP_STRCHR(server, '/')) != NULL)
				{
					*slash = '\0';
					volume = ++slash;
					if ((colon = XP_STRCHR(volume, ':')) != NULL)
					{
						*colon = '\0';
						/* no support for folder/file references at end of AFP URLs */
					}
				}
			}
			if ((colon = XP_STRRCHR(server, ':')) != NULL)
			{
				*colon = '\0';
				zone = ++colon;
			}
			if (user != NULL)
			{
				if ((colon = XP_STRCHR(user, ':')) != NULL)
				{
					*colon = '\0';
					password = ++colon;
				}
			}		
			if (user != NULL)	user = copyString(user);
			if (password != NULL)	password = copyString(password);
			if (volume != NULL)	volume = copyString(volume);
			if (server != NULL)	server = copyString(server);
			if (zone != NULL)	zone = copyString(zone);
			freeMem(url);

			if (user == NULL || password == NULL)
			{
				if (server != NULL && zone != NULL)
				{
					msg = getMem(strlen(server) + strlen("\'\' @ ") + strlen(zone));
					if (msg != NULL)
					{
						sprintf(msg, "\'%s\' @ %s", server, zone);
					}
				}
				else if (server != NULL)
				{
					msg = getMem(strlen(server) + strlen("\'\':"));
					if (msg != NULL)
					{
						sprintf(msg, "\'%s\':", server);
					}
				}
				response = FE_PromptUsernameAndPassword(((MWContext *)gRDFMWContext()),
						(msg) ? msg : server, &user, &password);
				/* hmmm... don't free 'msg' as FE_PromptUsernameAndPassword does ??? */
				if (response == false)
				{
					return(retVal);
				}
			}

			if (useTCP == true)
			{
				XP_BZERO((void *)&afpXInfo, sizeof(afpXInfo));
				afpXInfo.length = sizeof(afpXInfo);
				afpXInfo.media = AppleShareMediaType;
				afpXInfo.flags = (volMountInteractMask | volMountExtendedFlagsMask);
				afpXInfo.nbpInterval = 10;
				afpXInfo.nbpCount = 10;
				afpXInfo.uamType = ((password != NULL) && (*password != '\0')) ? 
						kEncryptPassword : kNoUserAuthentication;

				len = (zone != NULL) ? strlen(zone) : 0;
				afpXInfo.zoneNameOffset = BASE_AFPX_OFFSET;
				afpXInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpXInfo.AFPData[pBlockSize], (zone) ? zone : "", len);
				pBlockSize += len;     

				len = (server != NULL) ? strlen(server) : 0;
				afpXInfo.serverNameOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpXInfo.AFPData[pBlockSize], (server) ? server : "", len);
				pBlockSize += len;                   

				len = (volume != NULL) ? strlen(volume) : 0;
				afpXInfo.volNameOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpXInfo.AFPData[pBlockSize], (volume) ? volume : "", len);
				pBlockSize += len;

				len = (user != NULL) ? strlen(user) : 0;
				afpXInfo.userNameOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpXInfo.AFPData[pBlockSize], (user) ? user : "", len);
				pBlockSize += len;

				len = (password != NULL) ? strlen(password) : 0;
				afpXInfo.userPasswordOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpXInfo.AFPData[pBlockSize], (password) ? password : "", len);
				pBlockSize += len;

				len = (volPassword != NULL) ? strlen(volPassword) : 0;
				afpXInfo.volPasswordOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpXInfo.AFPData[pBlockSize], (volPassword) ? volPassword : "", len);
				pBlockSize += len;

				afpXInfo.extendedFlags = kAFPExtendedFlagsAlternateAddressMask;

				afpXInfo.uamNameOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = 0;			/* No UAM support */

				afpXInfo.alternateAddressOffset = BASE_AFPX_OFFSET + pBlockSize;
				afpXInfo.AFPData[pBlockSize++] = AFPX_PROT_VERSION;

			        if (PR_GetHostByName(server, dbbuf, sizeof(dbbuf), &hpbuf) == PR_SUCCESS)
			        {
					afpXInfo.AFPData[pBlockSize++] = 0x01;	/* # of tags */

					/* XXX testing only!  Hard-coded IP address */

					afpXInfo.AFPData[pBlockSize++] = 0x08;	/* tag #1 length */
					afpXInfo.AFPData[pBlockSize++] = 0x02;	/* tag ID: IP address & port */

					afpXInfo.AFPData[pBlockSize++] = 0xD0;	/* address */
					afpXInfo.AFPData[pBlockSize++] = 0x0C;
					afpXInfo.AFPData[pBlockSize++] = 0x26;
					afpXInfo.AFPData[pBlockSize++] = 0x5C;

					afpXInfo.AFPData[pBlockSize++] = 0x02;	/* port */
					afpXInfo.AFPData[pBlockSize++] = 0x24;
			        }

				pBlock.ioParam.ioCompletion = NULL;
				pBlock.ioParam.ioBuffer = (Ptr)&afpXInfo;
			}
			else
			{
				XP_BZERO((void *)&afpInfo, sizeof(afpInfo));
				afpInfo.length = sizeof(afpInfo);
				afpInfo.media = AppleShareMediaType;
				afpInfo.flags = volMountInteractMask;
				afpInfo.nbpInterval = 10;
				afpInfo.nbpCount = 10;
				afpInfo.uamType = ((password != NULL) && (*password != '\0')) ? 
						kEncryptPassword : kNoUserAuthentication;


				len = (zone != NULL) ? strlen(zone) : 0;
				afpInfo.zoneNameOffset = BASE_AFP_OFFSET;
				afpInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpInfo.AFPData[pBlockSize], (zone) ? zone : "", len);
				pBlockSize += len;     

				len = (server != NULL) ? strlen(server) : 0;
				afpInfo.serverNameOffset = BASE_AFP_OFFSET + pBlockSize;
				afpInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpInfo.AFPData[pBlockSize], (server) ? server : "", len);
				pBlockSize += len;                   

				len = (volume != NULL) ? strlen(volume) : 0;
				afpInfo.volNameOffset = BASE_AFP_OFFSET + pBlockSize;
				afpInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpInfo.AFPData[pBlockSize], (volume) ? volume : "", len);
				pBlockSize += len;

				len = (user != NULL) ? strlen(user) : 0;
				afpInfo.userNameOffset = BASE_AFP_OFFSET + pBlockSize;
				afpInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpInfo.AFPData[pBlockSize], (user) ? user : "", len);
				pBlockSize += len;

				len = (password != NULL) ? strlen(password) : 0;
				afpInfo.userPasswordOffset = BASE_AFP_OFFSET + pBlockSize;
				afpInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpInfo.AFPData[pBlockSize], (password) ? password : "", len);
				pBlockSize += len;

				len = (volPassword != NULL) ? strlen(volPassword) : 0;
				afpInfo.volPasswordOffset = BASE_AFP_OFFSET + pBlockSize;
				afpInfo.AFPData[pBlockSize++] = len;
				strncpy(&afpInfo.AFPData[pBlockSize], (volPassword) ? volPassword : "", len);
				pBlockSize += len;

				pBlock.ioParam.ioCompletion = NULL;
				pBlock.ioParam.ioBuffer = (Ptr)&afpInfo;
			}

			err = PBVolumeMount (&pBlock);

			if (err == userCanceledErr)
			{
				return(retVal);
			}
			if (err != noErr)
			{
				/* XXX localization */

				errorMsg[0] = '\0';
				switch(err)
				{
					case	afpUserNotAuth:
					case	afpParmErr:
					sprintf(errorMsg, "User authentication failed.");
					break;

					case	afpPwdExpiredErr:
					sprintf(errorMsg, "Password expired.");
					break;

					case	afpAlreadyMounted:
					sprintf(errorMsg, "Volume already mounted.");
					break;

					case	afpCantMountMoreSrvre:
					sprintf(errorMsg, "Maximum number of volumes has been mounted.");
					break;

					case	afpNoServer:
					sprintf(errorMsg, "Server is not responding.");
					break;

					case	afpSameNodeErr:
					sprintf(errorMsg, "Failed to log on to a server running on this machine.");
					break;
				}
				sprintf(errorNum, "\rError %d", err);
				strcat (errorMsg, errorNum);
				FE_Alert(NULL, errorMsg);
			}

			if (user != NULL)		freeMem(user);
			if (password != NULL)		freeMem(password);
			if (server != NULL)		freeMem(server);
			if (zone != NULL)		freeMem(zone);
			if (volume != NULL)		freeMem(volume);
			if (volPassword != NULL)	freeMem(volPassword);
		}
	}
	else
	{
		retVal = remoteStoreHasAssertion (mcf, u, s, v, type, tv);
	}
	return(retVal);
}



PRBool
AtalkHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void *v, RDF_ValueType type, PRBool tv)
{
	PRBool		retVal = false;

	if (s == gNavCenter->RDF_Command)
	{
		if ((startsWith("afp:/", resourceID(u))) && (v == gNavCenter->RDF_Command_Launch))
		{
			/* mount AFP volume specified by 'u' */

			retVal = true;
		}
	}
	return(retVal);
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
			ntr->assert = AtalkAssert;
			ntr->unassert = NULL;
			ntr->hasAssertion = AtalkHasAssertion;
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
