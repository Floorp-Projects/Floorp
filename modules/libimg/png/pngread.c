
/* pngread.c - read a PNG file
 *
 * libpng 1.0.8 - July 24, 2000
 * For conditions of distribution and use, see copyright notice in png.h
 * Copyright (c) 1998, 1999, 2000 Glenn Randers-Pehrson
 * (Version 0.96 Copyright (c) 1996, 1997 Andreas Dilger)
 * (Version 0.88 Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.)
 *
 * This file contains routines that an application calls directly to
 * read a PNG file or stream.
 */

#define PNG_INTERNAL
#include "png.h"

/* Create a PNG structure for reading, and allocate any memory needed. */
png_structp PNGAPI
png_create_read_struct(png_const_charp user_png_ver, png_voidp error_ptr,
   png_error_ptr error_fn, png_error_ptr warn_fn)
{

#ifdef PNG_USER_MEM_SUPPORTED
   return (png_create_read_struct_2(user_png_ver, error_ptr, error_fn,
      warn_fn, NULL, NULL, NULL));
}

/* Alternate create PNG structure for reading, and allocate any memory needed. */
png_structp PNGAPI
png_create_read_struct_2(png_const_charp user_png_ver, png_voidp error_ptr,
   png_error_ptr error_fn, png_error_ptr warn_fn, png_voidp mem_ptr,
   png_malloc_ptr malloc_fn, png_free_ptr free_fn)
{
#endif /* PNG_USER_MEM_SUPPORTED */

   png_structp png_ptr;

#ifdef PNG_SETJMP_SUPPORTED
#ifdef USE_FAR_KEYWORD
   jmp_buf jmpbuf;
#endif
#endif

   int i;

   png_debug(1, "in png_create_read_struct\n");
#ifdef PNG_USER_MEM_SUPPORTED
   if ((png_ptr = (png_structp)png_create_struct_2(PNG_STRUCT_PNG,
      (png_malloc_ptr)malloc_fn)) == NULL)
#else
   if ((png_ptr = (png_structp)png_create_struct(PNG_STRUCT_PNG)) == NULL)
#endif
   {
      return (png_structp)NULL;
   }

#ifdef PNG_SETJMP_SUPPORTED
#ifdef USE_FAR_KEYWORD
   if (setjmp(jmpbuf))
#else
   if (setjmp(png_ptr->jmpbuf))
#endif
   {
      png_free(png_ptr, png_ptr->zbuf);
      png_ptr->zbuf=NULL;
      png_destroy_struct(png_ptr);
      return (png_structp)NULL;
   }
#ifdef USE_FAR_KEYWORD
   png_memcpy(png_ptr->jmpbuf,jmpbuf,sizeof(jmp_buf));
#endif
#endif

#ifdef PNG_USER_MEM_SUPPORTED
   png_set_mem_fn(png_ptr, mem_ptr, malloc_fn, free_fn);
#endif

   png_set_error_fn(png_ptr, error_ptr, error_fn, warn_fn);

   i=0;
   do
   {
     if(user_png_ver[i] != png_libpng_ver[i])
        png_ptr->flags |= PNG_FLAG_LIBRARY_MISMATCH;
   } while (png_libpng_ver[i++]);

   if (png_ptr->flags & PNG_FLAG_LIBRARY_MISMATCH)
   {
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

     /* Libpng 1.0.6 was not binary compatible, due to insertion of the
        info_ptr->free_me member.  Note to maintainer: this test can be
        removed from version 2.0.0 and beyond because the previous test
        would have already rejected it. */

     if (user_png_ver[4] == '6' && user_png_ver[2] == '0' && 
         user_png_ver[0] == '1' && user_png_ver[5] == '\0')
     {
        png_error(png_ptr,
           "Application must be recompiled; version 1.0.6 was incompatible");
     }
   }

   /* initialize zbuf - compression buffer */
   png_ptr->zbuf_size = PNG_ZBUF_SIZE;
   png_ptr->zbuf = (png_bytep)png_malloc(png_ptr,
     (png_uint_32)png_ptr->zbuf_size);
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
   This interface is deprecated in favour of the png_create_read_struct(),
   and it will eventually disappear. */
#undef png_read_init
void PNGAPI
png_read_init(png_structp png_ptr)
{
   /* We only come here via pre-1.0.7-compiled applications */
   png_read_init_2(png_ptr, "1.0.0", 10000, 10000);
}

void PNGAPI
png_read_init_2(png_structp png_ptr, png_const_charp user_png_ver,
   png_size_t png_struct_size, png_size_t png_info_size)
{
#ifdef PNG_SETJMP_SUPPORTED
   jmp_buf tmp_jmp;  /* to save current jump buffer */
#endif

   int i=0;
   do
   {
     if(user_png_ver[i] != png_libpng_ver[i])
     {
#ifdef PNG_LEGACY_SUPPORTED
       png_ptr->flags |= PNG_FLAG_LIBRARY_MISMATCH;
#else
       png_ptr->error_fn=(png_error_ptr)NULL;
       png_error(png_ptr,
        "Application uses deprecated png_read_init() and must be recompiled.");
#endif
     }
   } while (png_libpng_ver[i++]);

   if(sizeof(png_struct) > png_struct_size ||
      sizeof(png_info) > png_info_size)
     {
       png_ptr->error_fn=(png_error_ptr)NULL;
       png_error(png_ptr,
      "Application and library have different sized structs. Please recompile.");
     }

   png_debug(1, "in png_read_init_2\n");

#ifdef PNG_SETJMP_SUPPORTED
   /* save jump buffer and error functions */
   png_memcpy(tmp_jmp, png_ptr->jmpbuf, sizeof (jmp_buf));
#endif

   /* reset all variables to 0 */
   png_memset(png_ptr, 0, sizeof (png_struct));

#ifdef PNG_SETJMP_SUPPORTED
   /* restore jump buffer */
   png_memcpy(png_ptr->jmpbuf, tmp_jmp, sizeof (jmp_buf));
#endif

   /* initialize zbuf - compression buffer */
   png_ptr->zbuf_size = PNG_ZBUF_SIZE;
   png_ptr->zbuf = (png_bytep)png_malloc(png_ptr,
     (png_uint_32)png_ptr->zbuf_size);
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
 * changed in v0.90 to allow reading a file that already has the magic
 * bytes read from the stream.  You can tell libpng how many bytes have
 * been read from the beginning of the stream (up to the maximum of 8)
 * via png_set_sig_bytes(), and we will only check the remaining bytes
 * here.  The application can then have access to the signature bytes we
 * read if it is determined that this isn't a valid PNG file.
 */
void PNGAPI
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

   for(;;)
   {
#ifdef PNG_USE_LOCAL_ARRAYS
      PNG_IHDR;
      PNG_IDAT;
      PNG_IEND;
      PNG_PLTE;
#if defined(PNG_READ_bKGD_SUPPORTED)
      PNG_bKGD;
#endif
#if defined(PNG_READ_cHRM_SUPPORTED)
      PNG_cHRM;
#endif
#if defined(PNG_READ_gAMA_SUPPORTED)
      PNG_gAMA;
#endif
#if defined(PNG_READ_hIST_SUPPORTED)
      PNG_hIST;
#endif
#if defined(PNG_READ_iCCP_SUPPORTED)
      PNG_iCCP;
#endif
#if defined(PNG_READ_iTXt_SUPPORTED)
      PNG_iTXt;
#endif
#if defined(PNG_READ_oFFs_SUPPORTED)
      PNG_oFFs;
#endif
#if defined(PNG_READ_pCAL_SUPPORTED)
      PNG_pCAL;
#endif
#if defined(PNG_READ_pHYs_SUPPORTED)
      PNG_pHYs;
#endif
#if defined(PNG_READ_sBIT_SUPPORTED)
      PNG_sBIT;
#endif
#if defined(PNG_READ_sCAL_SUPPORTED)
      PNG_sCAL;
#endif
#if defined(PNG_READ_sPLT_SUPPORTED)
      PNG_sPLT;
#endif
#if defined(PNG_READ_sRGB_SUPPORTED)
      PNG_sRGB;
#endif
#if defined(PNG_READ_tEXt_SUPPORTED)
      PNG_tEXt;
#endif
#if defined(PNG_READ_tIME_SUPPORTED)
      PNG_tIME;
#endif
#if defined(PNG_READ_tRNS_SUPPORTED)
      PNG_tRNS;
#endif
#if defined(PNG_READ_zTXt_SUPPORTED)
      PNG_zTXt;
#endif
#endif /* PNG_GLOBAL_ARRAYS */
      png_byte chunk_length[4];
      png_uint_32 length;

      png_read_data(png_ptr, chunk_length, 4);
      length = png_get_uint_32(chunk_length);

      png_reset_crc(png_ptr);
      png_crc_read(png_ptr, png_ptr->chunk_name, 4);

      png_debug2(0, "Reading %s chunk, length=%lu.\n", png_ptr->chunk_name,
         length);

      /* This should be a binary subdivision search or a hash for
       * matching the chunk name rather than a linear search.
       */
      if (!png_memcmp(png_ptr->chunk_name, png_IHDR, 4))
         png_handle_IHDR(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_IEND, 4))
         png_handle_IEND(png_ptr, info_ptr, length);
#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
      else if (png_handle_as_unknown(png_ptr, png_ptr->chunk_name))
      {
         if (!png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
            png_ptr->mode |= PNG_HAVE_IDAT;
         png_handle_unknown(png_ptr, info_ptr, length);
         if (!png_memcmp(png_ptr->chunk_name, png_PLTE, 4))
            png_ptr->mode |= PNG_HAVE_PLTE;
         else if (!png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
         {
            if (!(png_ptr->mode & PNG_HAVE_IHDR))
               png_error(png_ptr, "Missing IHDR before IDAT");
            else if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE &&
                     !(png_ptr->mode & PNG_HAVE_PLTE))
               png_error(png_ptr, "Missing PLTE before IDAT");
            break;
         }
      }
#endif
      else if (!png_memcmp(png_ptr->chunk_name, png_PLTE, 4))
         png_handle_PLTE(png_ptr, info_ptr, length);
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
#if defined(PNG_READ_sCAL_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sCAL, 4))
         png_handle_sCAL(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_pHYs_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_pHYs, 4))
         png_handle_pHYs(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sBIT_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sBIT, 4))
         png_handle_sBIT(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sRGB_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sRGB, 4))
         png_handle_sRGB(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_iCCP_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_iCCP, 4))
         png_handle_iCCP(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sPLT_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sPLT, 4))
         png_handle_sPLT(png_ptr, info_ptr, length);
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
#if defined(PNG_READ_iTXt_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_iTXt, 4))
         png_handle_iTXt(png_ptr, info_ptr, length);
#endif
      else
         png_handle_unknown(png_ptr, info_ptr, length);
   }
}

/* optional call to update the users info_ptr structure */
void PNGAPI
png_read_update_info(png_structp png_ptr, png_infop info_ptr)
{
   png_debug(1, "in png_read_update_info\n");
   /* save jump buffer and error functions */
   if (!(png_ptr->flags & PNG_FLAG_ROW_INIT))
      png_read_start_row(png_ptr);
   png_read_transform_info(png_ptr, info_ptr);
}

/* Initialize palette, background, etc, after transformations
 * are set, but before any reading takes place.  This allows
 * the user to obtain a gamma-corrected palette, for example.
 * If the user doesn't call this, we will do it ourselves.
 */
void PNGAPI
png_start_read_image(png_structp png_ptr)
{
   png_debug(1, "in png_start_read_image\n");
   /* save jump buffer and error functions */
   if (!(png_ptr->flags & PNG_FLAG_ROW_INIT))
      png_read_start_row(png_ptr);
}

void PNGAPI
png_read_row(png_structp png_ptr, png_bytep row, png_bytep dsp_row)
{
#ifdef PNG_USE_LOCAL_ARRAYS
   PNG_IDAT;
   const int png_pass_dsp_mask[7] = {0xff, 0x0f, 0xff, 0x33, 0xff, 0x55, 0xff};
   const int png_pass_mask[7] = {0x80, 0x08, 0x88, 0x22, 0xaa, 0x55, 0xff};
#endif
   int ret;
   png_debug2(1, "in png_read_row (row %lu, pass %d)\n",
      png_ptr->row_number, png_ptr->pass);
   /* save jump buffer and error functions */
   if (!(png_ptr->flags & PNG_FLAG_ROW_INIT))
      png_read_start_row(png_ptr);
   if (png_ptr->row_number == 0 && png_ptr->pass == 0)
   {
   /* check for transforms that have been set but were defined out */
#if defined(PNG_WRITE_INVERT_SUPPORTED) && !defined(PNG_READ_INVERT_SUPPORTED)
   if (png_ptr->transformations & PNG_INVERT_MONO)
      png_warning(png_ptr, "PNG_READ_INVERT_SUPPORTED is not defined.");
#endif
#if defined(PNG_WRITE_FILLER_SUPPORTED) && !defined(PNG_READ_FILLER_SUPPORTED)
   if (png_ptr->transformations & PNG_FILLER)
      png_warning(png_ptr, "PNG_READ_FILLER_SUPPORTED is not defined.");
#endif
#if defined(PNG_WRITE_PACKSWAP_SUPPORTED) && !defined(PNG_READ_PACKSWAP_SUPPORTED)
   if (png_ptr->transformations & PNG_PACKSWAP)
      png_warning(png_ptr, "PNG_READ_PACKSWAP_SUPPORTED is not defined.");
#endif
#if defined(PNG_WRITE_PACK_SUPPORTED) && !defined(PNG_READ_PACK_SUPPORTED)
   if (png_ptr->transformations & PNG_PACK)
      png_warning(png_ptr, "PNG_READ_PACK_SUPPORTED is not defined.");
#endif
#if defined(PNG_WRITE_SHIFT_SUPPORTED) && !defined(PNG_READ_SHIFT_SUPPORTED)
   if (png_ptr->transformations & PNG_SHIFT)
      png_warning(png_ptr, "PNG_READ_SHIFT_SUPPORTED is not defined.");
#endif
#if defined(PNG_WRITE_BGR_SUPPORTED) && !defined(PNG_READ_BGR_SUPPORTED)
   if (png_ptr->transformations & PNG_BGR)
      png_warning(png_ptr, "PNG_READ_BGR_SUPPORTED is not defined.");
#endif
#if defined(PNG_WRITE_SWAP_SUPPORTED) && !defined(PNG_READ_SWAP_SUPPORTED)
   if (png_ptr->transformations & PNG_SWAP_BYTES)
      png_warning(png_ptr, "PNG_READ_SWAP_SUPPORTED is not defined.");
#endif
   }

#if defined(PNG_READ_INTERLACING_SUPPORTED)
   /* if interlaced and we do not need a new row, combine row and return */
   if (png_ptr->interlaced && (png_ptr->transformations & PNG_INTERLACE))
   {
      switch (png_ptr->pass)
      {
         case 0:
            if (png_ptr->row_number & 0x07)
            {
               if (dsp_row != NULL)
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 1:
            if ((png_ptr->row_number & 0x07) || png_ptr->width < 5)
            {
               if (dsp_row != NULL)
                  png_combine_row(png_ptr, dsp_row,
                     png_pass_dsp_mask[png_ptr->pass]);
               png_read_finish_row(png_ptr);
               return;
            }
            break;
         case 2:
            if ((png_ptr->row_number & 0x07) != 4)
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

   if(png_ptr->row_buf[0])
   png_read_filter_row(png_ptr, &(png_ptr->row_info),
      png_ptr->row_buf + 1, png_ptr->prev_row + 1,
      (int)(png_ptr->row_buf[0]));

   png_memcpy_check(png_ptr, png_ptr->prev_row, png_ptr->row_buf,
      png_ptr->rowbytes + 1);

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

   if (png_ptr->read_row_fn != NULL)
      (*(png_ptr->read_row_fn))(png_ptr, png_ptr->row_number, png_ptr->pass);
}

/* Read one or more rows of image data.  If the image is interlaced,
 * and png_set_interlace_handling() has been called, the rows need to
 * contain the contents of the rows from the previous pass.  If the
 * image has alpha or transparency, and png_handle_alpha()[*] has been
 * called, the rows contents must be initialized to the contents of the
 * screen.
 *
 * "row" holds the actual image, and pixels are placed in it
 * as they arrive.  If the image is displayed after each pass, it will
 * appear to "sparkle" in.  "display_row" can be used to display a
 * "chunky" progressive image, with finer detail added as it becomes
 * available.  If you do not want this "chunky" display, you may pass
 * NULL for display_row.  If you do not want the sparkle display, and
 * you have not called png_handle_alpha(), you may pass NULL for rows.
 * If you have called png_handle_alpha(), and the image has either an
 * alpha channel or a transparency chunk, you must provide a buffer for
 * rows.  In this case, you do not have to provide a display_row buffer
 * also, but you may.  If the image is not interlaced, or if you have
 * not called png_set_interlace_handling(), the display_row buffer will
 * be ignored, so pass NULL to it.
 *
 * [*] png_handle_alpha() does not exist yet, as of libpng version 1.0.8
 */

void PNGAPI
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
   if (rp != NULL && dp != NULL)
      for (i = 0; i < num_rows; i++)
      {
         png_bytep rptr = *rp++;
         png_bytep dptr = *dp++;

         png_read_row(png_ptr, rptr, dptr);
      }
   else if(rp != NULL)
      for (i = 0; i < num_rows; i++)
      {
         png_bytep rptr = *rp;
         png_read_row(png_ptr, rptr, NULL);
         rp++;
      }
   else if(dp != NULL)
      for (i = 0; i < num_rows; i++)
      {
         png_bytep dptr = *dp;
         png_read_row(png_ptr, NULL, dptr);
         dp++;
      }
}

/* Read the entire image.  If the image has an alpha channel or a tRNS
 * chunk, and you have called png_handle_alpha()[*], you will need to
 * initialize the image to the current image that PNG will be overlaying.
 * We set the num_rows again here, in case it was incorrectly set in
 * png_read_start_row() by a call to png_read_update_info() or
 * png_start_read_image() if png_set_interlace_handling() wasn't called
 * prior to either of these functions like it should have been.  You can
 * only call this function once.  If you desire to have an image for
 * each pass of a interlaced image, use png_read_rows() instead.
 *
 * [*] png_handle_alpha() does not exist yet, as of libpng version 1.0.8
 */
void PNGAPI
png_read_image(png_structp png_ptr, png_bytepp image)
{
   png_uint_32 i,image_height;
   int pass, j;
   png_bytepp rp;

   png_debug(1, "in png_read_image\n");
   /* save jump buffer and error functions */

#ifdef PNG_READ_INTERLACING_SUPPORTED
   pass = png_set_interlace_handling(png_ptr);
#else
   if (png_ptr->interlaced)
      png_error(png_ptr,
        "Cannot read interlaced image -- interlace handler disabled.");
   pass = 1;
#endif


   image_height=png_ptr->height;
   png_ptr->num_rows = image_height; /* Make sure this is set correctly */

   for (j = 0; j < pass; j++)
   {
      rp = image;
      for (i = 0; i < image_height; i++)
      {
         png_read_row(png_ptr, *rp, NULL);
         rp++;
      }
   }
}

/* Read the end of the PNG file.  Will not read past the end of the
 * file, will verify the end is accurate, and will read any comments
 * or time information at the end of the file, if info is not NULL.
 */
void PNGAPI
png_read_end(png_structp png_ptr, png_infop info_ptr)
{
   png_byte chunk_length[4];
   png_uint_32 length;

   png_debug(1, "in png_read_end\n");
   /* save jump buffer and error functions */
   png_crc_finish(png_ptr, 0); /* Finish off CRC from last IDAT chunk */

   do
   {
#ifdef PNG_USE_LOCAL_ARRAYS
      PNG_IHDR;
      PNG_IDAT;
      PNG_IEND;
      PNG_PLTE;
#if defined(PNG_READ_bKGD_SUPPORTED)
      PNG_bKGD;
#endif
#if defined(PNG_READ_cHRM_SUPPORTED)
      PNG_cHRM;
#endif
#if defined(PNG_READ_gAMA_SUPPORTED)
      PNG_gAMA;
#endif
#if defined(PNG_READ_hIST_SUPPORTED)
      PNG_hIST;
#endif
#if defined(PNG_READ_iCCP_SUPPORTED)
      PNG_iCCP;
#endif
#if defined(PNG_READ_iTXt_SUPPORTED)
      PNG_iTXt;
#endif
#if defined(PNG_READ_oFFs_SUPPORTED)
      PNG_oFFs;
#endif
#if defined(PNG_READ_pCAL_SUPPORTED)
      PNG_pCAL;
#endif
#if defined(PNG_READ_pHYs_SUPPORTED)
      PNG_pHYs;
#endif
#if defined(PNG_READ_sBIT_SUPPORTED)
      PNG_sBIT;
#endif
#if defined(PNG_READ_sCAL_SUPPORTED)
      PNG_sCAL;
#endif
#if defined(PNG_READ_sPLT_SUPPORTED)
      PNG_sPLT;
#endif
#if defined(PNG_READ_sRGB_SUPPORTED)
      PNG_sRGB;
#endif
#if defined(PNG_READ_tEXt_SUPPORTED)
      PNG_tEXt;
#endif
#if defined(PNG_READ_tIME_SUPPORTED)
      PNG_tIME;
#endif
#if defined(PNG_READ_tRNS_SUPPORTED)
      PNG_tRNS;
#endif
#if defined(PNG_READ_zTXt_SUPPORTED)
      PNG_zTXt;
#endif
#endif /* PNG_GLOBAL_ARRAYS */

      png_read_data(png_ptr, chunk_length, 4);
      length = png_get_uint_32(chunk_length);

      png_reset_crc(png_ptr);
      png_crc_read(png_ptr, png_ptr->chunk_name, 4);

      png_debug1(0, "Reading %s chunk.\n", png_ptr->chunk_name);

      if (!png_memcmp(png_ptr->chunk_name, png_IHDR, 4))
         png_handle_IHDR(png_ptr, info_ptr, length);
      else if (!png_memcmp(png_ptr->chunk_name, png_IEND, 4))
         png_handle_IEND(png_ptr, info_ptr, length);
#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
      else if (png_handle_as_unknown(png_ptr, png_ptr->chunk_name))
      {
         if (!png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
         {
            if (length > 0 || png_ptr->mode & PNG_AFTER_IDAT)
               png_error(png_ptr, "Too many IDAT's found");
         }
         else
            png_ptr->mode |= PNG_AFTER_IDAT;
         png_handle_unknown(png_ptr, info_ptr, length);
         if (!png_memcmp(png_ptr->chunk_name, png_PLTE, 4))
            png_ptr->mode |= PNG_HAVE_PLTE;
      }
#endif
      else if (!png_memcmp(png_ptr->chunk_name, png_IDAT, 4))
      {
         /* Zero length IDATs are legal after the last IDAT has been
          * read, but not after other chunks have been read.
          */
         if (length > 0 || png_ptr->mode & PNG_AFTER_IDAT)
            png_error(png_ptr, "Too many IDAT's found");
         png_crc_finish(png_ptr, length);
      }
      else if (!png_memcmp(png_ptr->chunk_name, png_PLTE, 4))
         png_handle_PLTE(png_ptr, info_ptr, length);
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
#if defined(PNG_READ_sCAL_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sCAL, 4))
         png_handle_sCAL(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_pHYs_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_pHYs, 4))
         png_handle_pHYs(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sBIT_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sBIT, 4))
         png_handle_sBIT(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sRGB_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sRGB, 4))
         png_handle_sRGB(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_iCCP_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_iCCP, 4))
         png_handle_iCCP(png_ptr, info_ptr, length);
#endif
#if defined(PNG_READ_sPLT_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_sPLT, 4))
         png_handle_sPLT(png_ptr, info_ptr, length);
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
#if defined(PNG_READ_iTXt_SUPPORTED)
      else if (!png_memcmp(png_ptr->chunk_name, png_iTXt, 4))
         png_handle_iTXt(png_ptr, info_ptr, length);
#endif
      else
         png_handle_unknown(png_ptr, info_ptr, length);
   } while (!(png_ptr->mode & PNG_HAVE_IEND));
}

/* free all memory used by the read */
void PNGAPI
png_destroy_read_struct(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
   png_infopp end_info_ptr_ptr)
{
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL, end_info_ptr = NULL;
#ifdef PNG_USER_MEM_SUPPORTED
   png_free_ptr free_fn = NULL;
#endif

   png_debug(1, "in png_destroy_read_struct\n");
   /* save jump buffer and error functions */
   if (png_ptr_ptr != NULL)
      png_ptr = *png_ptr_ptr;

   if (info_ptr_ptr != NULL)
      info_ptr = *info_ptr_ptr;

   if (end_info_ptr_ptr != NULL)
      end_info_ptr = *end_info_ptr_ptr;

#ifdef PNG_USER_MEM_SUPPORTED
   free_fn = png_ptr->free_fn;
#endif

   png_read_destroy(png_ptr, info_ptr, end_info_ptr);

   if (info_ptr != NULL)
   {
#if defined(PNG_TEXT_SUPPORTED)
      png_free_data(png_ptr, info_ptr, PNG_FREE_TEXT, -1);
#endif

#ifdef PNG_USER_MEM_SUPPORTED
      png_destroy_struct_2((png_voidp)info_ptr, free_fn);
#else
      png_destroy_struct((png_voidp)info_ptr);
#endif
      *info_ptr_ptr = (png_infop)NULL;
   }

   if (end_info_ptr != NULL)
   {
#if defined(PNG_READ_TEXT_SUPPORTED)
      png_free_data(png_ptr, end_info_ptr, PNG_FREE_TEXT, -1);
#endif
#ifdef PNG_USER_MEM_SUPPORTED
      png_destroy_struct_2((png_voidp)end_info_ptr, free_fn);
#else
      png_destroy_struct((png_voidp)end_info_ptr);
#endif
      *end_info_ptr_ptr = (png_infop)NULL;
   }

   if (png_ptr != NULL)
   {
#ifdef PNG_USER_MEM_SUPPORTED
      png_destroy_struct_2((png_voidp)png_ptr, free_fn);
#else
      png_destroy_struct((png_voidp)png_ptr);
#endif
      *png_ptr_ptr = (png_structp)NULL;
   }
}

/* free all memory used by the read (old method) */
void PNGAPI
png_read_destroy(png_structp png_ptr, png_infop info_ptr, png_infop end_info_ptr)
{
#ifdef PNG_SETJMP_SUPPORTED
   jmp_buf tmp_jmp;
#endif
   png_error_ptr error_fn;
   png_error_ptr warning_fn;
   png_voidp error_ptr;
#ifdef PNG_USER_MEM_SUPPORTED
   png_free_ptr free_fn;
#endif

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
#ifdef PNG_FREE_ME_SUPPORTED
   if (png_ptr->free_me & PNG_FREE_PLTE)
      png_zfree(png_ptr, png_ptr->palette);
   png_ptr->free_me &= ~PNG_FREE_PLTE;
#else
   if (png_ptr->flags & PNG_FLAG_FREE_PLTE)
      png_zfree(png_ptr, png_ptr->palette);
   png_ptr->flags &= ~PNG_FLAG_FREE_PLTE;
#endif
#if defined(PNG_tRNS_SUPPORTED) || \
    defined(PNG_READ_EXPAND_SUPPORTED) || defined(PNG_READ_BACKGROUND_SUPPORTED)
#ifdef PNG_FREE_ME_SUPPORTED
   if (png_ptr->free_me & PNG_FREE_TRNS)
      png_free(png_ptr, png_ptr->trans);
   png_ptr->free_me &= ~PNG_FREE_TRNS;
#else
   if (png_ptr->flags & PNG_FLAG_FREE_TRNS)
      png_free(png_ptr, png_ptr->trans);
   png_ptr->flags &= ~PNG_FLAG_FREE_TRNS;
#endif
#endif
#if defined(PNG_READ_hIST_SUPPORTED)
#ifdef PNG_FREE_ME_SUPPORTED
   if (png_ptr->free_me & PNG_FREE_HIST)
      png_free(png_ptr, png_ptr->hist);
   png_ptr->free_me &= ~PNG_FREE_HIST;
#else
   if (png_ptr->flags & PNG_FLAG_FREE_HIST)
      png_free(png_ptr, png_ptr->hist);
   png_ptr->flags &= ~PNG_FLAG_FREE_HIST;
#endif
#endif
#if defined(PNG_READ_GAMMA_SUPPORTED)
   if (png_ptr->gamma_16_table != NULL)
   {
      int i;
      int istop = (1 << (8 - png_ptr->gamma_shift));
      for (i = 0; i < istop; i++)
      {
         png_free(png_ptr, png_ptr->gamma_16_table[i]);
      }
   png_free(png_ptr, png_ptr->gamma_16_table);
   }
#if defined(PNG_READ_BACKGROUND_SUPPORTED)
   if (png_ptr->gamma_16_from_1 != NULL)
   {
      int i;
      int istop = (1 << (8 - png_ptr->gamma_shift));
      for (i = 0; i < istop; i++)
      {
         png_free(png_ptr, png_ptr->gamma_16_from_1[i]);
      }
   png_free(png_ptr, png_ptr->gamma_16_from_1);
   }
   if (png_ptr->gamma_16_to_1 != NULL)
   {
      int i;
      int istop = (1 << (8 - png_ptr->gamma_shift));
      for (i = 0; i < istop; i++)
      {
         png_free(png_ptr, png_ptr->gamma_16_to_1[i]);
      }
   png_free(png_ptr, png_ptr->gamma_16_to_1);
   }
#endif
#endif
#if defined(PNG_TIME_RFC1123_SUPPORTED)
   png_free(png_ptr, png_ptr->time_buffer);
#endif

   inflateEnd(&png_ptr->zstream);
#ifdef PNG_PROGRESSIVE_READ_SUPPORTED
   png_free(png_ptr, png_ptr->save_buffer);
#endif

   /* Save the important info out of the png_struct, in case it is
    * being used again.
    */
#ifdef PNG_SETJMP_SUPPORTED
   png_memcpy(tmp_jmp, png_ptr->jmpbuf, sizeof (jmp_buf));
#endif

   error_fn = png_ptr->error_fn;
   warning_fn = png_ptr->warning_fn;
   error_ptr = png_ptr->error_ptr;
#ifdef PNG_USER_MEM_SUPPORTED
   free_fn = png_ptr->free_fn;
#endif

   png_memset(png_ptr, 0, sizeof (png_struct));

   png_ptr->error_fn = error_fn;
   png_ptr->warning_fn = warning_fn;
   png_ptr->error_ptr = error_ptr;
#ifdef PNG_USER_MEM_SUPPORTED
   png_ptr->free_fn = free_fn;
#endif

#ifdef PNG_SETJMP_SUPPORTED
   png_memcpy(png_ptr->jmpbuf, tmp_jmp, sizeof (jmp_buf));
#endif

}

void PNGAPI
png_set_read_status_fn(png_structp png_ptr, png_read_status_ptr read_row_fn)
{
   png_ptr->read_row_fn = read_row_fn;
}

#if defined(PNG_INFO_IMAGE_SUPPORTED)
void PNGAPI
png_read_png(png_structp png_ptr, png_infop info_ptr,
                           int transforms,
                           voidp params)
{
   int row;

#if defined(PNG_READ_INVERT_ALPHA_SUPPORTED)
   /* invert the alpha channel from opacity to transparency */
   if (transforms & PNG_TRANSFORM_INVERT_ALPHA)
       png_set_invert_alpha(png_ptr);
#endif

   /* The call to png_read_info() gives us all of the information from the
    * PNG file before the first IDAT (image data chunk).
    */
   png_read_info(png_ptr, info_ptr);

   /* -------------- image transformations start here ------------------- */

#if defined(PNG_READ_16_TO_8_SUPPORTED)
   /* tell libpng to strip 16 bit/color files down to 8 bits/color */
   if (transforms & PNG_TRANSFORM_STRIP_16)
       png_set_strip_16(png_ptr);
#endif

#if defined(PNG_READ_STRIP_ALPHA_SUPPORTED)
   /* Strip alpha bytes from the input data without combining with the
    * background (not recommended).
    */
   if (transforms & PNG_TRANSFORM_STRIP_ALPHA)
       png_set_strip_alpha(png_ptr);
#endif

#if defined(PNG_READ_PACK_SUPPORTED) && !defined(PNG_READ_EXPAND_SUPPORTED)
   /* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
    * byte into separate bytes (useful for paletted and grayscale images).
    */
   if (transforms & PNG_TRANSFORM_PACKING)
       png_set_packing(png_ptr);
#endif

#if defined(PNG_READ_PACKSWAP_SUPPORTED)
   /* Change the order of packed pixels to least significant bit first
    * (not useful if you are using png_set_packing). */
   if (transforms & PNG_TRANSFORM_PACKSWAP)
       png_set_packswap(png_ptr);
#endif

#if defined(PNG_READ_EXPAND_SUPPORTED)
   /* Expand paletted colors into true RGB triplets
    * Expand grayscale images to full 8 bits from 1, 2, or 4 bits/pixel
    * Expand paletted or RGB images with transparency to full alpha
    * channels so the data will be available as RGBA quartets.
    */
   if (transforms & PNG_TRANSFORM_EXPAND)
       if ((png_ptr->bit_depth < 8) ||
           (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE) ||
           (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)))
         png_set_expand(png_ptr);
#endif

   /* We don't handle background color or gamma transformation or dithering. */

#if defined(PNG_READ_INVERT_SUPPORTED)
   /* invert monochrome files to have 0 as white and 1 as black */
   if (transforms & PNG_TRANSFORM_INVERT_MONO)
       png_set_invert_mono(png_ptr);
#endif

#if defined(PNG_READ_SHIFT_SUPPORTED)
   /* If you want to shift the pixel values from the range [0,255] or
    * [0,65535] to the original [0,7] or [0,31], or whatever range the
    * colors were originally in:
    */
   if ((transforms & PNG_TRANSFORM_SHIFT)
       && png_get_valid(png_ptr, info_ptr, PNG_INFO_sBIT))
   {
      png_color_8p sig_bit;

      png_get_sBIT(png_ptr, info_ptr, &sig_bit);
      png_set_shift(png_ptr, sig_bit);
   }
#endif

#if defined(PNG_READ_BGR_SUPPORTED)
   /* flip the RGB pixels to BGR (or RGBA to BGRA) */
   if (transforms & PNG_TRANSFORM_BGR)
       png_set_bgr(png_ptr);
#endif

#if defined(PNG_READ_SWAP_ALPHA_SUPPORTED)
   /* swap the RGBA or GA data to ARGB or AG (or BGRA to ABGR) */
   if (transforms & PNG_TRANSFORM_SWAP_ALPHA)
       png_set_swap_alpha(png_ptr);
#endif

#if defined(PNG_READ_SWAP_SUPPORTED)
   /* swap bytes of 16 bit files to least significant byte first */
   if (transforms & PNG_TRANSFORM_SWAP_ENDIAN)
       png_set_swap(png_ptr);
#endif

   /* We don't handle adding filler bytes */

   /* Optional call to gamma correct and add the background to the palette
    * and update info structure.  REQUIRED if you are expecting libpng to
    * update the palette for you (ie you selected such a transform above).
    */
   png_read_update_info(png_ptr, info_ptr);

   /* -------------- image transformations end here ------------------- */

#ifdef PNG_FREE_ME_SUPPORTED
   png_free_data(png_ptr, info_ptr, PNG_FREE_ROWS, 0);
#endif
   if(info_ptr->row_pointers == NULL)
   {
      info_ptr->row_pointers = (png_bytepp)png_malloc(png_ptr,
                                         info_ptr->height * sizeof(png_bytep));
#ifdef PNG_FREE_ME_SUPPORTED
      info_ptr->free_me |= PNG_FREE_ROWS;
#endif
      for (row = 0; row < (int)info_ptr->height; row++)
         info_ptr->row_pointers[row] = png_malloc(png_ptr,
            png_get_rowbytes(png_ptr, info_ptr));
   }

   png_read_image(png_ptr, info_ptr->row_pointers);
   info_ptr->valid |= PNG_INFO_IDAT;

   /* read rest of file, and get additional chunks in info_ptr - REQUIRED */
   png_read_end(png_ptr, info_ptr);

   if(transforms == 0 || params == (voidp)NULL)
      /* quiet compiler warnings */ return;

}
#endif
