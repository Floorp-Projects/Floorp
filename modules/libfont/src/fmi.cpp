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
 * fmi.cpp (FontMatchInfoObject.cpp)
 *
 * C++ implementation of the (fmi) FontMatchInfoObject
 *
 * dp Suresh <dp@netscape.com>
 */


#include "fmi.h"

static const char *fmi_attributes[] = {
		nfFmiName,
		nfFmiCharset,
		nfFmiEncoding,
		nfFmiWeight,
		nfFmiPitch,
		nfFmiStyle,
		nfFmiUnderline,
		nfFmiStrikeOut,
		nfFmiResolutionX,
		nfFmiResolutionY,
};

static char fmi_attributes_type[] = {
		FMI_TYPE_CSTRING,
		FMI_TYPE_CSTRING,
		FMI_TYPE_CSTRING,
		FMI_TYPE_JINT,
		FMI_TYPE_JINT,
		FMI_TYPE_JINT,
		FMI_TYPE_JINT,
		FMI_TYPE_JINT,
		FMI_TYPE_JINT,
		FMI_TYPE_JINT,
};

#define MAX_STANDARD_ATTRIBUTES (sizeof(fmi_attributes)/sizeof(*fmi_attributes))

FontMatchInfoObject::
FontMatchInfoObject(const char *iname, const char *icharset,
					const char *iencoding, jint iweight, jint ipitch,
					jint istyle, jint iunderline, jint istrikeOut, jint resX, jint resY)
					: wfList(free_fmi_attr_store), stringRepresentation(NULL),
					  stringLen(0), stringMaxLen(0)
{
	addAttribute(nfFmiName, iname);
	addAttribute(nfFmiCharset, icharset);
	addAttribute(nfFmiEncoding, iencoding);
	addAttribute(nfFmiWeight, iweight);
	addAttribute(nfFmiPitch, ipitch);
	addAttribute(nfFmiStyle, istyle);
	addAttribute(nfFmiUnderline, iunderline);
	addAttribute(nfFmiStrikeOut, istrikeOut);
	addAttribute(nfFmiResolutionX, resX);
	addAttribute(nfFmiResolutionY, resY);
	WF_TRACEMSG( ("NF: Created fmi (%s).", describe()) );
}

FontMatchInfoObject::
FontMatchInfoObject(const char *reconstructString)
: wfList(free_fmi_attr_store), stringRepresentation(NULL),
  stringLen(0), stringMaxLen(0)
{
	reconstruct(reconstructString);
}

FontMatchInfoObject::
~FontMatchInfoObject()
{
	WF_TRACEMSG( ("NF: Destroying fmi (%s).", describe()) );
	releaseStringRepresentation();
}

const char ** FontMatchInfoObject::
ListAttributes()
{
	return (fmi_attributes);
}

void * FontMatchInfoObject::
GetValue(const char *attr)
{
	wfListElement *tmp = head;
	void *value = NULL;

	if (!attr)
	  {
		// Invalid input
		return (NULL);
	  }

    for (; tmp; tmp = tmp->next)
	  {
		struct fmi_attr_store *ele = (struct fmi_attr_store *)tmp->item;
		if (ele->attr && !strcmp(ele->attr, attr))
		  {
			value = ele->u.genericValue;
			break;
		  }
	  }

	return (value);
}

jint FontMatchInfoObject::
IsEquivalent(struct nffmi *fmi)
{
	void *fmi_value;
	void *this_value;
	for (int i = 0; i < MAX_STANDARD_ATTRIBUTES; i++)
	{
		fmi_value = nffmi_GetValue(fmi, fmi_attributes[i], NULL);
		this_value = GetValue(fmi_attributes[i]);
		if (fmi_attributes_type[i] == FMI_TYPE_CSTRING)
		{
			// FMI_TYPE_CSTRING
			char * fmi_str_value = (char *)fmi_value;
			char * this_str_value = (char *)this_value;
			if (fmi_str_value != NULL && *fmi_str_value != '*' &&
				this_str_value != NULL && *this_str_value != '*' &&
				!strcmp(fmi_str_value, this_str_value))
			{
				// Mismatch
				return (0);
			}
			
		}
		else
		{
			// FMI_TYPE_INT
#ifdef OSF1
                        long fmi_int_value = (long)fmi_value;
                        long this_int_value = (long)this_value;
#else
                        int fmi_int_value = (int)fmi_value;
                        int this_int_value = (int)this_value;
#endif
			if (fmi_int_value != 0 && this_int_value != 0
				&& fmi_int_value != this_int_value)
			{
				// Mismatch
				return (0);
			}
				
		}
	}
	return (1);
}


//
// Check for equality of fmi
//
int
FontMatchInfoObject::isEqual(FontMatchInfoObject *fob)
{
	// Two fmi's are equal if their string representations ARE THE SAME
	return (!strcmp(describe(), fob->describe()));
}


int FontMatchInfoObject::
reconstruct(const char *reconstructString)
{
	// Release any stringRepresentation
	releaseStringRepresentation();

	// Remove all existing attributes
	removeAll();

	// Reconstruct the FMI from the string.
	int i = 0;
	int len = strlen(reconstructString);
	char *fontpart = new char[len+1];
	if (!fontpart)
	{
		// ERROR: No Memory.
		return (-1);
	}
	const char *attr = NULL;
	const char *value = NULL;
	while (reconstructString && *reconstructString)
	{
		if (i < MAX_STANDARD_ATTRIBUTES)
		{
			reconstructString = scanFontpart(reconstructString, fontpart, attr, value);
			if (fmi_attributes_type[i] == FMI_TYPE_CSTRING)
			{
				addAttribute(fmi_attributes[i], fontpart);
			}
			else
			{
				jint ival = atoi(fontpart);
				addAttribute(fmi_attributes[i], ival);
			}
		}
		else
		{
			reconstructString = scanFontpart(reconstructString, fontpart, attr, value);
			addAttribute(attr, (char *)value);
		}
		i++;
	}
	return (0);
}

const char * FontMatchInfoObject::
describe(void)
{
	if (stringRepresentation != NULL)
	{
		return (stringRepresentation);
	}
	
	wfListElement *tmp = head;
	stringLen = 0;
	stringMaxLen = 0;
	
	for (; tmp; tmp = tmp->next)
	{
		struct fmi_attr_store *ele = (struct fmi_attr_store *)tmp->item;
		addToString(stringRepresentation, stringLen, stringMaxLen, ele);
	}
	return (stringRepresentation);
}   

//
// Private method implementations
//

int FontMatchInfoObject::
addAttribute(const char *attr, const char *value)
{
	struct fmi_attr_store *ele = new fmi_attr_store;
	if (!ele)
	  {
		// no memory
		return (-1);
	  }

	ele->attr = attr;
	ele->valueType = FMI_TYPE_CSTRING;
	ele->u.stringValue = CopyString(value);
	add(ele);

	// Sync stringRepresentation
	addToString(stringRepresentation, stringLen, stringMaxLen, ele);
	
	return (0);
}


int FontMatchInfoObject::
addAttribute(const char *attr, jint value)
{
	struct fmi_attr_store *ele = new fmi_attr_store;
	if (!ele)
	  {
		// no memory
		return (-1);
	  }

	ele->attr = attr;
	ele->valueType = FMI_TYPE_JINT;
	ele->u.intValue = value;
	add(ele);

	// Sync stringRepresentation
	addToString(stringRepresentation, stringLen, stringMaxLen, ele);
	
	return (0);
}

void
/*ARGSUSED*/
free_fmi_attr_store(wfList *object, void *item)
{
	struct fmi_attr_store *ele = (struct fmi_attr_store *)item;
	if (ele)
	{
		if (ele->valueType == FMI_TYPE_CSTRING)
		{
			// Free the copy of the string we took
			delete[] (char *)ele->u.stringValue;
		}
		delete ele;
	}
}

#define MY_BLOCK_SIZE 16
static const char needsEscape[] = {
	FMI_FONTPART_DEFAULT, FMI_FONTPART_SEPERATOR,
	FMI_FONTPART_ESCAPE, FMI_FONTPART_ATTRSEPERATOR,
	'\0',
};


int FontMatchInfoObject::
addToString(const char *&str, int &strLen, int &strMaxLen, struct fmi_attr_store *ele)
{
	const char *value;
	char *newValue = NULL;
	char strValue[64];	// XXX There must be a better way to do this.
	int ret = 0;

	addToString(str, strLen, strMaxLen, FMI_FONTPART_SEPERATOR);
	if (count() > MAX_STANDARD_ATTRIBUTES)
	{
		// Add the attribute also
		addToString(str, strLen, strMaxLen, ele->attr);
		addToString(str, strLen, strMaxLen, ":");
	}
	
	if (ele->valueType == FMI_TYPE_JINT)
	{
		strValue[0] = '\0';
		if (ele->u.intValue > 0)
		{
			sprintf(strValue, "%d", ele->u.intValue);
		}
		value = strValue;
	}
	else
	{
		// Escape {:-*} in value. These have special meaning to us.
		value = ele->u.stringValue;
		if (value && *value)
		{
			newValue = EscapeString(value, needsEscape);
			if (!newValue)
			{
				// ERROR: No memory
				return (-1);
			}
			value = newValue;
		}
	}
	ret = addToString(str, strLen, strMaxLen, value);
	if (newValue)
	{
		delete[] newValue;
	}
	return (ret);
}

int FontMatchInfoObject::
addToString(const char *&str, int &strLen, int &strMaxLen, const char *value)
{
	int valueLen = 0;
	if (!value || !*value)
	{
		// Use default symbol
		return (0);
	}
	valueLen = strlen(value);

	while ((strMaxLen - strLen) <= valueLen)
	{
		str = (char *)WF_REALLOC((char*)str,strMaxLen + MY_BLOCK_SIZE);
		if (str == NULL) return 0;
		strMaxLen += MY_BLOCK_SIZE;
	}

	strcpy((char*)(str+strLen), value);
	strLen += valueLen;
	return strLen;
}

int FontMatchInfoObject::
addToString(const char *&str, int &strLen, int &strMaxLen, const char value)
{
	if ((strMaxLen - strLen) <= 1)
	{
		str = (char *)WF_REALLOC((char*)str,strMaxLen + MY_BLOCK_SIZE);
		if (str == NULL) return 0;
		strMaxLen += MY_BLOCK_SIZE;
	}

	*((char *)(str+strLen)) = value;
	strLen++;
	*((char *)(str+strLen)) = '\0';
	return strLen;
}

int FontMatchInfoObject::
releaseStringRepresentation(void)
{
	if (stringRepresentation)
	{
		WF_FREE((char*)stringRepresentation);
		stringRepresentation = NULL;
		stringLen = stringMaxLen = 0;
	}
	return (0);
}

//
// Scans for the next fontpart from str and copies it into fontpart.
// Assumes enough memory has been allocated for fontpart.
// If value/attr are non-null, then points value and attr to their
// appropriate places in fontpart
//
const char *FontMatchInfoObject::
scanFontpart(const char *str, char *fontpart, const char *&value, const char *&attr)
{
	char *attrEnd = NULL;

	if (attr) attr = NULL;
	if (value) value = fontpart;
	if (*str == FMI_FONTPART_SEPERATOR)
		str++;
	while (*str && *str != FMI_FONTPART_SEPERATOR)
	{
		if (*str == FMI_FONTPART_ESCAPE)
		{
			str++;
		}
		else
		{
			if (*str == FMI_FONTPART_ATTRSEPERATOR)
			{
				attrEnd = fontpart+1;
			}

		}
		*fontpart++ = *str++;
	}
	*fontpart = '\0';

	if (attrEnd)
	{
		if (attr) attr = fontpart;
		*attrEnd = '\0';
		if (value) value = attrEnd+1;
	}
	return (str);
}
