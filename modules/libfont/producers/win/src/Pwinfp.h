/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*
 * Private implementation of the FontDisplayer.
 */

#ifndef _Pwinfp_H_
#define _Pwinfp_H_

#ifndef XP_WIN
#define XP_WIN
#endif

#include "Mnffmi.h"
#include "Mnfrc.h"
#include "Mnfdoer.h"
#include "Mwinfp.h"			/* Generated header */

// structure to hold each font broker asked by lookupFont()
typedef struct NetscapePrimeFont_s {
    LOGFONT						logFontInPrimeFont;
	int							csIDInPrimeFont;
	int							encordingInPrimeFont;
	int							YPixelPerInch;
	struct NetscapePrimeFont_s	*nextFont;       // link for list.
}	* pPrimeFont_t ;


struct winfpImpl {
  winfpImplHeader	header;

  /*************************************************************************
   * FONTDISPLAYER Implementors:
   *	Add your private data here. If you are implementing in C++,
   *	then hang off a pointer to your actual object here.
   *************************************************************************/

    struct nffbp    *m_pBrokerObj;

	/*** The following list need not be maintained at all. - dp
    // header of font link list.
	pPrimeFont_t	m_pPrimeFontList;
	***/
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_winfp_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_winfp_finalize

#endif /* _Pwinfp_H_ */
