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
 * The Original Code is Mozilla Communicator client code.
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

#include "nsNativeAppSupport.h"

#define PX_IMAGE_MODULES
#define PX_BMP_SUPPORT
#include <photon/PxImage.h>

#include <Ph.h>
#include <Pt.h>

class nsSplashScreenPh : public nsISplashScreen {
public:
    nsSplashScreenPh()
        : mDialog( 0 ), mRefCnt( 0 ) {

     /* initialize widget library and attach to Photon */
      if( PtInit( NULL ) )
         exit( EXIT_FAILURE );

     /* increase the draw buffer */
      PgSetDrawBufferSize( 0x8000 );
    }
    ~nsSplashScreenPh() {
       Hide();
    }

    NS_IMETHOD Show();
    NS_IMETHOD Hide();

    // nsISupports methods
    NS_IMETHOD_(nsrefcnt) AddRef() {
        mRefCnt++;
        return mRefCnt;
    }
    NS_IMETHOD_(nsrefcnt) Release() {
        --mRefCnt;
        if ( !mRefCnt ) {
            delete this;
            return 0;
        }
        return mRefCnt;
    }
    NS_IMETHOD QueryInterface( const nsIID &iid, void**p ) {
        nsresult rv = NS_OK;
        if ( p ) {
            *p = 0;
            if ( iid.Equals( NS_GET_IID( nsISplashScreen ) ) ) {
                nsISplashScreen *result = this;
                *p = result;
                NS_ADDREF( result );
            } else if ( iid.Equals( NS_GET_IID( nsISupports ) ) ) {
                nsISupports *result = NS_STATIC_CAST( nsISupports*, this );
                *p = result;
                NS_ADDREF( result );
            } else {
                rv = NS_NOINTERFACE;
            }
        } else {
            rv = NS_ERROR_NULL_POINTER;
        }
        return rv;
    }

    PtWidget_t *mDialog;
    nsrefcnt mRefCnt;
}; // class nsSplashScreenPh

NS_IMETHODIMP
nsSplashScreenPh::Show()
{
  PhImage_t  *img = nsnull;
  char       *p = NULL;
  int         inp_grp = 0;
  PhRid_t     rid;
  PhRegion_t  region;
  PhRect_t    rect;
  PRInt32     aWidth, aHeight;
  PhRect_t  console;

   /* Get the Screen Size and Depth, so I can center the splash dialog, there has to be a better way!*/
   p = getenv("PHIG");
   if (p == nsnull)
   {
     fprintf( stderr, "The PHIG environment variable must be set, try setting it to 1\n");
     exit( EXIT_FAILURE );
   }

   inp_grp = atoi(p);
   PhQueryRids( 0, 0, inp_grp, Ph_INPUTGROUP_REGION, 0, 0, 0, &rid, 1 );
   PhRegionQuery( rid, &region, &rect, NULL, 0 );
   inp_grp = region.input_group;
   PhWindowQueryVisible( Ph_QUERY_INPUT_GROUP | Ph_QUERY_EXACT, 0, inp_grp, &rect );
   aWidth  = rect.lr.x - rect.ul.x + 1;
   aHeight = rect.lr.y - rect.ul.y + 1;  

   /* Get the current console offset */
   if (PhWindowQueryVisible( Ph_QUERY_GRAPHICS, 0, inp_grp, &console ) == 0)
      
   mDialog = nsnull;   

   if (img = PxLoadImage("splash.bmp", NULL))
   {
     PtArg_t     arg[5];
     PhPoint_t   pos;
     
       img->flags = img->flags | Ph_RELEASE_IMAGE_ALL;

       pos.x = (aWidth/2)  - (img->size.w/2);
       pos.y = (aHeight/2) - (img->size.h/2);

      pos.x += console.ul.x;
      pos.y += console.ul.y;
             
       PtSetArg( &arg[0], Pt_ARG_DIM, &img->size, 0 );
       PtSetArg( &arg[1], Pt_ARG_POS, &pos, 0 );
       PtSetArg( &arg[2], Pt_ARG_WINDOW_RENDER_FLAGS, 0, 0xFFFFFFFF );
       mDialog = PtCreateWidget( PtWindow, NULL, 3, arg );

       PtSetArg( &arg[0], Pt_ARG_LABEL_TYPE, Pt_IMAGE, 0 );
       PtSetArg( &arg[1], Pt_ARG_LABEL_DATA, img, sizeof(PhImage_t) );
       PtCreateWidget( PtLabel, mDialog, 2, arg );
       PtRealizeWidget( mDialog );
       PtFlush();
   }
   else
   {
     fprintf( stderr, "Error loading splash screen image %s\n", "splash.bmp" );
   }

   return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenPh::Hide() {
	if ( mDialog ) {
        PtDestroyWidget(mDialog);
        mDialog = nsnull;
	}
    return NS_OK;
}

nsresult NS_CreateSplashScreen(nsISplashScreen**aResult)
{
  if ( aResult ) {	
      *aResult = new nsSplashScreenPh;
      if ( *aResult ) {
          NS_ADDREF( *aResult );
          return NS_OK;
      } else {
          return NS_ERROR_OUT_OF_MEMORY;
      }
  } else {
      return NS_ERROR_NULL_POINTER;
  }
}

PRBool NS_CanRun() 
{
	return PR_TRUE;
}
