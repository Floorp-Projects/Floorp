
/* pngread.c - read a PNG file

   libpng 1.0 beta 5 - version 0.95
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   Copyright (c) 1996, 1997 Andreas Dilger
   March 30, 1997
   */

#define PNG_INTERNAL
#include "png.h"

/* Create a PNG structure for reading, and allocate any memory needed. */
png_structp
png_create_read_struct(png_charp user_png_ver, png_voidp error_ptr,
   png_error_ptr error_fn, png_error_ptr warn_fn)
{
   png_structp png_ptr;
#ifdef USE_FAR_KEYWORD
   jmp_buf jmpbuf;
#endif
   png_debug(1, "in png_create_read_struct\n");
   if ((png_ptr = (png_structp)png_create_struct(PNG_STRUCT_PNG)) == NULL)
   {
      return (png_structp)NULL;
   }
#ifdef USE_FAR_KEYWORD
   if (setjmp(jmpbuf))
#else
   if (setjmp(png_ptr->jmpbuf))
#endif
   {
	  png_free(png_ptr, png_ptr->zbuf);
	  png_destroy_struct(png_ptr);
	  
      return (png_structp)NULL;
   }
#ifdef USE_FAR_KEYWORD
   png_memcpy(png_ptr->jmpbuf,jmpbuf,sizeof(jmp_buf));
#endif
   png_set_error_fn(png_ptr, error_ptr, error_fn, warn_fn);

   /* Libpng 0.90 and later are binary incompatible with libpng 0.89, so
    * we must recompile any applications that use any older library version.
    * For versions after libpng 1.0, we will be compatible, so we need
    * only check the first digit.
    */
   if (user_png_ver == NULL || user_png_ver[0] != png_libpng_ver[0] ||
       (user_png_ver[0] == '0' && user_png_ver[2] < '9'))
   {
      png_error(png_ptr,
         "Incompatible libpng version in application and library");
   }

   /* initialize zbuf - compression buffer */
   png_ptr->zbuf_size = PNG_ZBUF_SIZE;
   png_ptr->zbuf = png_malloc(png_ptr, png_ptr->zbuf_size);
   memset(png_ptr->zbuf, NULL, png_ptr->zbuf_size );

   png_ptr->zstream.zalloc = png_zalloc;
   png_ptr->zstream.zfree = png_zfree;
   png_ptr->zstream.opaque = (voidpf)png_ptr;

   switch (inflateInit(&png_ptr->zstream))
   {
     case Z_OK: /* Do nothing */ break;
     case Z_MEM_ERROR:
     case Z_STREAM_ERROR: png_error(png_ptr, "zlib memory error"); break;
     case Z_VERSION_ERROR: png_error(png_ptr, "zlib version error"); break;
     default: png_error(png_ptr, "Unknown zlib error");
   }

   png_ptr->zstream.next_out = png_ptr->zbuf;
   png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;

   png_set_read_fn(png_ptr, NULL, NULL);

   return (png_ptr);
}


/* Initialize PNG structure for reading, and allocate any memory needed.
   This interface is depreciated in favour of the png_create_read_struct(),
   and it will eventually disappear. */
void
png_read_init(png_structp png_ptr)
{
   jmp_buf tmp_jmp;  /* to save current jump buffer */

   png_debug(1, "in png_read_init\n");
   /* save jump buffer and error functions */
   png_memcpy(tmp_jmp, png_ptr->jmpbuf, sizeof (jmp_buf));

   /* reset all variables to 0 */
   png_memset(png_ptr, 0, sizeof (png_struct));

   /* restore jump buffer */
   png_memcpy(png_ptr->jmpbuf, tmp_jmp, sizeof (jmp_buf));

   /* initialize zbuf - compression buffer */
   png_ptr->zbuf_size = PNG_ZBUF_SIZE;
   png_ptr->zbuf = png_malloc(png_ptr, png_ptr->zbuf_size);
   png_ptr->zstream.zalloc = png_zalloc;
   png_ptr->zstream.zfree = png_zfree;
   png_ptr->zstream.opaque = (voidpf)png_ptr;

   switch (inflateInit(&png_ptr->zstream))
   {
     case Z_OK: /* Do nothing */ break;
     case Z_MEM_ERROR:
     case Z_STREAM_ERROR: png_error(png_ptr, "zlib memory"); break;
     case Z_VERSION_ERROR: png_error(png_ptr, "zlib version"); break;
     default: png_error(png_ptr, "Unknown zlib error");
   }

   png_ptr->zstream.next_out = png_ptr->zbuf;
   png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;

   png_set_read_fn(png_ptr, NULL, NULL);
}

/* Read the information before the actual image data.  This has been
 * changed in v0.90 to allow reading a file which already has the magic
 * bytes read from the stream.  You can tell libpng how many bytes have
 * been read from the beginning of the stream (up to the maxumum of 8)
 * via png_set_sig_bytes(), and we will only check the remaining bytes
 * here.  The application can then have access to the signature bytes we
 * read if it is determined that this isn't a valid PNG file.
 */
void
png_read_info(png_structp png_ptr, png_infop info_ptr)
{
   png_debug(1, "in png_read_info\n");
   /* save jump buffer and error functions */
   /* If we haven't checked all of the PNG signature bytes, do so now. */
   if (png_ptr->sig_bytes < 8)
   {
      png_size_t num_checked = png_ptr->sig_bytes,
                 num_to_check = 8 - num_checked;

      png_read_data(png_ptr, &(info_ptr->signature[num_checked]), num_to_check);
      png_ptr->sig_bytes = 8;

      if (png_sig_cmp(info_ptr->signature, num_checked, num_to_check))
      {
         if (num_checked < 4 &&
             png_sig_cmp(info_ptr->signature, num_checked, num_to_check - 4))
            png_error(png_ptr, "Not a PNG file");
         else
            png_error(png_ptr, "PNG file corrupted by ASCII conversion");
      }
   }

   while (1)
   {
      png_byte chunk_length[4];
      png_uint_32 length;

      png_read_data(png_ptr, chunk_length, 4);
      length = png_get_uint_32(chunk_length);

      png_reset_crc(png_ptr);
      png_crc_read(png_ptr, png_ptr->chunk_name, 4);

      png_debug1(0, "Reading %s chunk.\n", png_ptr->chunk_name);

      /* This should be a binary subdivision search or a hash for
       * matching the chunk name rather than a linear search.
       */
      if (!png_memcmp(png_ptr->chunk_name, png_IHDR, 4))
         png_handle_IHDR(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_PLTE, 4))
         png_handle_PLTE(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_IEND, 4))
         png_handle_IEND(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
      {
         if (!(png_ptr->mode & PNG_HAVE_IHDR))
            png_error(png_ptr, "Missing IHDR before IDAT");
         else if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE &&
                  !(png_ptr->mode & PNG_HAVE_PLTE))
            png_error(png_ptr, "Missing PLTE before IDAT");

         png_ptr->idat_size = length;
         png_ptr->mode |= PNG_HAVE_IDAT;
         break;
      }
#if defined(PNG_READ_bKGD_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_bKGD, 4))
         png_handle_bKGD(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_cHRM_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_cHRM, 4))
         png_handle_cHRM(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_gAMA_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_gAMA, 4))
         png_handle_gAMA(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_hIST_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_hIST, 4))
         png_handle_hIST(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_oFFs_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_oFFs, 4))
         png_handle_oFFs(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_pCAL_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_pCAL, 4))
         png_handle_pCAL(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_pHYs_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_pHYs, 4))
         png_handle_pHYs(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sBIT_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sBIT, 4))
         png_handle_sBIT(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_tEXt_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_tEXt, 4))
         png_handle_tEXt(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_tIME_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_tIME, 4))
         png_handle_tIME(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_tRNS_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_tRNS, 4))
         png_handle_tRNS(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_zTXt_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_zTXt, 4))
         png_handle_zTXt(png_ptr, info_ptr, length);
#endif
      else
         png_handle_unknown(png_ptr, info_ptr, length);
   }
}

/* optional call to update the users info_ptr structure */
void
png_read_update_info(png_structp png_ptr, png_infop info_ptr)
{
   png_debug(1, "in png_read_update_info\n");
   /* save jump buffer and error functions */
   if (!(png_ptr->flags & PNG_FLAG_ROW_INIT))
      png_read_start_row(png_ptr);
   png_read_transform_info(png_ptr, info_ptr);
}

/* initialize palette, background, etc, after transformations
   are set, but before any reading takes place.  This allows
   the user to obtail a gamma corrected palette, for example.
   If the user doesn't call this, we will do it ourselves. */
void
png_start_read_image(png_structp png_ptr)
{
   png_debug(1, "in png_start_read_image\n");
   /* save jump buffer and error functions */
   if (!(png_ptr->flags & PNG_FLAG_ROW_INIT))
      png_read_start_row(png_ptr);
}

void
png_read_row(png_structp png_ptr, png_bytep row, png_bytep dsp_row)
{
   int ret;
   png_debug2(1, "in png_read_row (row %d, pass %d)\n",
      png_ptr->row_number, png_ptr->pass);
   /* save jump buffer and error functions */
   if (!(png_ptr->flags & PNG_FLAG_ROW_INIT))
      png_read_start_row(png_ptr);

#if defined(PNG_READ_INTERLACING_SUPPORTED)
   /* if interlaced and we do not need a new row, combine row and return */
   if (png_ptr->interlaced && (png_ptr->transformations & PNG_INTERLACE))
   {
      switch (png_ptr->pass)
      {
         case 0:
            if (png_ptr->row_number & 7)
            {
               if (dsp_row != NULL)
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 1:
            if ((png_ptr->row_number & 7) || png_ptr->width < 5)
            {
               if (dsp_row != NULL)
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 2:
            if ((png_ptr->row_number & 7) != 4)
            {
               if (dsp_row != NULL && (png_ptr->row_number & 4))
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 3:
            if ((png_ptr->row_number & 3) || png_ptr->width < 3)
            {
               if (dsp_row != NULL)
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 4:
            if ((png_ptr->row_number & 3) != 2)
            {
               if (dsp_row != NULL && (png_ptr->row_number & 2))
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 5:
            if ((png_ptr->row_number & 1) || png_ptr->width < 2)
            {
               if (dsp_row != NULL)
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 6:
            if (!(png_ptr->row_number & 1))
            {
               png_read_finish_row(png_ptr);
               return;
            }
            break;
      }
   }
#endif

   if (!(png_ptr->mode & PNG_HAVE_IDAT))
      png_error(png_ptr, "Invalid attempt to read row data");

   png_ptr->zstream.next_out = png_ptr->row_buf;
   png_ptr->zstream.avail_out = (uInt)png_ptr->irowbytes;
   do
   {   
      if (!(png_ptr->zstream.avail_in))
      {
         while (!png_ptr->idat_size)
         {
            png_byte chunk_length[4];

            png_crc_finish(png_ptr, 0);

            png_read_data(png_ptr, chunk_length, 4);
            png_ptr->idat_size = png_get_uint_32(chunk_length);

            png_reset_crc(png_ptr);
            png_crc_read(png_ptr, png_ptr->chunk_name, 4);
            if (png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
               png_error(png_ptr, "Not enough image data");
         }
         png_ptr->zstream.avail_in = (uInt)png_ptr->zbuf_size;
         png_ptr->zstream.next_in = png_ptr->zbuf;
         if (png_ptr->zbuf_size > png_ptr->idat_size)
            png_ptr->zstream.avail_in = (uInt)png_ptr->idat_size;
         png_crc_read(png_ptr, png_ptr->zbuf,
            (png_size_t)png_ptr->zstream.avail_in);
         png_ptr->idat_size -= png_ptr->zstream.avail_in;
      }
      ret = inflate(&png_ptr->zstream, Z_PARTIAL_FLUSH);

      if (ret == Z_STREAM_END)
      {
         if (png_ptr->zstream.avail_out || png_ptr->zstream.avail_in ||
            png_ptr->idat_size)
            png_error(png_ptr, "Extra compressed data");
         png_ptr->mode |= PNG_AFTER_IDAT;
         png_ptr->flags |= PNG_FLAG_ZLIB_FINISHED;
         break;
      }
      if (ret != Z_OK)
         png_error(png_ptr, png_ptr->zstream.msg ? png_ptr->zstream.msg :
                   "Decompression error");

   } while (png_ptr->zstream.avail_out);  

   png_ptr->row_info.color_type = png_ptr->color_type;
   png_ptr->row_info.width = png_ptr->iwidth;
   png_ptr->row_info.channels = png_ptr->channels;
   png_ptr->row_info.bit_depth = png_ptr->bit_depth;
   png_ptr->row_info.pixel_depth = png_ptr->pixel_depth;
   png_ptr->row_info.rowbytes = ((png_ptr->row_info.width *
      (png_uint_32)png_ptr->row_info.pixel_depth + 7) >> 3);

   png_read_filter_row(png_ptr, &(png_ptr->row_info),
      png_ptr->row_buf + 1, png_ptr->prev_row + 1,
      (int)(png_ptr->row_buf[0]));

   png_memcpy(png_ptr->prev_row, png_ptr->row_buf, png_ptr->rowbytes + 1);

   if (png_ptr->transformations)
      png_do_read_transformations(png_ptr);

#if defined(PNG_READ_INTERLACING_SUPPORTED)
   /* blow up interlaced rows to full size */
   if (png_ptr->interlaced &&
      (png_ptr->transformations & PNG_INTERLACE))
   {
      if (png_ptr->pass < 6)
         png_do_read_interlace(&(png_ptr->row_info),
            png_ptr->row_buf + 1, png_ptr->pass, png_ptr->transformations);

      if (dsp_row != NULL)
         png_combine_row(png_ptr, dsp_row,
            png_pass_dsp_mask[png_ptr->pass]);
      if (row != NULL)
         png_combine_row(png_ptr, row,
            png_pass_mask[png_ptr->pass]);
   }
   else
#endif
   {
      if (row != NULL)
         png_combine_row(png_ptr, row, 0xff);
      if (dsp_row != NULL)
         png_combine_row(png_ptr, dsp_row, 0xff);
   }
   png_read_finish_row(png_ptr);
}

/* Read one or more rows of image data.  If the image is interlaced,
   and png_set_interlace_handling() has been called, the rows need to
   to contain the contents of the rows from the previous pass.  If
   the image has alpha or transparency, and png_handle_alpha() has been
   called, the rows contents must be initialized to the contents of the
   screen.  "row" holds the actual image, and pixels are placed in it
   as they arrive.  If the image is displayed after each pass, it will
   appear to "sparkle" in.  "display_row" can be used to display a
   "chunky" progressive image, with finer detail added as it becomes
   available.  If you do not want this "chunky" display, you may pass
   NULL for display_row.  If you do not want the sparkle display, and
   you have not called png_handle_alpha(), you may pass NULL for rows.
   If you have called png_handle_alpha(), and the image has either an
   alpha channel or a transparency chunk, you must provide a buffer for
   rows.  In this case, you do not have to provide a display_row buffer
   also, but you may.  If the image is not interlaced, or if you have
   not called png_set_interlace_handling(), the display_row buffer will
   be ignored, so pass NULL to it. */

void
png_read_rows(png_structp png_ptr, png_bytepp row,
   png_bytepp display_row, png_uint_32 num_rows)
{
   png_uint_32 i;
   png_bytepp rp;
   png_bytepp dp;

   png_debug(1, "in png_read_rows\n");
   /* save jump buffer and error functions */
   rp = row;
   dp = display_row;
   for (i = 0; i < num_rows; i++)
   {
      png_bytep rptr;
      png_bytep dptr;

      if (rp != NULL)
         rptr = *rp;
      else
         rptr = NULL;
      if (dp != NULL)
         dptr = *dp;
      else
         dptr = NULL;
      png_read_row(png_ptr, rptr, dptr);
      if (row != NULL)
         rp++;
      if (display_row != NULL)
         dp++;
   }
}

/* Read the entire image.  If the image has an alpha channel or a tRNS
   chunk, and you have called png_handle_alpha(), you will need to
   initialize the image to the current image that PNG will be overlaying.
   We set the num_rows again here, in case it was incorrectly set in
   png_read_start_row() by a call to png_read_update_info() or
   png_start_read_image() if png_set_interlace_handling() wasn't called
   prior to either of these functions like it should have been.  You can
   only call this function once.  If you desire to have an image for
   each pass of a interlaced image, use png_read_rows() instead */
void
png_read_image(png_structp png_ptr, png_bytepp image)
{
   png_uint_32 i;
   int pass, j;
   png_bytepp rp;

   png_debug(1, "in png_read_image\n");
   /* save jump buffer and error functions */
   pass = png_set_interlace_handling(png_ptr);

   png_ptr->num_rows = png_ptr->height; /* Make sure this is set correctly */

   for (j = 0; j < pass; j++)
   {
      rp = image;
      for (i = 0; i < png_ptr->height; i++)
      {
         png_read_row(png_ptr, *rp, NULL);
         rp++;
      }
   }
}

/* Read the end of the PNG file.  Will not read past the end of the
   file, will verify the end is accurate, and will read any comments
   or time information at the end of the file, if info is not NULL. */
void
png_read_end(png_structp png_ptr, png_infop info_ptr)
{
   png_byte chunk_length[4];
   png_uint_32 length;

   png_debug(1, "in png_read_end\n");
   /* save jump buffer and error functions */
   png_crc_finish(png_ptr, 0); /* Finish off CRC from last IDAT chunk */

   do
   {
      png_read_data(png_ptr, chunk_length, 4);
      length = png_get_uint_32(chunk_length);

      png_reset_crc(png_ptr);
      png_crc_read(png_ptr, png_ptr->chunk_name, 4);

      png_debug1(0, "Reading %s chunk.\n", png_ptr->chunk_name);

      if (!png_memcmp(png_ptr->chunk_name, png_IHDR, 4))
         png_handle_IHDR(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
      {
         /* Zero length IDATs are legal after the last IDAT has been
          * read, but not after other chunks have been read.
          */
         if (length > 0 || png_ptr->mode & PNG_AFTER_IDAT)
            png_error(png_ptr, "Too many IDAT's found");
         else
            png_crc_finish(png_ptr, 0);
      }
      else if (!png_memcmp(png_ptr->chunk_name, png_PLTE, 4))
         png_handle_PLTE(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_IEND, 4))
         png_handle_IEND(png_ptr, info_ptr, length);
#if defined(PNG_READ_bKGD_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_bKGD, 4))
         png_handle_bKGD(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_cHRM_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_cHRM, 4))
         png_handle_cHRM(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_gAMA_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_gAMA, 4))
         png_handle_gAMA(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_hIST_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_hIST, 4))
         png_handle_hIST(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_oFFs_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_oFFs, 4))
         png_handle_oFFs(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_pCAL_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_pCAL, 4))
         png_handle_pCAL(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_pHYs_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_pHYs, 4))
         png_handle_pHYs(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sBIT_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sBIT, 4))
         png_handle_sBIT(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_tEXt_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_tEXt, 4))
         png_handle_tEXt(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_tIME_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_tIME, 4))
         png_handle_tIME(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_tRNS_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_tRNS, 4))
         png_handle_tRNS(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_zTXt_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_zTXt, 4))
         png_handle_zTXt(png_ptr, info_ptr, length);
#endif
      else
         png_handle_unknown(png_ptr, info_ptr, length);
   } while (!(png_ptr->mode & PNG_HAVE_IEND));
}

/* free all memory used by the read */
void
png_destroy_read_struct(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
   png_infopp end_info_ptr_ptr)
{
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL, end_info_ptr = NULL;

   png_debug(1, "in png_destroy_read_struct\n");
   /* save jump buffer and error functions */
   if (png_ptr_ptr != NULL)
      png_ptr = *png_ptr_ptr;

   if (info_ptr_ptr != NULL)
      info_ptr = *info_ptr_ptr;

   if (end_info_ptr_ptr != NULL)
      end_info_ptr = *end_info_ptr_ptr;

   png_read_destroy(png_ptr, info_ptr, end_info_ptr);

   if (info_ptr != NULL)
   {
      png_destroy_struct((png_voidp)info_ptr);
      *info_ptr_ptr = (png_infop)NULL;
   }

   if (end_info_ptr != NULL)
   {
      png_destroy_struct((png_voidp)end_info_ptr);
      *end_info_ptr_ptr = (png_infop)NULL;
   }

   if (png_ptr != NULL)
   {
      png_destroy_struct((png_voidp)png_ptr);
      *png_ptr_ptr = (png_structp)NULL;
   }
}

/* free all memory used by the read (old method) */
void
png_read_destroy(png_structp png_ptr, png_infop info_ptr, png_infop end_info_ptr)
{
   int i;
   jmp_buf tmp_jmp;
   png_error_ptr error_fn;
   png_error_ptr warning_fn;
   png_voidp error_ptr;

   png_debug(1, "in png_read_destroy\n");
   /* save jump buffer and error functions */
   if (info_ptr != NULL)
      png_info_destroy(png_ptr, info_ptr);

   if (end_info_ptr != NULL)
      png_info_destroy(png_ptr, end_info_ptr);

   png_free(png_ptr, png_ptr->zbuf);
   png_free(png_ptr, png_ptr->row_buf);
   png_free(png_ptr, png_ptr->prev_row);
#if defined(PNG_READ_DITHER_SUPPORTED)
   png_free(png_ptr, png_ptr->palette_lookup);
   png_free(png_ptr, png_ptr->dither_index);
#endif
#if defined(PNG_READ_GAMMA_SUPPORTED)
   png_free(png_ptr, png_ptr->gamma_table);
#endif
#if defined(PNG_READ_BACKGROUND_SUPPORTED)
   png_free(png_ptr, png_ptr->gamma_from_1);
   png_free(png_ptr, png_ptr->gamma_to_1);
#endif
   if (png_ptr->flags & PNG_FLAG_FREE_PALETTE)
      png_free(png_ptr, png_ptr->palette);
#if defined(PNG_READ_BACKGROUND_SUPPORTED) && defined(PNG_READ_bKGD_SUPPORTED)
   if (png_ptr->flags & PNG_FLAG_FREE_TRANS)
      png_free(png_ptr, png_ptr->trans);
#endif
#if defined(PNG_READ_hIST_SUPPORTED)
   if (png_ptr->flags & PNG_FLAG_FREE_HIST)
      png_free(png_ptr, png_ptr->hist);
#endif
#if defined(PNG_READ_GAMMA_SUPPORTED)
   if (png_ptr->gamma_16_table != NULL)
   {
      for (i = 0; i < (1 << (8 - png_ptr->gamma_shift)); i++)
      {
         png_free(png_ptr, png_ptr->gamma_16_table[i]);
      }
   }
#endif
#if defined(PNG_READ_BACKGROUND_SUPPORTED)
   png_free(png_ptr, png_ptr->gamma_16_table);
   if (png_ptr->gamma_16_from_1 != NULL)
   {
      for (i = 0; i < (1 << (8 - png_ptr->gamma_shift)); i++)
      {
         png_free(png_ptr, png_ptr->gamma_16_from_1[i]);
      }
   }
   png_free(png_ptr, png_ptr->gamma_16_from_1);
   if (png_ptr->gamma_16_to_1 != NULL)
   {
      for (i = 0; i < (1 << (8 - png_ptr->gamma_shift)); i++)
      {
         png_free(png_ptr, png_ptr->gamma_16_to_1[i]);
      }
   }
   png_free(png_ptr, png_ptr->gamma_16_to_1);
#endif

   inflateEnd(&png_ptr->zstream);
#ifdef PNG_PROGRESSIVE_READ_SUPPORTED
   png_free(png_ptr, png_ptr->save_buffer);
#endif

   /* Save the important info out of the png_struct, in case it is
    * being used again.
    */
   png_memcpy(tmp_jmp, png_ptr->jmpbuf, sizeof (jmp_buf));

   error_fn = png_ptr->error_fn;
   warning_fn = png_ptr->warning_fn;
   error_ptr = png_ptr->error_ptr;

   png_memset(png_ptr, 0, sizeof (png_struct));

   png_ptr->error_fn = error_fn;
   png_ptr->warning_fn = warning_fn;
   png_ptr->error_ptr = error_ptr;

   png_memcpy(png_ptr->jmpbuf, tmp_jmp, sizeof (jmp_buf));
}
