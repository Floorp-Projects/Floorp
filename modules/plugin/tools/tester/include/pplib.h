/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/************************************************************************************/
/*                                                                                  */
/*                              PRIVATE PROFILE LIBRARY                             */
/*                                                                                  */
/* Private profile is a text file consisting of sections and keys with their values */
/*                                                                                  */
/* [SectionName1]                                                                   */
/* key1=value2                                                                      */
/* key2=value2                                                                      */
/*                                                                                  */
/* [SectionName2]                                                                   */
/* key1=value2                                                                      */
/* key2=value2                                                                      */
/*                                                                                  */
/* ...                                                                              */
/*                                                                                  */
/*                                                                                  */
/************************************************************************************/

#ifndef __PPLIB_H__
#define __PPLIB_H__

/************************************************************************************/
/*                                                                                  */
/* PP_GetString copies the string value of the specified key in specified section   */
/* If the key in this section is not present or if the specified section is not     */
/* present in the file or file does not exist, szDefault is copied.                 */
/* If szSection is NULL all sections are copied to the supplied buffer (szString),  */
/* They are separated by 0 and the whole buffer is terminated by two 0's.           */
/* If szKey is NULL all keys of the specified sections are copied with the same     */
/* separation and termination logic                                                 */
/*                                                                                  */
/* Function returns number of bytes copied.                                         */
/*                                                                                  */
/************************************************************************************/
DWORD PP_GetString(char * szSection, char * szKey, char * szDefault, char * szString, 
                   DWORD dwSize,  XP_HFILE hFile);

/************************************************************************************/
/*                                                                                  */
/* PP_WriteString assigns specified value to specified key in specified section.    */
/* If section or key are not present in the file they are created.                  */
/* If file does not exist it is created.                                            */
/*                                                                                  */
/************************************************************************************/
BOOL PP_WriteString(char * szSection, char * szKey, char * szString, XP_HFILE hFile);

#endif // __PPLIB_H__
