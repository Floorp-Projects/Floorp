/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * syd@netscape.com 2/12/00. 
 */

#include "nsNativeAppSupport.h"
#include "gdk/gdk.h"
#include "gdk/gdkprivate.h"

class nsSplashScreenGtk : public nsISplashScreen {
public:
    nsSplashScreenGtk()
        : mDialog( 0 ), mRefCnt( 0 ) {
		// if this fails, we exit
		gdk_init( 0, NULL );
    }
    ~nsSplashScreenGtk() {
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

    GdkWindow *mDialog;
    nsrefcnt mRefCnt;
}; // class nsSplashScreenGtk

NS_IMETHODIMP
nsSplashScreenGtk::Show()
{
   GdkPixmap *pmap;
   GdkBitmap *mask;
   GdkWindowAttr attr;
   gint attrmask;
   int x, y, depth, wwidth, wheight;
   int c, width, height, num_cols, cpp;

   FILE *fp;
   GdkColor transparentColor;

   /* figure out what we are dealing with here */

   if ( ( fp = fopen( "splash.xpm", "r" ) ) == (FILE *) NULL ) 
	return NS_ERROR_FAILURE;
 
   // find the first '"'
 
   while ( !feof( fp ) ) {
	c = fgetc( fp );
	if ( c == '"' )
		break;
   } 
  
   if ( feof( fp ) ) {
	fclose( fp );
	return NS_ERROR_FAILURE;
   }	

   /* read size info */

   fscanf( fp, "%d %d %d %d", &width, &height, &num_cols, &cpp );

   fclose( fp );

   /* create a window */

   gdk_window_get_geometry( NULL, &x, &y, &wwidth, &wheight, &depth ); 

   attr.window_type = GDK_WINDOW_TOPLEVEL;
   attr.wclass = GDK_INPUT_OUTPUT;
   attr.x = wwidth >> 1; attr.x -= width >> 1;
   attr.y = wheight >> 1; attr.y -= height >> 1;
   attr.width = width;
   attr.height = height;
   attr.event_mask = 0;

   mDialog = gdk_window_new( NULL, &attr, GDK_WA_X | GDK_WA_Y );	
   gdk_window_set_override_redirect(mDialog, true);
   gdk_window_show( mDialog );

   GdkWindowPrivate *mprivate;

   mprivate = (GdkWindowPrivate*) mDialog;
   XFlush( mprivate->xdisplay );

   /* create a pixmap based on xpm data */

   pmap = gdk_pixmap_colormap_create_from_xpm( mDialog, 
	gdk_colormap_get_system(), &mask, &transparentColor, "splash.xpm" );

   if ( !pmap ) {
   	gdk_window_destroy(mDialog);
	mDialog = nsnull;
	return NS_ERROR_FAILURE;
   }

   GdkGC *gc;

   gc = gdk_gc_new( mDialog );

   gdk_window_copy_area( mDialog, gc, 0, 0, pmap, 0, 0, width, height );
   XFlush( mprivate->xdisplay );

   return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenGtk::Hide() {
	if ( mDialog ) {
        	gdk_window_destroy(mDialog);
        	mDialog = nsnull;
	}
    return NS_OK;
}

nsresult NS_CreateSplashScreen(nsISplashScreen**aResult)
{
  if ( aResult ) {	
      *aResult = new nsSplashScreenGtk;
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
