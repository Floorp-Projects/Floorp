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
 *
 * All function that are of general use in libfont/ are implemented here.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "libfont.h"
#include "nf.h"
#include "wfMisc.h"
#include "Mnffmi.h"

#ifdef XP_MAC
#include "probslet.h"
#else
#include "obsolete/probslet.h"
#endif


// The logic we are using to merge these is brute force.
// For each size in newSizes list, if it is already in the oldSizes,
// then we wont add it. If not we will add it in.
// We should make sure the we are using the public memory allocator
// here because this memory is expected to be freed by the caller.
#define INTS_AT_A_TIME 128
void
MergeSizes(jdouble * &oldSizes, int &oldLenMax, jdouble *newSizes)
{
    int newlen = 0;
    
    if (!newSizes)
      {
		return;
      }
    
    while (newSizes[newlen] >= 0)
      {
		int i = 0;
		int found = 0;
		if (oldSizes == NULL)
		  {
			oldSizes = (jdouble *) WF_ALLOC(sizeof(*oldSizes) * INTS_AT_A_TIME);
			if (!oldSizes) return;
			oldSizes[0] = -1;
			oldLenMax = INTS_AT_A_TIME;
		  }
		for (;oldSizes[i] >= 0; i++)
		  {
			if (newSizes[newlen] == oldSizes[i])
			  {
				found++;
				break;
			  }
		  }
		if (!found)
		  {
			// Add newSizes[newlen] to oldSizes
			if (i >= oldLenMax-1)
			  {
				// Need more memory.
				oldSizes = (jdouble *)
				  WF_REALLOC(oldSizes,
							 sizeof(*oldSizes) * (oldLenMax + INTS_AT_A_TIME));
				if (!oldSizes) return;
				oldLenMax += INTS_AT_A_TIME;
			  }
			oldSizes[i++] = newSizes[newlen];
			oldSizes[i++] = -1;
		  }
		newlen++;
      }
}

//
// CopyString
//
// Make a copy of the input C-String and return a new C++ allocated
// C-String.
//
char *
CopyString(const char *str)
{
	char *newStr = NULL;
	int len = 0;
	const char *tmp1;
	char *tmp2;

	if (str && *str)
	  {
		for (tmp1=str; *tmp1; len++, tmp1++);
		newStr = new char[len+1];
		if (newStr)
		  {
			for (tmp1=str, tmp2=newStr; *tmp1; *tmp2++ = *tmp1++);
			*tmp2 = '\0';
		  }
	  }
	return (newStr);
}


//
// Returns a newly allocated string that Escapes characters {:-*\}
// in the input str.
// For eg.
//	input string	: string-to*be-escaped\n
//	return		: string\-to\*be\-escaped\\n
//
// WARNING: Caller must delete the return string.

char *
EscapeString(const char *str, const char *escapeThese)
{
	if (!str || !*str)
	{
		return NULL;
	}
	char *newstr = new char [2*strlen(str)+1];
	if (!newstr)
	{
		// ERROR: No memory
		return (NULL);
	}
	char* t = newstr;
	while (*str)
	{
		if (strchr(escapeThese, *str))
			*t++ = '\\';
		*t++ = *str++;
	}
	*t = '\0';
	return (newstr);
}

//
// ScanToken
//  Scans a token delimited by 'stopChars' from 'str' and copies it out
//	into 'buf'
//	By default this compresses multiple spaces in the token to a single space.
//	If 'noSpacesOnOutput' is set, all spaces, leading/trailing/intermediate
//	spaces are stripped.
//
//	Returns a pointer to the character after the token in 'str'
//
const char *
wf_scanToken(const char *str, char *buf, int len, char *stopChars,
			int noSpacesOnOutput)
{
	int pos = 0;

	str = wf_skipSpaces(str);

    while (*str != '\0')
	{
		while (*str != '\0' && ! isspace(*str) &&
			(stopChars == NULL || strchr(stopChars, *str) == NULL))
		{
			buf[pos++] = *str++;
        }

		str = wf_skipSpaces(str);
		if (*str == '\0' || stopChars == NULL ||
			strchr(stopChars, *str) != NULL)
		{
			break;
        }
		if (! noSpacesOnOutput)
		{
			buf[pos++] = ' ';
		}
	}
	buf[pos] = '\0';
	return str;
}

//
// wf_scanVariableAndValue
// Modifies buf such that variable and value point to the the LHS and RHS of
// a buf of the format " variable = value "
// Leading and trailing spaces for both variable and value are trimmed.
//
int
wf_scanVariableAndValue(char *buf, int buflen, char *&variable, char *&value)
{
	variable = buf;
	value = NULL;
	while (*buf && *buf != '=')
	{
		buf ++;
	}
	if ('=' == *buf)
	{
		*buf = '\0';
		buf++;
		value = buf;
	}
	variable = wf_trimSpaces(variable);
	value = wf_trimSpaces(value);
	return (0);
}

/*
 * wf_StringEndsWith
 *
 * returns nonzero if string 's' ends with string 'endstr'
 * else returns zero
 */
int
wf_stringEndsWith(const char *s, const char *endstr)
{
  int l, lend;
  int retval = 0;

  if (!endstr)
  {
	  /* All strings ends in NULL */
	  retval = 1;
  }
  else if (!s)
  {
	  /* NULL strings will never have endstr at its end */
	  retval = 0;
  }
  else
  {
	  lend = strlen(endstr);
	  l = strlen(s);
#ifdef XP_WIN
	  if (l >= lend && !stricmp(s+l-lend, endstr))  /* ignore case for windows */
#else
      if (l >= lend && !strcmp(s+l-lend, endstr))
#endif
	  {
		  retval = 1;
	  }
  }
  return (retval);
}


//
// wf_skipSpaces
// Returns a pointer to the first non-space character in 'str'
//
const char *
wf_skipSpaces(const char *str)
{
    while (*str != '\0' && isspace(*str))
	{
        str++;
	}
    return str;
}       


//
// wf_trimSpaces
// Trims initial and trailing spaces from the input string.
//
char *
wf_trimSpaces(char *inputString)
{
	char *str = inputString;

	if (!str || !*str)
	{
		return (str);
	}

	// Fix trailing spaces
	int nstr = strlen(str);
	int len = nstr;
	while (len && isspace(str[len-1]))
	{
		--len;
	}
	if (len < nstr)
	{
		str[len] = '\0';
		nstr = len+1;
	}

	// Fix leading spaces
	while (*str && isspace(*str))
	{
		str++;
		nstr--;
	}
	if (str != inputString)
	{
		memmove(inputString, str, nstr+1);
	}
	
	return (inputString);
}

//
// Returns
// 0	: if both strings are the same on a case insensitive compare
// -1	: if any of the string were NULL
// 1	: if string 'two' is a case insensitive substring of string 'one'
// 2	: if string 'one' is a case insensitive substring of string 'two'
// 3	: if case insensitive compare failed
//
int
wf_strcasecmp(const char *one, const char *two)
{
	if (!one || !two)
	{
		return (-1);
	}

	for(; *one && *two; one++, two++)
	{
		if ((*one != *two) && (tolower(*one) != tolower(*two)))
			return (3);
	}
	if (*one)
		return (1);
	if (*two)
		return (2);
	return 0;
}

//
// return 0 if the 2 strings are same on case insensitive compare upto
//		atmost n bytes
// else -1
//
int
wf_strncasecmp(const char *one, const char *two, int n)
{
	if (!one || !two)
	{
		return (-1);
	}

	for(; *one && *two && n-- > 0; one++, two++)
	{
		if ((*one != *two) && (tolower(*one) != tolower(*two)))
			return (-1);
	}

	if (n <= 0 || (*one == '\0' && *two == '\0'))
		return (0);
	else
		return (-1);
}


// returns a pointer to the extension part of the input string
const char *wf_getExtension(const char *fullname)
{
	const char *ext = NULL;
	if (!fullname || !*fullname)
	{
		return (ext);
	}

	while (*fullname)
	{
		if (*fullname == '.')
		{
			ext = fullname+1;
		}
		fullname++;
	}
	return(ext);
}

int wf_addToString(char **str, int *len, int *maxlen, const char *s)
{
	if (!s || !*s)
	{
		return (0);
	}

	if (!maxlen || !str)
	{
		// Invalid params.
		return (-1);
	}

	int slen = strlen(s);
	int curlen = 0;
	if (!len)
	{
		curlen = strlen(*str);
	}
	else
	{
		curlen = *len;
	}

	if (curlen + slen + 1 > *maxlen)
	{
		int newlen = WF_ROUND_INT(curlen+slen+1, WF_STRING_ALLOCATION_STEP);
		*str = (char *)WF_REALLOC(*str, sizeof(char)*newlen);
		if (!*str)
		{
			// No memory
			return (-1);
		}
		*maxlen = newlen;
	}
	memcpy((*str)+curlen, s, slen+1);

	if (len)
	{
		*len += slen;
	}

	return (0);
}

//
// Webfont related miscellaneous routines
//

int
wf_releaseFmis(struct nffmi **fmis)
{
	if (!fmis)
	{
		return (-1);
	}
	while (*fmis)
	{
		nffmi_release(*fmis, NULL);
		fmis++;
	}
	return (0);	
}


//-----------------
// File operations
//-----------------

int wf_isFileReadable(const char *filename)
{
	if (!filename || !*filename)
		return (-1);

	FILE *fp = fopen(filename, "r");
	int readable = (fp != NULL);
	fclose(fp);

	return (readable);
}

int wf_isFileDirectory(const char *filename)
{
	if (!filename || !*filename)
		return (0);

	PRFileInfo finfo;

	if (PR_GetFileInfo(filename, &finfo) == PR_FAILURE)
		return (0);

	return (finfo.type == PR_FILE_DIRECTORY);
}


//
// expandFileName:
//
// expands the following in the filename
// beginning ~	: HOME
//
// Returns the no: of expansions done. If none was done, returns 0.
// Returns -ve if there was no space to do the expansion.
//
extern int wf_expandFilename(char *filename, int maxlen)
{
	int ret = 0;

	// Expand initial ~ to HOME
	if (*filename == '~')
	{
		const char *home = getenv("HOME");
		if (home && *home)
		{
			// Insert home into filename
			int homelen = strlen(home);
			int len = strlen(filename);
			
			if (len+homelen <= maxlen)
			{
				// Shift all characters in filename by homelen bytes
				memmove(filename+homelen, filename+1, len);
				// Copy home into filename
				memcpy(filename, home, homelen);
				ret ++;
			}
			else
			{
				ret = -1;
			}
		}
	}
	return (ret);
}
