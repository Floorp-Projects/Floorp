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
   This file implements Find support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "find2rdf.h"

	/* globals */
static RDFT		gRDFFindDB = NULL;



void
parseResourceIntoFindTokens(RDF_Resource u, findTokenStruct *tokens)
{
	char		*id, *token, *value;
	int		loop;

	if ((id = copyString(resourceID(u) + strlen("find:"))) != NULL)
	{
		/* parse ID, build up token list */
		if ((token = strtok(id, "&")) != NULL)
		{
			while (token != NULL)
			{
				if ((value = strstr(token, "=")) != NULL)
				{
					*value++ = '\0';
				}
				for (loop=0; tokens[loop].token != NULL; loop++)
				{
					if (!strcmp(token, tokens[loop].token))
					{
						tokens[loop].value = copyString(value);
						break;
					}
				}
				token = strtok(NULL, "&");
			}
		}
		freeMem(id);
	}
}



RDF_Cursor
parseFindURL(RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type,  PRBool inversep, PRBool tv)
{
	RDF_Cursor		c = NULL;
	RDF_Resource		searchOn = NULL, matchOn = NULL;
	int			loop;
	findTokenStruct		tokens[5];

	/* build up a token list */
	tokens[0].token = "location";		tokens[0].value = NULL;
	tokens[1].token = "attribute";		tokens[1].value = NULL;
	tokens[2].token = "method";		tokens[2].value = NULL;
	tokens[3].token = "value";		tokens[3].value = NULL;
	tokens[4].token = NULL;			tokens[4].value = NULL;

	parseResourceIntoFindTokens(u, tokens);

	if ((tokens[1].value != NULL) && (tokens[3].value != NULL))
	{
		if ((searchOn = RDF_GetResource(NULL, tokens[1].value, 0)) != NULL)
		{
			if ((matchOn = RDF_GetResource(NULL, tokens[2].value, 0)) == NULL)
			{
				matchOn = gCoreVocab->RDF_substring;
			}
			if ((c = getMem(sizeof(struct RDF_CursorStruct))) != NULL)
			{
				c->u = u;
				c->s = s;
				c->type = type;
				c->tv = tv;
				c->inversep = inversep;
				c->count = 0;
				/* Note: need to copy value string [its a local variable] */
				c->pdata = (void *)RDF_Find(searchOn, matchOn,
					copyString(tokens[3].value), RDF_STRING_TYPE);
				if (c->pdata == NULL)
				{
					freeMem(c);
					c = NULL;
				}
			}
		}
	}

	/* free values in token list */
	for (loop=0; tokens[loop].token != NULL; loop++)
	{
		if (tokens[loop].value != NULL)
		{
			freeMem(tokens[loop].value);
			tokens[loop].value = NULL;
		}
	}

	return(c);
}



RDF_Cursor
FindGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type,  PRBool inversep, PRBool tv)
{
	RDF_Cursor		c = NULL;
	PRBool			passThru = PR_TRUE;

	if ((((s == gCoreVocab->RDF_child) && (!inversep)) ||
		((s == gCoreVocab->RDF_parent) && (inversep))) &&
		(type == RDF_RESOURCE_TYPE) && (tv))
	{
		if (startsWith("find:", resourceID(u)))
		{
			passThru = PR_FALSE;
			c = parseFindURL(rdf, u, s, type, inversep, tv);
		}
	}

	if (passThru == PR_TRUE)
	{
		c = remoteStoreGetSlotValues(rdf, u, s, type, inversep, tv);
	}
	return(c);
}



void *
findNextURL(RDF_Cursor c)
{
	RDF_Resource	r = NULL;
	PRBool		valid = PR_FALSE;

	do
	{
		if ((r = (RDF_Resource)RDF_NextValue(c)) != NULL)
		{
			if (strstr(resourceID(r), ":"))
			{
				if ((!startsWith("find:", resourceID(r))) &&
				    (!startsWith("NC:", resourceID(r))) &&
				    (!startsWith("Command:", resourceID(r))))
				{
					valid = PR_TRUE;
				}
			}
		}
	} while((r != NULL) && (valid == PR_FALSE));
	return((void *)r);
}



void *
FindNextValue(RDFT rdf, RDF_Cursor c)
{
	PRBool			passThru = PR_TRUE;
	void			*retVal = NULL;

	XP_ASSERT(c != NULL);
	if (c == NULL)		return(NULL);

	if ((c->u != NULL) && (((c->s == gCoreVocab->RDF_child) && (!(c->inversep))) ||
		((c->s == gCoreVocab->RDF_parent) && (c->inversep))) &&
		(c->type == RDF_RESOURCE_TYPE) && (c->tv))
	{
		if (startsWith("find:", resourceID(c->u)))
		{
			if (c->pdata != NULL)
			{
				passThru = PR_FALSE;
				retVal = findNextURL((RDF_Cursor)(c->pdata));
			}
		}
	}

	if (passThru == PR_TRUE)
	{
		retVal = remoteStoreNextValue(rdf, c);
	}
	return(retVal);
}



void *
FindGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s,
		RDF_ValueType type, PRBool inversep, PRBool tv)
{
	void			*retVal = NULL;
	PRBool			passThru = PR_TRUE;

	if ((startsWith("find:", resourceID(u))) && (s == gCoreVocab->RDF_name))
	{
          /*		DebugStr("\p FindGetSlotValue on name"); */
	}

	if (passThru == PR_TRUE)
	{
		retVal = remoteStoreGetSlotValue(rdf, u, s, type, inversep, tv);
	}
	XP_ASSERT( (RDF_STRING_TYPE != type) || ( IsUTF8String((const char*) retVal)));
	return(retVal);
}



void
FindPossible(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep)
{
	RDF_Cursor		c;
	RDF_Resource		r;

	if ((startsWith("find:", resourceID(u))) && (containerp(u) &&
		(((s == gCoreVocab->RDF_parent) && (inversep)) ||
		((s == gCoreVocab->RDF_child) && (!inversep))) ))
	{
		if ((c = FindGetSlotValues (rdf, u, s, RDF_RESOURCE_TYPE, inversep, PR_TRUE)) != NULL)
		{
			while((r = (RDF_Resource)FindNextValue(rdf, c)) != NULL)
			{
				if (!remoteStoreHasAssertion (rdf, r, gCoreVocab->RDF_parent,
					u, RDF_RESOURCE_TYPE, PR_TRUE))
				{
					remoteStoreAdd(rdf, r, gCoreVocab->RDF_parent,
						u, RDF_RESOURCE_TYPE, PR_TRUE);
				}
			}
			RDF_DisposeCursor(c);
		}
	}
}



RDF_Error
FindDisposeCursor(RDFT mcf, RDF_Cursor c)
{
	RDF_Error		err = noRDFErr;
	char			*value;

	if (c != NULL)
	{
		if ((((c->s == gCoreVocab->RDF_child) && (!(c->inversep))) ||
			((c->s == gCoreVocab->RDF_parent) && (c->inversep))) &&
			(c->type == RDF_RESOURCE_TYPE) && (c->tv))
		{
			if (startsWith("find:", resourceID(c->u)))
			{
				if (c->pdata != NULL)
				{
					/* Note: at creation, we had to copy "v",
					   so free it here */
					if ((value = ((RDF_Cursor)(c->pdata))->value) != NULL)
					{
						freeMem(value);
						((RDF_Cursor)(c->pdata))->value = NULL;
					}
					RDF_DisposeCursor((RDF_Cursor)(c->pdata));
					c->pdata = NULL;
				}
			}
			freeMem(c);
		}
		else
		{
			err = remoteStoreDisposeCursor (mcf, c);
		}
	}
	return(0);
}



void
findPossiblyAddName(RDFT rdf, RDF_Resource u)
{
	findTokenStruct		tokens[5];
	char			*name;

	if ((name = (char *)remoteStoreGetSlotValue(rdf, u, gCoreVocab->RDF_name,
		RDF_STRING_TYPE, PR_FALSE, PR_TRUE)) == NULL)
	{
		/* build up a token list */
		tokens[0].token = "location";		tokens[0].value = NULL;
		tokens[1].token = "attribute";		tokens[1].value = NULL;
		tokens[2].token = "method";		tokens[2].value = NULL;
		tokens[3].token = "value";		tokens[3].value = NULL;
		tokens[4].token = NULL;			tokens[4].value = NULL;

		parseResourceIntoFindTokens(u, tokens);

		if ((name = PR_smprintf(XP_GetString(RDF_FIND_FULLNAME_STR),
			((tokens[1].value != NULL) ? tokens[1].value : ""),
			((tokens[2].value != NULL) ? tokens[2].value : ""),
			((tokens[3].value != NULL) ? tokens[3].value : ""))) != NULL)
		{
			remoteStoreAdd(rdf, u, gCoreVocab->RDF_name, name,
				RDF_STRING_TYPE, PR_TRUE);
		}
	}
}



PRBool
FindAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
		   RDF_ValueType type, PRBool tv)
{
	/* don't handle the assert, but simply add a name if it doesn't already have one */

	if (startsWith("find:", resourceID(u)))
	{
		findPossiblyAddName(rdf, u);
	}
	return(PR_FALSE);
}



RDFT
MakeFindStore (char *url)
{
	RDFT		ntr = NULL;

	if (gRDFFindDB != NULL)	return(gRDFFindDB);

	if (strstr(url, "rdf:find"))
	{
		if ((ntr = (RDFT)getMem(sizeof(struct RDF_TranslatorStruct))) != NULL)
		{
			ntr->assert = FindAssert;
			ntr->unassert = NULL;
			ntr->hasAssertion = remoteStoreHasAssertion;
			ntr->getSlotValue = FindGetSlotValue;
			ntr->getSlotValues = FindGetSlotValues;
			ntr->nextValue = FindNextValue;
			ntr->disposeCursor = FindDisposeCursor;
			ntr->possiblyAccessFile =  FindPossible;
			ntr->destroy = NULL;
			ntr->url = copyString(url);
			gRDFFindDB = ntr;
		}
	}
	return(ntr);
}
