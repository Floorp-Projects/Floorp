/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 */

#ifndef _SHORTCUT_H_
#define _SHORTCUT_H_

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

#ifdef __cplusplus
extern "C"
{
#endif

HRESULT CreateALink(LPSTR lpszPathObj, LPSTR lpszPathLink, LPSTR lpszDesc, LPSTR lpszWorkingPath, LPSTR lpszArgs, LPSTR lpszIconFullPath, int iIcon);

#ifdef __cplusplus
}
#endif

#endif