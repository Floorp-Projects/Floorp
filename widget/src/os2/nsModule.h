/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by         Description of modification
 * 03/20/2001       achimha@innotek.de  created
 * 
 */

#ifndef _H_NSMODULE
#define _H_NSMODULE

#include "nsISupports.h"
#include "nsDragService.h"
#include "nsWidgetDefs.h"
#include "nsHashtable.h"

#define NS_MODULEDATAOS2_CID \
 { 0xa506d27e, 0x1dd1, 0x11b2, \
   { 0x8a, 0x52, 0x82, 0xc3, 0x9, 0xe6, 0xdc, 0xc30 } }

// Module data -- anything that would be static, should be module-visible,
//                or any magic constants.
class nsWidgetModuleData : public nsISupports
{
 public:

   // nsISupports
   NS_DECL_ISUPPORTS

   HMODULE hModResources;   // resource module
   SIZEL   szScreen;        // size of screen in pixels
   BOOL    bIsDBCS;         // true if system is dbcs
   PRBool    bIsTrackPoint;   // true if system has a TrackPoint
   HPOINTER hptrArray[17];

   // xptoolkit services we look after, & the primaeval appshell too.
   nsDragService           *dragService;
   nsIAppShell             *appshell;

   // We're caching resource-loaded things here too.  This may be
   // better-suited elsewhere, but there shouldn't be very many of them.
   HPOINTER GetPointer( nsCursor aCursor);

   const char *DBCSstrchr( const char *string, int c );
   const char *DBCSstrrchr( const char *string, int c );

   nsWidgetModuleData();
  ~nsWidgetModuleData();

   void Init( nsIAppShell *aPrimaevalAppShell);
};

#endif
