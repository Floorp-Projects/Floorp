
/* pngget.c - retrieval of values from info struct

   libpng 1.0 beta 5 - version 0.95
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1996, 1997 Andreas Dilger
   March 30, 1997
   */

#define PNG_INTERNAL
#include "png.h"

png_uint_32
png_get_valid(png_structp png_ptr, png_infop info_ptr, png_uint_32 flag)
{
   if (info_ptr != NULL)
      return(info_ptr->valid & flag);
   else
      return(0);
}

png_uint_32
png_get_rowbytes(png_structp png_ptr, png_infop info_ptr)
{
   if (info_ptr != NULL)
      return(info_ptr->rowbytes);
   else
      return(0);
}

png_byte
png_get_channels(png_structp png_ptr, png_infop info_ptr)
{
   if (info_ptr != NULL)
      return(info_ptr->channels);
   else
      return(0);
}

png_bytep
png_get_signature(png_structp png_ptr, png_infop info_ptr)
{
   if (info_ptr != NULL)
      return(info_ptr->signature);
   else
      return(NULL);
}

#if defined(PNG_READ_bKGD_SUPPORTED)
png_uint_32
png_get_bKGD(png_structp png_ptr, png_infop info_ptr,
   png_color_16p *background)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_bKGD &&
      background != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "bKGD");
      *background = &(info_ptr->background);
      return (PNG_INFO_bKGD);
   }
   return (0);
}
#endif

#if defined(PNG_READ_cHRM_SUPPORTED)
png_uint_32
png_get_cHRM(png_structp png_ptr, png_infop info_ptr,
   double *white_x, double *white_y, double *red_x, double *red_y,
   double *green_x, double *green_y, double *blue_x, double *blue_y)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_cHRM)
   {
      png_debug1(1, "in %s retrieval function\n", "cHRM");
      if (white_x != NULL)
         *white_x = (double)info_ptr->x_white;
      if (white_y != NULL)
         *white_y = (double)info_ptr->y_white;
      if (red_x != NULL)
         *red_x = (double)info_ptr->x_red;
      if (red_y != NULL)
         *red_y = (double)info_ptr->y_red;
      if (green_x != NULL)
         *green_x = (double)info_ptr->x_green;
      if (green_y != NULL)
         *green_y = (double)info_ptr->y_green;
      if (blue_x != NULL)
         *blue_x = (double)info_ptr->x_blue;
      if (blue_y != NULL)
         *blue_y = (double)info_ptr->y_blue;
      return (PNG_INFO_cHRM);
   }
   return (0);
}
#endif

#if defined(PNG_READ_gAMA_SUPPORTED)
png_uint_32
png_get_gAMA(png_structp png_ptr, png_infop info_ptr, double *file_gamma)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_gAMA &&
      file_gamma != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "gAMA");
      *file_gamma = (double)info_ptr->gamma;
      return (PNG_INFO_gAMA);
   }
   return (0);
}
#endif

#if defined(PNG_READ_hIST_SUPPORTED)
png_uint_32
png_get_hIST(png_structp png_ptr, png_infop info_ptr, png_uint_16p *hist)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_hIST && hist != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "hIST");
      *hist = info_ptr->hist;
      return (PNG_INFO_hIST);
   }
   return (0);
}
#endif

png_uint_32
png_get_IHDR(png_structp png_ptr, png_infop info_ptr,
   png_uint_32 *width, png_uint_32 *height, int *bit_depth,
   int *color_type, int *interlace_type, int *compression_type,
   int *filter_type)
   
{
   if (info_ptr != NULL && width != NULL && height != NULL &&
      bit_depth != NULL && color_type != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "IHDR");
      *width = info_ptr->width;
      *height = info_ptr->height;
      *bit_depth = info_ptr->bit_depth;
      *color_type = info_ptr->color_type;
      if (compression_type != NULL)
         *compression_type = info_ptr->compression_type;
      if (filter_type != NULL)
         *filter_type = info_ptr->filter_type;
      if (interlace_type != NULL)
         *interlace_type = info_ptr->interlace_type;
      return (1);
   }
   return (0);
}

#if defined(PNG_READ_oFFs_SUPPORTED)
png_uint_32
png_get_oFFs(png_structp png_ptr, png_infop info_ptr,
   png_uint_32 *offset_x, png_uint_32 *offset_y, int *unit_type)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_oFFs &&
      offset_x != NULL && offset_y != NULL && unit_type != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "oFFs");
      *offset_x = info_ptr->x_offset;
      *offset_y = info_ptr->y_offset;
      *unit_type = (int)info_ptr->offset_unit_type;
      return (PNG_INFO_oFFs);
   }
   return (0);
}
#endif

#if defined(PNG_READ_pCAL_SUPPORTED)
png_uint_32
png_get_pCAL(png_structp png_ptr, png_infop info_ptr,
   png_charp *purpose, png_int_32 *X0, png_int_32 *X1, int *type, int *nparams,
   png_charp *units, png_charpp *params)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_pCAL &&
      purpose != NULL && X0 != NULL && X1 != NULL && type != NULL &&
      nparams != NULL && units != NULL && params != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "pCAL");
      *purpose = info_ptr->pcal_purpose;
      *X0 = info_ptr->pcal_X0;
      *X1 = info_ptr->pcal_X1;
      *type = (int)info_ptr->pcal_type;
      *nparams = (int)info_ptr->pcal_nparams;
      *units = info_ptr->pcal_units;
      *params = info_ptr->pcal_params;
      return (PNG_INFO_pCAL);
   }
   return (0);
}
#endif

#if defined(PNG_READ_pHYs_SUPPORTED)
png_uint_32
png_get_pHYs(png_structp png_ptr, png_infop info_ptr,
   png_uint_32 *res_x, png_uint_32 *res_y, int *unit_type)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_pHYs &&
      res_x != NULL && res_y != NULL && unit_type != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "pHYs");
      *res_x = info_ptr->x_pixels_per_unit;
      *res_y = info_ptr->y_pixels_per_unit;
      *unit_type = (int)info_ptr->phys_unit_type;
      return (PNG_INFO_pHYs);
   }
   return (0);
}
#endif

png_uint_32
png_get_PLTE(png_structp png_ptr, png_infop info_ptr, png_colorp *palette,
   int *num_palette)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_PLTE && palette != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "PLTE");
      *palette = info_ptr->palette;
      *num_palette = info_ptr->num_palette;
      png_debug1(3, "num_palette = %d\n", *num_palette);
      return (PNG_INFO_PLTE);
   }
   return (0);
}

#if defined(PNG_READ_sBIT_SUPPORTED)
png_uint_32
png_get_sBIT(png_structp png_ptr, png_infop info_ptr, png_color_8p *sig_bit)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_sBIT && sig_bit != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "sBIT");
      *sig_bit = &(info_ptr->sig_bit);
      return (PNG_INFO_sBIT);
   }
   return (0);
}
#endif

#if defined(PNG_READ_tEXt_SUPPORTED) || defined(PNG_READ_zTXt_SUPPORTED)
png_uint_32
png_get_text(png_structp png_ptr, png_infop info_ptr, png_textp *text_ptr,
   int *num_text)
{
   if ((info_ptr != NULL) || (info_ptr->num_text > 0))
   {
      png_debug1(1, "in %s retrieval function\n",
	 (png_ptr->chunk_name[0] == '\0' ? "text" : png_ptr->chunk_name));
      if (text_ptr != NULL)
         *text_ptr = info_ptr->text;
      if (num_text != NULL)
         *num_text = info_ptr->num_text;
      return (info_ptr->num_text);
   }
   return(0);
}
#endif

#if defined(PNG_READ_tIME_SUPPORTED)
png_uint_32
png_get_tIME(png_structp png_ptr, png_infop info_ptr, png_timep *mod_time)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_tIME && mod_time != NULL)
   {
      png_debug1(1, "in %s retrieval function\n", "tIME");
      *mod_time = &(info_ptr->mod_time);
      return (PNG_INFO_tIME);
   }
   return (0);
}
#endif

#if defined(PNG_READ_tRNS_SUPPORTED)
png_uint_32
png_get_tRNS(png_structp png_ptr, png_infop info_ptr,
   png_bytep *trans, int *num_trans, png_color_16p *trans_values)
{
   if (info_ptr != NULL && info_ptr->valid & PNG_INFO_tRNS)
   {
      png_debug1(1, "in %s retrieval function\n", "tRNS");
      if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE && trans != NULL)
      {
          *trans = info_ptr->trans;
      }
      else if (trans_values != NULL)
      {
         *trans_values = &(info_ptr->trans_values);
      }
      else
      {
         return (0);
      }
      *num_trans = info_ptr->num_trans;
      return (PNG_INFO_tRNS);
   }
   return (0);
}
#endif

