/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * Contributor(s): 
 *
 *   Rusty Lynch <rusty.lynch@intel.com>
 */
/*
 * Declares the nsSanePluginInterface class for the SANE plugin.
 */

#ifndef __NS_SANE_PLUGIN_H__
#define __NS_SANE_PLUGIN_H__

#include "prthread.h"
#include "nscore.h"
#include "nsplugin.h"
#include "gdksuperwin.h"
#include "gtkmozbox.h"
#include "gtk/gtk.h"
#include "nsIPlugin.h"
#include "nsSanePluginControl.h"
#include "nsIScriptContext.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

///////////////////////////////////
// Needed for encoding jpeg images
extern "C"
{
#include <jpeglib.h>
}

///////////////////////////////////
// Needed for SANE interface
extern "C"
{
#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>
}

typedef struct _PlatformInstance
{
    Window        window;
    GtkWidget   * widget;
    GdkSuperWin * superwin;
    Display     * display;
    uint16        x;
    uint16        y;
    uint32        width; 
    uint32        height;

} PlatformInstance;

// Threaded routine for grabbing a frame from device.
// If a callback for onScanComplete was set in the embed/object
// tag, then this routine will call it before exiting.
//void PR_CALLBACK scanimage_thread_routine( void * arg);
void PR_CALLBACK scanimage_thread_routine( void * arg );

class nsSanePluginInstance : public nsIPluginInstance, 
                             public nsISanePluginInstance
{
    friend void PR_CALLBACK scanimage_thread_routine( void *);

    public:
  
    nsSanePluginInstance(void);
    virtual ~nsSanePluginInstance(void);

    /////////////////////////////////////////////////////////////////////
    // nsIPluginInstance inherited interface
    NS_IMETHOD Initialize(nsIPluginInstancePeer* peer);
    NS_IMETHOD GetPeer(nsIPluginInstancePeer* *result);
    NS_IMETHOD Start(void);
    NS_IMETHOD Stop(void);
    NS_IMETHOD Destroy( void );
    NS_IMETHOD SetWindow( nsPluginWindow* window );
    NS_IMETHOD NewStream( nsIPluginStreamListener** listener );
    NS_IMETHOD Print( nsPluginPrint* platformPrint );
    NS_IMETHOD GetValue( nsPluginInstanceVariable variable, void *value );

    // Not used for this platform! Only a placeholder.
    NS_IMETHOD HandleEvent( nsPluginEvent* event, PRBool* handled );

    // End of nsIPlugIninstance inherited interface.
    /////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // nsSanePluginInstance specific methods:
    NS_DECL_ISUPPORTS ;
    
    // Execute given callback in window's JavaScript
    NS_IMETHOD DoScanCompleteCallback();
    NS_IMETHOD DoInitCompleteCallback();

    void SetMode(nsPluginMode mode) { fMode = mode; }
    void SetState(PRInt32 aState) { mState = aState; };
    NS_IMETHOD PaintImage(void);
    char * GetImageFilename();
    GtkWidget * GetFileSelection();
    PRBool IsUIThread();
    nsresult OpenSaneDeviceIF( void );

    //*** Methods exposed through the XPConnect interface ***
    NS_DECL_NSISANEPLUGININSTANCE

private:

    GtkWidget                  *mDrawing_area;
    GtkWidget                  *mEvent_box;
    PlatformInstance            fPlatform;
    char                        mImageFilename[255];
    GtkWidget                  *mFileSelection;
    GdkRectangle                mZoom_box;
    unsigned char              *mRGBData;
    int                         mRGBWidth, mRGBHeight;

    // line attributes for zoom box
    PRInt32                     mLineWidth;
    GdkLineStyle                mLineStyle;
    GdkCapStyle                 mCapStyle;
    GdkJoinStyle                mJoinStyle;

    // zoom box change variables
    float                       mTopLeftXChange;
    float                       mTopLeftYChange;
    float                       mBottomRightXChange;
    float                       mBottomRightYChange;

    // jpeg compression attributes
    int                         mCompQuality;
    enum J_DCT_METHOD           mCompMethod;

    // sane specific members
    SANE_Handle                 mSaneHandle;
    SANE_String                 mSaneDevice;
    SANE_Bool                   mSaneOpen;
    PRBool                      mSuccess;
    PRInt32                     mState;

    // needed for JavaScript Callbacks
    char                        *mOnScanCompleteScript;
    char                        *mOnInitCompleteScript;
    PRThread                    *mScanThread;
    PRThread                    *mUIThread;

protected:

    nsIPluginInstancePeer*      fPeer;
    nsPluginWindow*             fWindow;
    nsPluginMode                fMode;
    nsIPluginManager*           mPluginManager;

private:

    int                         WritePNMHeader (int fd, SANE_Frame format, 
                                                int width, int height, 
                                                int depth);

    void                        PlatformNew( void );
    nsresult                    PlatformDestroy( void );
    PRInt16                     PlatformHandleEvent( nsPluginEvent* event );
    nsresult                    PlatformSetWindow( nsPluginWindow* window );  
};

class nsSanePluginStreamListener : public nsIPluginStreamListener
{
    public:
  
    NS_DECL_ISUPPORTS ;
  
    /*
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @return The return value is currently ignored.  In the future it may be
     * used to cancel the URL load..
     */
    NS_IMETHOD OnStartBinding( nsIPluginStreamInfo* pluginInfo );
  
    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param aIStream  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the 
     * stream.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD OnDataAvailable( nsIPluginStreamInfo* pluginInfo,
                                nsIInputStream* input, 
                                PRUint32 length );
    NS_IMETHOD OnFileAvailable( nsIPluginStreamInfo* pluginInfo,
                         const char* fileName );
  
    /**
     * Notify the observer that the URL has finished loading.  This method is 
     * called once when the networking library has finished processing the 
     * URL transaction initiatied via the nsINetService::Open(...) call.
     * <BR><BR>
     * 
     * This method is called regardless of whether the URL loaded 
     * successfully.<BR><BR>
     * 
     * @param status    Status code for the URL load.
     * @param msg       A text string describing the error.
     * @return          The return value is currently ignored.
     */
    NS_IMETHOD OnStopBinding( nsIPluginStreamInfo* pluginInfo,
                              nsresult status );
    NS_IMETHOD OnNotify( const char* url, nsresult status );
    NS_IMETHOD GetStreamType( nsPluginStreamType *result );
  
    ///////////////////////////////////////////////////////////////////////////
    // snPluginStreamListener specific methods:
  
    nsSanePluginStreamListener( nsSanePluginInstance* inst );
    virtual ~nsSanePluginStreamListener( void );
  
    nsSanePluginInstance* mPlugInst;
};

#endif // __NS_SANE_PLUGIN_H__
