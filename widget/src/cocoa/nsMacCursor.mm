/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Camino Cursor code.
 *
 * The Initial Developer of the Original Code is 
 * Andrew Thompson.
 * Portions created by the Andrew Thompson are Copyright (C) 2004
 * Andrew Thompson. All Rights Reserved.
 * 
 * Contributor(s):
 *    Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMacCursor.h"
#include "nsDebug.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsString.h"

/*! @category   nsMacCursor (PrivateMethods)
    @abstract   Private methods internal to the nsMacCursor class.
    @discussion <code>nsMacCursor</code> is effectively an abstract class. It does not define complete 
                behaviour in and of itself, the subclasses defined in this file provide the useful implementations.
*/
@interface nsMacCursor (PrivateMethods)

/*! @method     getNextCursorFrame
    @abstract   get the index of the next cursor frame to display.
    @discussion Increments and returns the frame counter of an animated cursor.
    @result     The index of the next frame to display in the cursor animation
*/
- (int) getNextCursorFrame;

/*! @method     numFrames
    @abstract   Query the number of frames in this cursor's animation.
    @discussion Returns the number of frames in this cursor's animation. Static cursors return 1.
*/
- (int) numFrames;

/*! @method     createTimer
    @abstract   Create a Timer to use to animate the cursor.
    @discussion Creates an instance of <code>NSTimer</code> which is used to drive the cursor animation.
                This method should only be called for cursors that are animated.
*/
- (void) createTimer;

/*! @method     destroyTimer
    @abstract   Destroy any timer instance associated with this cursor.
    @discussion Invalidates and releases any <code>NSTimer</code> instance associated with this cursor.
 */
- (void) destroyTimer;
/*! @method     destroyTimer
    @abstract   Destroy any timer instance associated with this cursor.
    @discussion Invalidates and releases any <code>NSTimer</code> instance associated with this cursor.
*/

/*! @method     spinCursor:
    @abstract   Method called by animation timer to perform animation.
    @discussion Called by an animated cursor's associated timer to advance the animation to the next frame.
                Determines which frame should occur next and sets the cursor to that frame.
    @param      aTimer the timer causing the animation
*/
- (void) spinCursor: (NSTimer *) aTimer;

/*! @method     setFrame:
    @abstract   Sets the current cursor, using an index to determine which frame in the animation to display.
    @discussion Sets the current cursor. The frame index determines which frame is shown if the cursor is animated.
                Frames and numbered from <code>0</code> to <code>-[nsMacCursor numFrames] - 1</code>. A static cursor
                has a single frame, numbered 0.
    @param      aFrameIndex the index indicating which frame from the animation to display
*/
- (void) setFrame: (int) aFrameIndex;
@end

/*! @class      nsThemeCursor
    @abstract   Implementation of <code>nsMacCursor</code> that uses Carbon Appearance Manager cursors.
    @discussion Displays a static or animated <code>ThemeCursor</code> using Carbon Appearance Manager functions.
                Understands how many frames exist in each of the built-in <code>ThemeCursor</code>s.
*/
@interface nsThemeCursor : nsMacCursor
{
  @private
  ThemeCursor mCursor;
}

/*! @method     initWithThemeCursor:
    @abstract   Create a cursor by specifying a Carbon Apperance Manager <code>ThemeCursor</code> constant.
    @discussion Creates a cursor representing the given Appearance Manager built in cursor.
    @param      aCursor the <code>ThemeCursor</code> to use
    @result     an instance of <code>nsThemeCursor</code> representing the given <code>ThemeCursor</code>
*/
- (id) initWithThemeCursor: (ThemeCursor) aCursor;
@end

/*! @class      nsCocoaCursor
    @abstract   Implementation of <code>nsMacCursor</code> that uses Cocoa <code>NSCursor</code> instances.
    @discussion Displays a static or animated cursor, using Cocoa <code>NSCursor</code> instances. These can be either
                built-in <code>NSCursor</code> instances, or custom <code>NSCursor</code>s created from images.
                When more than one <code>NSCursor</code> is provided, the cursor will use these as animation frames.
*/
@interface nsCocoaCursor : nsMacCursor
{
  @private
  NSArray *mFrames;
}

/*! @method     initWithFrames:
    @abstract   Create an animated cursor by specifying the frames to use for the animation.
    @discussion Creates a cursor that will animate by cycling through the given frames. Each element of the array
                must be an instance of <code>NSCursor</code>
    @param      aCursorFrames an array of <code>NSCursor</code>, representing the frames of an animated cursor, in the
                order they should be played.
    @result     an instance of <code>nsCocoaCursor</code> that will animate the given cursor frames
 */
- (id) initWithFrames: (NSArray *) aCursorFrames;

/*! @method     initWithCursor:
    @abstract   Create a cursor by specifying a Cocoa <code>NSCursor</code>.
    @discussion Creates a cursor representing the given Cocoa built-in cursor.
    @param      aCursor the <code>NSCursor</code> to use
    @result     an instance of <code>nsCocoaCursor</code> representing the given <code>NSCursor</code>
*/
- (id) initWithCursor: (NSCursor *) aCursor;

/*! @method     initWithImageNamed:hotSpot:
    @abstract   Create a cursor by specifying the name of an image resource to use for the cursor and a hotspot.
    @discussion Creates a cursor by loading the named image using the <code>+[NSImage imageNamed:]</code> method.
                <p>The image must be compatible with any restrictions laid down by <code>NSCursor</code>. These vary
                by operating system version. eg, Jaguar has a smaller maximum size than Panther.</p>
                <p>The hotspot precisely determines the point where the user clicks when using the cursor.</p>
    @param      aCursor the name of the image to use for the cursor
    @param      aPoint the point within the cursor to use as the hotspot
    @result     an instance of <code>nsCocoaCursor</code> that uses the given image and hotspot
*/
- (id) initWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint;
@end

/*! @class      nsResourceCursor
    @abstract   Implementation of <code>nsMacCursor</code> that uses Carbon <code>CURS</code> resources.
    @discussion Displays a static or animated cursor, using Carbon <code>CURS</code> resources.
                <p>Animated cursors are produced by cycling through a range of cursor resource ids.</p>
                <p>The frames are loaded from the compiled version of the resource file nsMacWidget.r.</p>
 */
@interface nsResourceCursor : nsMacCursor
{
  @private
  int mFirstFrame;
  int mLastFrame;
}

/*! @method     initWithResources:lastFrame:
    @abstract   Create an animated cursor by specifying a range of <code>CURS</code> resources to load and animate.
    @discussion Creates a cursor that will animate by cycling through the given range of cursor resource ids. Each
                resource in the range must be the next frame in the animation.
                <p>To create a static cursor, simply pass the same resource id for both parameters.</p>
                <p>The frames are loaded from the compiled version of the resource file nsMacWidget.r.</p>
    @param      aFirstFrame the resource id for the first frame of the animation. Must be 128 or greated
    @param      aLastFrame the resource id for the last frame of the animation. Must be 128 or greater, and greater than
                or equal to <code>aFirstFrame</code>
    @result     an instance of <code>nsResourceCursor</code> that will animate the given cursor resources
*/
- (id) initWithFirstFrame: (int) aFirstFrame lastFrame: (int) aLastFrame;
@end

@implementation nsMacCursor

+ (nsMacCursor *) cursorWithThemeCursor: (ThemeCursor) aCursor
{
  return [[[nsThemeCursor alloc] initWithThemeCursor: aCursor] autorelease];
}

+ (nsMacCursor *) cursorWithResources: (int) aFirstFrame lastFrame: (int) aLastFrame
{
  return [[[nsResourceCursor alloc] initWithFirstFrame: aFirstFrame lastFrame: aLastFrame] autorelease];
}

+ (nsMacCursor *) cursorWithCursor: (NSCursor *) aCursor
{
  return [[[nsCocoaCursor alloc] initWithCursor: aCursor] autorelease];
}

+ (nsMacCursor *) cursorWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint
{
  return [[[nsCocoaCursor alloc] initWithImageNamed: aCursorImage hotSpot: aPoint] autorelease];
}

+ (nsMacCursor *) cursorWithFrames: (NSArray *) aCursorFrames
{
  return [[[nsCocoaCursor alloc] initWithFrames: aCursorFrames] autorelease];
}

- (void) set
{
  if ([self isAnimated]) {
    [self createTimer];
  }
  //if the cursor isn't animated or the timer creation fails for any reason...
  if (!mTimer) {
    [self setFrame: 0];
  }
}

- (void) unset
{
  [self destroyTimer];    
}

- (BOOL) isAnimated
{
  return [self numFrames] > 1;
}

- (int) numFrames
{
  //subclasses need to override this to support animation
  return 1;
}

- (int) getNextCursorFrame
{
  mFrameCounter = (mFrameCounter + 1) % [self numFrames];
  return mFrameCounter;
}

- (void) createTimer
{
  if (mTimer == nil) {
    mTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.25
                                               target: self
                                             selector: @selector(spinCursor:)
                                             userInfo: nil
                                              repeats: YES] retain];
  }
}

- (void) destroyTimer
{
  if (mTimer) {
      [mTimer invalidate];
      [mTimer release];
      mTimer = nil;
  }
}

- (void) spinCursor: (NSTimer *) aTimer
{
  if ([aTimer isValid]) {
    [self setFrame: [self getNextCursorFrame]];
  }
}

- (void) setFrame: (int) aFrameIndex
{
  //subclasses need to do something useful here
}

- (void) dealloc
{
  [self destroyTimer];
  [super dealloc];    
}

@end

@implementation nsThemeCursor
- (id) initWithThemeCursor: (ThemeCursor) aCursor
{
  self = [super init];
  //Appearance Manager cursors all fall into the range 0..127. Custom application CURS resources begin at id 128.
  NS_ASSERTION(mCursor >= 0 && mCursor < 128, "Theme cursors must be in the range 0 <= num < 128");
  mCursor = aCursor;    
  return self;
}

- (void) setFrame: (int) aFrameIndex
{
  if ([self isAnimated]) {
    //if the cursor is animated try to draw the appropriate frame
    OSStatus err = ::SetAnimatedThemeCursor(mCursor, aFrameIndex);
    if (err != noErr) {
      //in the event of any kind of problem, just try to show the first frame
      ::SetThemeCursor(mCursor);
    }
  }
  else {
    ::SetThemeCursor(mCursor);
  }
}

- (int) numFrames
{
  //These don't appear to be documented. Trial and Error...
  switch (mCursor)
  {
    case kThemeWatchCursor:
    case kThemeSpinningCursor:            
      return 8;
    default:
      return 1;
  }
}

@end

@implementation nsCocoaCursor
- (id) initWithFrames: (NSArray *) aCursorFrames
{
  self = [super init];
  NSEnumerator *it = [aCursorFrames objectEnumerator];
  NSObject *frame = nil;
  while ((frame = [it nextObject])) {
    NS_ASSERTION([frame isKindOfClass: [NSCursor class]], "Invalid argument: All frames must be of type NSCursor");
  }
  mFrames = [aCursorFrames retain];
  mFrameCounter = 0;
  return self;
}

- (id) initWithCursor: (NSCursor *) aCursor
{
  NSArray *frame = [NSArray arrayWithObjects: aCursor, nil];
  return [self initWithFrames: frame];
}

- (id) initWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint
{
  nsCOMPtr<nsIFile> resDir;
  nsCAutoString resPath;
  NSString* pathToImage;
  NSImage* cursorImage;

  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(resDir));
  if (NS_FAILED(rv)) goto INIT_FAILURE;
  resDir->AppendNative(NS_LITERAL_CSTRING("res"));
  resDir->AppendNative(NS_LITERAL_CSTRING("cursors"));

  rv = resDir->GetNativePath(resPath);
  if (NS_FAILED(rv)) goto INIT_FAILURE;

  pathToImage = [NSString stringWithUTF8String:(const char*)resPath.get()];
  if (!pathToImage) goto INIT_FAILURE;
  pathToImage = [pathToImage stringByAppendingPathComponent:aCursorImage];
  pathToImage = [pathToImage stringByAppendingPathExtension:@"tiff"];

  cursorImage = [[[NSImage alloc] initWithContentsOfFile:pathToImage] autorelease];
  if (!cursorImage) goto INIT_FAILURE;
  return [self initWithCursor: [[NSCursor alloc] initWithImage: cursorImage hotSpot: aPoint]];

INIT_FAILURE:
  NS_WARNING("Problem getting path to cursor image file!");
  [self release];
  return nil;
}

- (void) setFrame: (int) aFrameIndex
{
  [[mFrames objectAtIndex: aFrameIndex] performSelectorOnMainThread: @selector(set)  withObject: nil waitUntilDone: NO];
}

- (int) numFrames
{
  return [mFrames count];
}

- (NSString *) description
{
  return [mFrames description];
}

- (void) dealloc
{
  [mFrames release];
  [super dealloc];    
}
@end

@implementation nsResourceCursor

static short sRefNum = kResFileNotOpened;
static short sSaveResFile = 0;

// this could be simplified if it was rewritten using Cocoa
+(void)openLocalResourceFile
{
  if (sRefNum == kResFileNotOpened) {
    CFBundleRef appBundle = ::CFBundleGetMainBundle();
    if (appBundle) {
      CFURLRef executable = ::CFBundleCopyExecutableURL(appBundle);
      if (executable) {
        CFURLRef binDir = ::CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, executable);
        if (binDir) {
          CFURLRef resourceFile = ::CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, binDir,
                                                                          CFSTR("libwidget.rsrc"), PR_FALSE);
          if (resourceFile) {
            FSRef resourceRef;
            if (::CFURLGetFSRef(resourceFile, &resourceRef))
              ::FSOpenResourceFile(&resourceRef, 0, NULL, fsRdPerm, &sRefNum);
            ::CFRelease(resourceFile);
          }
          ::CFRelease(binDir);
        }
        ::CFRelease(executable);
      }
    }
  }

  if (sRefNum == kResFileNotOpened)
    return;
  
  sSaveResFile = ::CurResFile();
  ::UseResFile(sRefNum);
}

+(void)closeLocalResourceFile
{
  if (sRefNum == kResFileNotOpened)
    return;

  ::UseResFile(sSaveResFile);
}

-(id) initWithFirstFrame: (int) aFirstFrame lastFrame: (int) aLastFrame
{
  if ((self = [super init])) {
    //Appearance Manager cursors all fall into the range 0..127. Custom application CURS resources begin at id 128.
    NS_ASSERTION(aFirstFrame >= 128 && aLastFrame >= 128 && aLastFrame >= aFirstFrame, "Nonsensical frame indicies");
    mFirstFrame = aFirstFrame;
    mLastFrame = aLastFrame;
  }
  return self;
}

- (void) setFrame: (int) aFrameIndex
{
  [nsResourceCursor openLocalResourceFile];
  CursHandle cursHandle = ::GetCursor(mFirstFrame + aFrameIndex);
  NS_ASSERTION(cursHandle, "Can't load cursor, is the resource file installed correctly?");
  if (cursHandle) {
    ::SetCursor(*cursHandle);
  }
  [nsResourceCursor closeLocalResourceFile];
}

- (int) numFrames
{
  return (mLastFrame - mFirstFrame) + 1;
}

@end
