/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#include "nsScreenPh.h"

#include <Pt.h>
#include "nsPhGfxLog.h"

nsScreenPh :: nsScreenPh ( ) {
  PhSysInfo_t       SysInfo;
  PhRect_t          rect;
  char              *p = NULL;
  int               inp_grp;
  PhRid_t           rid;
  PhRegion_t        region;
  
  NS_INIT_REFCNT();

	/* Initialize the data members */
	/* Get the Screen Size and Depth*/
	p = getenv("PHIG");
	if( p ) inp_grp = atoi(p);
	else inp_grp = 1;

	PhQueryRids( 0, 0, inp_grp, Ph_INPUTGROUP_REGION, 0, 0, 0, &rid, 1 );
	PhRegionQuery( rid, &region, &rect, NULL, 0 );
	inp_grp = region.input_group;
	PhWindowQueryVisible( Ph_QUERY_INPUT_GROUP | Ph_QUERY_EXACT, 0, inp_grp, &rect );
	mWidth  = rect.lr.x - rect.ul.x + 1;
	mHeight = rect.lr.y - rect.ul.y + 1;  

	/* Get the System Info for the RID */
	if( PhQuerySystemInfo(rid, NULL, &SysInfo ) ) {
		/* Make sure the "color_bits" field is valid */
		if( SysInfo.gfx.valid_fields & Ph_GFX_COLOR_BITS ) mPixelDepth = SysInfo.gfx.color_bits;
		}
	}

nsScreenPh :: ~nsScreenPh( ) { }

// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenPh, nsIScreen)

NS_IMETHODIMP nsScreenPh :: GetPixelDepth( PRInt32 *aPixelDepth ) {
	*aPixelDepth = mPixelDepth;
	return NS_OK;
	} // GetPixelDepth


NS_IMETHODIMP nsScreenPh :: GetColorDepth( PRInt32 *aColorDepth ) {
  return GetPixelDepth ( aColorDepth );
	}

NS_IMETHODIMP nsScreenPh :: GetRect( PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight ) {
  *outTop = 0;
  *outLeft = 0;
  *outWidth = mWidth;
  *outHeight = mHeight;
  return NS_OK;
	}

NS_IMETHODIMP nsScreenPh :: GetAvailRect( PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight ) {
  *outTop = 0;
  *outLeft = 0;
  *outWidth = mWidth;
  *outHeight = mHeight;
  return NS_OK;
	}
