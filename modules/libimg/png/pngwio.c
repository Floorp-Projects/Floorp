
/* pngwio.c - functions for data output

   libpng 1.0 beta 5 - version 0.95
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   Copyright (c) 1996, 1997 Andreas Dilger
   March 30, 1997

   This file provides a location for all output.  Users which need
   special handling are expected to write functions which have the same
   arguments as these, and perform similar functions, but possibly use
   different output methods.  Note that you shouldn't change these
   functions, but rather write replacement functions and then change
   them at run time with png_set_write_fn(...) */

#define PNG_INTERNAL
#include "png.h"

/* Write the data to whatever output you are using.  The default routine
   writes to a file pointer.  Note that this routine sometimes gets called
   with very small lengths, so you should implement some kind of simple
   buffering if you are using unbuffered writes.  This should never be asked
   to write more then 64K on a 16 bit machine.  */

void
png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   if (png_ptr->write_data_fn != NULL )
      (*(png_ptr->write_data_fn))(png_ptr, data, length);
   else
      png_error(png_ptr, "Call to NULL write function");
}

/* This is the function which does the actual writing of data.  If you are
   not writing to a standard C stream, you should create a replacement
   write_data function and use it at run time with png_set_write_fn(), rather
   than changing the library. */
#ifndef USE_FAR_KEYWORD
static void
png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   png_uint_32 check;

   check = fwrite(data, 1, length, (FILE *)(png_ptr->io_ptr));
   if (check != length)
   {
      png_error(png_ptr, "Write Error");
   }
}
#else
/* this is the model-independent version. Since the standard I/O library
   can't handle far buffers in the medium and small models, we have to copy
   the data.
*/

#define NEAR_BUF_SIZE 1024
#define MIN(a,b) (a <= b ? a : b)

static void
png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   png_uint_32 check;
   png_byte *near_data;  /* Needs to be "png_byte *" instead of "png_bytep" */
   FILE *io_ptr;

   /* Check if data really is near. If so, use usual code. */
   near_data = (png_byte *)CVT_PTR_NOCHECK(data);
   io_ptr = (FILE *)CVT_PTR(png_ptr->io_ptr);
   if ((png_bytep)near_data == data)
   {
      check = fwrite(near_data, 1, length, io_ptr);
   }
   else
   {
      png_byte buf[NEAR_BUF_SIZE];
      png_size_t written, remaining, err;
      check = 0;
      remaining = length;
      do
      {
         written = MIN(NEAR_BUF_SIZE, remaining);
         png_memcpy(buf, data, written); /* copy far buffer to near buffer */
         err = fwrite(buf, 1, written, io_ptr);
         if (err != written)
            break;
         else
            check += err;
         data += written;
         remaining -= written;
      }
      while (remaining != 0);
   }
   if (check != length)
   {
      png_error(png_ptr, "Write Error");
   }
}

#endif

/* This function is called to output any data pending writing (normally
   to disk).  After png_flush is called, there should be no data pending
   writing in any buffers. */
#if defined(PNG_WRITE_FLUSH_SUPPORTED)
void
png_flush(png_structp png_ptr)
{
   if (png_ptr->output_flush_fn != NULL)
      (*(png_ptr->output_flush_fn))(png_ptr);
}

static void
png_default_flush(png_structp png_ptr)
{
   FILE *io_ptr;
   io_ptr = (FILE *)CVT_PTR((png_ptr->io_ptr));
   if (io_ptr != NULL)
      fflush(io_ptr);
}
#endif

/* This function allows the application to supply new output functions for
   libpng if standard C streams aren't being used.

   This function takes as its arguments:
   png_ptr       - pointer to a png output data structure
   io_ptr        - pointer to user supplied structure containing info about
                   the output functions.  May be NULL.
   write_data_fn - pointer to a new output function which takes as its
                   arguments a pointer to a png_struct, a pointer to
                   data to be written, and a 32-bit unsigned int which is
                   the number of bytes to be written.  The new write
                   function should call png_error(png_ptr, "Error msg")
                   to exit and output any fatal error messages.
   flush_data_fn - pointer to a new flush function which takes as its
                   arguments a pointer to a png_struct.  After a call to
                   the flush function, there should be no data in any buffers
                   or pending transmission.  If the output method doesn't do
                   any buffering of ouput, a function prototype must still be
                   supplied although it doesn't have to do anything.  If
                   PNG_WRITE_FLUSH_SUPPORTED is not defined at libpng compile
                   time, output_flush_fn will be ignored, although it must be
                   supplied for compatibility. */
void
png_set_write_fn(png_structp png_ptr, png_voidp io_ptr,
   png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn)
{
   png_ptr->io_ptr = io_ptr;

   if (write_data_fn != NULL)
      png_ptr->write_data_fn = write_data_fn;
   else
      png_ptr->write_data_fn = png_default_write_data;

#if defined(PNG_WRITE_FLUSH_SUPPORTED)
   if (output_flush_fn != NULL)
      png_ptr->output_flush_fn = output_flush_fn;
   else
      png_ptr->output_flush_fn = png_default_flush;
#endif /* PNG_WRITE_FLUSH_SUPPORTED */

   /* It is an error to read while writing a png file */
   png_ptr->read_data_fn = NULL;
}

#if defined(USE_FAR_KEYWORD) 
#if defined(_MSC_VER)   
void *far_to_near(png_structp png_ptr,png_voidp ptr, int check)
{
   void *near_ptr;   
   void FAR *far_ptr;
   FP_OFF(near_ptr) = FP_OFF(ptr);
   far_ptr = (void FAR *)near_ptr;
   if(check != 0)
      if(FP_SEG(ptr) != FP_SEG(far_ptr))
         png_error(png_ptr,"segment lost in conversion");
   return(near_ptr);
}
#  else
void *far_to_near(png_structp png_ptr,png_voidp ptr, int check)
{
   void *near_ptr;   
   void FAR *far_ptr;
   near_ptr = (void FAR *)ptr;
   far_ptr = (void FAR *)near_ptr;
   if(check != 0)
      if(far_ptr != ptr)
         png_error(png_ptr,"segment lost in conversion");
   return(near_ptr);
}
#   endif
#   endif
