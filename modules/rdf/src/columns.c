/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   This file synthesizes default columns for a given node.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "columns.h"



RDF_Cursor
ColumnsGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type,  PRBool inversep, PRBool tv)
{
	RDF_Cursor		c;

	if (!containerp(u) || (s != gNavCenter->RDF_Column) || (inversep) ||
		(!tv) || (type != RDF_RESOURCE_TYPE)) 
	{
		return(NULL);
	}
	if ((c = (RDF_Cursor)getMem(sizeof(struct RDF_CursorStruct))) != NULL)
	{
		c->u = u;
		c->value = NULL;
		c->count = 0;
	}
	return(c);
}



void *
ColumnsGetSlotValue(RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,
		PRBool inversep, PRBool tv)
{
	void			*val = NULL;

	if (u == NULL)	return(NULL);

	if ((s == gCoreVocab->RDF_name) && (type == RDF_STRING_TYPE)
		&& (!inversep) && (tv))
	{
		if (u == gCoreVocab->RDF_name)			val = copyString(XP_GetString(RDF_NAME_STR));
		else if (u == gNavCenter->RDF_URLShortcut)	val = copyString(XP_GetString(RDF_SHORTCUT_STR));
		else if (u == gWebData->RDF_URL)		val = copyString(XP_GetString(RDF_URL_STR));
		else if (u == gWebData->RDF_description)	val = copyString(XP_GetString(RDF_DESCRIPTION_STR));
		else if (u == gWebData->RDF_firstVisitDate)	val = copyString(XP_GetString(RDF_FIRST_VISIT_STR));
		else if (u == gWebData->RDF_lastVisitDate)	val = copyString(XP_GetString(RDF_LAST_VISIT_STR));
		else if (u == gWebData->RDF_numAccesses)	val = copyString(XP_GetString(RDF_NUM_ACCESSES_STR));
		else if (u == gWebData->RDF_creationDate)	val = copyString(XP_GetString(RDF_CREATED_ON_STR));
		else if (u == gWebData->RDF_lastModifiedDate)	val = copyString(XP_GetString(RDF_LAST_MOD_STR));
		else if (u == gWebData->RDF_size)		val = copyString(XP_GetString(RDF_SIZE_STR));
		else if (u == gNavCenter->RDF_bookmarkAddDate)	val = copyString(XP_GetString(RDF_ADDED_ON_STR));
	}
	else if ((s == gNavCenter->RDF_ColumnDataType) &&
		(type == RDF_INT_TYPE) && (!inversep) && (tv))
	{
		if (u == gNavCenter->RDF_bookmarkAddDate ||
		    u == gWebData->RDF_firstVisitDate ||
		    u == gWebData->RDF_lastVisitDate ||
		    u == gWebData->RDF_lastModifiedDate ||
		    u == gWebData->RDF_creationDate)
		{
			val = (void *)HT_COLUMN_STRING;
		}
		else if (u == gWebData->RDF_size ||
			 u == gWebData->RDF_numAccesses ||
			 u == gNavCenter->cookieDomain ||
			 u == gNavCenter->cookieSecure)
		{
			val = (void *)HT_COLUMN_INT;
		}
		else
		{
			/* default to string... XXX wrong thing to do? */
			val = (void *)HT_COLUMN_STRING;
		}
	}
	else if ((s == gNavCenter->RDF_ColumnWidth) &&
		(type == RDF_INT_TYPE) && (!inversep) && (tv))
	{
		if (u == gCoreVocab->RDF_name)		val = (void *)128L;
		else if (u == gWebData->RDF_URL)	val = (void *)200L;
		else					val = (void *)80;
	}
	return(val);
}



void *
ColumnsNextValue (RDFT rdf, RDF_Cursor c)
{
	void			*arc = NULL;

	XP_ASSERT(c != NULL);
	if (c == NULL)		return(NULL);

	switch( resourceType(c->u) )
	{
		case	RDF_RT:
		if ((c->u == gNavCenter->RDF_Sitemaps) || (c->u == gNavCenter->RDF_Mail))
		{
			switch(c->count)
			{
				case	0:	arc = gCoreVocab->RDF_name;		break;
				case	1:	arc = gWebData->RDF_URL;		break;
			}
		}
		else do
		{
			switch(c->count)
			{
				case	0:	arc = gCoreVocab->RDF_name;		break;
				case	1:	
				if ((idenEqual(c->u, gNavCenter->RDF_BookmarkFolderCategory)) ||
					((!startsWith("http://", resourceID(c->u))) && endsWith(".rdf", resourceID(c->u))))
				{
					arc = gNavCenter->RDF_URLShortcut;
				}
				else
				{
					/* disallow shortcuts from external RDF graphs, so skip to next column */
					arc = NULL;
					++(c->count);
				}
				break;

				case	2:	arc = gWebData->RDF_URL;		break;
				case	3:	arc = gWebData->RDF_description;	break;
				case	4:	arc = gWebData->RDF_keyword;		break;
				case	5:	arc = gNavCenter->RDF_bookmarkAddDate;	break;
				case	6:	arc = gWebData->RDF_lastVisitDate;	break;
				case	7:	arc = gWebData->RDF_lastModifiedDate;	break;
				case	8:	arc = gNavCenter->pos;	break;
			}
		} while ((c->count <= 6) && (arc == NULL));
		break;

    

		case	HISTORY_RT:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
			case	1:	arc = gWebData->RDF_URL;		break;
			case	2:	arc = gWebData->RDF_firstVisitDate;	break;
			case	3:	arc = gWebData->RDF_lastVisitDate;	break;
			case	4:	arc = NULL;				break;
			case	5:	arc = gWebData->RDF_numAccesses;	break;
		}
		break;

		case   COOKIE_RT:
		switch(c->count) 
	        {
			case	0:	arc = gCoreVocab->RDF_name;		break;
			case	1:	arc = gNavCenter->cookieHost;		break;
			case	2:	arc = gNavCenter->cookiePath;		break;
			case	3:	arc = gNavCenter->cookieValue;		break;
			case	4:	arc = gNavCenter->cookieExpires;	break;
			case	5:	arc = gNavCenter->cookieDomain;		break;
			case	6:	arc = gNavCenter->cookieSecure;		break;
		}
		break;


		case	FTP_RT:
		case	ES_RT:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
			case	1:	arc = gWebData->RDF_URL;		break;
			case	2:	arc = gWebData->RDF_description;	break;
			case	3:	arc = gWebData->RDF_size;		break;
			case	4:	arc = gWebData->RDF_lastModifiedDate;	break;
		}
		break;

		case	LFS_RT:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
			case	1:	arc = gWebData->RDF_URL;		break;
			case	2:	arc = gWebData->RDF_description;	break;
			case	3:	arc = gWebData->RDF_size;		break;
			case	4:	arc = gWebData->RDF_lastModifiedDate;	break;
			case	5:	arc = gWebData->RDF_creationDate;	break;
		}
		break;

		case	LDAP_RT:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
		}
		break;

		case	PM_RT:
		case	IM_RT:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
			case	1:	arc = gWebData->RDF_URL;		break;
		}
		break;

		case	SEARCH_RT:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
			case	1:	arc = gNavCenter->RDF_URLShortcut;	break;
			case	2:	arc = gWebData->RDF_URL;		break;
			case	3:	arc = gWebData->RDF_description;	break;
			case	4:	arc = gWebData->RDF_keyword;		break;
		}
		break;

		default:
		switch(c->count)
		{
			case	0:	arc = gCoreVocab->RDF_name;		break;
		}
		break;
	}
	++(c->count);
	return(arc);
}



RDF_Error
ColumnsDisposeCursor (RDFT rdf, RDF_Cursor c)
{
	if (c != NULL)
	{
		freeMem(c);
	} 
	return(0);
}



RDFT
MakeColumnStore (char* url)
{
	RDFT		ntr = NULL;

	if (strstr(url, "rdf:columns"))
	{
		if ((ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct))) != NULL)
		{
			ntr->getSlotValues = ColumnsGetSlotValues;
			ntr->getSlotValue = ColumnsGetSlotValue;
			ntr->nextValue = ColumnsNextValue;
			ntr->disposeCursor = ColumnsDisposeCursor;
			ntr->url = copyString(url);
		}
	       
	}
	
	return(ntr);
}
