
/* pngwutil.c - utilities to write a PNG file

   libpng 1.0 beta 5 - version 0.95
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   Copyright (c) 1996, 1997 Andreas Dilger
   March 30, 1997
   */

#define PNG_INTERNAL
#include "png.h"

/* Place a 32-bit number into a buffer in PNG byte order.  We work
 * with unsigned numbers for convenience, although one supported
 * ancillary chunk uses signed (two's complement) numbers.
 */
void
png_save_uint_32(png_bytep buf, png_uint_32 i)
{
   buf[0] = (png_byte)((i >> 24) & 0xff);
   buf[1] = (png_byte)((i >> 16) & 0xff);
   buf[2] = (png_byte)((i >> 8) & 0xff);
   buf[3] = (png_byte)(i & 0xff);
}

#if defined(PNG_WRITE_pCAL_SUPPORTED)
/* The png_save_int_32 function assumes integers are stored in two's
   complement format.  If this isn't the case, then this routine needs to
   be modified to write data in two's complement format. */
void
png_save_int_32(png_bytep buf, png_int_32 i)
{
   buf[0] = (png_byte)((i >> 24) & 0xff);
   buf[1] = (png_byte)((i >> 16) & 0xff);
   buf[2] = (png_byte)((i >> 8) & 0xff);
   buf[3] = (png_byte)(i & 0xff);
}
#endif

/* Place a 16-bit number into a buffer in PNG byte order. */
void
png_save_uint_16(png_bytep buf, png_uint_16 i)
{
   buf[0] = (png_byte)((i >> 8) & 0xff);
   buf[1] = (png_byte)(i & 0xff);
}

/* Write a PNG chunk all at once.  The type is an array of ASCII characters
   representing the chunk name.  The array must be at least 4 bytes in
   length, and does not need to be null terminated.  To be safe, pass the
   pre-defined chunk names here, and if you need a new one, define it
   where the others are defined.  The length is the length of the data.
   All the data must be present.  If that is not possible, use the
   png_write_chunk_start(), png_write_chunk_data(), and png_write_chunk_end()
   functions instead.  */
void
png_write_chunk(png_structp png_ptr, png_bytep chunk_name,
   png_bytep data, png_size_t length)
{
   png_write_chunk_start(png_ptr, chunk_name, (png_uint_32)length);
   png_write_chunk_data(png_ptr, data, length);
   png_write_chunk_end(png_ptr);
}

/* Write the start of a PNG chunk.  The type is the chunk type.
   The total_length is the sum of the lengths of all the data you will be
   passing in png_write_chunk_data() */
void
png_write_chunk_start(png_structp png_ptr, png_bytep chunk_name,
   png_uint_32 length)
{
   png_byte buf[4];
   png_debug2(0, "Writing %s chunk (%d bytes)\n", chunk_name, length);

   /* write the length */
   png_save_uint_32(buf, length);
   png_write_data(png_ptr, buf, (png_size_t)4);

   /* write the chunk name */
   png_write_data(png_ptr, chunk_name, (png_size_t)4);
   /* reset the crc and run it over the chunk name */
   png_reset_crc(png_ptr);
   png_calculate_crc(png_ptr, chunk_name, (png_size_t)4);
}

/* Write the data of a PNG chunk started with png_write_chunk_start().
   Note that multiple calls to this function are allowed, and that the
   sum of the lengths from these calls *must* add up to the total_length
   given to png_write_chunk_start(). */
void
png_write_chunk_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   /* write the data, and run the CRC over it */
   if (data != NULL && length > 0)
   {
      png_calculate_crc(png_ptr, data, length);
      png_write_data(png_ptr, data, length);
   }
}

/* Finish a chunk started with png_write_chunk_start(). */
void
png_write_chunk_end(png_structp png_ptr)
{
   png_byte buf[4];

   /* write the crc */
#ifdef PNG_USE_OWN_CRC
   png_save_uint_32(buf, ~png_ptr->crc);
#else
   png_save_uint_32(buf, png_ptr->crc);
#endif

   png_write_data(png_ptr, buf, (png_size_t)4);
}

/* Simple function to write the signature.  If we have already written
 * the magic bytes of the signature, or more likely, the PNG stream is
 * being embedded into another stream and doesn't need its own signature,
 * we should call png_set_sig_bytes() to tell libpng how many of the
 * bytes have already been written. */
void
png_write_sig(png_structp png_ptr)
{
   /* write the rest of the 8 byte signature */
   png_write_data(png_ptr, &png_sig[png_ptr->sig_bytes],
      (png_size_t)8 - png_ptr->sig_bytes);
}

/* Write the IHDR chunk, and update the png_struct with the necessary
   information.  Note that the rest of this code depends upon this
   information being correct.  */
void
png_write_IHDR(png_structp png_ptr, png_uint_32 width, png_uint_32 height,
   int bit_depth, int color_type, int compression_type, int filter_type,
   int interlace_type)
{
   png_byte buf[13]; /* buffer to store the IHDR info */

   png_debug(1, "in png_write_IHDR\n");
   /* Check that we have valid input data from the application info */
   switch (color_type)
   {
      case PNG_COLOR_TYPE_GRAY:
         switch (bit_depth)
         {
            case 1:
            case 2:
            case 4:
            case 8:
            case 16: png_ptr->channels = 1; break;
            default: png_error(png_ptr,"Invalid bit depth for grayscale image");
         }
         break;
      case PNG_COLOR_TYPE_RGB:
         if (bit_depth != 8 && bit_depth != 16)
            png_error(png_ptr, "Invalid bit depth for RGB image");
         png_ptr->channels = 3;
         break;
      case PNG_COLOR_TYPE_PALETTE:
         switch (bit_depth)
         {
            case 1:
            case 2:
            case 4:
            case 8: png_ptr->channels = 1; break;
            default: png_error(png_ptr, "Invalid bit depth for paletted image");
         }
         break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
         if (bit_depth != 8 && bit_depth != 16)
            png_error(png_ptr, "Invalid bit depth for grayscale+alpha image");
         png_ptr->channels = 2;
         break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
         if (bit_depth != 8 && bit_depth != 16)
            png_error(png_ptr, "Invalid bit depth for RGBA image");
         png_ptr->channels = 4;
         break;
      default:
         png_error(png_ptr, "Invalid image color type specified");
   }

   if (compression_type != PNG_COMPRESSION_TYPE_BASE)
   {
      png_warning(png_ptr, "Invalid compression type specified");
      compression_type = PNG_COMPRESSION_TYPE_BASE;
   }

   if (filter_type != PNG_FILTER_TYPE_BASE)
   {
      png_warning(png_ptr, "Invalid filter type specified");
      filter_type = PNG_FILTER_TYPE_BASE;
   }

   if (interlace_type != PNG_INTERLACE_NONE &&
      interlace_type != PNG_INTERLACE_ADAM7)
   {
      png_warning(png_ptr, "Invalid interlace type specified");
      interlace_type = PNG_INTERLACE_ADAM7;
   }

   /* save off the relevent information */
   png_ptr->bit_depth = (png_byte)bit_depth;
   png_ptr->color_type = (png_byte)color_type;
   png_ptr->interlaced = (png_byte)interlace_type;
   png_ptr->width = width;
   png_ptr->height = height;

   png_ptr->pixel_depth = (png_byte)(bit_depth * png_ptr->channels);
   png_ptr->rowbytes = ((width * (png_size_t)png_ptr->pixel_depth + 7) >> 3);
   /* set the usr info, so any transformations can modify it */
   png_ptr->usr_width = png_ptr->width;
   png_ptr->usr_bit_depth = png_ptr->bit_depth;
   png_ptr->usr_channels = png_ptr->channels;

   /* pack the header information into the buffer */
   png_save_uint_32(buf, width);
   png_save_uint_32(buf + 4, height);
   buf[8] = (png_byte)bit_depth;
   buf[9] = (png_byte)color_type;
   buf[10] = (png_byte)compression_type;
   buf[11] = (png_byte)filter_type;
   buf[12] = (png_byte)interlace_type;

   /* write the chunk */
   png_write_chunk(png_ptr, png_IHDR, buf, (png_size_t)13);

   /* initialize zlib with PNG info */
   png_ptr->zstream.zalloc = png_zalloc;
   png_ptr->zstream.zfree = png_zfree;
   png_ptr->zstream.opaque = (voidpf)png_ptr;
   if (!(png_ptr->do_filter))
   {
      if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE ||
         png_ptr->bit_depth < 8)
         png_ptr->do_filter = PNG_FILTER_NONE;
      else
         png_ptr->do_filter = PNG_ALL_FILTERS;
   }
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_STRATEGY))
   {
      if (png_ptr->do_filter != PNG_FILTER_NONE)
         png_ptr->zlib_strategy = Z_FILTERED;
      else
         png_ptr->zlib_strategy = Z_DEFAULT_STRATEGY;
   }
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_LEVEL))
      png_ptr->zlib_level = Z_DEFAULT_COMPRESSION;
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_MEM_LEVEL))
      png_ptr->zlib_mem_level = 8;
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_WINDOW_BITS))
      png_ptr->zlib_window_bits = 15;
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_METHOD))
      png_ptr->zlib_method = 8;
   deflateInit2(&png_ptr->zstream, png_ptr->zlib_level,
      png_ptr->zlib_method, png_ptr->zlib_window_bits,
      png_ptr->zlib_mem_level, png_ptr->zlib_strategy);
   png_ptr->zstream.next_out = png_ptr->zbuf;
   png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;

   png_ptr->mode = PNG_HAVE_IHDR;
}

/* write the palette.  We are careful not to trust png_color to be in the
   correct order for PNG, so people can redefine it to any convient
   structure. */
void
png_write_PLTE(png_structp png_ptr, png_colorp palette, png_uint_32 num_pal)
{
   png_uint_32 i;
   png_colorp pal_ptr;
   png_byte buf[3];

   png_debug(1, "in png_write_PLTE\n");
   if (num_pal == 0 || num_pal > 256)
   {
      if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
      {
         png_error(png_ptr, "Invalid number of colors in palette");
      }
      else
      {
         png_warning(png_ptr, "Invalid number of colors in palette");
         return;
      }
   }

   png_ptr->num_palette = (png_uint_16)num_pal;
   png_debug1(3, "num_palette = %d\n", png_ptr->num_palette);

   png_write_chunk_start(png_ptr, png_PLTE, num_pal * 3);
   for (i = 0, pal_ptr = palette; i < num_pal; i++, pal_ptr++)
   {
      buf[0] = pal_ptr->red;
      buf[1] = pal_ptr->green;
      buf[2] = pal_ptr->blue;
      png_write_chunk_data(png_ptr, buf, (png_size_t)3);
   }
   png_write_chunk_end(png_ptr);
   png_ptr->mode |= PNG_HAVE_PLTE;
}

/* write an IDAT chunk */
void
png_write_IDAT(png_structp png_ptr, png_bytep data, png_size_t length)
{
   png_debug(1, "in png_write_IDAT\n");
   png_write_chunk(png_ptr, png_IDAT, data, length);
   png_ptr->mode |= PNG_HAVE_IDAT;
}

/* write an IEND chunk */
void
png_write_IEND(png_structp png_ptr)
{
   png_debug(1, "in png_write_IEND\n");
   png_write_chunk(png_ptr, png_IEND, NULL, (png_size_t)0);
   png_ptr->mode |= PNG_HAVE_IEND;
}

#if defined(PNG_WRITE_gAMA_SUPPORTED)
/* write a gAMA chunk */
void
png_write_gAMA(png_structp png_ptr, double file_gamma)
{
   png_uint_32 igamma;
   png_byte buf[4];

   png_debug(1, "in png_write_gAMA\n");
   /* file_gamma is saved in 1/100,000ths */
   igamma = (png_uint_32)(file_gamma * 100000.0 + 0.5);
   png_save_uint_32(buf, igamma);
   png_write_chunk(png_ptr, png_gAMA, buf, (png_size_t)4);
}
#endif

#if defined(PNG_WRITE_sBIT_SUPPORTED)
/* write the sBIT chunk */
void
png_write_sBIT(png_structp png_ptr, png_color_8p sbit, int color_type)
{
   png_byte buf[4];
   png_size_t size;

   png_debug(1, "in png_write_sBIT\n");
   /* make sure we don't depend upon the order of PNG_COLOR_8 */
   if (color_type & PNG_COLOR_MASK_COLOR)
   {
      int maxbits;

      maxbits = color_type==PNG_COLOR_TYPE_PALETTE ? 8:png_ptr->usr_bit_depth;
      if (sbit->red == 0 || sbit->red > maxbits || 
          sbit->green == 0 || sbit->green > maxbits || 
          sbit->blue == 0 || sbit->blue > maxbits)
      {
         png_warning(png_ptr, "Invalid sBIT depth specified");
         return;
      }
      buf[0] = sbit->red;
      buf[1] = sbit->green;
      buf[2] = sbit->blue;
      size = 3;
   }
   else
   {
      if (sbit->gray == 0 || sbit->gray > png_ptr->usr_bit_depth)
      {
         png_warning(png_ptr, "Invalid sBIT depth specified");
         return;
      }
      buf[0] = sbit->gray;
      size = 1;
   }

   if (color_type & PNG_COLOR_MASK_ALPHA)
   {
      if (sbit->alpha == 0 || sbit->alpha > png_ptr->usr_bit_depth)
      {
         png_warning(png_ptr, "Invalid sBIT depth specified");
         return;
      }
      buf[size++] = sbit->alpha;
   }

   png_write_chunk(png_ptr, png_sBIT, buf, size);
}
#endif

#if defined(PNG_WRITE_cHRM_SUPPORTED)
/* write the cHRM chunk */
void
png_write_cHRM(png_structp png_ptr, double white_x, double white_y,
   double red_x, double red_y, double green_x, double green_y,
   double blue_x, double blue_y)
{
   png_uint_32 itemp;
   png_byte buf[32];

   png_debug(1, "in png_write_cHRM\n");
   /* each value is saved int 1/100,000ths */
   if (white_x < 0 || white_x > 0.8 || white_y < 0 || white_y > 0.8 ||
       white_x + white_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM white point specified");
      return;
   }
   itemp = (png_uint_32)(white_x * 100000.0 + 0.5);
   png_save_uint_32(buf, itemp);
   itemp = (png_uint_32)(white_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 4, itemp);

   if (red_x < 0 || red_x > 0.8 || red_y < 0 || red_y > 0.8 ||
       red_x + red_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM red point specified");
      return;
   }
   itemp = (png_uint_32)(red_x * 100000.0 + 0.5);
   png_save_uint_32(buf + 8, itemp);
   itemp = (png_uint_32)(red_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 12, itemp);

   if (green_x < 0 || green_x > 0.8 || green_y < 0 || green_y > 0.8 ||
       green_x + green_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM green point specified");
      return;
   }
   itemp = (png_uint_32)(green_x * 100000.0 + 0.5);
   png_save_uint_32(buf + 16, itemp);
   itemp = (png_uint_32)(green_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 20, itemp);

   if (blue_x < 0 || blue_x > 0.8 || blue_y < 0 || blue_y > 0.8 ||
       blue_x + blue_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM blue point specified");
      return;
   }
   itemp = (png_uint_32)(blue_x * 100000.0 + 0.5);
   png_save_uint_32(buf + 24, itemp);
   itemp = (png_uint_32)(blue_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 28, itemp);

   png_write_chunk(png_ptr, png_cHRM, buf, (png_size_t)32);
}
#endif

#if defined(PNG_WRITE_tRNS_SUPPORTED)
/* write the tRNS chunk */
void
png_write_tRNS(png_structp png_ptr, png_bytep trans, png_color_16p tran,
   int num_trans, int color_type)
{
   png_byte buf[6];

   png_debug(1, "in png_write_tRNS\n");
   if (color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if (num_trans <= 0 || num_trans > png_ptr->num_palette)
      {
         png_warning(png_ptr,"Invalid number of transparent colors specified");
         return;
      }
      /* write the chunk out as it is */
      png_write_chunk(png_ptr, png_tRNS, trans, (png_size_t)num_trans);
   }
   else if (color_type == PNG_COLOR_TYPE_GRAY)
   {
      /* one 16 bit value */
      png_save_uint_16(buf, tran->gray);
      png_write_chunk(png_ptr, png_tRNS, buf, (png_size_t)2);
   }
   else if (color_type == PNG_COLOR_TYPE_RGB)
   {
      /* three 16 bit values */
      png_save_uint_16(buf, tran->red);
      png_save_uint_16(buf + 2, tran->green);
      png_save_uint_16(buf + 4, tran->blue);
      png_write_chunk(png_ptr, png_tRNS, buf, (png_size_t)6);
   }
   else
   {
      png_warning(png_ptr, "Can't write tRNS with and alpha channel");
   }
}
#endif

#if defined(PNG_WRITE_bKGD_SUPPORTED)
/* write the background chunk */
void
png_write_bKGD(png_structp png_ptr, png_color_16p back, int color_type)
{
   png_byte buf[6];

   png_debug(1, "in png_write_bKGD\n");
   if (color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if (back->index > png_ptr->num_palette)
      {
         png_warning(png_ptr, "Invalid background palette index");
         return;
      }
      buf[0] = back->index;
      png_write_chunk(png_ptr, png_bKGD, buf, (png_size_t)1);
   }
   else if (color_type & PNG_COLOR_MASK_COLOR)
   {
      png_save_uint_16(buf, back->red);
      png_save_uint_16(buf + 2, back->green);
      png_save_uint_16(buf + 4, back->blue);
      png_write_chunk(png_ptr, png_bKGD, buf, (png_size_t)6);
   }
   else
   {
      png_save_uint_16(buf, back->gray);
      png_write_chunk(png_ptr, png_bKGD, buf, (png_size_t)2);
   }
}
#endif

#if defined(PNG_WRITE_hIST_SUPPORTED)
/* write the histogram */
void
png_write_hIST(png_structp png_ptr, png_uint_16p hist, png_uint_32 num_hist)
{
   png_uint_32 i;
   png_byte buf[3];

   png_debug(1, "in png_write_hIST\n");
   if (num_hist > png_ptr->num_palette)
   {
      png_debug2(3, "num_hist = %d, num_palette = %d\n", num_hist,
         png_ptr->num_palette);
      png_warning(png_ptr, "Invalid number of histogram entries specified");
      return;
   }

   png_write_chunk_start(png_ptr, png_hIST, num_hist * 2);
   for (i = 0; i < num_hist; i++)
   {
      png_save_uint_16(buf, hist[i]);
      png_write_chunk_data(png_ptr, buf, (png_size_t)2);
   }
   png_write_chunk_end(png_ptr);
}
#endif

#if defined(PNG_WRITE_tEXt_SUPPORTED) || defined(PNG_WRITE_zTXt_SUPPORTED)
/* Check that the tEXt or zTXt keyword is valid per PNG 1.0 specification,
 * and if invalid, correct the keyword rather than discarding the entire
 * chunk.  The PNG 1.0 specification requires keywords 1-79 characters in
 * length, forbids leading or trailing whitespace, multiple internal spaces,
 * and the non-break space (0x80) from ISO 8859-1.  Returns keyword length.
 */
png_size_t
png_check_keyword(png_structp png_ptr, png_charp key)
{
   png_size_t key_len;
   png_charp kp, dp;
   int kflag;

   png_debug(1, "in png_check_keyword\n");
   if (key == NULL || (key_len = png_strlen(key)) == 0)
   {
      char msg[40];

      sprintf(msg, "Zero length %s keyword", png_ptr->chunk_name);
      png_warning(png_ptr, msg);
      return 0;
   }

   png_debug1(2, "Keyword to be checked is '%s'\n", key);

   /* Replace non-printing characters with a blank and print a warning */
   for (kp = key; *kp != '\0'; kp++)
   {
     if (*kp < 0x20 || ((png_byte)*kp > 0x7E && (png_byte)*kp < 0xA1))
     {
       char msg[40];

       sprintf(msg, "Invalid %s keyword character 0x%02X",
          png_ptr->chunk_name, *kp);
       png_warning(png_ptr, msg);
       *kp = ' ';
     }
   }

   /* Remove any trailing white space. */
   kp = key + key_len - 1;
   if (*kp == ' ')
   {
      char msg[50];
      sprintf(msg, "Trailing spaces removed from %s keyword",
         png_ptr->chunk_name);

      png_warning(png_ptr, msg);

      while (*kp == ' ')
      {
        *(kp--) = '\0';
        key_len--;
      }
   }

   /* Remove any leading white space. */
   kp = key;
   if (*kp == ' ')
   {
      char msg[50];
      sprintf(msg, "Leading spaces removed from %s keyword",
         png_ptr->chunk_name);

      png_warning(png_ptr, msg);

      while (*kp == ' ')
      {
        kp++;
        key_len--;
      }
   }

   png_debug1(2, "Checking for multiple internal spaces in '%s'\n", kp);

   /* Remove multiple internal spaces. */
   for (kflag = 0, dp = key; *kp != '\0'; kp++)
   {
      if (*kp == ' ' && kflag == 0)
      {
         *(dp++) = *kp;
         kflag = 1;
      }
      else if (*kp == ' ')
      {
         key_len--;
      }
      else
      {
         *(dp++) = *kp;
         kflag = 0;
      }
   }
   *dp = '\0';

   if (key_len == 0)
   {
      char msg[40];

      sprintf(msg, "Zero length %s keyword", png_ptr->chunk_name);
      png_warning(png_ptr, msg);
   }

   if (key_len > 79)
   {
      char msg[50];

      sprintf(msg, "%s keyword length must be 1 - 79 characters",
         png_ptr->chunk_name);
      png_warning(png_ptr, msg);
      key[79] = '\0';
      key_len = 79;
   }

   return key_len;
}
#endif

#if defined(PNG_WRITE_tEXt_SUPPORTED)
/* write a tEXt chunk */
void
png_write_tEXt(png_structp png_ptr, png_charp key, png_charp text,
   png_size_t text_len)
{
   png_size_t key_len;

   png_debug(1, "in png_write_tEXt\n");
   if (key == NULL || (key_len = png_check_keyword(png_ptr, key)) == 0)
   {
      char msg[40];

      sprintf(msg, "Empty keyword in %s chunk", "tEXt");
      png_warning(png_ptr, msg);
      return;
   }

   if (text == NULL || *text == '\0')
      text_len = 0;

   /* make sure we include the 0 after the key */
   png_write_chunk_start(png_ptr, png_tEXt, (png_uint_32)key_len+text_len+1);
   png_write_chunk_data(png_ptr, (png_bytep)key, key_len + 1);
   if (text_len)
      png_write_chunk_data(png_ptr, (png_bytep)text, text_len);

   png_write_chunk_end(png_ptr);
}
#endif

#if defined(PNG_WRITE_zTXt_SUPPORTED)
/* write a compressed text chunk */
void
png_write_zTXt(png_structp png_ptr, png_charp key, png_charp text,
   png_size_t text_len, int compression)
{
   png_size_t key_len;
   char buf[1];
   int i, ret;
   png_charpp output_ptr = NULL; /* array of pointers to output */
   int num_output_ptr = 0; /* number of output pointers used */
   int max_output_ptr = 0; /* size of output_ptr */

   png_debug(1, "in png_write_zTXt\n");
   if (key == NULL || (key_len = png_check_keyword(png_ptr, key)) == 0)
   {
      char msg[40];

      sprintf(msg, "Empty keyword in %s chunk", "zTXt");
      png_warning(png_ptr, msg);
      return;
   }

   if (text == NULL || *text == '\0' || compression==PNG_TEXT_COMPRESSION_NONE)
   {
      png_write_tEXt(png_ptr, key, text, (png_size_t)0);
      return;
   }

   if (compression >= PNG_TEXT_COMPRESSION_LAST)
   {
      char msg[50];
      sprintf(msg, "Unknown zTXt compression type %d", compression);
      png_warning(png_ptr, msg);
      compression = PNG_TEXT_COMPRESSION_zTXt;
   }

   /* We can't write the chunk until we find out how much data we have,
    * which means we need to run the compressor first, and save the
    * output.  This shouldn't be a problem, as the vast majority of
    * comments should be reasonable, but we will set up an array of
    * malloc'd pointers to be sure.
    *
    * If we knew the application was well behaved, we could simplify this
    * greatly by assuming we can always malloc an output buffer large
    * enough to hold the compressed text ((1001 * text_len / 1000) + 12)
    * and malloc this directly.  The only time this would be a bad idea is
    * if we can't malloc more than 64K and we have 64K of random input
    * data, or if the input string is incredibly large (although this
    * wouldn't cause a failure, just a slowdown due to swapping).
    */

   /* set up the compression buffers */
   png_ptr->zstream.avail_in = (uInt)text_len;
   png_ptr->zstream.next_in = (Bytef *)text;
   png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
   png_ptr->zstream.next_out = (Bytef *)png_ptr->zbuf;

   /* this is the same compression loop as in png_write_row() */
   do
   {
      /* compress the data */
      ret = deflate(&png_ptr->zstream, Z_NO_FLUSH);
      if (ret != Z_OK)
      {
         /* error */
         if (png_ptr->zstream.msg != NULL)
            png_error(png_ptr, png_ptr->zstream.msg);
         else
            png_error(png_ptr, "zlib error");
      }
      /* check to see if we need more room */
      if (!png_ptr->zstream.avail_out && png_ptr->zstream.avail_in)
      {
         /* make sure the output array has room */
         if (num_output_ptr >= max_output_ptr)
         {
            int old_max;

            old_max = max_output_ptr;
            max_output_ptr = num_output_ptr + 4;
            if (output_ptr != NULL)
            {
               png_charpp old_ptr;

               old_ptr = output_ptr;
               output_ptr = (png_charpp)png_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charpp));
               png_memcpy(output_ptr, old_ptr, old_max * sizeof (png_charp));
               png_free(png_ptr, old_ptr);
            }
            else
               output_ptr = (png_charpp)png_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charp));
         }

         /* save the data */
         output_ptr[num_output_ptr] = png_malloc(png_ptr, png_ptr->zbuf_size);
         png_memcpy(output_ptr[num_output_ptr], png_ptr->zbuf,
            png_ptr->zbuf_size);
         num_output_ptr++;

         /* and reset the buffer */
         png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
         png_ptr->zstream.next_out = png_ptr->zbuf;
      }
   /* continue until we don't have anymore to compress */
   } while (png_ptr->zstream.avail_in);

   /* finish the compression */
   do
   {
      /* tell zlib we are finished */
      ret = deflate(&png_ptr->zstream, Z_FINISH);
      if (ret != Z_OK && ret != Z_STREAM_END)
      {
         /* we got an error */
         if (png_ptr->zstream.msg != NULL)
            png_error(png_ptr, png_ptr->zstream.msg);
         else
            png_error(png_ptr, "zlib error");
      }

      /* check to see if we need more room */
      if (!(png_ptr->zstream.avail_out) && ret == Z_OK)
      {
         /* check to make sure our output array has room */
         if (num_output_ptr >= max_output_ptr)
         {
            int old_max;

            old_max = max_output_ptr;
            max_output_ptr = num_output_ptr + 4;
            if (output_ptr != NULL)
            {
               png_charpp old_ptr;

               old_ptr = output_ptr;
               /* This could be optimized to realloc() */
               output_ptr = (png_charpp)png_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charpp));
               png_memcpy(output_ptr, old_ptr, old_max * sizeof (png_charp));
               png_free(png_ptr, old_ptr);
            }
            else
               output_ptr = (png_charpp)png_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charp));
         }

         /* save off the data */
         output_ptr[num_output_ptr] = png_malloc(png_ptr,
            png_ptr->zbuf_size);
         png_memcpy(output_ptr[num_output_ptr], png_ptr->zbuf,
            png_ptr->zbuf_size);
         num_output_ptr++;

         /* and reset the buffer pointers */
         png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
         png_ptr->zstream.next_out = png_ptr->zbuf;
      }
   } while (ret != Z_STREAM_END);

   /* text length is number of buffers plus last buffer */
   text_len = png_ptr->zbuf_size * num_output_ptr;
   if (png_ptr->zstream.avail_out < png_ptr->zbuf_size)
      text_len += png_ptr->zbuf_size - (png_size_t)png_ptr->zstream.avail_out;

   /* write start of chunk */
   png_write_chunk_start(png_ptr, png_zTXt, (png_uint_32)(key_len+text_len+2));
   /* write key */
   png_write_chunk_data(png_ptr, (png_bytep)key, key_len + 1);
   buf[0] = (png_byte)compression;
   /* write compression */
   png_write_chunk_data(png_ptr, (png_bytep)buf, (png_size_t)1);

   /* write saved output buffers, if any */
   for (i = 0; i < num_output_ptr; i++)
   {
      png_write_chunk_data(png_ptr,(png_bytep)output_ptr[i],png_ptr->zbuf_size);
      png_free(png_ptr, output_ptr[i]);
   }
   if (max_output_ptr != 0)
      png_free(png_ptr, output_ptr);
   /* write anything left in zbuf */
   if (png_ptr->zstream.avail_out < (png_uint_32)png_ptr->zbuf_size)
      png_write_chunk_data(png_ptr, png_ptr->zbuf,
         png_ptr->zbuf_size - png_ptr->zstream.avail_out);
   /* close the chunk */
   png_write_chunk_end(png_ptr);

   /* reset zlib for another zTXt or the image data */
   deflateReset(&png_ptr->zstream);
}
#endif


#if defined(PNG_WRITE_oFFs_SUPPORTED)
/* write the oFFs chunk */
void
png_write_oFFs(png_structp png_ptr, png_uint_32 x_offset,
   png_uint_32 y_offset,
   int unit_type)
{
   png_byte buf[9];

   png_debug(1, "in png_write_oFFs\n");
   if (unit_type >= PNG_OFFSET_LAST)
      png_warning(png_ptr, "Unrecognized unit type for oFFs chunk");

   png_save_uint_32(buf, x_offset);
   png_save_uint_32(buf + 4, y_offset);
   buf[8] = (png_byte)unit_type;

   png_write_chunk(png_ptr, png_oFFs, buf, (png_size_t)9);
}
#endif

#if defined(PNG_WRITE_pCAL_SUPPORTED)
/* write the pCAL chunk (png-scivis-19970203) */
void
png_write_pCAL(png_structp png_ptr, png_charp purpose, png_int_32 X0,
   png_int_32 X1, int type, int nparams, png_charp units, png_charpp params)
{
   png_size_t purpose_len, units_len, total_len; 
   png_uint_32p params_len;
   png_byte buf[10];
   int i;

   png_debug1(1, "in png_write_pCAL (%d parameters)\n", nparams);
   if (type >= PNG_EQUATION_LAST)
      png_warning(png_ptr, "Unrecognized equation type for pCAL chunk");

   purpose_len = png_check_keyword(png_ptr, purpose) + 1;
   png_debug1(3, "pCAL purpose length = %d\n", purpose_len);
   units_len = png_strlen(units) + (nparams == 0 ? 0 : 1);
   png_debug1(3, "pCAL units length = %d\n", units_len);
   total_len = purpose_len + units_len + 10;

   params_len = (png_uint_32p)png_malloc(png_ptr, nparams*sizeof(png_uint_32));

   /* Find the length of each parameter, making sure we don't count the
      null terminator for the last parameter. */
   for (i = 0; i < nparams; i++)
   {
      params_len[i] = png_strlen(params[i]) + (i == nparams - 1 ? 0 : 1);
      png_debug2(3, "pCAL parameter %d length = %d\n", i, params_len[i]);
      total_len += (png_size_t)params_len[i];
   }

   png_debug1(3, "pCAL total length = %d\n", total_len);
   png_write_chunk_start(png_ptr, png_pCAL, total_len);
   png_write_chunk_data(png_ptr, (png_bytep)purpose, purpose_len);
   png_save_int_32(buf, X0);
   png_save_int_32(buf + 4, X1);
   buf[8] = (png_byte)type;
   buf[9] = (png_byte)nparams;
   png_write_chunk_data(png_ptr, buf, (png_size_t)10);
   png_write_chunk_data(png_ptr, (png_bytep)units, (png_size_t)units_len);

   for (i = 0; i < nparams; i++)
   {
      png_write_chunk_data(png_ptr, (png_bytep)params[i],
         (png_size_t)params_len[i]);
   }

   png_write_chunk_end(png_ptr);
}
#endif

#if defined(PNG_WRITE_pHYs_SUPPORTED)
/* write the pHYs chunk */
void
png_write_pHYs(png_structp png_ptr, png_uint_32 x_pixels_per_unit,
   png_uint_32 y_pixels_per_unit,
   int unit_type)
{
   png_byte buf[9];

   png_debug(1, "in png_write_pHYs\n");
   if (unit_type >= PNG_RESOLUTION_LAST)
      png_warning(png_ptr, "Unrecognized unit type for pHYs chunk");

   png_save_uint_32(buf, x_pixels_per_unit);
   png_save_uint_32(buf + 4, y_pixels_per_unit);
   buf[8] = (png_byte)unit_type;

   png_write_chunk(png_ptr, png_pHYs, buf, (png_size_t)9);
}
#endif

#if defined(PNG_WRITE_tIME_SUPPORTED)
/* write the tIME chunk.  Use either png_convert_from_struct_tm()
   or png_convert_from_time_t(), or fill in the structure yourself */
void
png_write_tIME(png_structp png_ptr, png_timep mod_time)
{
   png_byte buf[7];

   png_debug(1, "in png_write_tIME\n");
   if (mod_time->month  > 12 || mod_time->month  < 1 ||
       mod_time->day    > 31 || mod_time->day    < 1 ||
       mod_time->hour   > 23 || mod_time->second > 60)
   {
      png_warning(png_ptr, "Invalid time specified for tIME chunk");
      return;
   }

   png_save_uint_16(buf, mod_time->year);
   buf[2] = mod_time->month;
   buf[3] = mod_time->day;
   buf[4] = mod_time->hour;
   buf[5] = mod_time->minute;
   buf[6] = mod_time->second;

   png_write_chunk(png_ptr, png_tIME, buf, (png_size_t)7);
}
#endif

/* initializes the row writing capability of libpng */
void
png_write_start_row(png_structp png_ptr)
{
   png_size_t buf_size;

   png_debug(1, "in png_write_start_row\n");
   buf_size = (png_size_t)(((png_ptr->width * png_ptr->usr_channels *
                            png_ptr->usr_bit_depth + 7) >> 3) + 1);

   /* set up row buffer */
   png_ptr->row_buf = (png_bytep)png_malloc(png_ptr, buf_size);
   png_ptr->row_buf[0] = PNG_FILTER_VALUE_NONE;

   /* set up filtering buffer, if using this filter */
   if (png_ptr->do_filter & PNG_FILTER_SUB)
   {
      png_ptr->sub_row = (png_bytep)png_malloc(png_ptr,
         png_ptr->rowbytes + 1);
      png_ptr->sub_row[0] = PNG_FILTER_VALUE_SUB;
   }

   /* We only need to keep the previous row if we are using one of these. */
   if (png_ptr->do_filter & (PNG_FILTER_AVG | PNG_FILTER_UP | PNG_FILTER_PAETH))
   {
     /* set up previous row buffer */
      png_ptr->prev_row = (png_bytep)png_malloc(png_ptr, buf_size);
      png_memset(png_ptr->prev_row, 0, buf_size);

      if (png_ptr->do_filter & PNG_FILTER_UP)
      {
         png_ptr->up_row = (png_bytep )png_malloc(png_ptr,
            png_ptr->rowbytes + 1);
         png_ptr->up_row[0] = PNG_FILTER_VALUE_UP;
      }

      if (png_ptr->do_filter & PNG_FILTER_AVG)
      {
         png_ptr->avg_row = (png_bytep )png_malloc(png_ptr,
            png_ptr->rowbytes + 1);
         png_ptr->avg_row[0] = PNG_FILTER_VALUE_AVG;
      }

      if (png_ptr->do_filter & PNG_FILTER_PAETH)
      {
         png_ptr->paeth_row = (png_bytep )png_malloc(png_ptr,
            png_ptr->rowbytes + 1);
         png_ptr->paeth_row[0] = PNG_FILTER_VALUE_PAETH;
      }
   }

   /* if interlaced, we need to set up width and height of pass */
   if (png_ptr->interlaced)
   {
      if (!(png_ptr->transformations & PNG_INTERLACE))
      {
         png_ptr->num_rows = (png_ptr->height + png_pass_yinc[0] - 1 -
            png_pass_ystart[0]) / png_pass_yinc[0];
         png_ptr->usr_width = (png_ptr->width + png_pass_inc[0] - 1 -
            png_pass_start[0]) / png_pass_inc[0];
      }
      else
      {
         png_ptr->num_rows = png_ptr->height;
         png_ptr->usr_width = png_ptr->width;
      }
   }
   else
   {
      png_ptr->num_rows = png_ptr->height;
      png_ptr->usr_width = png_ptr->width;
   }
   png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
   png_ptr->zstream.next_out = png_ptr->zbuf;
}

/* Internal use only.  Called when finished processing a row of data. */
void
png_write_finish_row(png_structp png_ptr)
{
   int ret;

   png_debug(1, "in png_write_finish_row\n");
   /* next row */
   png_ptr->row_number++;

   /* see if we are done */
   if (png_ptr->row_number < png_ptr->num_rows)
      return;

   /* if interlaced, go to next pass */
   if (png_ptr->interlaced)
   {
      png_ptr->row_number = 0;
      if (png_ptr->transformations & PNG_INTERLACE)
      {
         png_ptr->pass++;
      }
      else
      {
         /* loop until we find a non-zero width or height pass */
         do
         {
            png_ptr->pass++;
            if (png_ptr->pass >= 7)
               break;
            png_ptr->usr_width = (png_ptr->width +
               png_pass_inc[png_ptr->pass] - 1 -
               png_pass_start[png_ptr->pass]) /
               png_pass_inc[png_ptr->pass];
            png_ptr->num_rows = (png_ptr->height +
               png_pass_yinc[png_ptr->pass] - 1 -
               png_pass_ystart[png_ptr->pass]) /
               png_pass_yinc[png_ptr->pass];
            if (png_ptr->transformations & PNG_INTERLACE)
               break;
         } while (png_ptr->usr_width == 0 || png_ptr->num_rows == 0);

      }

      /* reset the row above the image for the next pass */
      if (png_ptr->pass < 7)
      {
         if (png_ptr->prev_row != NULL)
            png_memset(png_ptr->prev_row, 0, 
               (png_size_t) (((png_uint_32)png_ptr->usr_channels *
               (png_uint_32)png_ptr->usr_bit_depth *
               png_ptr->width + 7) >> 3) + 1);
         return;
      }
   }

   /* if we get here, we've just written the last row, so we need
      to flush the compressor */
   do
   {
      /* tell the compressor we are done */
      ret = deflate(&png_ptr->zstream, Z_FINISH);
      /* check for an error */
      if (ret != Z_OK && ret != Z_STREAM_END)
      {
         if (png_ptr->zstream.msg != NULL)
            png_error(png_ptr, png_ptr->zstream.msg);
         else
            png_error(png_ptr, "zlib error");
      }
      /* check to see if we need more room */
      if (!(png_ptr->zstream.avail_out) && ret == Z_OK)
      {
         png_write_IDAT(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size);
         png_ptr->zstream.next_out = png_ptr->zbuf;
         png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
      }
   } while (ret != Z_STREAM_END);

   /* write any extra space */
   if (png_ptr->zstream.avail_out < png_ptr->zbuf_size)
   {
      png_write_IDAT(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size -
         png_ptr->zstream.avail_out);
   }

   deflateReset(&png_ptr->zstream);
}

#if defined(PNG_WRITE_INTERLACING_SUPPORTED)
/* pick out the correct pixels for the interlace pass.

   The basic idea here is to go through the row with a source
   pointer and a destination pointer (sp and dp), and copy the
   correct pixels for the pass.  As the row gets compacted,
   sp will always be >= dp, so we should never overwrite anything.
   See the default: case for the easiest code to understand.
   */
void
png_do_write_interlace(png_row_infop row_info, png_bytep row, int pass)
{
   png_debug(1, "in png_do_write_interlace\n");
   /* we don't have to do anything on the last pass (6) */
#if defined(PNG_USELESS_TESTS_SUPPORTED)
   if (row != NULL && row_info != NULL && pass < 6)
#else
   if (pass < 6)
#endif
   {
      /* each pixel depth is handled seperately */
      switch (row_info->pixel_depth)
      {
         case 1:
         {
            png_bytep sp;
            png_bytep dp;
            int shift;
            int d;
            int value;
            png_uint_32 i;

            dp = row;
            d = 0;
            shift = 7;
            for (i = png_pass_start[pass]; i < row_info->width;
               i += png_pass_inc[pass])
            {
               sp = row + (png_size_t)(i >> 3);
               value = (int)(*sp >> (7 - (int)(i & 7))) & 0x1;
               d |= (value << shift);

               if (shift == 0)
               {
                  shift = 7;
                  *dp++ = (png_byte)d;
                  d = 0;
               }
               else
                  shift--;

            }
            if (shift != 7)
               *dp = (png_byte)d;
            break;
         }
         case 2:
         {
            png_bytep sp;
            png_bytep dp;
            int shift;
            int d;
            int value;
            png_uint_32 i;

            dp = row;
            shift = 6;
            d = 0;
            for (i = png_pass_start[pass]; i < row_info->width;
               i += png_pass_inc[pass])
            {
               sp = row + (png_size_t)(i >> 2);
               value = (*sp >> ((3 - (int)(i & 3)) << 1)) & 0x3;
               d |= (value << shift);

               if (shift == 0)
               {
                  shift = 6;
                  *dp++ = (png_byte)d;
                  d = 0;
               }
               else
                  shift -= 2;
            }
            if (shift != 6)
                   *dp = (png_byte)d;
            break;
         }
         case 4:
         {
            png_bytep sp;
            png_bytep dp;
            int shift;
            int d;
            int value;
            png_uint_32 i;

            dp = row;
            shift = 4;
            d = 0;
            for (i = png_pass_start[pass]; i < row_info->width;
               i += png_pass_inc[pass])
            {
               sp = row + (png_size_t)(i >> 1);
               value = (*sp >> ((1 - (int)(i & 1)) << 2)) & 0xf;
               d |= (value << shift);

               if (shift == 0)
               {
                  shift = 4;
                  *dp++ = (png_byte)d;
                  d = 0;
               }
               else
                  shift -= 4;
            }
            if (shift != 4)
               *dp = (png_byte)d;
            break;
         }
         default:
         {
            png_bytep sp;
            png_bytep dp;
            png_uint_32 i;
            png_size_t pixel_bytes;

            /* start at the beginning */
            dp = row;
            /* find out how many bytes each pixel takes up */
            pixel_bytes = (row_info->pixel_depth >> 3);
            /* loop through the row, only looking at the pixels that
               matter */
            for (i = png_pass_start[pass]; i < row_info->width;
               i += png_pass_inc[pass])
            {
               /* find out where the original pixel is */
               sp = row + i * pixel_bytes;
               /* move the pixel */
               if (dp != sp)
                  png_memcpy(dp, sp, pixel_bytes);
               /* next pixel */
               dp += pixel_bytes;
            }
            break;
         }
      }
      /* set new row width */
      row_info->width = (row_info->width +
         png_pass_inc[pass] - 1 -
         png_pass_start[pass]) /
         png_pass_inc[pass];
      row_info->rowbytes = ((row_info->width *
         row_info->pixel_depth + 7) >> 3);

   }
}
#endif

/* This filters the row, chooses which filter to use, if it has not already
 * been specified by the application, and then writes the row out with the
 * chosen filter. */
#define PNG_MAXSUM (~0x0UL >> 1)
#define PNG_HISHIFT 10
#define PNG_LOMASK 0xffffL
#define PNG_HIMASK (~PNG_LOMASK >> PNG_HISHIFT)
void
png_write_find_filter(png_structp png_ptr, png_row_infop row_info)
{
   png_bytep prev_row, best_row, row_buf;
   png_uint_32 mins, bpp;

   png_debug(1, "in png_write_find_filter\n");
   /* find out how many bytes offset each pixel is */
   bpp = (row_info->pixel_depth + 7) / 8;

   prev_row = png_ptr->prev_row;
   best_row = row_buf = png_ptr->row_buf;
   mins = PNG_MAXSUM;

   /* The prediction method we use is to find which method provides the
    * smallest value when summing the absolute values of the distances
    * from zero using anything >= 128 as negative numbers.  This is known
    * as the "minimum sum of absolute differences" heuristic.  Other
    * heruistics are the "weighted minumum sum of absolute differences"
    * (experimental and can in theory improve compression), and the "zlib
    * predictive" method (not implemented in libpng 0.95), which does test
    * compressions of lines using different filter methods, and then chooses
    * the (series of) filter(s) which give minimum compressed data size (VERY
    * computationally expensive).
    */

   /* We don't need to test the 'no filter' case if this is the only filter
    * that has been chosen, as it doesn't actually do anything to the data.
    */
   if (png_ptr->do_filter & PNG_FILTER_NONE &&
       png_ptr->do_filter != PNG_FILTER_NONE)
   {
      png_bytep rp;
      png_uint_32 sum = 0;
      png_size_t i;
      int v;

      for (i = 0, rp = row_buf + 1; i < row_info->rowbytes; i++, rp++)
      {
         v = *rp;
         sum += (v < 128) ? v : 256 - v;
      }

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 sumhi, sumlo;
         sumlo = sum & PNG_LOMASK;
         sumhi = (sum >> PNG_HISHIFT) & PNG_HIMASK; /* Gives us some footroom */

         /* Reduce the sum if we match any of the previous rows */
         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_NONE)
            {
               sumlo = (sumlo * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               sumhi = (sumhi * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         /* Factor in the cost of this filter (this is here for completeness,
          * but it makes no sense to have a "cost" for the NONE filter, as
          * it has the minimum possible computational cost - none).
          */
         sumlo = (sumlo * png_ptr->filter_costs[PNG_FILTER_VALUE_NONE]) >>
            PNG_COST_SHIFT;
         sumhi = (sumhi * png_ptr->filter_costs[PNG_FILTER_VALUE_NONE]) >>
            PNG_COST_SHIFT;

         if (sumhi > PNG_HIMASK)
            sum = PNG_MAXSUM;
         else
            sum = (sumhi << PNG_HISHIFT) + sumlo;
      }
#endif
      mins = sum;
   }

   /* sub filter */
   if (png_ptr->do_filter & PNG_FILTER_SUB)
   {
      png_bytep rp, dp, lp;
      png_uint_32 sum = 0, lmins = mins;
      png_size_t i;
      int v;

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      /* We temporarily increase the "minumum sum" by the factor we
       * would reduce the sum of this filter, so that we can do the
       * early exit comparison without scaling the sum each time.
       */
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 lmhi, lmlo;
         lmlo = lmins & PNG_LOMASK;
         lmhi = (lmins >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_VALUE_SUB)
            {
               lmlo = (lmlo * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               lmhi = (lmhi * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         lmlo = (lmlo * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_SUB]) >>
            PNG_COST_SHIFT;
         lmhi = (lmhi * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_SUB]) >>
            PNG_COST_SHIFT;

         if (lmhi > PNG_HIMASK)
            lmins = PNG_MAXSUM;
         else
            lmins = (lmhi << PNG_HISHIFT) + lmlo;
      }
#endif

      for (i = 0, rp = row_buf + 1, dp = png_ptr->sub_row + 1; i < bpp;
           i++, rp++, dp++)
      {
         v = *dp = *rp;

         sum += (v < 128) ? v : 256 - v;
      }
      for (lp = row_buf + 1; i < row_info->rowbytes; i++, rp++, lp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (int)*lp) & 0xff);

         sum += (v < 128) ? v : 256 - v;

         if (sum > lmins)  /* We are already worse, don't continue. */
            break;
      }

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 sumhi, sumlo;
         sumlo = sum & PNG_LOMASK;
         sumhi = (sum >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_VALUE_SUB)
            {
               sumlo = (sumlo * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               sumhi = (sumhi * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         sumlo = (sumlo * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_SUB]) >>
            PNG_COST_SHIFT;
         sumhi = (sumhi * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_SUB]) >>
            PNG_COST_SHIFT;

         if (sumhi > PNG_HIMASK)
            sum = PNG_MAXSUM;
         else
            sum = (sumhi << PNG_HISHIFT) + sumlo;
      }
#endif

      if (sum < mins)
      {
         mins = sum;
         best_row = png_ptr->sub_row;
      }
   }

   /* up filter */
   if (png_ptr->do_filter & PNG_FILTER_UP)
   {
      png_bytep rp, dp, pp;
      png_uint_32 sum = 0, lmins = mins;
      png_size_t i;
      int v;

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 lmhi, lmlo;
         lmlo = lmins & PNG_LOMASK;
         lmhi = (lmins >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_VALUE_UP)
            {
               lmlo = (lmlo * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               lmhi = (lmhi * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         lmlo = (lmlo * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_UP]) >>
            PNG_COST_SHIFT;
         lmhi = (lmhi * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_UP]) >>
            PNG_COST_SHIFT;

         if (lmhi > PNG_HIMASK)
            lmins = PNG_MAXSUM;
         else
            lmins = (lmhi << PNG_HISHIFT) + lmlo;
      }
#endif

      for (i = 0, rp = row_buf + 1, dp = png_ptr->up_row + 1,
           pp = prev_row + 1; i < row_info->rowbytes; i++, rp++, pp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (int)*pp) & 0xff);

         sum += (v < 128) ? v : 256 - v;

         if (sum > lmins)  /* We are already worse, don't continue. */
            break;
      }

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 sumhi, sumlo;
         sumlo = sum & PNG_LOMASK;
         sumhi = (sum >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_UP)
            {
               sumlo = (sumlo * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               sumhi = (sumhi * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         sumlo = (sumlo * png_ptr->filter_costs[PNG_FILTER_VALUE_UP]) >>
            PNG_COST_SHIFT;
         sumhi = (sumhi * png_ptr->filter_costs[PNG_FILTER_VALUE_UP]) >>
            PNG_COST_SHIFT;

         if (sumhi > PNG_HIMASK)
            sum = PNG_MAXSUM;
         else
            sum = (sumhi << PNG_HISHIFT) + sumlo;
      }
#endif

      if (sum < mins)
      {
         mins = sum;
         best_row = png_ptr->up_row;
      }
   }

   /* avg filter */
   if (png_ptr->do_filter & PNG_FILTER_AVG)
   {
      png_bytep rp, dp, pp, lp;
      png_uint_32 sum = 0, lmins = mins;
      png_size_t i;
      int v;

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 lmhi, lmlo;
         lmlo = lmins & PNG_LOMASK;
         lmhi = (lmins >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_VALUE_AVG)
            {
               lmlo = (lmlo * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               lmhi = (lmhi * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         lmlo = (lmlo * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_AVG]) >>
            PNG_COST_SHIFT;
         lmhi = (lmhi * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_AVG]) >>
            PNG_COST_SHIFT;

         if (lmhi > PNG_HIMASK)
            lmins = PNG_MAXSUM;
         else
            lmins = (lmhi << PNG_HISHIFT) + lmlo;
      }
#endif

      for (i = 0, rp = row_buf + 1, dp = png_ptr->avg_row + 1,
           pp = prev_row + 1; i < bpp; i++, rp++, pp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - ((int)*pp / 2)) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      for (lp = row_buf + 1; i < row_info->rowbytes;
           i++, rp++, pp++, lp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (((int)*pp + (int)*lp) / 2)) & 0xff);

         sum += (v < 128) ? v : 256 - v;

         if (sum > lmins)  /* We are already worse, don't continue. */
            break;
      }

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 sumhi, sumlo;
         sumlo = sum & PNG_LOMASK;
         sumhi = (sum >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_NONE)
            {
               sumlo = (sumlo * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               sumhi = (sumhi * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         sumlo = (sumlo * png_ptr->filter_costs[PNG_FILTER_VALUE_AVG]) >>
            PNG_COST_SHIFT;
         sumhi = (sumhi * png_ptr->filter_costs[PNG_FILTER_VALUE_AVG]) >>
            PNG_COST_SHIFT;

         if (sumhi > PNG_HIMASK)
            sum = PNG_MAXSUM;
         else
            sum = (sumhi << PNG_HISHIFT) + sumlo;
      }
#endif

      if (sum < mins)
      {
         mins = sum;
         best_row = png_ptr->avg_row;
      }
   }

   /* Paeth filter */
   if (png_ptr->do_filter & PNG_FILTER_PAETH)
   {
      png_bytep rp, dp, pp, cp, lp;
      png_uint_32 sum = 0, lmins = mins;
      png_size_t i;
      int v;

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 lmhi, lmlo;
         lmlo = lmins & PNG_LOMASK;
         lmhi = (lmins >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_VALUE_PAETH)
            {
               lmlo = (lmlo * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               lmhi = (lmhi * png_ptr->inv_filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         lmlo = (lmlo * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_PAETH]) >>
            PNG_COST_SHIFT;
         lmhi = (lmhi * png_ptr->inv_filter_costs[PNG_FILTER_VALUE_PAETH]) >>
            PNG_COST_SHIFT;

         if (lmhi > PNG_HIMASK)
            lmins = PNG_MAXSUM;
         else
            lmins = (lmhi << PNG_HISHIFT) + lmlo;
      }
#endif

      for (i = 0, rp = row_buf + 1, dp = png_ptr->paeth_row + 1,
           pp = prev_row + 1; i < bpp; i++, rp++, pp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (int)*pp) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      for (lp = row_buf + 1, cp = prev_row + 1; i < row_info->rowbytes;
           i++, rp++, pp++, lp++, dp++, cp++)
      {
         int a, b, c, pa, pb, pc, p;

         b = *pp;
         c = *cp;
         a = *lp;

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

         v = *dp = (png_byte)(((int)*rp - p) & 0xff);

         sum += (v < 128) ? v : 256 - v;

         if (sum > lmins)  /* We are already worse, don't continue. */
            break;
      }

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
      if (png_ptr->heuristic_method == PNG_FILTER_HEURISTIC_WEIGHTED)
      {
         png_uint_32 sumhi, sumlo;
         sumlo = sum & PNG_LOMASK;
         sumhi = (sum >> PNG_HISHIFT) & PNG_HIMASK;

         for (i = 0; i < png_ptr->num_prev_filters; i++)
         {
            if (png_ptr->prev_filters[i] == PNG_FILTER_PAETH)
            {
               sumlo = (sumlo * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
               sumhi = (sumhi * png_ptr->filter_weights[i]) >>
                  PNG_WEIGHT_SHIFT;
            }
         }

         sumlo = (sumlo * png_ptr->filter_costs[PNG_FILTER_VALUE_PAETH]) >>
            PNG_COST_SHIFT;
         sumhi = (sumhi * png_ptr->filter_costs[PNG_FILTER_VALUE_PAETH]) >>
            PNG_COST_SHIFT;

         if (sumhi > PNG_HIMASK)
            sum = PNG_MAXSUM;
         else
            sum = (sumhi << PNG_HISHIFT) + sumlo;
      }
#endif

      if (sum < mins)
      {
         best_row = png_ptr->paeth_row;
      }
   }

   /* Do the actual writing of the filtered row data from the chosen filter. */
   png_write_filtered_row(png_ptr, best_row);

#if defined(PNG_WRITE_WEIGHTED_FILTER_SUPPORTED)
   /* Save the type of filter we picked this time for future calculations */
   if (png_ptr->num_prev_filters > 0)
   {
      png_byte i;

      for (i = 1; i < png_ptr->num_prev_filters; i++)
      {
         png_ptr->prev_filters[i] = png_ptr->prev_filters[i - 1];
      }
      png_ptr->prev_filters[i] = best_row[0];
   }
#endif
}


/* Do the actual writing of a previously filtered row. */
void
png_write_filtered_row(png_structp png_ptr, png_bytep filtered_row)
{
   png_debug(1, "in png_write_filtered_row\n");
   png_debug1(2, "filter = %d\n", filtered_row[0]);
   /* set up the zlib input buffer */
   png_ptr->zstream.next_in = filtered_row;
   png_ptr->zstream.avail_in = (uInt)png_ptr->row_info.rowbytes + 1;
   /* repeat until we have compressed all the data */
   do
   {
      int ret; /* return of zlib */

      /* compress the data */
      ret = deflate(&png_ptr->zstream, Z_NO_FLUSH);
      /* check for compression errors */
      if (ret != Z_OK)
      {
         if (png_ptr->zstream.msg != NULL)
            png_error(png_ptr, png_ptr->zstream.msg);
         else
            png_error(png_ptr, "zlib error");
      }

      /* see if it is time to write another IDAT */
      if (!(png_ptr->zstream.avail_out))
      {
         /* write the IDAT and reset the zlib output buffer */
         png_write_IDAT(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size);
         png_ptr->zstream.next_out = png_ptr->zbuf;
         png_ptr->zstream.avail_out = (uInt)png_ptr->zbuf_size;
      }
   /* repeat until all data has been compressed */
   } while (png_ptr->zstream.avail_in);

   /* swap the current and previous rows */
   if (png_ptr->prev_row != NULL)
   {
      png_bytep tptr;

      tptr = png_ptr->prev_row;
      png_ptr->prev_row = png_ptr->row_buf;
      png_ptr->row_buf = tptr;
   }

   /* finish row - updates counters and flushes zlib if last row */
   png_write_finish_row(png_ptr);

#if defined(PNG_WRITE_FLUSH_SUPPORTED)
   png_ptr->flush_rows++;

   if (png_ptr->flush_dist > 0 &&
       png_ptr->flush_rows >= png_ptr->flush_dist)
   {
      png_write_flush(png_ptr);
   }
#endif /* PNG_WRITE_FLUSH_SUPPORTED */
}

