/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   This file implements (Mac) Appletalk support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

/* docs at 'http://developer.apple.com/technotes/tn/tn1111.html' */

#include "atalk.h"
#include "fs2rdf.h"

	/* externs */
void	startAsyncCursors();
void	stopAsyncCursors();


	/* globals */
static RDFT		gRDFDB = NULL;



#ifdef	XP_MAC
PRBool
isAFPVolume(short ioVRefNum)
{
	QHdrPtr		queueHeader;
	OSErr		err;
	PRBool		retVal = PR_FALSE;
	VCBPtr		vcb;
	short		afpDRefNum;

	if ((err = OpenDriver("\p.AFPTranslator", &afpDRefNum)) == noErr)
	{
		queueHeader = GetVCBQHdr();
		if (queueHeader != NULL)
		{
			vcb = (VCBPtr)(queueHeader->qHead);
			while(vcb != NULL)
			{
				if ((vcb->vcbDRefNum == afpDRefNum) && (vcb->vcbVRefNum == ioVRefNum))
				{
					retVal = PR_TRUE;
					break;
				}
				vcb = (VCBPtr)(vcb->qLink);
			}
		}
	}
	return(retVal);
}
#endif



void
getZones(RDFT rdf)
{
	XPPParamBlock		xBlock;
	OSErr			err = noErr;
	XP_Bool			noHierarchyFlag;

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
			noHierarchyFlag = false;
			PREF_GetBoolPref(ATALK_NOHIERARCHY_PREF, &noHierarchyFlag);
			processZones(rdf, xBlock.XCALL.zipBuffPtr, xBlock.XCALL.zipNumZones, noHierarchyFlag);
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
processZones(RDFT rdf, char *zones, uint16 numZones, XP_Bool noHierarchyFlag)
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

		if (noHierarchyFlag == false)
		{
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
				if (!remoteStoreHasAssertion (rdf, r, gCoreVocab->RDF_parent,
					parent, RDF_RESOURCE_TYPE, PR_TRUE))
				{
					remoteStoreAdd(rdf, r, gCoreVocab->RDF_parent,
						parent, RDF_RESOURCE_TYPE, PR_TRUE);
				}
				parent = r;
			}
		}
		else
		{
			escapedURL = NET_Escape(url, URL_XALPHAS);	/* URL_PATH */
			if (escapedURL == NULL)	return;
			virtualURL[0] = 0;
			XP_STRCAT(virtualURL, "at://");
			XP_STRCAT(virtualURL, escapedURL);
			XP_FREE(escapedURL);
			if ((r = RDF_GetResource(NULL, virtualURL, PR_TRUE)) != NULL)
			{
				setResourceType(r, ATALK_RT);
				setContainerp(r, PR_TRUE);
				if (!remoteStoreHasAssertion (rdf, r, gCoreVocab->RDF_parent,
					parent, RDF_RESOURCE_TYPE, PR_TRUE))
				{
					remoteStoreAdd(rdf, r, gCoreVocab->RDF_parent,
						parent, RDF_RESOURCE_TYPE, PR_TRUE);
				}
			}
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
	ourNBPUserDataPtr	ourNBPData;

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

						if ((ourNBPData = (ourNBPUserDataPtr)nbp->NBP.userData) != NULL)
						{
							strcat(afpUrl, ourNBPData->parentID + strlen("at://"));
							if ((parent = RDF_GetResource(NULL, (char *)(ourNBPData->parentID), PR_TRUE)) != NULL)
							{
								if ((r = RDF_GetResource(NULL, afpUrl, PR_TRUE)) != NULL)
								{
									setResourceType(r, ATALK_RT);
									remoteStoreAdd(ourNBPData->rdf, r, gCoreVocab->RDF_parent,
										parent, RDF_RESOURCE_TYPE, PR_TRUE);
								}
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
			if ((ourNBPData = (ourNBPUserDataPtr)nbp->NBP.userData) != NULL)
			{
				if (ourNBPData->parentID != NULL)
				{
					freeMem(ourNBPData->parentID);
					ourNBPData->parentID = NULL;
				}
				freeMem((char *)ourNBPData);
			}
			freeMem(nbp);
			break;
		}
	}
}



void
getServers(RDFT rdf, RDF_Resource parent)
{
	EntityName		*entity;
	MPPParamBlock		*nbp;
	OSErr			err = noErr;
	char			*buffer, *parentID, *zone, url[128];
	ourNBPUserDataPtr	ourNBPData;

	if (!startsWith("at://", resourceID(parent)))	return;

	nbp = (MPPParamBlock *)getMem(sizeof(MPPParamBlock));
	entity = (EntityName *)getMem(sizeof(EntityName));
	buffer = getMem(10240);
	ourNBPData = getMem(sizeof(ourNBPUserDataStruct));
	zone = unescapeURL(resourceID(parent)+strlen("at://"));
	parentID = copyString(resourceID(parent));

	if ((nbp != NULL) && (entity != NULL) && (buffer != NULL) &&
		(zone != NULL) && (parentID != NULL) && (ourNBPData != NULL))
	{
		ourNBPData->rdf = rdf;
		ourNBPData->parentID = parentID;
	
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
		nbp->NBP.userData = (long)ourNBPData;
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
AtalkPossible(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
	char		*id;

	if ((u == gNavCenter->RDF_Appletalk) &&
		(((s == gCoreVocab->RDF_parent) && (inversep)) ||
		((s == gCoreVocab->RDF_child) && (!inversep))))
	{
		getZones(rdf);
	}
	else if ((startsWith("at://", resourceID(u))) && containerp(u) &&
		(((s == gCoreVocab->RDF_parent) && (inversep)) ||
		((s == gCoreVocab->RDF_child) && (!inversep))))
	{
		id = resourceID(u);
		getServers(rdf, u);
	}
}



RDF_Error
AtalkDestroy (RDFT r)
{
	/* XXX to do - kill off any outstanding NBP lookups */
	return(0);
}



PRBool
AtalkAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void *v, RDF_ValueType type, PRBool tv)
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
	char			errorMsg[256], errorNum[256];
	PRHostEnt		hpbuf;
	char			dbbuf[PR_NETDB_BUF_SIZE];

	if (s == gNavCenter->RDF_Command)
	{
		if ((startsWith("afp:/", resourceID(u))) && (type == RDF_RESOURCE_TYPE) &&
			(tv) && (v == gNavCenter->RDF_Command_Launch))
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
				if ((err = Gestalt(kAppleShareVerGestalt, &result)) != noErr)
				{
					return(retVal);
				}
				if ((result & 0x0000FFFF) < kAppleShareVer_3_7)
				{
					FE_Alert(NULL, XP_GetString(RDF_AFP_CLIENT_37_STR));
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
				response = FE_PromptUsernameAndPassword(((MWContext *)gRDFMWContext(rdf)),
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
				errorMsg[0] = '\0';
				switch(err)
				{
					case	afpUserNotAuth:
					case	afpParmErr:
					sprintf(errorMsg, XP_GetString(RDF_AFP_AUTH_FAILED_STR));
					break;

					case	afpPwdExpiredErr:
					sprintf(errorMsg, XP_GetString(RDF_AFP_PW_EXPIRED_STR));
					break;

					case	afpAlreadyMounted:
					sprintf(errorMsg, XP_GetString(RDF_AFP_ALREADY_MOUNTED_STR));
					break;

					case	afpCantMountMoreSrvre:
					sprintf(errorMsg, XP_GetString(RDF_AFP_MAX_SERVERS_STR));
					break;

					case	afpNoServer:
					sprintf(errorMsg, XP_GetString(RDF_AFP_NOT_RESPONDING_STR));
					break;

					case	afpSameNodeErr:
					sprintf(errorMsg, XP_GetString(RDF_AFP_SAME_NODE_STR));
					break;
				}
				sprintf(errorNum, XP_GetString(RDF_AFP_ERROR_NUM_STR), err);
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
		else if (((u == gNavCenter->RDF_Appletalk) || (startsWith("virtualat://", resourceID(u))) ||
			(startsWith("at://", resourceID(u)))) && (type == RDF_RESOURCE_TYPE) && (tv) &&
			(startsWith(ATALK_CMD_PREFIX, resourceID(v))))
		{
			if (v == gNavCenter->RDF_Command_Atalk_FlatHierarchy)
			{
				/* set pref to view flat zone list, refresh remote store */
				PREF_SetBoolPref(ATALK_NOHIERARCHY_PREF, PR_TRUE);
				remoteStoreAdd(rdf, gNavCenter->RDF_Appletalk, gNavCenter->RDF_Command,
					gNavCenter->RDF_Command_Refresh, RDF_RESOURCE_TYPE, PR_TRUE);
				getZones(rdf);
			}
			else if (v == gNavCenter->RDF_Command_Atalk_Hierarchy)
			{
				/* set pref to view hierarchical zone list, refresh remote store */
				PREF_SetBoolPref(ATALK_NOHIERARCHY_PREF, PR_FALSE);
				remoteStoreAdd(rdf, gNavCenter->RDF_Appletalk, gNavCenter->RDF_Command,
					gNavCenter->RDF_Command_Refresh, RDF_RESOURCE_TYPE, PR_TRUE);
				getZones(rdf);
			}
		}
		else if ((type == RDF_RESOURCE_TYPE) && (tv) && (v == gNavCenter->RDF_Command_Refresh))
		{
			remoteStoreAdd(rdf, u, s, v, type, tv);
			if ((u == gNavCenter->RDF_Appletalk) || (startsWith("virtualat:/", resourceID(u))))
			{
				getZones(rdf);
			}
		}
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
	else if ((s == gCoreVocab->RDF_parent) && (type == RDF_RESOURCE_TYPE) &&
		(resourceType(u) == ATALK_RT) && (containerp(v)) && 
		(resourceType((RDF_Resource)v) == ATALK_RT) &&
		(startsWith(resourceID((RDF_Resource)v),  resourceID(u))))
	{
		/* XXX ??? */
	}
	else
	{
		retVal = remoteStoreHasAssertion (rdf, u, s, v, type, tv);
	}
	return(retVal);
}



char *
convertAFPtoUnescapedFile(char *id)
{
	char		*url = NULL, *slash, *temp;

	XP_ASSERT(id != NULL);

	if (id != NULL)
	{
		if (startsWith("afp:/", id))
		{
			/* mangle AFP URL to file URL */
			if ((slash = strstr(id, "/")) != NULL)
			{
				if ((slash = strstr(slash+1, "/")) != NULL)
				{
					if ((slash = strstr(slash+1, "/")) != NULL)
					{
						if ((temp = append2Strings("file://", slash)) != NULL)
						{
							url = unescapeURL(&temp[FS_URL_OFFSET]);
							freeMem(temp);
						}
					}
				}
			}
		}
	}
	return(url);
}



void *
AtalkGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv)
{
	PRBool		passThru = PR_TRUE;
	PRFileInfo	fn;
	PRStatus	err;
	PRTime		oneMillion, dateVal;
	struct tm	*time;
	char		buffer[128], *fileURL, *pathname, *slash, *colon, *url;
	void		*retVal = NULL;
	int32		creationTime, modifyTime;
	int		len, n;
	XP_Bool		noHierarchyFlag;

	if (startsWith("afp:/", resourceID(u)))
	{
		if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE) && (tv))
		{
			passThru = PR_FALSE;
			if ((slash = strstr(resourceID(u), "/")) != NULL)
			{
				if ((slash = strstr(slash+1, "/")) != NULL)
				{
					slash = strstr(slash+1, "/");
				}
			}
			if (slash != NULL)
			{
				if ((pathname = copyString(slash)) != NULL)
				{
					if ((len = strlen(pathname)) > 0)
					{
						if (pathname[len-1] == '/')  pathname[--len] = '\0';
						n = revCharSearch('/', pathname);
						retVal = (void *)unescapeURL(&pathname[n+1]);
						passThru = PR_FALSE;
						freeMem(pathname);
					}
				}
			}
			else
			{
				if ((slash = strstr(resourceID(u), "/")) != NULL)
				{
					if ((slash = strstr(slash+1, "/")) != NULL)
					{
						if ((pathname = copyString(slash+1)) != NULL)
						{
							if ((colon = strstr(pathname, ":")) != NULL)
							{
								*colon++ = '\0';
								retVal = (void *)unescapeURL(pathname);
								passThru = PR_FALSE;
							}
							freeMem(pathname);
						}
					}
				}
			}
		}
		else if ((s == gWebData->RDF_description) && (type == RDF_STRING_TYPE) && (tv))
		{
			passThru = PR_FALSE;
			if (endsWith("/", resourceID(u)))
			{
				retVal = (void *)copyString(XP_GetString(RDF_DIRECTORY_DESC_STR));
			}
			else
			{
				retVal = (void *)copyString(XP_GetString(RDF_FILE_DESC_STR));
			}
		}
		else if ((s == gWebData->RDF_size) && (type == RDF_INT_TYPE) && (tv))
		{
			if ((fileURL = convertAFPtoUnescapedFile(resourceID(u))) != NULL)
			{
				passThru = PR_FALSE;
				err=PR_GetFileInfo(fileURL, &fn);
				if (!err)	retVal = (void *)fn.size;
				freeMem(fileURL);
			}
		}
		else if ((s == gWebData->RDF_creationDate) && (tv))
		{
			if ((fileURL = convertAFPtoUnescapedFile(resourceID(u))) != NULL)
			{
				passThru = PR_FALSE;
				err = PR_GetFileInfo(fileURL, &fn);
				if (!err)
				{
					LL_I2L(oneMillion, PR_USEC_PER_SEC);
					LL_DIV(dateVal, fn.creationTime, oneMillion);
					LL_L2I(creationTime, dateVal);
					if (type == RDF_STRING_TYPE)
					{
						if ((time = localtime((time_t *) &creationTime)) != NULL)
						{
#ifdef	XP_MAC
							time->tm_year += 4;
							strftime(buffer, sizeof(buffer), XP_GetString(RDF_HTML_MACDATE), time);
#else
							strftime(buffer, sizeof(buffer), XP_GetString(RDF_HTML_WINDATE), time);
#endif
							retVal = copyString(buffer);
						}
					}
					else if (type == RDF_INT_TYPE)
					{
						retVal = (void *)creationTime;
					}
				}
				freeMem(fileURL);
			}
		}
		else if ((s == gWebData->RDF_lastModifiedDate) && (tv))
		{
			if ((fileURL = convertAFPtoUnescapedFile(resourceID(u))) != NULL)
			{
				passThru = PR_FALSE;
				err = PR_GetFileInfo(fileURL, &fn);
				if (!err)
				{
					LL_I2L(oneMillion, PR_USEC_PER_SEC);
					LL_DIV(dateVal, fn.modifyTime, oneMillion);
					LL_L2I(modifyTime, dateVal);
					if (type == RDF_STRING_TYPE)
					{
						if ((time = localtime((time_t *) &modifyTime)) != NULL)
						{
#ifdef	XP_MAC
							time->tm_year += 4;
							strftime(buffer, sizeof(buffer), XP_GetString(RDF_HTML_MACDATE), time);
#else
							strftime(buffer, sizeof(buffer), XP_GetString(RDF_HTML_WINDATE), time);
#endif
							retVal = copyString(buffer);
						}
					}
					else if (type == RDF_INT_TYPE)
					{
						retVal = (void *)modifyTime;
					}
				}
				freeMem(fileURL);
			}
		}
	}
	else if (startsWith("at://", resourceID(u)))
	{
		if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE) && (tv))
		{
			passThru = PR_FALSE;
			if ((url = unescapeURL(resourceID(u) + strlen("at://"))) != NULL)
			{
				noHierarchyFlag = false;
				PREF_GetBoolPref(ATALK_NOHIERARCHY_PREF, &noHierarchyFlag);
				if (noHierarchyFlag == true)
				{
					retVal = copyString(url);
				}
				else
				{
					if ((retVal = XP_STRRCHR(url, ' ')) != NULL)
					{
						retVal = copyString(((char *)retVal) + 1);
					}
					else
					{
						retVal = copyString(url);
					}
				}
				freeMem(url);
			}
		}
	}
	else if (startsWith("virtualat://", resourceID(u)))
	{
		if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE) && (tv))
		{
			passThru = PR_FALSE;
			if ((url = unescapeURL(resourceID(u) + strlen("virtualat://"))) != NULL)
			{
				if ((retVal = XP_STRRCHR(url, ' ')) != NULL)
				{
					retVal = copyString(((char *)retVal) + 1);
				}
				else
				{
					retVal = copyString(url);
				}
				freeMem(url);
			}
		}
	}
	else if ((startsWith(ATALK_CMD_PREFIX, resourceID(u))) && (s == gCoreVocab->RDF_name) &&
		(type == RDF_STRING_TYPE) && (!inversep) && (tv))
	{
		passThru = PR_FALSE;
		retVal = (void *)copyString(resourceID(u) + strlen(ATALK_CMD_PREFIX));
	}

	if (passThru == PR_TRUE)
	{
		retVal = remoteStoreGetSlotValue(rdf, u, s, type, inversep, tv);
	}
	return(retVal);
}



RDF_Cursor
AtalkGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type,  PRBool inversep, PRBool tv)
{
	PRBool		passThru = PR_TRUE;
	PRDir		*dir = NULL;
	RDF_Cursor	c = NULL;
	char		*slash, *temp;

	if ((((s == gCoreVocab->RDF_child) && (!inversep)) ||
		((s == gCoreVocab->RDF_parent) && (inversep))) &&
		(type == RDF_RESOURCE_TYPE) && (tv))
	{
		if (u == gNavCenter->RDF_LocalFiles)
		{
			passThru = PR_FALSE;
			if ((c = getMem(sizeof(struct RDF_CursorStruct))) != NULL)
			{
				c->u = u;
				c->s = s;
				c->type = type;
				c->tv = tv;
				c->inversep = inversep;
				c->count = 0;
				c->pdata = NULL;
			}
		}
		else if (startsWith("afp:/", resourceID(u)))
		{
			passThru = PR_FALSE;

			/* mangle AFP URLs (with associated volume info) to file URL */
			if ((slash = strstr(resourceID(u), "/")) != NULL)
			{
				if ((slash = strstr(slash+1, "/")) != NULL)
				{
					if ((slash = strstr(slash+1, "/")) != NULL)
					{
						if ((temp = append2Strings("file://", slash)) != NULL)
						{
							if ((c = getMem(sizeof(struct RDF_CursorStruct))) != NULL)
							{
								if ((dir = OpenDir(temp)) != NULL)
								{
									c->u = u;
									c->s = s;
									c->type = type;
									c->tv = tv;
									c->inversep = inversep;
									c->count = PR_SKIP_BOTH;
									c->pdata = dir;
								}
								else
								{
									freeMem(c);
									c = NULL;
								}
							}
							freeMem(temp);
						}
					}
				}
			}
		}
	}
	else if (((u == gNavCenter->RDF_Appletalk) || (startsWith("virtualat://", resourceID(u)) ||
		(startsWith("at://", resourceID(u))))) && (s == gNavCenter->RDF_Command) &&
		(type == RDF_RESOURCE_TYPE) && (inversep) && (tv))
	{
		passThru = PR_FALSE;
		if ((c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct))) != NULL)
		{
			c->u = u;
			c->s = s;
			c->type = type;
			c->inversep = inversep;
			c->tv = tv;
			c->count = 0;
			c->pdata = NULL;
		}
	}

	if (passThru == PR_TRUE)
	{
		c = remoteStoreGetSlotValues(rdf, u, s, type, inversep, tv);
	}
	return(c);
}



void *
AtalkNextValue (RDFT rdf, RDF_Cursor c)
{
	PRBool			passThru = PR_TRUE, isDirectoryFlag = false, sep;
	PRDirEntry		*de;
	PRFileInfo		fn;
	RDF_Resource		r;
	XP_Bool			noHierarchyFlag;
	char			*base, *slash, *temp = NULL, *encoded = NULL, *url, *url2;
	void			*retVal = NULL;

	XP_ASSERT(c != NULL);
	if (c == NULL)		return(NULL);

	if ((((c->s == gCoreVocab->RDF_child) && (!c->inversep)) ||
		((c->s == gCoreVocab->RDF_parent) && (c->inversep))) &&
		(c->type == RDF_RESOURCE_TYPE) && (c->tv))
	{
		if (c->u == gNavCenter->RDF_LocalFiles)
		{
			passThru = PR_FALSE;
			do
			{
				retVal = NULL;
				if ((url = getVolume(c->count++, PR_TRUE)) != NULL)
				{
					if ((r = RDF_GetResource(NULL, url, 1)) != NULL)
					{
						setContainerp(r, 1);
						setResourceType(r, ATALK_RT);
						retVal = (void *)r;
					}
					XP_FREE(url);
				}
			} while((retVal == NULL) && (c->count <= 26));
		}
		else if (startsWith("afp:/", resourceID(c->u)) && (c->pdata != NULL))
		{
			passThru = PR_FALSE;

			while ((de = PR_ReadDir((PRDir*)c->pdata, (PRDirFlags)(c->count++))) != NULL)
			{
				if ((base = NET_Escape(de->name, URL_XALPHAS)) != NULL)		/* URL_PATH */
				{
					sep = ((resourceID(c->u))[strlen(resourceID(c->u))-1] == XP_DIRECTORY_SEPARATOR);
					if (sep)
					{
						url = PR_smprintf("%s%s",  resourceID(c->u), base);
					}
					else
					{
						url = PR_smprintf("%s/%s",  resourceID(c->u), base);
					}
					XP_FREE(base);

					if (url != NULL)
					{
						/* mangle AFP URLs (with associated volume info) to file URL */
						if ((slash = strstr(url, "/")) != NULL)
						{
							if ((slash = strstr(slash+1, "/")) != NULL)
							{
								if ((slash = strstr(slash+1, "/")) != NULL)
								{
									temp = append2Strings("file://", slash);
								}
							}
						}
						if (temp != NULL)
						{
							encoded = unescapeURL(&temp[FS_URL_OFFSET]);
							if (encoded != NULL)
							{
								PR_GetFileInfo(encoded, &fn);
								if (fn.type == PR_FILE_DIRECTORY)
								{
									isDirectoryFlag = TRUE;
									url2 = PR_smprintf("%s/", url);
									XP_FREE(url);
									url = url2;
								}
								freeMem(encoded);
							}
							if (isFileVisible(temp) == PR_FALSE)
							{
								XP_FREE(url);
								url = NULL;
							}
							freeMem(temp);
							if (url != NULL)
							{
								retVal = (void *)CreateAFPFSUnit(url, isDirectoryFlag);
								XP_FREE(url);
								url = NULL;
								break;
							}
						}
					}
				}
			}
			if (de == NULL)
			{
				PR_CloseDir((PRDir *)c->pdata);
				c->pdata = NULL;
			}
		}
	}
	else if (((c->u == gNavCenter->RDF_Appletalk) || (startsWith("virtualat://", resourceID(c->u))) ||
		startsWith("at://", resourceID(c->u))) && (c->s == gNavCenter->RDF_Command) &&
		(c->type == RDF_RESOURCE_TYPE))
	{
		passThru = PR_FALSE;

		/* return cookie commands here */
		switch(c->count)
		{
			case	0:
			noHierarchyFlag = false;
			PREF_GetBoolPref(ATALK_NOHIERARCHY_PREF, &noHierarchyFlag);
			if (noHierarchyFlag == PR_TRUE)
			{
				retVal = gNavCenter->RDF_Command_Atalk_Hierarchy;
			}
			else
			{
				retVal = gNavCenter->RDF_Command_Atalk_FlatHierarchy;
			}
			break;
		}
		++(c->count);
	}

	if (passThru == PR_TRUE)
	{
		retVal = remoteStoreNextValue(rdf, c);
	}
	return(retVal);
}



RDF_Resource
CreateAFPFSUnit (char *nname, PRBool isDirectoryFlag)
{
	RDF_Resource		r = NULL;

	if (startsWith("afp:/", nname))
	{
		if ((r = RDF_GetResource(NULL, nname, 0)) == NULL)
		{
			if ((r = RDF_GetResource(NULL, nname, 1)) != NULL)
			{
				setResourceType(r, ATALK_RT);
				setContainerp(r, isDirectoryFlag);
			}
		}	
	}
	return(r);
}



RDFT
MakeAtalkStore (char* url)
{
	RDFT		ntr = NULL;

	if (gRDFDB != NULL)	return(gRDFDB);

	if (strstr(url, "rdf:appletalk"))
	{
		if ((ntr = NewRemoteStore(url)) != NULL)
		{
			ntr->assert = AtalkAssert;
			ntr->unassert = NULL;
			ntr->hasAssertion = AtalkHasAssertion;
			ntr->getSlotValue = AtalkGetSlotValue;
			ntr->getSlotValues = AtalkGetSlotValues;
			ntr->nextValue = AtalkNextValue;
			ntr->disposeCursor = remoteStoreDisposeCursor;
			ntr->possiblyAccessFile =  AtalkPossible;
			ntr->destroy = AtalkDestroy;
			gRDFDB  = ntr;

			getZones(ntr);
			remoteStoreAdd(gRemoteStore, gNavCenter->RDF_Appletalk,
				gCoreVocab->RDF_parent, gNavCenter->RDF_LocalFiles,
				RDF_RESOURCE_TYPE, 1);
		}
	       
	}
	return(ntr);
}
