package netscape.libimg;
import netscape.jmc.*;

public interface IMGCBIF {

/* This is the callback function for allocating pixmap storage and
   platform-specific pixmap resources.

   On entry, the native color space and the original dimensions of the
   source image and its mask are initially filled in the two provided
   IL_Pixmap arguments.  (If no mask or alpha channel is present, the
   second pixmap is NULL.)

   The width and height arguments represent the desired dimensions of the
   target image.  If the display front-end supports scaling, then the
   storage allocated for the IL_Pixmaps may be based on the original
   dimensions of the source image.  In this case, the headers of the
   IL_Pixmap should not be modified, however, the display front-end
   should be able to determine the target dimensions of the image for
   a given IL_Pixmap.  (The opaque client_data pointer in the IL_Pixmap
   structure can be used to store the target image dimensions or a scale
   factor.)

   If the display front-end does not support scaling, the supplied width
   and height must be used as the dimensions of the created pixmap storage
   and the headers within the IL_Pixmap should be side-effected to reflect
   that change.

   The allocator may side-effect the image and mask headers to target
   a different colorspace.

   The allocation function should side-effect bits, a member of the
   IL_Pixmap structure, to point to allocated storage.  If there are
   insufficient resources to allocate both the image and mask, neither
   should be allocated. (The bits pointers, initially NULL-valued,
   should not be altered.) */

  public void
  NewPixmap(CType display_context, int width, int height,
	    ILPix_t image, ILPix_t mask);


/* Inform the front-end that the specified rectangular portion of the
   pixmap has been modified.  This might be used, for example, to
   transfer the altered area to the X server on a unix client.

   x_offset and y_offset are measured in pixels, with the
   upper-left-hand corner of the pixmap as the origin, increasing
   left-to-right, top-to-bottom. */

  public void
  UpdatePixmap(CType display_context, ILPix_t pixmap,
	       int x_offset, int y_offset, int width, int height);


/* Informs the callee that the imagelib has acquired or relinquished control
   over the IL_Pixmap's bits.  The message argument should be one of
   IL_LOCK_BITS, IL_UNLOCK_BITS or IL_RELEASE_BITS.

   The imagelib will issue an IL_LOCK_BITS message whenever it wishes to
   alter the bits.  When the imaglib has finished altering the bits, it will
   issue an IL_UNLOCK_BITS message.  These messages are provided so that
   the client may perform memory-management tasks during the time that
   the imagelib is not writing to the pixmap's buffer.

   Once the imagelib is sure that it will not modify the pixmap any further
   and, therefore, will no longer dereference the bits pointer in the
   IL_Pixmap, it will issue an IL_RELEASE_BITS request.  (Requests may still
   be made to display the pixmap, however, using whatever opaque pixmap
   storage the display front-end may retain.)  The IL_RELEASE_BITS message
   could be used, for example, by an X11 front-end to free the client-side
   image data, preserving only the server pixmap. */

  public void
  ControlPixmapBits(CType display_context, ILPix_t pixmap,
		    ILPixCtl message);


  /* Release the memory storage and other resources associated with an image
     pixmap; the pixmap will never be referenced again.  The pixmap's header
     information and the IL_Pixmap structure itself will be freed by the Image
     Library. */

  public void
  DestroyPixmap(CType display_context, ILPix_t pixmap);


/* Render a rectangular portion of the given pixmap.
 
   Render the image using transparency if mask is non-NULL.
   x and y are measured in pixels and are in document coordinates.
   x_offset and y_offset are with respect to the image origin.

   If the width and height values would otherwise cause the sub-image
   to extend off the edge of the source image, the function should
   perform tiling of the source image.  This is used to draw document,
   layer and table cell backdrops.  (Note: it is assumed this case will
   apply only to images which do not require any scaling.)

   All coordinates are in terms of the target pixmap dimensions, which
   may differ from those of the pixmap storage if the display
   front-end supports scaling. */

  public void
  DisplayPixmap(CType display_context,
		ILPix_t image, ILPix_t mask,
		int x, int y, int x_offset, int y_offset,
		int width, int height, int req_w, int req_h );


/* These are the icon display functions.  It's not clear whether or
   not icon display (including ALT text) should be entirely removed
   from the core image library and placed in a wrapper.  (See comments
   for IL_DisplaySubImage.) */

/* Display an icon. x and y are in document coordinates. */

  public void
  DisplayIcon(CType display_context, int x, int y, int icon_number);


/* This callback should side-effect the targets of the width and
   height pointers to indicate icon dimensions */

  public void
  GetIconDimensions(CType display_context, int_t width, int_t height,
		    int icon_number);
}
