/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


// XXX Must make this a class of its own.

//
// Rounds an integer to the next higher integer that is divisible by roundto
//
#define WF_ROUND_INT(a, roundto) (((int)((a)/(roundto)) + 1) * (roundto))

//
// MergeSizes
//
// Merge size lists together. A size list is an array of size values
// (non-negative) terminated by a negative value.
// It takes a newSizes list and merges this into the oldSizes list allocating
// memory for the oldSizes list in the process.
//
extern void MergeSizes(jdouble * &oldSizes, int &oldLenMax, jdouble *newSizes);

//
// CopyString
//
// Make a copy of the input C-String and return a new C++ allocated
// C-String.
//
extern char *CopyString(const char *str);

//
// EscapeString
//
// Returns a newly allocated string that Escapes characters from escapeThese
// in the input str.
// For eg.
//	input string	: string-to*be-escaped\n
//	return		: string\-to\*be\-escaped\\n
//
// WARNING: Caller must delete the return string.
//
extern char *EscapeString(const char *str, const char *escapeThese);

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
extern const char *wf_scanToken(const char *str, char *buf, int len,
								char *stopChars, int noSpacesOnOutput);

//
// wf_scanVariableAndValue
// Modifies buf such that variable and value point to the the LHS and RHS of
// a buf of the format " variable = value "
// Leading and trailing spaces for both variable and value are trimmed.
//
extern int wf_scanVariableAndValue(char *buf, int buflen,
								   char *&variable, char *&value);


/*
 * wf_StringEndsWith
 *
 * returns nonzero if string 's' ends with string 'endstr'
 * else returns zero
 */
extern int wf_stringEndsWith(const char *s, const char *endstr);


//
// wf_skipSpaces
// Returns a pointer to the first non-space character in 'str'
//
extern const char *wf_skipSpaces(const char *str);


//
// wf_trimSpaces
// Trims initial and trailing spaces from the input string.
//
char *wf_trimSpaces(char *inputString);

//
// Returns
// 0	: if both strings are the same on a case insensitive compare
// -1	: if any of the string were NULL
// 1	: if string 'two' is a case insensitive substring of string 'one'
// 2	: if string 'one' is a case insensitive substring of string 'two'
// 3	: if case insensitive compare failed
//
extern int wf_strcasecmp(const char *one, const char *two);

//
// Returns
// 0	: if both strings are the same on a case insensitive compare of atmost
//			n bytes.
// -1	: if any of the string were NULL
extern int wf_strncasecmp(const char *one, const char *two, int n);

//
// returns a pointer to the extension part of the input string
extern const char *wf_getExtension(const char *fullname);

// =====================================================================
// Webfonts related miscellaneous routines
// =====================================================================

// Release an array of fmis. Will call nffmi::release() on each fmi.
// 
// WARNING: WILL NOT RELEASE THE MEMORY FOR THE fmis array.
extern int wf_releaseFmis(struct nffmi **fmis);

//
// Adds a string to an existing string. If more memory is required, it extends
// the srcstring (realloc) to fit the new concatenated string.
//
#define WF_STRING_ALLOCATION_STEP 64
extern int wf_addToString(char **str, int *len, int *maxlen, const char *s);


// ------------------------------------------------------------
//                         FILE operations
// ------------------------------------------------------------

// All these return
// 0   : False
// +ve : True
// -1  : Error
extern int wf_isFileDirectory(const char *name);
extern int wf_isFileReadable(const char *filename);

//
// expandFileName:
//
// expands the following in the filename
// beginning ~	: HOME
//
// Returns the no: of expansions done. If none was done, returns 0.
// Returns -ve if there was no space to do the expansion.
//
extern int wf_expandFilename(char *filename, int maxlen);

