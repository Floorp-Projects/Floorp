
/* png.c - location for general purpose png functions

   libpng 1.0 beta 5 - version 0.95
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   Copyright (c) 1996, 1997 Andreas Dilger
   March 30, 1997
   */

#define PNG_INTERNAL
#define PNG_NO_EXTERN
#include "png.h"

/* Version information for C files.  This had better match the version
   string defined in png.h */
char png_libpng_ver[] = "0.95";

/* Place to hold the signiture string for a PNG file. */
png_byte FARDATA png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};

/* constant strings for known chunk types.  If you need to add a chunk,
   add a string holding the name here.  If you want to make the code
   portable to EBCDIC machines, use ASCII numbers, not characters. */
png_byte FARDATA png_IHDR[5] = { 73,  72,  68,  82, '\0'};
png_byte FARDATA png_IDAT[5] = { 73,  68,  65,  84, '\0'};
png_byte FARDATA png_IEND[5] = { 73,  69,  78,  68, '\0'};
png_byte FARDATA png_PLTE[5] = { 80,  76,  84,  69, '\0'};
png_byte FARDATA png_bKGD[5] = { 98,  75,  71,  68, '\0'};
png_byte FARDATA png_cHRM[5] = { 99,  72,  82,  77, '\0'};
png_byte FARDATA png_gAMA[5] = {103,  65,  77,  65, '\0'};
png_byte FARDATA png_hIST[5] = {104,  73,  83,  84, '\0'};
png_byte FARDATA png_oFFs[5] = {111,  70,  70, 115, '\0'};
png_byte FARDATA png_pCAL[5] = {112,  67,  65,  76, '\0'};
png_byte FARDATA png_pHYs[5] = {112,  72,  89, 115, '\0'};
png_byte FARDATA png_sBIT[5] = {115,  66,  73,  84, '\0'};
png_byte FARDATA png_tEXt[5] = {116,  69,  88, 116, '\0'};
png_byte FARDATA png_tIME[5] = {116,  73,  77,  69, '\0'};
png_byte FARDATA png_tRNS[5] = {116,  82,  78,  83, '\0'};
png_byte FARDATA png_zTXt[5] = {122,  84,  88, 116, '\0'};

/* arrays to facilitate easy interlacing - use pass (0 - 6) as index */

/* start of interlace block */
int FARDATA png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};

/* offset to next interlace block */
int FARDATA png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};

/* start of interlace block in the y direction */
int FARDATA png_pass_ystart[] = {0, 0, 4, 0, 2, 0, 1};

/* offset to next interlace block in the y direction */
int FARDATA png_pass_yinc[] = {8, 8, 8, 4, 4, 2, 2};

/* width of interlace block */
/* this is not currently used - if you need it, uncomment it here and
   in png.h
int FARDATA png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
*/

/* height of interlace block */
/* this is not currently used - if you need it, uncomment it here and
   in png.h
int FARDATA png_pass_height[] = {8, 8, 4, 4, 2, 2, 1};
*/

/* mask to determine which pixels are valid in a pass */
int FARDATA png_pass_mask[] = {0x80, 0x08, 0x88, 0x22, 0xaa, 0x55, 0xff};

/* mask to determine which pixels to overwrite while displaying */
int FARDATA png_pass_dsp_mask[] = {0xff, 0x0f, 0xff, 0x33, 0xff, 0x55, 0xff};


/* Tells libpng that we have already handled the first "num_bytes" bytes
 * of the PNG file signature.  If the PNG data is embedded into another
 * stream we can set num_bytes = 8 so that libpng will not attempt to read
 * or write any of the magic bytes before it starts on the IHDR.
 */
void
png_set_sig_bytes(png_structp png_ptr, int num_bytes)
{
   png_debug(1, "in png_set_sig_bytes\n");
   if (num_bytes > 8)
      png_error(png_ptr, "Too many bytes for PNG signature.");

   png_ptr->sig_bytes = num_bytes < 0 ? 0 : num_bytes;
}

/* Checks whether the supplied bytes match the PNG signature.  We allow
 * checking less than the full 8-byte signature so that those apps that
 * already read the first few bytes of a file to determine the file type
 * can simply check the remaining bytes for extra assurance.  Returns
 * an integer less than, equal to, or greater than zero if sig is found,
 * respectively, to be less than, to match, or be greater than the correct
 * PNG signature (this is the same behaviour as strcmp, memcmp, etc).
 */
int
png_sig_cmp(png_bytep sig, png_size_t start, png_size_t num_to_check)
{
   if (num_to_check > 8)
      num_to_check = 8;
   else if (num_to_check < 1)
      return 0;

   if (start > 7)
      return 0;

   if (start + num_to_check > 8)
      num_to_check = 8 - start;

   return (png_memcmp(&sig[start], &png_sig[start], num_to_check));
}

/* (Obsolete) function to check signature bytes.  It does not allow one
   to check a partial signature.  This function will be removed in the
   future - use png_sig_cmp(). */
int
png_check_sig(png_bytep sig, int num)
{
  return !png_sig_cmp(sig, (png_size_t)0, (png_size_t)num);
}

/* Function to allocate memory for zlib. */
voidpf
png_zalloc(voidpf png_ptr, uInt items, uInt size)
{
   png_voidp ptr;
   png_uint_32 num_bytes;

   num_bytes = (png_uint_32)items * size;
   ptr = png_malloc((png_structp)png_ptr, num_bytes);
   if (num_bytes > (png_uint_32)0x8000)
   {
      png_memset(ptr, 0, (png_size_t)0x8000L);
      png_memset((png_bytep)ptr + (png_size_t)0x8000L, 0,
         (png_size_t)(num_bytes - (png_uint_32)0x8000L));
   }
   else
   {
      png_memset(ptr, 0, (png_size_t)num_bytes);
   }
   return (voidpf)(ptr);
}

/* function to free memory for zlib */
void
png_zfree(voidpf png_ptr, voidpf ptr)
{
   png_free((png_structp)png_ptr, (png_voidp)ptr);
}

/* Reset the CRC variable to 32 bits of 1's.  Care must be taken
   in case CRC is > 32 bits to leave the top bits 0. */
void
png_reset_crc(png_structp png_ptr)
{
   /* set CRC to all 1's */
#ifdef PNG_USE_OWN_CRC
   png_ptr->crc = 0xffffffffL;
#else
   png_ptr->crc = crc32(0, Z_NULL, 0);
#endif
}

#ifdef PNG_USE_OWN_CRC
/* Table of CRCs of all 8-bit messages.  By default, we use the tables made
   by zlib, to save some memory.  If you wish to png_malloc() this
   table, turn this into a pointer, and png_malloc() it in make_crc_table().
   You may then want to hook it into png_struct and free it with the
   destroy functions.  Another alternative is to pre-fill the table.  */
static png_uint_32 crc_table[256];

/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;

/* make the table for a fast crc */
static void
make_crc_table(void)
{
  png_uint_32 c;
  int n, k;

  for (n = 0; n < 256; n++)
  {
   c = (png_uint_32)n;
   for (k = 0; k < 8; k++)
     c = c & 1 ? 0xedb88320L ^ (c >> 1) : c >> 1;
   crc_table[n] = c;
  }
  crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1] - the CRC should be
   initialized to all 1's, and the transmitted value is the 1's complement
   of the final running CRC. */
static png_uint_32
update_crc(png_uint_32 crc, png_bytep buf, png_size_t len)
{
  png_uint_32 c;
  png_bytep p;
  png_uint_32 n;

  c = crc;
  p = buf;
  n = len;

  if (!crc_table_computed)
  {
   make_crc_table();
  }

  if (n > 0) do
  {
   c = crc_table[(png_byte)((c ^ (*p++)) & 0xff)] ^ (c >> 8);
  } while (--n);

  return c;
}
#endif /* PNG_USE_OWN_CRC */

/* Calculate the CRC over a section of data.  We can only pass as
   much data to this routine as the largest single buffer size.  We
   also check that this data will actually be used before going to the
   trouble of calculating it. */
void
png_calculate_crc(png_structp png_ptr, png_bytep ptr, png_size_t length)
{
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

   if (need_crc)
#ifdef PNG_USE_OWN_CRC
      png_ptr->crc = update_crc(png_ptr->crc, ptr, length);
#else
      png_ptr->crc = crc32(png_ptr->crc, ptr, length);
#endif
}

/* Allocate the memory for an info_struct for the application.  We don't
   really need the png_ptr, but it could potentially be useful in the
   future.  This should be used in favour of malloc(sizeof(png_info))
   and png_info_init() so that applications that want to use a shared
   libpng don't have to be recompiled if png_info changes size. */
png_infop
png_create_info_struct(png_structp png_ptr)
{
   png_infop info_ptr;

   png_debug(1, "in png_create_info_struct\n");
   if ((info_ptr = (png_infop)png_create_struct(PNG_STRUCT_INFO)) != NULL)
   {
      png_info_init(info_ptr);
   }

   return info_ptr;
}

/* This function frees the memory associated with a single info struct.
   Normally, one would use either png_destroy_read_struct() or
   png_destroy_write_struct() to free an info struct, but this may be
   useful for some applications. */
void
png_destroy_info_struct(png_structp png_ptr, png_infopp info_ptr_ptr)
{
   png_infop info_ptr = NULL;

   png_debug(1, "in png_destroy_info_struct\n");
   if (info_ptr_ptr != NULL)
      info_ptr = *info_ptr_ptr;

   if (info_ptr != NULL)
   {
      png_info_destroy(png_ptr, info_ptr);

      png_destroy_struct((png_voidp)info_ptr);
      *info_ptr_ptr = (png_infop)NULL;
   }
}

/* Initialize the info structure.  This is now an internal function (0.89)
   and applications using it are urged to use png_create_info_struct()
   instead. */
void
png_info_init(png_infop info_ptr)
{
   png_debug(1, "in png_info_init\n");
   /* set everything to 0 */
   png_memset(info_ptr, 0, sizeof (png_info));
}

/* This is an internal routine to free any memory that the info struct is
 * pointing to before re-using it or freeing the struct itself.  Recall
 * that png_free() checks for NULL pointers for us.
 */
void
png_info_destroy(png_structp png_ptr, png_infop info_ptr)
{
#if defined(PNG_READ_tEXt_SUPPORTED) || defined(PNG_READ_zTXt_SUPPORTED)
   int i;

   png_debug(1, "in png_info_destroy\n");
   if (info_ptr->text != NULL)
   {
      for (i = 0; i < info_ptr->num_text; i++)
      {
         png_free(png_ptr, info_ptr->text[i].key);
      }
      png_free(png_ptr, info_ptr->text);
   }
#endif
#if defined(PNG_READ_pCAL_SUPPORTED)
   png_free(png_ptr, info_ptr->pcal_purpose);
   png_free(png_ptr, info_ptr->pcal_units);
   if (info_ptr->pcal_params != NULL)
   {
      for (i = 0; i < info_ptr->pcal_nparams; i++)
      {
         png_free(png_ptr, info_ptr->pcal_params[i]);
      }
      png_free(png_ptr, info_ptr->pcal_params);
   }
#endif

   png_info_init(info_ptr);
}

/* This function returns a pointer to the io_ptr associated with the user
   functions.  The application should free any memory associated with this
   pointer before png_write_destroy() or png_read_destroy() are called. */
png_voidp
png_get_io_ptr(png_structp png_ptr)
{
   return png_ptr->io_ptr;
}

#if !defined(PNG_NO_STDIO)
/* Initialize the default input/output functions for the PNG file.  If you
   use your own read or write routines, you can call either png_set_read_fn()
   or png_set_write_fn() instead of png_init_io(). */
void
png_init_io(png_structp png_ptr, FILE *fp)
{
   png_debug(1, "in png_init_io\n");
   png_ptr->io_ptr = (png_voidp)fp;
}
#endif
