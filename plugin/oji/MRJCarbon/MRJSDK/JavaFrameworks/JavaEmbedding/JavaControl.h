/*
     File:       JavaEmbedding/JavaControl.h
 
     Contains:   interface to embedding Java code in a Carbon Control
 
     Version:    JavaEmbedding-3~36
 
     Copyright:  © 2000-2001 by Apple Computer, Inc., all rights reserved.
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
#ifndef __JAVACONTROL__
#define __JAVACONTROL__

#if __MACHO__
#ifndef __HITOOLBOX__
#include <HIToolbox/HIToolbox.h>
#endif

#include <JavaVM/jni.h>
#else
#include <jni.h>
#endif

#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_STRUCT_ALIGN
    #pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
    #pragma pack(2)
#endif


/*
 *  Summary:
 *    JavaControl Embedding errors
 *  
 *  Discussion:
 *    The following are all errors which can be returned from the
 *    routines contained in this file. Most are self explanatory.
 */
enum {
  errJavaEmbeddingNotYetImplemented = -9950,
  errJavaEmbeddingIntializationFailed = -9962, /* previously errClassConstructorNotFound, errClassNotFound, and errMethodNotFound*/
  errJavaEmbeddingMissingURL    = -9955, /* previously errMissingURL*/
  errJavaEmbeddingCouldNotCreateApplet = -9956, /* previously errCouldNotCreateApplet*/
  errJavaEmbeddingCouldNotEmbedFrame = -9957, /* previously errCouldNotEmbedFrame   */
  errJavaEmbeddingCouldNotConvertURL = -9958, /* previously errCouldNotConvertURL*/
  errJavaEmbeddingNotAFrame     = -9959, /* previously errNotAFrame*/
  errJavaEmbeddingControlNotEmbedded = -9960, /* previously errControlNotEmbedded*/
  errJavaEmbeddingExceptionThrown = -9961 /* previously errExceptionThrown*/
};



/*
 *  MoveAndClipJavaControl()
 *  
 *  Summary:
 *    Positions the control in the containing window, and sets the clip
 *    bounds for drawing.
 *  
 *  Discussion:
 *    All coordinates are local to the host window, and 0,0 is the top
 *    left corner of the content area of the host window - just below
 *    the title bar. 
 *    Usually a call to MoveAndClipJavaControl is followed by a call to
 *    DrawJavaControl.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    theControl:
 *      The Java control (applet).
 *    
 *    posX:
 *      The x position of the control.
 *    
 *    posY:
 *      The y position of the control.
 *    
 *    clipX:
 *      The left of the clip region.
 *    
 *    clipY:
 *      The top of the clip region.
 *    
 *    clipWidth:
 *      The width of the clip region. (Notice this is not right, but
 *      width)
 *    
 *    clipHeight:
 *      The height of the clip region. (Notice this is not bottom, but
 *      height)
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
MoveAndClipJavaControl(
  JNIEnv *     env,
  ControlRef   theControl,
  int          posX,
  int          posY,
  int          clipX,
  int          clipY,
  int          clipWidth,
  int          clipHeight);


/*
 *  SizeJavaControl()
 *  
 *  Summary:
 *    Sets the size of the Java control.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
SizeJavaControl(
  JNIEnv *     env,
  ControlRef   theControl,
  int          width,
  int          height);


/*
 *  ShowHideJavaControl()
 *  
 *  Summary:
 *    Makes a Java control visible or invisible.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    theControl:
 *      The Java control (applet).
 *    
 *    visible:
 *      True shows the control.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
ShowHideJavaControl(
  JNIEnv *     env,
  ControlRef   theControl,
  Boolean      visible);


/*
 *  StopJavaControlAsyncDrawing()
 *  
 *  Summary:
 *    Stops a Java applet from drawing asynchonously.
 *  
 *  Discussion:
 *    Many applets are used for animation and they draw themselves at
 *    times other than when the control is drawn. In order to handle
 *    things like live resize and scrolling a host app must be able to
 *    suspend asynchronous drawing otherwise a draw may occur before
 *    the host app is able to reposition/reclip the control thus
 *    causing drawing in the wrong location. When async drawing is off
 *    normal paint events in an applet are ignored. Only
 *    DrawJavaControl events are allowed to paint. This allows
 *    temporary fine grained control of when an applet can paint, and
 *    should only be used when needed.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    theControl:
 *      The Java control (applet).
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
StopJavaControlAsyncDrawing(
  JNIEnv *     env,
  ControlRef   theControl);


/*
 *  RestartJavaControlAsyncDrawing()
 *  
 *  Summary:
 *    Allows a Java applet to draw asynchonously.
 *  
 *  Discussion:
 *    This should be called when it is safe again for an applet to draw
 *    asynchronously. See StopJavaControlAsyncDrawing.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    theControl:
 *      The Java control (applet).
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
RestartJavaControlAsyncDrawing(
  JNIEnv *     env,
  ControlRef   theControl);


/*
 *  DrawJavaControl()
 *  
 *  Summary:
 *    Causes a Java control that to be drawn.
 *  
 *  Discussion:
 *    This should be called whenever the host app needs the
 *    applet/control to be redrawn. In the case where Async drawing is
 *    paused, DrawJavaControl will still cause the applet to draw. So
 *    if the host app is stopping async drawing for something like live
 *    scrolling, if there are convenient times the host app should call
 *    DrawJavaControl (usually after a call to MoveAndClipJavaControl)
 *    to provide some feedback to the user.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    theControl:
 *      The corresponding Java control (applet) that is to be drawn.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
DrawJavaControl(
  JNIEnv *     env,
  ControlRef   theControl);



/*
   ========================================================================================
   UTILITY API - functions to determine the status of a window or control
   ========================================================================================
*/

/*
 *  GetJavaWindowFromWindow()
 *  
 *  Discussion:
 *    Given a native Carbon window this returns the corresponding Java
 *    window.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    inMacWindow:
 *      A reference to a native window.
 *    
 *    outJavaWindow:
 *      The corresponding Java window.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
GetJavaWindowFromWindow(
  JNIEnv *    env,
  WindowRef   inMacWindow,
  jobject *   outJavaWindow);


/*
 *  GetWindowFromJavaWindow()
 *  
 *  Discussion:
 *    Given a Java window this returns the corresponding native Carbon
 *    window.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    inJavaWindow:
 *      A reference to a Java window.
 *    
 *    outMacWindow:
 *      The corresponding native window.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
GetWindowFromJavaWindow(
  JNIEnv *     env,
  jobject      inJavaWindow,
  WindowRef *  outMacWindow);


/*
 *  GetJavaFrameFromControl()
 *  
 *  Discussion:
 *    Given an embedded control this returns the corresponding Java
 *    applet frame.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    inMacControl:
 *      A reference to the control for the applet.
 *    
 *    outJavaFrame:
 *      The applet reference.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
GetJavaFrameFromControl(
  JNIEnv *     env,
  ControlRef   inMacControl,
  jobject *    outJavaFrame);


/*
 *  GetControlFromJavaFrame()
 *  
 *  Discussion:
 *    Given a Java applet frame reference this returns the embedded
 *    control.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    inJavaFrame:
 *      The applet reference obtained from CreateJavaApplet.
 *    
 *    outMacControl:
 *      A reference to the control for the applet.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
GetControlFromJavaFrame(
  JNIEnv *      env,
  jobject       inJavaFrame,
  ControlRef *  outMacControl);



/*
 *  CreateJavaControl()
 *  
 *  Discussion:
 *    Creates a control for the specified applet whose content is drawn
 *    and events processed by java. 
 *    All communication to this control should be through the APIs
 *    specified here in JavaControl.h.
 *  
 *  Parameters:
 *    
 *    env:
 *      The JNI Environment for the current thread.
 *    
 *    inNativeWindow:
 *      The carbon window that will host the applet.
 *    
 *    inBounds:
 *      The starting location for the applet.
 *    
 *    inAppletFrame:
 *      The applet reference obtained from CreateJavaApplet.
 *    
 *    inVisible:
 *      True if the applet should start out hidden.
 *    
 *    outControl:
 *      A reference to the control that is created for the applet.
 *  
 *  Availability:
 *    Mac OS X:         in version 10.1 and later in Carbon.framework
 *    CarbonLib:        not available
 *    Non-Carbon CFM:   not available
 */
extern OSStatus 
CreateJavaControl(
  JNIEnv *      env,
  WindowRef     inNativeWindow,
  const Rect *  inBounds,
  jobject       inAppletFrame,
  Boolean       inVisible,
  ControlRef *  outControl);



#if PRAGMA_STRUCT_ALIGN
    #pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
    #pragma pack()
#endif

#ifdef __cplusplus
}
#endif

#endif /* __JAVACONTROL__ */

