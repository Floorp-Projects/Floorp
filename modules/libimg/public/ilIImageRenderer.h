/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef ilIImageRenderer_h___
#define ilIImageRenderer_h___

#include <stdio.h>
#include "libimg.h"
#include "nsISupports.h"

// IID for the nsIImageRenderer interface
#define IL_IIMAGERENDERER_IID    \
{ 0xec4e9fc0, 0xb1f3, 0x11d1,   \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } };

/**
 * Interface to be implemented by image creation and rendering 
 * component plugged into the image library.
 */
class ilIImageRenderer : public nsISupports {
public:
  /**
   *  This is the function invoked for allocating pixmap storage and
   *  platform-specific pixmap resources.
   *
   *  On entry, the native color space and the original dimensions of the
   *  source image and its mask are initially filled in the two provided
   *  IL_Pixmap arguments.  (If no mask or alpha channel is present, the
   *  second pixmap is NULL.)
   * 
   *  The width and height arguments represent the desired dimensions of the
   *  target image.  If the implementation supports scaling, then the
   *  storage allocated for the IL_Pixmaps may be based on the original
   *  dimensions of the source image.  In this case, the headers of the
   *  IL_Pixmap should not be modified, however, the implementation
   *  should be able to determine the target dimensions of the image for
   *  a given IL_Pixmap.  (The opaque client_data pointer in the IL_Pixmap
   *  structure can be used to store the target image dimensions or a scale
   *  factor.)
   * 
   *  If the implementation does not support scaling, the supplied width
   *  and height must be used as the dimensions of the created pixmap storage
   *  and the headers within the IL_Pixmap should be side-effected to reflect
   *  that change.
   * 
   *  The allocator may side-effect the image and mask headers to target
   *  a different colorspace.
   * 
   *  The allocation function should side-effect bits, a member of the
   *  IL_Pixmap structure, to point to allocated storage.  If there are
   *  insufficient resources to allocate both the image and mask, neither
   *  should be allocated. (The bits pointers, initially NULL-valued,
   *  should not be altered.) 
   */
  virtual void NewPixmap(void* aDisplayContext, 
			PRInt32 aWidth, PRInt32 aHeight, 
			IL_Pixmap* aImage, IL_Pixmap* aMask)=0;

  /** 
   *  Inform the implementation that the specified rectangular portion of 
   *  the pixmap has been modified.  This might be used, for example, to
   *  transfer the altered area to the X server on a unix client.
   *
   *  x_offset and y_offset are measured in pixels, with the
   *  upper-left-hand corner of the pixmap as the origin, increasing
   *  left-to-right, top-to-bottom. 
   */
  virtual void UpdatePixmap(void* aDisplayContext, 
			    IL_Pixmap* aImage, 
			    PRInt32 aXOffset, PRInt32 aYOffset, 
			    PRInt32 aWidth, PRInt32 aHeight)=0;

  /**
   *  Informs the callee that the imagelib has acquired or relinquished 
   *  control over the IL_Pixmap's bits.  The message argument should be
   *  one of IL_LOCK_BITS, IL_UNLOCK_BITS or IL_RELEASE_BITS.
   *
   *  The imagelib will issue an IL_LOCK_BITS message whenever it wishes to
   *  alter the bits.  When the imaglib has finished altering the bits, it will
   *  issue an IL_UNLOCK_BITS message.  These messages are provided so that
   *  the callee  may perform memory-management tasks during the time that
   *  the imagelib is not writing to the pixmap's buffer.
   *
   *  Once the imagelib is sure that it will not modify the pixmap any further
   *  and, therefore, will no longer dereference the bits pointer in the
   *  IL_Pixmap, it will issue an IL_RELEASE_BITS request.  (Requests may still
   *  be made to display the pixmap, however, using whatever opaque pixmap
   *  storage the callee may retain.)  The IL_RELEASE_BITS message
   *  could be used, for example, by an X11 front-end to free the client-side
   *  image data, preserving only the server pixmap. 
   */
  virtual void ControlPixmapBits(void* aDisplayContext, 
				 IL_Pixmap* aImage, PRUint32 aControlMsg)=0;

  /**
   *  Release the memory storage and other resources associated with an image
   *  pixmap; the pixmap will never be referenced again.  The pixmap's header
   *  information and the IL_Pixmap structure itself will be freed by the Image
   *  Library. 
   */
  virtual void DestroyPixmap(void* aDisplayContext, IL_Pixmap* aImage)=0;

  /** 
   *  Render a rectangular portion of the given pixmap.
   *
   *  Render the image using transparency if mask is non-NULL.
   *  x and y are measured in pixels and are in document coordinates.
   *  x_offset and y_offset are with respect to the image origin.
   *
   *  If the width and height values would otherwise cause the sub-image
   *  to extend off the edge of the source image, the function should
   *  perform tiling of the source image.  This is used to draw document,
   *  layer and table cell backdrops.  (Note: it is assumed this case will
   *  apply only to images which do not require any scaling.)
   *  
   *  All coordinates are in terms of the target pixmap dimensions, which
   *  may differ from those of the pixmap storage if the callee
   *  supports scaling. 
   */
  virtual void DisplayPixmap(void* aDisplayContext, 
			     IL_Pixmap* aImage, IL_Pixmap* aMask, 
			     PRInt32 aX, PRInt32 aY, 
			     PRInt32 aXOffset, PRInt32 aYOffset, 
			     PRInt32 aWidth, PRInt32 aHeight)=0;

  /**
   *  <bold>(Probably temporary and subject to change).</bold>
   *  Display an icon. x and y are in document coordinates. 
   */
  virtual void DisplayIcon(void* aDisplayContext, 
			   PRInt32 aX, PRInt32 aY, PRUint32 aIconNumber)=0;

  /**
   *  <bold>(Probably temporary and subject to change).</bold>
   *  This method should fill in the targets of the width and
   *  height pointers to indicate icon dimensions 
   */
  virtual void GetIconDimensions(void* aDisplayContext, 
				 PRInt32 *aWidthPtr, PRInt32 *aHeightPtr, 
				 PRUint32 aIconNumber)=0;
};

#endif
