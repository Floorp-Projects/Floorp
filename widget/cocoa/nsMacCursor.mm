/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacCursor.h"
#include "nsObjCExceptions.h"
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

/*! @method     advanceAnimatedCursor:
    @abstract   Method called by animation timer to perform animation.
    @discussion Called by an animated cursor's associated timer to advance the animation to the next frame.
                Determines which frame should occur next and sets the cursor to that frame.
    @param      aTimer the timer causing the animation
*/
- (void) advanceAnimatedCursor: (NSTimer *) aTimer;

/*! @method     setFrame:
    @abstract   Sets the current cursor, using an index to determine which frame in the animation to display.
    @discussion Sets the current cursor. The frame index determines which frame is shown if the cursor is animated.
                Frames and numbered from <code>0</code> to <code>-[nsMacCursor numFrames] - 1</code>. A static cursor
                has a single frame, numbered 0.
    @param      aFrameIndex the index indicating which frame from the animation to display
*/
- (void) setFrame: (int) aFrameIndex;

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
  NSCursor *mLastSetCocoaCursor;
}

/*! @method     initWithFrames:
    @abstract   Create an animated cursor by specifying the frames to use for the animation.
    @discussion Creates a cursor that will animate by cycling through the given frames. Each element of the array
                must be an instance of <code>NSCursor</code>
    @param      aCursorFrames an array of <code>NSCursor</code>, representing the frames of an animated cursor, in the
                order they should be played.
    @param      aType the corresponding <code>nsCursor</code> constant
    @result     an instance of <code>nsCocoaCursor</code> that will animate the given cursor frames
 */
- (id) initWithFrames: (NSArray *) aCursorFrames type: (nsCursor) aType;

/*! @method     initWithCursor:
    @abstract   Create a cursor by specifying a Cocoa <code>NSCursor</code>.
    @discussion Creates a cursor representing the given Cocoa built-in cursor.
    @param      aCursor the <code>NSCursor</code> to use
    @param      aType the corresponding <code>nsCursor</code> constant
    @result     an instance of <code>nsCocoaCursor</code> representing the given <code>NSCursor</code>
*/
- (id) initWithCursor: (NSCursor *) aCursor type: (nsCursor) aType;

/*! @method     initWithImageNamed:hotSpot:
    @abstract   Create a cursor by specifying the name of an image resource to use for the cursor and a hotspot.
    @discussion Creates a cursor by loading the named image using the <code>+[NSImage imageNamed:]</code> method.
                <p>The image must be compatible with any restrictions laid down by <code>NSCursor</code>. These vary
                by operating system version.</p>
                <p>The hotspot precisely determines the point where the user clicks when using the cursor.</p>
    @param      aCursor the name of the image to use for the cursor
    @param      aPoint the point within the cursor to use as the hotspot
    @param      aType the corresponding <code>nsCursor</code> constant
    @result     an instance of <code>nsCocoaCursor</code> that uses the given image and hotspot
*/
- (id) initWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint type: (nsCursor) aType;

@end

@implementation nsMacCursor

+ (nsMacCursor *) cursorWithCursor: (NSCursor *) aCursor type: (nsCursor) aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[[nsCocoaCursor alloc] initWithCursor:aCursor type:aType] autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

+ (nsMacCursor *) cursorWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint type: (nsCursor) aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[[nsCocoaCursor alloc] initWithImageNamed:aCursorImage hotSpot:aPoint type:aType] autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

+ (nsMacCursor *) cursorWithFrames: (NSArray *) aCursorFrames type: (nsCursor) aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[[nsCocoaCursor alloc] initWithFrames:aCursorFrames type:aType] autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

+ (NSCursor *) cocoaCursorWithImageNamed: (NSString *) imageName hotSpot: (NSPoint) aPoint
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsCOMPtr<nsIFile> resDir;
  nsAutoCString resPath;
  NSString* pathToImage, *pathToHiDpiImage;
  NSImage* cursorImage, *hiDpiCursorImage;

  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(resDir));
  if (NS_FAILED(rv))
    goto INIT_FAILURE;
  resDir->AppendNative(NS_LITERAL_CSTRING("res"));
  resDir->AppendNative(NS_LITERAL_CSTRING("cursors"));

  rv = resDir->GetNativePath(resPath);
  if (NS_FAILED(rv))
    goto INIT_FAILURE;

  pathToImage = [NSString stringWithUTF8String:(const char*)resPath.get()];
  if (!pathToImage)
    goto INIT_FAILURE;
  pathToImage = [pathToImage stringByAppendingPathComponent:imageName];
  pathToHiDpiImage = [pathToImage stringByAppendingString:@"@2x"];
  // Add same extension to both image paths.
  pathToImage      = [pathToImage      stringByAppendingPathExtension:@"png"];
  pathToHiDpiImage = [pathToHiDpiImage stringByAppendingPathExtension:@"png"];

  cursorImage = [[[NSImage alloc] initWithContentsOfFile:pathToImage] autorelease];
  if (!cursorImage)
    goto INIT_FAILURE;

  // Note 1: There are a few different ways to get a hidpi image via
  // initWithContentsOfFile. We let the OS handle this here: when the
  // file basename ends in "@2x", it will be displayed at native resolution
  // instead of being pixel-doubled. See bug 784909 comment 7 for alternates ways.
  //
  // Note 2: The OS is picky, and will ignore the hidpi representation
  // unless it is exactly twice the size of the lowdpi image.
  hiDpiCursorImage = [[[NSImage alloc] initWithContentsOfFile:pathToHiDpiImage] autorelease];
  if (hiDpiCursorImage) {
    NSImageRep *imageRep = [[hiDpiCursorImage representations] objectAtIndex:0];
    [cursorImage addRepresentation: imageRep];
  }
  return [[[NSCursor alloc] initWithImage:cursorImage hotSpot:aPoint] autorelease];

INIT_FAILURE:
  NS_WARNING("Problem getting path to cursor image file!");
  [self release];
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL) isSet
{
  // implemented by subclasses
  return NO;
}

- (void) set
{
  if ([self isAnimated]) {
    [self createTimer];
  }
  // if the cursor isn't animated or the timer creation fails for any reason...
  if (!mTimer) {
    [self setFrame:0];
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
  // subclasses need to override this to support animation
  return 1;
}

- (int) getNextCursorFrame
{
  mFrameCounter = (mFrameCounter + 1) % [self numFrames];
  return mFrameCounter;
}

- (void) createTimer
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mTimer) {
    mTimer = [[NSTimer scheduledTimerWithTimeInterval:0.25
                                               target:self
                                             selector:@selector(advanceAnimatedCursor:)
                                             userInfo:nil
                                              repeats:YES] retain];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) destroyTimer
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mTimer) {
      [mTimer invalidate];
      [mTimer release];
      mTimer = nil;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) advanceAnimatedCursor: (NSTimer *) aTimer
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([aTimer isValid]) {
    [self setFrame:[self getNextCursorFrame]];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) setFrame: (int) aFrameIndex
{
  // subclasses need to do something useful here
}

- (nsCursor) type {
  return mType;
}

- (void) dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self destroyTimer];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

@implementation nsCocoaCursor

- (id) initWithFrames: (NSArray *) aCursorFrames type: (nsCursor) aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  self = [super init];
  NSEnumerator *it = [aCursorFrames objectEnumerator];
  NSObject *frame = nil;
  while ((frame = [it nextObject])) {
    NS_ASSERTION([frame isKindOfClass:[NSCursor class]], "Invalid argument: All frames must be of type NSCursor");
  }
  mFrames = [aCursorFrames retain];
  mFrameCounter = 0;
  mType = aType;
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id) initWithCursor: (NSCursor *) aCursor type: (nsCursor) aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSArray *frame = [NSArray arrayWithObjects:aCursor, nil];
  return [self initWithFrames:frame type:aType];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id) initWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint type: (nsCursor) aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [self initWithCursor:[nsMacCursor cocoaCursorWithImageNamed:aCursorImage hotSpot:aPoint] type:aType];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL) isSet
{
  return [NSCursor currentCursor] == mLastSetCocoaCursor;
}

- (void) setFrame: (int) aFrameIndex
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSCursor* newCursor = [mFrames objectAtIndex:aFrameIndex];
  [newCursor set];
  mLastSetCocoaCursor = newCursor;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (int) numFrames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [mFrames count];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (NSString *) description
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [mFrames description];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mFrames release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end
