
/* pngrutil.c - utilities to read a PNG file

   libpng 1.0 beta 5 - version 0.95
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   Copyright (c) 1996, 1997 Andreas Dilger
   March 30, 1997
   */

#define PNG_INTERNAL
#include "png.h"

#ifndef PNG_READ_BIG_ENDIAN_SUPPORTED
/* Grab an unsigned 32-bit integer from a buffer in big endian format. */
png_uint_32
png_get_uint_32(png_bytep buf)
{
   png_uint_32 i;

   i = ((png_uint_32)(*buf) << 24) +
      ((png_uint_32)(*(buf + 1)) << 16) +
      ((png_uint_32)(*(buf + 2)) << 8) +
      (png_uint_32)(*(buf + 3));

   return i;
}

#if defined(PNG_READ_pCAL_SUPPORTED)
/* Grab a signed 32-bit integer from a buffer in big endian format.  The
 * data is stored in the PNG file in two's complement format, and it is
 * assumed that the machine format for signed integers is the same. */
png_int_32
png_get_int_32(png_bytep buf)
{
   png_int_32 i;

   i = ((png_int_32)(*buf) << 24) +
      ((png_int_32)(*(buf + 1)) << 16) +
      ((png_int_32)(*(buf + 2)) << 8) +
      (png_int_32)(*(buf + 3));

   return i;
}
#endif /* PNG_READ_BIG_ENDIAN_SUPPORTED */

/* Grab an unsigned 16-bit integer from a buffer in big endian format. */
png_uint_16
png_get_uint_16(png_bytep buf)
{
   png_uint_16 i;

   i = (png_uint_16)(((png_uint_16)(*buf) << 8) +
      (png_uint_16)(*(buf + 1)));

   return i;
}
#endif /* PNG_READ_BIG_ENDIAN_SUPPORTED */

/* Set the action on getting a CRC error for an ancillary or critical chunk. */
void
png_set_crc_action(png_structp png_ptr, int crit_action, int ancil_action)
{
   png_debug(1, "in png_set_crc_action\n");
   /* Tell libpng how we react to CRC errors in critical chunks */
   switch (crit_action)
   {
      case PNG_CRC_NO_CHANGE:                        /* leave setting as is */
         break;
      case PNG_CRC_WARN_USE:                               /* warn/use data */
         png_ptr->flags &= ~PNG_FLAG_CRC_CRITICAL_MASK;
         png_ptr->flags |= PNG_FLAG_CRC_CRITICAL_USE;
         break;
      case PNG_CRC_QUIET_USE:                             /* quiet/use data */
         png_ptr->flags &= ~PNG_FLAG_CRC_CRITICAL_MASK;
         png_ptr->flags |= PNG_FLAG_CRC_CRITICAL_USE |
                           PNG_FLAG_CRC_CRITICAL_IGNORE;
         break;
      case PNG_CRC_WARN_DISCARD:    /* not a valid action for critical data */
         png_warning(png_ptr, "Can't discard critical data on CRC error.");
      case PNG_CRC_ERROR_QUIT:                                /* error/quit */
      case PNG_CRC_DEFAULT:
      default:
         png_ptr->flags &= ~PNG_FLAG_CRC_CRITICAL_MASK;
         break;
   }

   switch (ancil_action)
   {
      case PNG_CRC_NO_CHANGE:                       /* leave setting as is */
         break;
      case PNG_CRC_WARN_USE:                              /* warn/use data */
         png_ptr->flags &= ~PNG_FLAG_CRC_ANCILLARY_MASK;
         png_ptr->flags |= PNG_FLAG_CRC_ANCILLARY_USE;
         break;
      case PNG_CRC_QUIET_USE:                            /* quiet/use data */
         png_ptr->flags &= ~PNG_FLAG_CRC_ANCILLARY_MASK;
         png_ptr->flags |= PNG_FLAG_CRC_ANCILLARY_USE |
                           PNG_FLAG_CRC_ANCILLARY_NOWARN;
         break;
      case PNG_CRC_ERROR_QUIT:                               /* error/quit */
         png_ptr->flags &= ~PNG_FLAG_CRC_ANCILLARY_MASK;
         png_ptr->flags |= PNG_FLAG_CRC_ANCILLARY_NOWARN;
         break;
      case PNG_CRC_WARN_DISCARD:                      /* warn/discard data */
      case PNG_CRC_DEFAULT:
      default:
         png_ptr->flags &= ~PNG_FLAG_CRC_ANCILLARY_MASK;
         break;
   }
}

/* Read data, and (optionally) run it through the CRC. */
void
png_crc_read(png_structp png_ptr, png_bytep buf, png_size_t length)
{
   png_read_data(png_ptr, buf, length);
   png_calculate_crc(png_ptr, buf, length);
}

/* Optionally skip data and then check the CRC.  Depending on whether we
   are reading a ancillary or critical chunk, and how the program has set
   things up, we may calculate the CRC on the data and print a message.
   Returns '1' if there was a CRC error, '0' otherwise. */
int
png_crc_finish(png_structp png_ptr, png_uint_32 skip)
{
   png_uint_32 i;

   for (i = skip; i > (png_uint_32)png_ptr->zbuf_size; i -= png_ptr->zbuf_size)
   {
      png_crc_read(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size);
   }
   if (i)
   {
      png_crc_read(png_ptr, png_ptr->zbuf, (png_size_t)i);
   }

   if (png_crc_error(png_ptr))
   {
      char msg[80];

      sprintf(msg,"CRC error in %s", png_ptr->chunk_name);

      if ((png_ptr->chunk_name[0] & 0x20 &&                /* Ancillary */
           !(png_ptr->flags & PNG_FLAG_CRC_ANCILLARY_NOWARN)) ||
          (!(png_ptr->chunk_name[0] & 0x20) &&             /* Critical  */
           png_ptr->flags & PNG_FLAG_CRC_CRITICAL_USE))
      {
         png_warning(png_ptr, msg);
      }
      else
      {
         png_error(png_ptr, msg);
      }
      return 1;
   }

   return 0;
}

/* Compare the CRC stored in the PNG file with that calulated by libpng from
   the data it has read thus far. */
int
png_crc_error(png_structp png_ptr)
{
   png_byte crc_bytes[4];
   png_uint_32 crc;
   int need_crc = 1;

   if (png_ptr->chunk_name[0] & 0x20)                     /* ancillary */
   {
      if ((png_ptr->flags & PNG_FLAG_CRC_ANCILLARY_MASK) ==
          (PNG_FLAG_CRC_ANCILLARY_USE | PNG_FLAG_CRC_ANCILLARY_NOWARN))
         need_crc = 0;
   }
   else                                                    /* critical */
   {
      if (png_ptr->flags & PNG_FLAG_CRC_CRITICAL_IGNORE)
         need_crc = 0;
   }

   png_read_data(png_ptr, crc_bytes, 4);

   if (need_crc)
   {
      crc = png_get_uint_32(crc_bytes);
#ifdef PNG_USE_OWN_CRC
      return (((crc^0xffffffffL)&0xffffffffL) != (png_ptr->crc&0xffffffffL));
#else
      return (crc != png_ptr->crc);
#endif
   }
   else
      return 0;
}


/* read and check the IDHR chunk */
void
png_handle_IHDR(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_byte buf[13];
   png_uint_32 width, height;
   int bit_depth, color_type, compression_type, filter_type;
   int interlace_type;

   png_debug(1, "in png_handle_IHDR\n");

   if (png_ptr->mode != PNG_BEFORE_IHDR)
      png_error(png_ptr, "Out of place IHDR");

   /* check the length */
   if (length != 13)
      png_error(png_ptr, "Invalid IHDR chunk");

   png_ptr->mode |= PNG_HAVE_IHDR;

   png_crc_read(png_ptr, buf, 13);
   png_crc_finish(png_ptr, 0);

   width = png_get_uint_32(buf);
   height = png_get_uint_32(buf + 4);
   bit_depth = buf[8];
   color_type = buf[9];
   compression_type = buf[10];
   filter_type = buf[11];
   interlace_type = buf[12];

   /* check for width and height valid values */
   if (width == 0 || width > 2147483647 || height == 0 || height > 2147483647)
      png_error(png_ptr, "Invalid image size in IHDR");

   /* check other values */
   if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 &&
      bit_depth != 8 && bit_depth != 16)
      png_error(png_ptr, "Invalid bit depth in IHDR");

   if (color_type < 0 || color_type == 1 ||
      color_type == 5 || color_type > 6)
      png_error(png_ptr, "Invalid color type in IHDR");

   if ((color_type == PNG_COLOR_TYPE_PALETTE && bit_depth) > 8 ||
       ((color_type == PNG_COLOR_TYPE_RGB ||
         color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
         color_type == PNG_COLOR_TYPE_RGB_ALPHA) && bit_depth < 8))
      png_error(png_ptr, "Invalid color type/bit depth combination in IHDR");

   if (interlace_type > PNG_INTERLACE_ADAM7)
      png_error(png_ptr, "Unknown interlace method in IHDR");

   if (compression_type != PNG_COMPRESSION_TYPE_BASE)
      png_error(png_ptr, "Unknown compression method in IHDR");

   if (filter_type != PNG_FILTER_TYPE_BASE)
      png_error(png_ptr, "Unknown filter method in IHDR");

   /* set internal variables */
   png_ptr->width = width;
   png_ptr->height = height;
   png_ptr->bit_depth = (png_byte)bit_depth;
   png_ptr->interlaced = (png_byte)interlace_type;
   png_ptr->color_type = (png_byte)color_type;

   /* find number of channels */
   switch (png_ptr->color_type)
   {
      case PNG_COLOR_TYPE_GRAY:
      case PNG_COLOR_TYPE_PALETTE:
         png_ptr->channels = 1;
         break;
      case PNG_COLOR_TYPE_RGB:
         png_ptr->channels = 3;
         break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
         png_ptr->channels = 2;
         break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
         png_ptr->channels = 4;
         break;
   }

   /* set up other useful info */
   png_ptr->pixel_depth = (png_byte)(png_ptr->bit_depth *
      png_ptr->channels);
   png_ptr->rowbytes = ((png_ptr->width *
      (png_uint_32)png_ptr->pixel_depth + 7) >> 3);
   png_debug1(3,"bit_depth = %d\n", png_ptr->bit_depth);
   png_debug1(3,"channels = %d\n", png_ptr->channels);
   png_debug1(3,"rowbytes = %d\n", png_ptr->rowbytes);
   png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth,
      color_type, interlace_type, compression_type, filter_type);
  
}

/* read and check the palette */
void
png_handle_PLTE(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_colorp palette;
   int num, i;

   png_debug(1, "in png_handle_PLTE\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before PLTE");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid PLTE after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (png_ptr->mode & PNG_HAVE_PLTE)
      png_error(png_ptr, "Duplicate PLTE chunk");

   png_ptr->mode |= PNG_HAVE_PLTE;

#if !defined(PNG_READ_OPT_PLTE_SUPPORTED)
   if (png_ptr->color_type != PNG_COLOR_TYPE_PALETTE)
   {
      png_crc_finish(png_ptr, length);
      return;
   }
#endif

   if (length % 3)
   {
      if (png_ptr->color_type != PNG_COLOR_TYPE_PALETTE)
      {
         png_warning(png_ptr, "Invalid palette chunk");
         png_crc_finish(png_ptr, length);
         return;
      }
      else
      {
         png_error(png_ptr, "Invalid palette chunk");
      }
   }

   num = (int)length / 3;
   palette = (png_colorp)png_malloc(png_ptr, num * sizeof (png_color));
   png_ptr->flags |= PNG_FLAG_FREE_PALETTE;
   for (i = 0; i < num; i++)
   {
      png_byte buf[3];

      png_crc_read(png_ptr, buf, 3);
      /* don't depend upon png_color being any order */
      palette[i].red = buf[0];
      palette[i].green = buf[1];
      palette[i].blue = buf[2];
   }

   /* If we actually NEED the PLTE chunk (ie for a paletted image), we do
      whatever the normal CRC configuration tells us.  However, if we
      have an RGB image, the PLTE can be considered ancillary, so
      we will act as though it is. */
   if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
   {
      png_crc_finish(png_ptr, 0);
   }
   else if (png_crc_error(png_ptr))  /* Only if we have a CRC error */
   {
      char msg[80];

      sprintf(msg,"CRC error in %s", png_ptr->chunk_name);

      /* If we don't want to use the data from an ancillary chunk,
         we have two options: an error abort, or a warning and we
         ignore the data in this chunk (which should be OK, since
         it's considered ancillary for a RGB or RGBA image). */
      if (!(png_ptr->flags & PNG_FLAG_CRC_ANCILLARY_USE))
      {
         if (png_ptr->flags & PNG_FLAG_CRC_ANCILLARY_NOWARN)
         {
            png_error(png_ptr, msg);
         }
         else
         {
            png_warning(png_ptr, msg);
            png_ptr->flags &= ~PNG_FLAG_FREE_PALETTE;
            png_free(png_ptr, palette);
            return;
         }
      }
      /* Otherwise, we (optionally) emit a warning and use the chunk. */
      else if (!(png_ptr->flags & PNG_FLAG_CRC_ANCILLARY_NOWARN))
      {
         png_warning(png_ptr, msg);
      }
   }

   png_ptr->palette = palette;
   png_ptr->num_palette = (png_uint_16)num;
   png_set_PLTE(png_ptr, info_ptr, palette, num);
}

void
png_handle_IEND(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_debug(1, "in png_handle_IEND\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR) || !(png_ptr->mode & PNG_HAVE_IDAT))
   {
      png_error(png_ptr, "No image in file");
   }

   png_ptr->mode |= PNG_AFTER_IDAT | PNG_HAVE_IEND;

   if (length != 0)
   {
      png_warning(png_ptr, "Incorrect IEND chunk length");
      png_crc_finish(png_ptr, length);
   }
}

#if defined(PNG_READ_gAMA_SUPPORTED)
void
png_handle_gAMA(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_uint_32 igamma;
   float file_gamma;
   png_byte buf[4];

   png_debug(1, "in png_handle_gAMA\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before gAMA");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid gAMA after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (png_ptr->mode & PNG_HAVE_PLTE)
      /* Should be an error, but we can cope with it */
      png_warning(png_ptr, "Out of place gAMA chunk");
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_gAMA)
   {
      png_warning(png_ptr, "Duplicate gAMA chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (length != 4)
   {
      png_warning(png_ptr, "Incorrect gAMA chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, 4);
   if (png_crc_finish(png_ptr, 0))
      return;

   igamma = png_get_uint_32(buf);
   /* check for zero gamma */
   if (igamma == 0)
      return;

   file_gamma = (float)igamma / (float)100000.0;
   png_ptr->gamma = file_gamma;
   png_set_gAMA(png_ptr, info_ptr, file_gamma);
}
#endif

#if defined(PNG_READ_sBIT_SUPPORTED)
void
png_handle_sBIT(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_size_t truelen;
   png_byte buf[4];

   png_debug(1, "in png_handle_sBIT\n");

   buf[0] = buf[1] = buf[2] = buf[3] = 0;

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before sBIT");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid sBIT after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (png_ptr->mode & PNG_HAVE_PLTE)
      /* Should be an error, but we can cope with it */
      png_warning(png_ptr, "Out of place sBIT chunk");
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_sBIT)
   {
      png_warning(png_ptr, "Duplicate sBIT chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
      truelen = 3;
   else
      truelen = (png_size_t)png_ptr->channels;

   if (length != truelen)
   {
      png_warning(png_ptr, "Incorrect sBIT chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, truelen);
   if (png_crc_finish(png_ptr, 0))
      return;

   if (png_ptr->color_type & PNG_COLOR_MASK_COLOR)
   {
      png_ptr->sig_bit.red = buf[0];
      png_ptr->sig_bit.green = buf[1];
      png_ptr->sig_bit.blue = buf[2];
      png_ptr->sig_bit.alpha = buf[3];
   }
   else
   {
      png_ptr->sig_bit.gray = buf[0];
      png_ptr->sig_bit.alpha = buf[1];
   }
   png_set_sBIT(png_ptr, info_ptr, &(png_ptr->sig_bit));
}
#endif

#if defined(PNG_READ_cHRM_SUPPORTED)
void
png_handle_cHRM(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_byte buf[4];
   png_uint_32 val;
   float white_x, white_y, red_x, red_y, green_x, green_y, blue_x, blue_y;

   png_debug(1, "in png_handle_cHRM\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before sBIT");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid cHRM after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (png_ptr->mode & PNG_HAVE_PLTE)
      /* Should be an error, but we can cope with it */
      png_warning(png_ptr, "Missing PLTE before cHRM");
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_cHRM)
   {
      png_warning(png_ptr, "Duplicate cHRM chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (length != 32)
   {
      png_warning(png_ptr, "Incorrect cHRM chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   white_x = (float)val / (float)100000.0;

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   white_y = (float)val / (float)100000.0;

   if (white_x < 0 || white_x > 0.8 || white_y < 0 || white_y > 0.8 ||
       white_x + white_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM white point");
      png_crc_finish(png_ptr, 24);
      return;
   }

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   red_x = (float)val / (float)100000.0;

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   red_y = (float)val / (float)100000.0;

   if (red_x < 0 || red_x > 0.8 || red_y < 0 || red_y > 0.8 ||
       red_x + red_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM red point");
      png_crc_finish(png_ptr, 16);
      return;
   }

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   green_x = (float)val / (float)100000.0;

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   green_y = (float)val / (float)100000.0;

   if (green_x < 0 || green_x > 0.8 || green_y < 0 || green_y > 0.8 ||
       green_x + green_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM green point");
      png_crc_finish(png_ptr, 8);
      return;
   }

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   blue_x = (float)val / (float)100000.0;

   png_crc_read(png_ptr, buf, 4);
   val = png_get_uint_32(buf);
   blue_y = (float)val / (float)100000.0;

   if (blue_x < 0 || blue_x > 0.8 || blue_y < 0 || blue_y > 0.8 ||
       blue_x + blue_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM blue point");
      png_crc_finish(png_ptr, 0);
      return;
   }

   if (png_crc_finish(png_ptr, 0))
      return;

   png_set_cHRM(png_ptr, info_ptr,
      white_x, white_y, red_x, red_y, green_x, green_y, blue_x, blue_y);
}
#endif

#if defined(PNG_READ_tRNS_SUPPORTED)
void
png_handle_tRNS(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_debug(1, "in png_handle_tRNS\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before tRNS");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid tRNS after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_tRNS)
   {
      png_warning(png_ptr, "Duplcate tRNS chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if (!(png_ptr->mode & PNG_HAVE_PLTE))
      {
         /* Should be an error, but we can cope with it */
         png_warning(png_ptr, "Missing PLTE before tRNS");
      }
      else if (length > png_ptr->num_palette)
      {
         png_warning(png_ptr, "Incorrect tRNS chunk length");
         png_crc_finish(png_ptr, length);
         return;
      }

      png_ptr->trans = (png_bytep)png_malloc(png_ptr, length);
      png_ptr->flags |= PNG_FLAG_FREE_TRANS;
      png_crc_read(png_ptr, png_ptr->trans, (png_size_t)length);
      png_ptr->num_trans = (png_uint_16)length;
   }
   else if (png_ptr->color_type == PNG_COLOR_TYPE_RGB)
   {
      png_byte buf[6];

      if (length != 6)
      {
         png_warning(png_ptr, "Incorrect tRNS chunk length");
         png_crc_finish(png_ptr, length);
         return;
      }

      png_crc_read(png_ptr, buf, (png_size_t)length);
      png_ptr->num_trans = 3;
      png_ptr->trans_values.red = png_get_uint_16(buf);
      png_ptr->trans_values.green = png_get_uint_16(buf + 2);
      png_ptr->trans_values.blue = png_get_uint_16(buf + 4);
   }
   else if (png_ptr->color_type == PNG_COLOR_TYPE_GRAY)
   {
      png_byte buf[6];

      if (length != 2)
      {
         png_warning(png_ptr, "Incorrect tRNS chunk length");
         png_crc_finish(png_ptr, length);
         return;
      }

      png_crc_read(png_ptr, buf, 2);
      png_ptr->num_trans = 1;
      png_ptr->trans_values.gray = png_get_uint_16(buf);
   }
   else
   {
      png_warning(png_ptr, "tRNS chunk not allowed with alpha channel");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (png_crc_finish(png_ptr, 0))
      return;

   png_set_tRNS(png_ptr, info_ptr, png_ptr->trans, png_ptr->num_trans,
      &(png_ptr->trans_values));
}
#endif

#if defined(PNG_READ_bKGD_SUPPORTED)
void
png_handle_bKGD(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_size_t truelen;
   png_byte buf[6];

   png_debug(1, "in png_handle_bKGD\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before bKGD");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid bKGD after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE &&
            !(png_ptr->mode & PNG_HAVE_PLTE))
   {
      png_warning(png_ptr, "Missing PLTE before bKGD");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_bKGD)
   {
      png_warning(png_ptr, "Duplicate bKGD chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
      truelen = 1;
   else if (png_ptr->color_type & PNG_COLOR_MASK_COLOR)
      truelen = 6;
   else
      truelen = 2;

   if (length != truelen)
   {
      png_warning(png_ptr, "Incorrect bKGD chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, truelen);
   if (png_crc_finish(png_ptr, 0))
      return;

   /* We convert the index value into RGB components so that we can allow
    * arbitrary RGB values for background when we have transparency, and
    * so it is easy to determine the RGB values of the background color
    * from the info_ptr struct. */
   if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
   {
      png_ptr->background.index = buf[0];
      png_ptr->background.red = (png_uint_16)png_ptr->palette[buf[0]].red;
      png_ptr->background.green = (png_uint_16)png_ptr->palette[buf[0]].green;
      png_ptr->background.blue = (png_uint_16)png_ptr->palette[buf[0]].blue;
   }
   else if (!(png_ptr->color_type & PNG_COLOR_MASK_COLOR)) /* GRAY */
   {
      png_ptr->background.red =
      png_ptr->background.green =
      png_ptr->background.blue =
      png_ptr->background.gray = png_get_uint_16(buf);
   }
   else
   {
      png_ptr->background.red = png_get_uint_16(buf);
      png_ptr->background.green = png_get_uint_16(buf + 2);
      png_ptr->background.blue = png_get_uint_16(buf + 4);
   }

   png_set_bKGD(png_ptr, info_ptr, &(png_ptr->background));
}
#endif

#if defined(PNG_READ_hIST_SUPPORTED)
void
png_handle_hIST(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   int num, i;

   png_debug(1, "in png_handle_hIST\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before hIST");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid hIST after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (!(png_ptr->mode & PNG_HAVE_PLTE))
   {
      png_warning(png_ptr, "Missing PLTE before hIST");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_hIST)
   {
      png_warning(png_ptr, "Duplicate hIST chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (length != (png_uint_32)(2 * png_ptr->num_palette))
   {
      png_warning(png_ptr, "Incorrect hIST chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   num = (int)length / 2;
   png_ptr->hist = (png_uint_16p)png_malloc(png_ptr,
      num * sizeof (png_uint_16));
   png_ptr->flags |= PNG_FLAG_FREE_HIST;
   for (i = 0; i < num; i++)
   {
      png_byte buf[2];

      png_crc_read(png_ptr, buf, 2);
      png_ptr->hist[i] = png_get_uint_16(buf);
   }

   if (png_crc_finish(png_ptr, 0))
      return;

   png_set_hIST(png_ptr, info_ptr, png_ptr->hist);
}
#endif

#if defined(PNG_READ_pHYs_SUPPORTED)
void
png_handle_pHYs(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_byte buf[9];
   png_uint_32 res_x, res_y;
   int unit_type;

   png_debug(1, "in png_handle_pHYs\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before pHYS");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid pHYS after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_pHYs)
   {
      png_warning(png_ptr, "Duplicate pHYS chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (length != 9)
   {
      png_warning(png_ptr, "Incorrect pHYs chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, 9);
   if (png_crc_finish(png_ptr, 0))
      return;

   res_x = png_get_uint_32(buf);
   res_y = png_get_uint_32(buf + 4);
   unit_type = buf[8];
   png_set_pHYs(png_ptr, info_ptr, res_x, res_y, unit_type);
}
#endif

#if defined(PNG_READ_oFFs_SUPPORTED)
void
png_handle_oFFs(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_byte buf[9];
   png_uint_32 offset_x, offset_y;
   int unit_type;

   png_debug(1, "in png_handle_oFFs\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before oFFs");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid oFFs after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_oFFs)
   {
      png_warning(png_ptr, "Duplicate oFFs chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (length != 9)
   {
      png_warning(png_ptr, "Incorrect oFFs chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, 9);
   if (png_crc_finish(png_ptr, 0))
      return;

   offset_x = png_get_uint_32(buf);
   offset_y = png_get_uint_32(buf + 4);
   unit_type = buf[8];
   png_set_oFFs(png_ptr, info_ptr, offset_x, offset_y, unit_type);
}
#endif

#if defined(PNG_READ_pCAL_SUPPORTED)
/* read the pCAL chunk (png-scivis-19970203) */
void
png_handle_pCAL(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_charp purpose;
   png_int_32 X0, X1;
   png_byte type, nparams;
   png_charp buf, units, endptr;
   png_charpp params;
   int i;

   png_debug(1, "in png_handle_pCAL\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before pCAL");
   else if (png_ptr->mode & PNG_HAVE_IDAT)
   {
      png_warning(png_ptr, "Invalid pCAL after IDAT");
      png_crc_finish(png_ptr, length);
      return;
   }
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_pCAL)
   {
      png_warning(png_ptr, "Duplicate pCAL chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_debug1(2, "Allocating and reading pCAL chunk data (%d bytes)\n",
      length + 1);
   purpose = (png_charp)png_malloc(png_ptr, length + 1);
   png_crc_read(png_ptr, (png_bytep)purpose, (png_size_t)length);

   if (png_crc_finish(png_ptr, 0))
   {
      png_free(png_ptr, purpose);
      return;
   }

   purpose[length] = '\0'; /* null terminate the last string */

   png_debug(3, "Finding end of pCAL purpose string\n");
   for (buf = purpose; *buf != '\0'; buf++)
      /* empty loop */;

   endptr = purpose + length;

   /* We need to have at least 12 bytes after the purpose string
      in order to get the parameter information. */
   if (endptr <= buf + 12)
   {
      png_warning(png_ptr, "Invalid pCAL data");
      png_free(png_ptr, purpose);
      return;
   }

   png_debug(3, "Reading pCAL X0, X1, type, nparams, and units\n");
   X0 = png_get_int_32((png_bytep)buf+1);
   X1 = png_get_int_32((png_bytep)buf+5);
   type = buf[9];
   nparams = buf[10];
   units = buf + 11;

   png_debug(3, "Checking pCAL equation type and number of parameters\n");
   /* Check that we have the right number of parameters for known
      equation types. */
   if ((type == PNG_EQUATION_LINEAR && nparams != 2) ||
       (type == PNG_EQUATION_BASE_E && nparams != 3) ||
       (type == PNG_EQUATION_ARBITRARY && nparams != 3) ||
       (type == PNG_EQUATION_HYPERBOLIC && nparams != 4))
   {
      png_warning(png_ptr, "Invalid pCAL parameters for equation type");
      png_free(png_ptr, purpose);
      return;
   }
   else if (type >= PNG_EQUATION_LAST)
   {
      png_warning(png_ptr, "Unrecognized equation type for pCAL chunk");
   }

   /* Empty loop to move past the units string. */
   for (buf = units; *buf != '\0'; buf++);

   png_debug(3, "Allocating pCAL parameters array\n");
   params = (png_charpp)png_malloc(png_ptr, nparams*sizeof(png_charp)) ;

   /* Get pointers to the start of each parameter string. */
   for (i = 0; i < nparams; i++)
   {
      buf++; /* Skip the null string terminator from previous parameter. */

      png_debug1(3, "Reading pCAL parameter %d\n", i);
      /* Empty loop to move past each paramter string */
      for (params[i] = buf; *buf != '\0' && buf <= endptr; buf++);

      /* Make sure we haven't run out of data yet */
      if (buf > endptr)
      {
         png_warning(png_ptr, "Invalid pCAL data");
         png_free(png_ptr, purpose);
         png_free(png_ptr, params);
         return;
      }
   }

   png_set_pCAL(png_ptr, info_ptr, purpose, X0, X1, type, nparams,
      units, params);

   png_free(png_ptr, purpose);
   png_free(png_ptr, params);
}
#endif

#if defined(PNG_READ_tIME_SUPPORTED)
void
png_handle_tIME(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_byte buf[7];
   png_time mod_time;

   png_debug(1, "in png_handle_tIME\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Out of place tIME chunk");
   else if (info_ptr != NULL && info_ptr->valid & PNG_INFO_tIME)
   {
      png_warning(png_ptr, "Duplicate tIME chunk");
      png_crc_finish(png_ptr, length);
      return;
   }

   if (png_ptr->mode & PNG_HAVE_IDAT)
      png_ptr->mode |= PNG_AFTER_IDAT;

   if (length != 7)
   {
      png_warning(png_ptr, "Incorrect tIME chunk length");
      png_crc_finish(png_ptr, length);
      return;
   }

   png_crc_read(png_ptr, buf, 7);
   if (png_crc_finish(png_ptr, 0))
      return;

   mod_time.second = buf[6];
   mod_time.minute = buf[5];
   mod_time.hour = buf[4];
   mod_time.day = buf[3];
   mod_time.month = buf[2];
   mod_time.year = png_get_uint_16(buf);

   png_set_tIME(png_ptr, info_ptr, &mod_time);
}
#endif

#if defined(PNG_READ_tEXt_SUPPORTED)
/* Note: this does not properly handle chunks that are > 64K under DOS */
void
png_handle_tEXt(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_textp text_ptr;
   png_charp key;
   png_charp text;
   png_uint_32 skip = 0;

   png_debug(1, "in png_handle_tEXt\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before tEXt");

   if (png_ptr->mode & PNG_HAVE_IDAT)
      png_ptr->mode |= PNG_AFTER_IDAT;

#ifdef PNG_MAX_MALLOC_64K
   if (length > 65535L)
   {
      png_warning(png_ptr, "tEXt chunk too large to fit in memory");
      skip = length - 65535L;
      length = 65535L;
   }
#endif

   key = (png_charp)png_malloc(png_ptr, length + 1);
   png_crc_read(png_ptr, (png_bytep)key, (png_size_t)length);

   if (png_crc_finish(png_ptr, skip))
   {
      png_free(png_ptr, key);
      return;
   }

   key[length] = '\0';

   for (text = key; *text; text++)
      /* empty loop to find end of key */ ;

   if (text != key + (png_size_t)length)
      text++;

   text_ptr = (png_textp)png_malloc(png_ptr, sizeof(png_text));
   text_ptr->compression = PNG_TEXT_COMPRESSION_NONE;
   text_ptr->key = key;
   text_ptr->text = text;

   png_set_text(png_ptr, info_ptr, text_ptr, 1);

   png_free(png_ptr, text_ptr);
}
#endif

#if defined(PNG_READ_zTXt_SUPPORTED)
/* note: this does not correctly handle chunks that are > 64K under DOS */
void
png_handle_zTXt(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   static char msg[] = "Error decoding zTXt chunk";
   png_textp text_ptr;
   png_charp key;
   png_charp text;
   int comp_type = PNG_TEXT_COMPRESSION_NONE;

   png_debug(1, "in png_handle_zTXt\n");

   if (!(png_ptr->mode & PNG_HAVE_IHDR))
      png_error(png_ptr, "Missing IHDR before zTXt");

   if (png_ptr->mode & PNG_HAVE_IDAT)
      png_ptr->mode |= PNG_AFTER_IDAT;

#ifdef PNG_MAX_MALLOC_64K
   /* We will no doubt have problems with chunks even half this size, but
      there is no hard and fast rule to tell us where to stop. */
   if (length > 65535L)
   {
     png_warning(png_ptr,"zTXt chunk too large to fit in memory");
     png_crc_finish(png_ptr, length);
     return;
   }
#endif

   key = (png_charp)png_malloc(png_ptr, length + 1);
   png_crc_read(png_ptr, (png_bytep)key, (png_size_t)length);
   if (png_crc_finish(png_ptr, 0))
   {
      png_free(png_ptr, key);
      return;
   }

   key[length] = '\0';

   for (text = key; *text; text++)
      /* empty loop */ ;

   /* zTXt must have some text after the keyword */
   if (text == key + (png_size_t)length)
   {
      png_warning(png_ptr, "Zero length zTXt chunk");
   }
   else if ((comp_type = *(++text)) == PNG_TEXT_COMPRESSION_zTXt)
   {
      png_size_t text_size, key_size;
      text++;

      png_ptr->zstream.next_in = (png_bytep)text;
      png_ptr->zstream.avail_in = (uInt)(length - (text - key));
      png_ptr->zstream.next_out = png_ptr->zbuf;
      png_ptr->zstream.avail_out = png_ptr->zbuf_size;

      key_size = text - key;
      text_size = 0;
      text = NULL;

      while (png_ptr->zstream.avail_in)
      {
         int ret;

         ret = inflate(&png_ptr->zstream, Z_PARTIAL_FLUSH);
         if (ret != Z_OK && ret != Z_STREAM_END)
         {
            if (png_ptr->zstream.msg != NULL)
               png_warning(png_ptr, png_ptr->zstream.msg);
            else
               png_warning(png_ptr, msg);
            inflateReset(&png_ptr->zstream);
            png_ptr->zstream.avail_in = 0;

            if (text ==  NULL)
            {
               text_size = key_size + sizeof(msg) + 1;
               text = (png_charp)png_malloc(png_ptr, text_size);
               png_memcpy(text, key, key_size);
            }

            text[text_size - 1] = '\0';

            /* Copy what we can of the error message into the text chunk */
            text_size = length - (text - key) - 1;
            text_size = sizeof(msg) > text_size ? text_size : sizeof(msg);
            png_memcpy(text + key_size, msg, text_size + 1);
            break;
         }
         if (!png_ptr->zstream.avail_out || ret == Z_STREAM_END)
         {
            if (text == NULL)
            {
               text = (png_charp)png_malloc(png_ptr,
                  png_ptr->zbuf_size - png_ptr->zstream.avail_out +
                     key_size + 1);
               png_memcpy(text + key_size, png_ptr->zbuf,
                  png_ptr->zbuf_size - png_ptr->zstream.avail_out);
               png_memcpy(text, key, key_size);
               text_size = key_size + png_ptr->zbuf_size -
                  png_ptr->zstream.avail_out;
               *(text + text_size) = '\0';
            }
            else
            {
               png_charp tmp;

               tmp = text;
               text = png_malloc(png_ptr, text_size +
                  png_ptr->zbuf_size - png_ptr->zstream.avail_out + 1);
               png_memcpy(text, tmp, text_size);
               png_free(png_ptr, tmp);
               png_memcpy(text + text_size, png_ptr->zbuf,
                  (png_ptr->zbuf_size - png_ptr->zstream.avail_out));
               text_size += png_ptr->zbuf_size - png_ptr->zstream.avail_out;
               *(text + text_size) = '\0';
            }
            if (ret != Z_STREAM_END)
            {
               png_ptr->zstream.next_out = png_ptr->zbuf;
               png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
            }
            else
            {
               break;
            }
         }
      }

      inflateReset(&png_ptr->zstream);
      png_ptr->zstream.avail_in = 0;

      png_free(png_ptr, key);
      key = text;
      text += key_size;
      text_size -= key_size;
   }
   else /* if (comp_type >= PNG_TEXT_COMPRESSION_LAST) */
   {
      png_size_t text_size;
      char umsg[50];

      sprintf(umsg, "Unknown zTXt compression type %d", comp_type);
      png_warning(png_ptr, umsg);

      /* Copy what we can of the error message into the text chunk */
      text_size = (png_size_t)length - (text - key) - 1;
      text_size = sizeof(msg) > text_size ? text_size : sizeof(msg);
      png_memcpy(text, msg, text_size + 1);
   }

   text_ptr = (png_textp)png_malloc(png_ptr, sizeof(png_text));
   text_ptr->compression = comp_type;
   text_ptr->key = key;
   text_ptr->text = text;

   png_set_text(png_ptr, info_ptr, text_ptr, 1);

   png_free(png_ptr, text_ptr);
}
#endif

/* This function is called when we haven't found a handler for a
   chunk.  If there isn't a problem with the chunk itself (ie bad
   chunk name, CRC, or a critical chunk), the chunk is silently ignored. */
void
png_handle_unknown(png_structp png_ptr, png_infop info_ptr, png_uint_32 length)
{
   png_debug(1, "in png_handle_unknown\n");

   /* In the future we can have code here that calls user-supplied
    * callback functions for unknown chunks before they are ignored or
    * cause an error.
    */
   png_check_chunk_name(png_ptr, png_ptr->chunk_name);

   if (!(png_ptr->chunk_name[0] & 0x20))
   {
      char msg[40];

      sprintf(msg, "Unknown critical chunk %s", png_ptr->chunk_name);
      png_error(png_ptr, msg);
   }

   if (png_ptr->mode & PNG_HAVE_IDAT)
      png_ptr->mode |= PNG_AFTER_IDAT;

   png_crc_finish(png_ptr, length);
}

/* This function is called to verify that a chunk name is valid.
   This function can't have the "critical chunk check" incorporated
   into it, since in the future we will need to be able to call user
   functions to handle unknown critical chunks after we check that
   the chunk name itself is valid. */

#define isnonalpha(c) ((c) < 41 || (c) > 122 || ((c) > 90 && (c) < 97))

void
png_check_chunk_name(png_structp png_ptr, png_bytep chunk_name)
{
   png_debug(1, "in png_check_chunk_name\n");
   if (isnonalpha(chunk_name[0]) || isnonalpha(chunk_name[1]) ||
       isnonalpha(chunk_name[2]) || isnonalpha(chunk_name[3]))
   {
      char msg[45];

      sprintf(msg, "Invalid chunk type 0x%02X 0x%02X 0x%02X 0x%02X",
         chunk_name[0], chunk_name[1], chunk_name[2], chunk_name[3]);
      png_error(png_ptr, msg);
   }
}

/* Combines the row recently read in with the previous row.
   This routine takes care of alpha and transparency if requested.
   This routine also handles the two methods of progressive display
   of interlaced images, depending on the mask value.
   The mask value describes which pixels are to be combined with
   the row.  The pattern always repeats every 8 pixels, so just 8
   bits are needed.  A one indicates the pixels is to be combined,
   a zero indicates the pixel is to be skipped.  This is in addition
   to any alpha or transparency value associated with the pixel.  If
   you want all pixels to be combined, pass 0xff (255) in mask.  */
void
png_combine_row(png_structp png_ptr, png_bytep row,
   int mask)
{
   png_debug(1,"in png_combine_row\n");
   if (mask == 0xff)
   {
      png_memcpy(row, png_ptr->row_buf + 1,
         (png_size_t)((png_ptr->width *
         png_ptr->row_info.pixel_depth + 7) >> 3));
   }
   else
   {
      switch (png_ptr->row_info.pixel_depth)
      {
         case 1:
         {
            png_bytep sp;
            png_bytep dp;
            int s_inc, s_start, s_end;
            int m;
            int shift;
            png_uint_32 i;

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (png_ptr->transformations & PNG_PACKSWAP)
            {
                s_start = 0;
                s_end = 7;
                s_inc = 1;
            }
            else
#endif
            {
                s_start = 7;
                s_end = 0;
                s_inc = -1;
            }

            shift = s_start;

            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  int value;

                  value = (*sp >> shift) & 0x1;
                  *dp &= (png_byte)((0x7f7f >> (7 - shift)) & 0xff);
                  *dp |= (png_byte)(value << shift);
               }

               if (shift == s_end)
               {
                  shift = s_start;
                  sp++;
                  dp++;
               }
               else
                  shift += s_inc;

               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }
         case 2:
         {
            png_bytep sp;
            png_bytep dp;
            int s_start, s_end, s_inc;
            int m;
            int shift;
            png_uint_32 i;
            int value;

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (png_ptr->transformations & PNG_PACKSWAP)
            {
               s_start = 0;
               s_end = 6;
               s_inc = 2;
            }
            else
#endif
            {
               s_start = 6;
               s_end = 0;
               s_inc = -2;
            }

            shift = s_start;

            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  value = (*sp >> shift) & 0x3;
                  *dp &= (png_byte)((0x3f3f >> (6 - shift)) & 0xff);
                  *dp |= (png_byte)(value << shift);
               }

               if (shift == s_end)
               {
                  shift = s_start;
                  sp++;
                  dp++;
               }
               else
                  shift += s_inc;
               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }
         case 4:
         {
            png_bytep sp;
            png_bytep dp;
            int s_start, s_end, s_inc;
            int m;
            int shift;
            png_uint_32 i;
            int value;

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (png_ptr->transformations & PNG_PACKSWAP)
            {
               s_start = 0;
               s_end = 4;
               s_inc = 4;
            }
            else
#endif
            {
               s_start = 4;
               s_end = 0;
               s_inc = -4;
            }
            shift = s_start;

            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  value = (*sp >> shift) & 0xf;
                  *dp &= (png_byte)((0xf0f >> (4 - shift)) & 0xff);
                  *dp |= (png_byte)(value << shift);
               }

               if (shift == s_end)
               {
                  shift = s_start;
                  sp++;
                  dp++;
               }
               else
                  shift += s_inc;
               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }
         default:
         {
            png_bytep sp;
            png_bytep dp;
            png_size_t pixel_bytes;
            png_uint_32 i;
            png_byte m;

            pixel_bytes = (png_ptr->row_info.pixel_depth >> 3);

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  png_memcpy(dp, sp, pixel_bytes);
               }

               sp += pixel_bytes;
               dp += pixel_bytes;

               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }
      }
   }
}

#if defined(PNG_READ_INTERLACING_SUPPORTED)
void
png_do_read_interlace(png_row_infop row_info, png_bytep row, int pass,
   png_uint_32 transformations)
{
   png_debug(1,"in png_do_read_interlace\n");
   if (row != NULL && row_info != NULL)
   {
      png_uint_32 final_width;

      final_width = row_info->width * png_pass_inc[pass];

      switch (row_info->pixel_depth)
      {
         case 1:
         {
            png_bytep sp, dp;
            int sshift, dshift;
            int s_start, s_end, s_inc;
            png_byte v;
            png_uint_32 i;
            int j;

            sp = row + (png_size_t)((row_info->width - 1) >> 3);
            dp = row + (png_size_t)((final_width - 1) >> 3);
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (transformations & PNG_PACKSWAP)
            {
                sshift = (int)((row_info->width + 7) & 7);
                dshift = (int)((final_width + 7) & 7);
                s_start = 7;
                s_end = 0;
                s_inc = -1;
            }
            else
#endif
            {
                sshift = 7 - (int)((row_info->width + 7) & 7);
                dshift = 7 - (int)((final_width + 7) & 7);
                s_start = 0;
                s_end = 7;
                s_inc = +1;
            }

            for (i = row_info->width; i; i--)
            {
               v = (png_byte)((*sp >> sshift) & 0x1);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  *dp &= (png_byte)((0x7f7f >> (7 - dshift)) & 0xff);
                  *dp |= (png_byte)(v << dshift);
                  if (dshift == s_end)
                  {
                     dshift = s_start;
                     dp--;
                  }
                  else
                     dshift += s_inc;
               }
               if (sshift == s_end)
               {
                  sshift = s_start;
                  sp--;
               }
               else
                  sshift += s_inc;
            }
            break;
         }
         case 2:
         {
            png_bytep sp, dp;
            int sshift, dshift;
            int s_start, s_end, s_inc;
            png_uint_32 i;

            sp = row + (png_size_t)((row_info->width - 1) >> 2);
            dp = row + (png_size_t)((final_width - 1) >> 2);
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (transformations & PNG_PACKSWAP)
            {
               sshift = (png_size_t)(((row_info->width + 3) & 3) << 1);
               dshift = (png_size_t)(((final_width + 3) & 3) << 1);
               s_start = 6;
               s_end = 0;
               s_inc = -2;
            }
            else
#endif
            {
               sshift = (png_size_t)((3 - ((row_info->width + 3) & 3)) << 1);
               dshift = (png_size_t)((3 - ((final_width + 3) & 3)) << 1);
               s_start = 0;
               s_end = 6;
               s_inc = +2;
            }

            for (i = row_info->width; i; i--)
            {
               png_byte v;
	       int j;

               v = (png_byte)((*sp >> sshift) & 0x3);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  *dp &= (png_byte)((0x3f3f >> (6 - dshift)) & 0xff);
                  *dp |= (png_byte)(v << dshift);
                  if (dshift == s_end)
                  {
                     dshift = s_start;
                     dp--;
                  }
                  else
                     dshift += s_inc;
               }
               if (sshift == s_end)
               {
                  sshift = s_start;
                  sp--;
               }
               else
                  sshift += s_inc;
            }
            break;
         }
         case 4:
         {
            png_bytep sp, dp;
            int sshift, dshift;
            int s_start, s_end, s_inc;
            png_uint_32 i;

            sp = row + (png_size_t)((row_info->width - 1) >> 1);
            dp = row + (png_size_t)((final_width - 1) >> 1);
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (transformations & PNG_PACKSWAP)
            {
               sshift = (png_size_t)(((row_info->width + 1) & 1) << 2);
               dshift = (png_size_t)(((final_width + 1) & 1) << 2);
               s_start = 4;
               s_end = 0;
               s_inc = -4;
            }
            else
#endif
            {
               sshift = (png_size_t)((1 - ((row_info->width + 1) & 1)) << 2);
               dshift = (png_size_t)((1 - ((final_width + 1) & 1)) << 2);
               s_start = 0;
               s_end = 4;
               s_inc = 4;
            }

            for (i = row_info->width; i; i--)
            {
	       png_byte v;
	       int j;

               v = (png_byte)((*sp >> sshift) & 0xf);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  *dp &= (png_byte)((0xf0f >> (4 - dshift)) & 0xff);
                  *dp |= (png_byte)(v << dshift);
                  if (dshift == s_end)
                  {
                     dshift = s_start;
                     dp--;
                  }
                  else
                     dshift += s_inc;
               }
               if (sshift == s_end)
               {
                  sshift = s_start;
                  sp--;
               }
               else
                  sshift += s_inc;
            }
            break;
         }
         default:
         {
            png_bytep sp, dp;
            png_uint_32 i;
            png_size_t pixel_bytes;

            pixel_bytes = (row_info->pixel_depth >> 3);

            sp = row + (row_info->width - 1) * pixel_bytes;
            dp = row + (final_width - 1) * pixel_bytes;
            for (i = row_info->width; i; i--)
            {
               png_byte v[8];
               int j;

               png_memcpy(v, sp, pixel_bytes);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  png_memcpy(dp, v, pixel_bytes);
                  dp -= pixel_bytes;
               }
               sp -= pixel_bytes;
            }
            break;
         }
      }
      row_info->width = final_width;
      row_info->rowbytes = ((final_width *
         (png_uint_32)row_info->pixel_depth + 7) >> 3);
   }
}
#endif

void
png_read_filter_row(png_structp png_ptr, png_row_infop row_info, png_bytep row,
   png_bytep prev_row, int filter)
{
   png_debug(1, "in png_read_filter_row\n");
   png_debug2(2,"row = %d, filter = %d\n", png_ptr->row_number, filter);

   switch (filter)
   {
      case PNG_FILTER_VALUE_NONE:
         break;
      case PNG_FILTER_VALUE_SUB:
      {
         png_uint_32 i;
         int bpp;
         png_bytep rp;
         png_bytep lp;

         bpp = (row_info->pixel_depth + 7) / 8;
         for (i = (png_uint_32)bpp, rp = row + bpp, lp = row;
            i < row_info->rowbytes; i++, rp++, lp++)
         {
            *rp = (png_byte)(((int)(*rp) + (int)(*lp)) & 0xff);
         }
         break;
      }
      case PNG_FILTER_VALUE_UP:
      {
         png_uint_32 i;
         png_bytep rp;
         png_bytep pp;

         for (i = 0, rp = row, pp = prev_row;
            i < row_info->rowbytes; i++, rp++, pp++)
         {
            *rp = (png_byte)(((int)(*rp) + (int)(*pp)) & 0xff);
         }
         break;
      }
      case PNG_FILTER_VALUE_AVG:
      {
         png_uint_32 i;
         int bpp;
         png_bytep rp;
         png_bytep pp;
         png_bytep lp;

         bpp = (row_info->pixel_depth + 7) / 8;
         for (i = 0, rp = row, pp = prev_row;
            i < (png_uint_32)bpp; i++, rp++, pp++)
         {
            *rp = (png_byte)(((int)(*rp) +
               ((int)(*pp) / 2)) & 0xff);
         }
         for (lp = row; i < row_info->rowbytes; i++, rp++, lp++, pp++)
         {
            *rp = (png_byte)(((int)(*rp) +
               (int)(*pp + *lp) / 2) & 0xff);
         }
         break;
      }
      case PNG_FILTER_VALUE_PAETH:
      {
         int bpp;
         png_uint_32 i;
         png_bytep rp;
         png_bytep pp;
         png_bytep lp;
         png_bytep cp;

         bpp = (row_info->pixel_depth + 7) / 8;
         for (i = 0, rp = row, pp = prev_row,
            lp = row - bpp, cp = prev_row - bpp;
            i < row_info->rowbytes; i++, rp++, pp++, lp++, cp++)
         {
            int a, b, c, pa, pb, pc, p;

            b = *pp;
            if (i >= (png_uint_32)bpp)
            {
               c = *cp;
               a = *lp;
            }
            else
            {
               a = c = 0;
            }
            p = a + b - c;
            pa = abs(p - a);
            pb = abs(p - b);
            pc = abs(p - c);

            if (pa <= pb && pa <= pc)
               p = a;
            else if (pb <= pc)
               p = b;
            else
               p = c;

            *rp = (png_byte)(((int)(*rp) + p) & 0xff);
         }
         break;
      }
      default:
         png_error(png_ptr, "Bad adaptive filter type");
         break;
   }
}

void
png_read_finish_row(png_structp png_ptr)
{
   png_debug(1, "in png_read_finish_row\n");
   png_ptr->row_number++;
   if (png_ptr->row_number < png_ptr->num_rows)
      return;

   if (png_ptr->interlaced)
   {
      png_ptr->row_number = 0;
      png_memset(png_ptr->prev_row, 0, png_ptr->rowbytes + 1);
      do
      {
         png_ptr->pass++;
         if (png_ptr->pass >= 7)
            break;
         png_ptr->iwidth = (png_ptr->width +
            png_pass_inc[png_ptr->pass] - 1 -
            png_pass_start[png_ptr->pass]) /
            png_pass_inc[png_ptr->pass];
         png_ptr->irowbytes = ((png_ptr->iwidth *
            png_ptr->pixel_depth + 7) >> 3) + 1;
         if (!(png_ptr->transformations & PNG_INTERLACE))
         {
            png_ptr->num_rows = (png_ptr->height +
               png_pass_yinc[png_ptr->pass] - 1 -
               png_pass_ystart[png_ptr->pass]) /
               png_pass_yinc[png_ptr->pass];
            if (!(png_ptr->num_rows))
               continue;
         }
         if (png_ptr->transformations & PNG_INTERLACE)
            break;
      } while (png_ptr->iwidth == 0);

      if (png_ptr->pass < 7)
         return;
   }

   if (!(png_ptr->flags & PNG_FLAG_ZLIB_FINISHED))
   {
      char extra;
      int ret;

      png_ptr->zstream.next_out = (Byte *)&extra;
      png_ptr->zstream.avail_out = (uInt)1;
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
            png_crc_read(png_ptr, png_ptr->zbuf, png_ptr->zstream.avail_in);
            png_ptr->idat_size -= png_ptr->zstream.avail_in;
         }
         ret = inflate(&png_ptr->zstream, Z_PARTIAL_FLUSH);
         if (ret == Z_STREAM_END)
         {
            if (!(png_ptr->zstream.avail_out) || png_ptr->zstream.avail_in ||
               png_ptr->idat_size)
               png_error(png_ptr, "Extra compressed data");
            png_ptr->mode |= PNG_AFTER_IDAT;
            png_ptr->flags |= PNG_FLAG_ZLIB_FINISHED;
            break;
         }
         if (ret != Z_OK)
            png_error(png_ptr, png_ptr->zstream.msg ? png_ptr->zstream.msg :
                      "Decompression Error");

         if (!(png_ptr->zstream.avail_out))
            png_error(png_ptr, "Extra compressed data");

      } while (1);
      png_ptr->zstream.avail_out = 0;
   }

   if (png_ptr->idat_size || png_ptr->zstream.avail_in)
      png_error(png_ptr, "Extra compression data");

   inflateReset(&png_ptr->zstream);

   png_ptr->mode |= PNG_AFTER_IDAT;
}

void
png_read_start_row(png_structp png_ptr)
{
   int max_pixel_depth;
   png_uint_32 rowbytes;

   png_debug(1, "in png_read_start_row\n");
   png_ptr->zstream.avail_in = 0;
 /*  png_init_read_transformations(png_ptr);*/

   if (png_ptr->interlaced)
   {
      if (!(png_ptr->transformations & PNG_INTERLACE))
         png_ptr->num_rows = (png_ptr->height + png_pass_yinc[0] - 1 -
            png_pass_ystart[0]) / png_pass_yinc[0];
      else
         png_ptr->num_rows = png_ptr->height;

      png_ptr->iwidth = (png_ptr->width +
         png_pass_inc[png_ptr->pass] - 1 -
         png_pass_start[png_ptr->pass]) /
         png_pass_inc[png_ptr->pass];
      png_ptr->irowbytes = ((png_ptr->iwidth *
         png_ptr->pixel_depth + 7) >> 3) + 1;
   }
   else
   {
      png_ptr->num_rows = png_ptr->height;
      png_ptr->iwidth = png_ptr->width;
      png_ptr->irowbytes = png_ptr->rowbytes + 1;
   }
   max_pixel_depth = png_ptr->pixel_depth;

#if defined(PNG_READ_PACK_SUPPORTED)
   if ((png_ptr->transformations & PNG_PACK) && png_ptr->bit_depth < 8)
      max_pixel_depth = 8;
#endif

#if defined(PNG_READ_EXPAND_SUPPORTED)
   if (png_ptr->transformations & PNG_EXPAND)
   {
      if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
      {
         if (png_ptr->num_trans)
            max_pixel_depth = 32;
         else
            max_pixel_depth = 24;
      }
      else if (png_ptr->color_type == PNG_COLOR_TYPE_GRAY)
      {
         if (max_pixel_depth < 8)
            max_pixel_depth = 8;
         if (png_ptr->num_trans)
            max_pixel_depth *= 2;
      }
      else if (png_ptr->color_type == PNG_COLOR_TYPE_RGB)
      {
         if (png_ptr->num_trans)
         {
            max_pixel_depth *= 4;
            max_pixel_depth /= 3;
         }
      }
   }
#endif

#if defined(PNG_READ_FILLER_SUPPORTED)
   if (png_ptr->transformations & (PNG_FILLER))
   {
      if (max_pixel_depth < 32)
         max_pixel_depth = 32;
   }
#endif

#if defined(PNG_READ_GRAY_TO_RGB_SUPPORTED)
   if (png_ptr->transformations & PNG_GRAY_TO_RGB)
   {
      if ((png_ptr->num_trans && (png_ptr->transformations & PNG_EXPAND)) ||
         png_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      {
         if (max_pixel_depth <= 16)
            max_pixel_depth = 32;
         else if (max_pixel_depth <= 32)
            max_pixel_depth = 64;
      }
      else
      {
         if (max_pixel_depth <= 8)
            max_pixel_depth = 24;
         else if (max_pixel_depth <= 16)
            max_pixel_depth = 48;
      }
   }
#endif

   /* align the width on the next larger 8 pixels.  Mainly used
      for interlacing */
   rowbytes = ((png_ptr->width + 7) & ~((png_uint_32)7));
   /* calculate the maximum bytes needed, adding a byte and a pixel
      for safety sake */
   rowbytes = ((rowbytes * (png_uint_32)max_pixel_depth + 7) >> 3) +
      1 + ((max_pixel_depth + 7) >> 3);
#ifdef PNG_MAX_MALLOC_64K
   if (rowbytes > 65536L)
      png_error(png_ptr, "This image requires a row greater than 64KB");
#endif
   png_ptr->row_buf = (png_bytep)png_malloc(png_ptr, rowbytes);
   png_memset(png_ptr->row_buf, 0, png_ptr->rowbytes);

#ifdef PNG_MAX_MALLOC_64K
   if (png_ptr->rowbytes + 1 > 65536L)
      png_error(png_ptr, "This image requires a row greater than 64KB");
#endif
   png_ptr->prev_row = (png_bytep)png_malloc(png_ptr, png_ptr->rowbytes + 1);
   png_memset(png_ptr->prev_row, 0, png_ptr->rowbytes + 1);

   png_debug1(3, "width = %d,\n", png_ptr->width);
   png_debug1(3, "height = %d,\n", png_ptr->height);
   png_debug1(3, "iwidth = %d,\n", png_ptr->iwidth);
   png_debug1(3, "num_rows = %d\n", png_ptr->num_rows);
   png_debug1(3, "rowbytes = %d,\n", png_ptr->rowbytes);
   png_debug1(3, "irowbytes = %d,\n", png_ptr->irowbytes);

   png_ptr->flags |= PNG_FLAG_ROW_INIT;
}

