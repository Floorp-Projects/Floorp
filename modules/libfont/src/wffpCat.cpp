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
 * wffpCat.cpp (FontDisplayerCatalogObject.cpp)
 *
 * This maintains the displayer catalog.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "wffpCat.h"

#include "Pcfmi.h"
#include "fmi.h"

static void free_catalog_store(wfList *object, void *item);

#define WF_FMI_CHUNK_SIZE 16

FontDisplayerCatalogObject::
FontDisplayerCatalogObject(const char *reconstructString)
: wfList(free_catalog_store)
{
	if (reconstructString && *reconstructString)
	{
		// reconstruct(reconstructString);
	}
}

FontDisplayerCatalogObject::
~FontDisplayerCatalogObject(void)
{
	finalize();
}

int FontDisplayerCatalogObject::
finalize()
{
	wfList::removeAll();
	return (0);
}


void
/*ARGSUSED*/
free_catalog_store(wfList *object, void *item)
{
	struct catalog_store *ele = (struct catalog_store *) item;
	if (ele->fmiCount > 0)
	{
		for (int i = 0; i < ele->fmiCount; i++)
		{
			nffmi_release(ele->fmis[i], NULL);
		}
		delete ele->fmis;
		ele->fmis = NULL;
		ele->fmiCount = 0;
		ele->maxFmiCount = 0;
	}
}

int FontDisplayerCatalogObject::
update(struct nfrc *rc, struct nffmi **fmis)
{
	struct wfListElement *tmp = head;
	while (tmp)
	{
		struct catalog_store *ele = (struct catalog_store *) tmp->item;
		if (nfrc_IsEquivalent(rc, ele->rcMajorType, ele->rcMinorType, NULL))
		{
			copyFmis(ele, fmis);
			break;
		}
		tmp = tmp->next;
	}
	if (!tmp)
	{
		// Add these into the list
		struct catalog_store *newele = (struct catalog_store *)
			new catalog_store;
		newele->rcMajorType = nfrc_GetMajorType(rc, NULL);
		newele->rcMinorType = nfrc_GetMinorType(rc, NULL);
		newele->fmis = NULL;
		newele->fmiCount = newele->maxFmiCount = 0;
		copyFmis(newele, fmis);
		add(newele);
	}
	return (0);
}


int FontDisplayerCatalogObject::
addFmi(jint rcMajorType, jint rcMinorType, struct nffmi *fmi)
{
	struct wfListElement *tmp = head;
	while (tmp)
	{
		struct catalog_store *ele = (struct catalog_store *) tmp->item;
		if ((ele->rcMajorType == rcMajorType) &&
			(ele->rcMinorType == rcMinorType))
		{
			addFmi(ele, fmi);
			break;
		}
		tmp = tmp->next;
	}

	if (!tmp)
	{
		// Add this {rc, fmi} as a new element
		struct catalog_store *newele = (struct catalog_store *)
			new catalog_store;
		newele->rcMajorType = rcMajorType;
		newele->rcMinorType = rcMinorType;
		newele->fmis = NULL;
		newele->fmiCount = newele->maxFmiCount = 0;
		addFmi(newele, fmi);
		add(newele);
	}
	return (0);
}


int FontDisplayerCatalogObject::
isInitialized(struct nfrc *rc)
{
	int ret = 0;
	struct wfListElement *tmp = head;
	while (tmp)
	{
		struct catalog_store *ele = (struct catalog_store *) tmp->item;
		if (nfrc_IsEquivalent(rc, ele->rcMajorType, ele->rcMinorType, NULL))
		{
			// yes. Initialized.
			ret = 1;
			break;
		}
		tmp = tmp->next;
	}
	return (ret);
}


int FontDisplayerCatalogObject::
supportsFmi(struct nfrc *rc, struct nffmi *fmi)
{
	int supports = 0;
	struct wfListElement *tmp = head;
	struct catalog_store *ele = NULL;

	while (tmp)
	{
		ele = (struct catalog_store *) tmp->item;
		if (nfrc_IsEquivalent(rc, ele->rcMajorType, ele->rcMinorType, NULL))
		{
			break;
		}
		tmp = tmp->next;
	}

	if (!tmp)
	{
		// Catalog was never intialized for this rc
		supports = -1;
	}
	else if (ele->fmiCount > 0)
	{
		// Check if ele supports the fmi
		for (int i = 0; i < ele->fmiCount; i++)
		{
			if (nffmi_equals(fmi, ele->fmis[i], NULL))
			{
				supports = 1;
				break;
			}
		}
	}

	return (supports);
}

int FontDisplayerCatalogObject::
describe(FontCatalogFile &fc)
{
	struct wfListElement *tmp = head;
    while (tmp)
	{
		struct catalog_store *ele = (struct catalog_store *) tmp->item;
		fc.output("{");
		fc.indentIn();

		fc.output("MajorType", (int)ele->rcMajorType);
		fc.output("MinorType", (int)ele->rcMinorType);
		int i = 0;
		for (i = 0; i < ele->fmiCount; i++)
		{
			fc.output("font", nffmi_toString(ele->fmis[i], NULL));
		}
		
		fc.indentOut();
		fc.output("} // End of an element");
		tmp = tmp->next;
	}
	return (0);
}

int FontDisplayerCatalogObject::
reconstruct(FontCatalogFile &fc)
{
	char buf[1024];
	int buflen;
	int over = 0;
	char *variable, *value;
	int inElement = 0;
	char *ret;
	jint rcMajorType = 0, rcMinorType = 0;
	int fmiCount =0;

	finalize();
	
	while (!over)
	{
		ret = fc.readline(buf, sizeof(buf));
		if (!ret)
		{
			over = 1;
			continue;
		}
		buflen = strlen(buf);
		if (buf[buflen-1] == '\n')
		{
			buf[buflen-1] = '\0';
			buflen--;
		}
		
		wf_scanVariableAndValue(buf, buflen, variable, value);
		
		if (inElement && !wf_strcasecmp(variable, "majortype"))
		{
			rcMajorType = atoi(value);
		}
		else if (inElement && !wf_strcasecmp(variable, "minortype"))
		{
			rcMinorType = atoi(value);
		}
		else if (inElement && !wf_strcasecmp(variable, "font"))
		{
			// Make an fmi out of the buffer
			struct nffmi *fmi = nffbu_CreateFontMatchInfo( WF_fbu, NULL, NULL,
				NULL, 0, 0, 0, 0, 0, 0, 0, NULL);
			cfmiImpl *oimpl = cfmi2cfmiImpl(fmi);
			FontMatchInfoObject *fmiobj = (FontMatchInfoObject *)oimpl->object;
			fmiobj->reconstruct(buf);
			
			// Add the fmi to this element
			addFmi(rcMajorType, rcMinorType, fmi);
			fmiCount++;
		}
		else if (!strncmp(variable, "{", 1))
		{
			// Begin of an element
			inElement = 1;
			fmiCount = 0;
		}
		else if (!strncmp(variable, "}", 1))
		{
			if (inElement)
			{
				inElement = 0;
				// Take care of no fmis.
				if (fmiCount == 0)
				{
					addFmi(rcMajorType, rcMinorType, NULL);
				}
			}
			else
			{
				over = 1;
				continue;
			}
		}
	}
	return (0);
}

//
// Private methods
//

int FontDisplayerCatalogObject::
copyFmis(struct catalog_store *ele, struct nffmi **fmis)
{	
	if (ele->fmiCount > 0)
	{
		for (int i = 0; i < ele->fmiCount; i++)
		{
			nffmi_release(ele->fmis[i], NULL);
		}
		delete ele->fmis;
		ele->fmis = NULL;
		ele->fmiCount = 0;
		ele->maxFmiCount = 0;
	}

	if (!fmis)
	{
		return (-1);
	}

	int i = 0;
	while (fmis[i]) i++;
	if (i == 0)
	{
		// No fmis to be copied.
		return (0);
	}
	ele->fmis = (struct nffmi **) WF_ALLOC(sizeof(struct nffmi *) * i);
	if (!ele->fmis)
	{
		// No memory.
		return (-2);
	}
	ele->fmiCount = i;
	ele->maxFmiCount = i;
	while (--i >= 0)
	{
		ele->fmis[i] = fmis[i];
		nffmi_addRef(ele->fmis[i], NULL);
	}

	return (0);
}

int FontDisplayerCatalogObject::
addFmi(struct catalog_store *ele, struct nffmi *fmi)
{
	if (!fmi)
	{
		return (-1);
	}

	if (ele->maxFmiCount <= ele->fmiCount)
	{
		ele->fmis = (struct nffmi **)
			WF_REALLOC(ele->fmis, sizeof(struct nffmi *) * (ele->fmiCount + WF_FMI_CHUNK_SIZE));
		if (!ele->fmis)
		{
			return (-1);
		}
		ele->maxFmiCount = ele->fmiCount + WF_FMI_CHUNK_SIZE;
	}
	ele->fmis[ele->fmiCount++] = fmi;
	nffmi_addRef(fmi, NULL);
	return (0);
}

// ==========================================================================

FontCatalogFile::FontCatalogFile(const char *name, int write)
: fp(NULL), indentLevel(0), writing(write)
{

	if (writing)
	{
		fp = fopen(name, "w");
		
		// Write catalog file header information
		// NO I18N required
		output(
			"#\n"
			"# Netscape Fonts Catalog File\n"
			"#\n"
			"# Architect: Suresh Duddi <dp@netscape.com>\n"
			"#\n"
			"# This file stores all data about font displayers and the kind of fonts\n"
			"# they server. This will be used to optimize loading of font displayers\n"
			"#\n"
			"# #####################################################################\n"
			"# THIS FILE IS AUTOMATICALLY GENERATED. DO NOT EDIT THIS FILE.\n"
			"# #####################################################################\n"
			"# Copyright Netscape Communications Corp (C) 1996, 1997"
			);
		
		output("Version", WF_CATALOG_FILE_VERSION);
		// PR_ExplodeTime(&pr_time, PR_Now());
		// PR_FormatTime(buf, sizeof(buf), "", pr_time);
		// output("Created", buf);
	}
	else
	{
		fp = fopen(name, "r");
	}
}

FontCatalogFile::~FontCatalogFile(void)
{
	if (fp)
	{
		fclose(fp);
		fp = NULL;
	}
}

char *
FontCatalogFile::readline(char *buf, int buflen)
{
	if (!fp || writing)
	{
		return (NULL);
	}

	return (fgets(buf, buflen, fp));
}


int
FontCatalogFile::isEof(void)
{
	int ret = 0;

	if (!fp || writing)
	{
		ret = -1;
	}
	else
	{
		ret = feof(fp);
	}

	return (ret);
}


int
FontCatalogFile::status(void)
{
	int ret = 0;

	if (fp == NULL)
	{
		ret = -1;
	}

	return (ret);
}

int
FontCatalogFile::output(const char *name)
{
	if (!fp || !writing)
	{
		return (-1);
	}

	if (!name)
	{
		return (-2);
	}

	for (int i=0; i < indentLevel; i++)
	{
		fprintf(fp, "\t");
	}
	fprintf(fp, "%s\n", name);
	return (0);
}


int
FontCatalogFile::output(const char *name, int value)
{
	if (!fp || !writing)
	{
		return (-1);
	}

	if (!name)
	{
		return (-2);
	}

	for (int i=0; i < indentLevel; i++)
	{
		fprintf(fp, "\t");
	}
	fprintf(fp, "%s = %d\n", name, value);
	return (0);
}


int
FontCatalogFile::output(const char *name, const char *str)
{
	if (!fp || !writing)
	{
		return (-1);
	}

	if (!name)
	{
		return (-2);
	}
	for (int i=0; i < indentLevel; i++)
	{
		fprintf(fp, "\t");
	}
	if (str)
	{
		fprintf(fp, "%s = %s\n", name, str);
	}
	else
	{
			fprintf(fp, "%s =\n", name);
	}
	return (0);
}


int
FontCatalogFile::indentIn(void)
{
	indentLevel++;
	return (indentLevel);
}

int
FontCatalogFile::indentOut(void)
{
	if (indentLevel)
	{
		indentLevel--;
	}
	return (indentLevel);
}
