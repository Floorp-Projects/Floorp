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
 */

#ifndef _nstooltipmgr_h
#define _nstooltipmgr_h

// Tooltip manager: code to watch windows & send tooltip events.
//
// Created 25-9-98; interface determined by nsWindow requirements.

#include "nscore.h"
#include "nsrect.h"
#include "nsWidgetDefs.h"
#include "nshashtable.h"

class nsWindow;
class TTData;

class nsTooltipManager
{
 protected:
   static nsTooltipManager *sMgr;

   nsHashtable mManaged;
   PRInt32     mElements;
   PRBool      mTTMode;

   MRESULT HandleMsg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

   TTData *GetEntry( nsWindow *);
   TTData *GetEntry( HWND);

   nsTooltipManager();
  ~nsTooltipManager();

   friend MRESULT EXPENTRY HandleMsg( HWND hwnd, ULONG msg, MPARAM, MPARAM);

 public:
   static nsTooltipManager *GetManager();

   void SetTooltips( nsWindow *window, PRUint32 cTips, nsRect *rects[]);
   void UpdateTooltips( nsWindow *window, nsRect *rects[]);
   void RemoveTooltips( nsWindow *window);
};

#endif
