/*
  dirac.c
*/

#include "config.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "dirac.h"


typedef struct
dirac_bs_s
{
  uint8_t *p_start;
  uint8_t *p;
  uint8_t *p_end;

  int     i_left;    /* i_count number of available bits */
} dirac_bs_t;

static inline void
dirac_bs_init( dirac_bs_t *s, void *p_data, int i_data )
{
  s->p_start = p_data;
  s->p       = p_data;
  s->p_end   = s->p + i_data;
  s->i_left  = 8;
}

static inline ogg_uint32_t
dirac_bs_read( dirac_bs_t *s, int i_count )
{
   static ogg_uint32_t i_mask[33] =
   {  0x00,
      0x01,      0x03,      0x07,      0x0f,
      0x1f,      0x3f,      0x7f,      0xff,
      0x1ff,     0x3ff,     0x7ff,     0xfff,
      0x1fff,    0x3fff,    0x7fff,    0xffff,
      0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
      0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
      0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
      0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
  int      i_shr;
  ogg_uint32_t i_result = 0;

  while( i_count > 0 )
  {
    if( s->p >= s->p_end )
    {
      break;
    }

    if( ( i_shr = s->i_left - i_count ) >= 0 )
    {
      /* more in the buffer than requested */
      i_result |= ( *s->p >> i_shr )&i_mask[i_count];
      s->i_left -= i_count;
      if( s->i_left == 0 )
      {
        s->p++;
        s->i_left = 8;
      }
      return( i_result );
    }
    else
    {
        /* less in the buffer than requested */
       i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
       i_count  -= s->i_left;
       s->p++;
       s->i_left = 8;
    }
  }

  return( i_result );
}

static inline void
dirac_bs_skip( dirac_bs_t *s, int i_count )
{
  s->i_left -= i_count;

  while( s->i_left <= 0 )
  {
    s->p++;
    s->i_left += 8;
  }
}

static ogg_uint32_t
dirac_uint ( dirac_bs_t *p_bs )
{
  ogg_uint32_t count = 0, value = 0;
  while( !dirac_bs_read ( p_bs, 1 ) ) {
    count++;
    value <<= 1;
    value |= dirac_bs_read ( p_bs, 1 );
  }

  return (1<<count) - 1 + value;
}

static int
dirac_bool ( dirac_bs_t *p_bs )
{
  return dirac_bs_read ( p_bs, 1 );
}

int
dirac_parse_info (dirac_info *info, unsigned char * data, long len)
{
  dirac_bs_t bs;
  ogg_uint32_t video_format;

  static const struct {
    ogg_uint32_t fps_numerator, fps_denominator;
  } dirac_frate_tbl[] = { /* table 10.3 */
    {1,1}, /* this first value is never used */
    {24000,1001}, {24,1}, {25,1}, {30000,1001}, {30,1},
    {50,1}, {60000,1001}, {60,1}, {15000,1001}, {25,2}
  };

  static const ogg_uint32_t dirac_vidfmt_frate[] = { /* table C.1 */
    1, 9, 10, 9, 10, 9, 10, 4, 3, 7, 6, 4, 3, 7, 6, 2, 2, 7, 6, 7, 6
  };

  static const int dirac_source_sampling[] = { /* extracted from table C.1 */
    0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0
  };
  static const int dirac_top_field_first[] = { /* from table C.1 */
    0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
  };

  static const struct {
    ogg_uint32_t width, height;
  } dirac_fsize_tbl[] = { /* table 10.3 framesize */
    {640,460}, {24,1}, {176,120}, {352,240}, {352,288},
    {704,480}, {704,576}, {720,480}, {720,576},
    {1280, 720}, {1280, 720}, {1920, 1080}, {1920, 1080},
    {1920, 1080}, {1920, 1080}, {2048, 1080}, {4096, 2160}
  };

  /* read in useful bits from sequence header */
  dirac_bs_init( &bs, data, len);
  dirac_bs_skip( &bs, 13*8); /* parse_info_header */
  info->major_version = dirac_uint( &bs ); /* major_version */
  info->minor_version = dirac_uint( &bs ); /* minor_version */
  info->profile = dirac_uint( &bs ); /* profile */
  info->level = dirac_uint( &bs ); /* level */
  info->video_format = video_format = dirac_uint( &bs ); /* index */

  if (video_format >= (sizeof(dirac_fsize_tbl) / sizeof(dirac_fsize_tbl[0]))) {
    return -1; 
  }

  info->width = dirac_fsize_tbl[video_format].width;
  info->height = dirac_fsize_tbl[video_format].height;
  if (dirac_bool( &bs )) {
    info->width = dirac_uint( &bs ); /* frame_width */
    info->height = dirac_uint( &bs ); /* frame_height */
  }

  if (dirac_bool( &bs )) {
    info->chroma_format = dirac_uint( &bs ); /* chroma_format */
  }

  if (dirac_bool( &bs )) { /* custom_scan_format_flag */
    int scan_format = dirac_uint( &bs ); /* scan_format */
    if (scan_format < 2) {
      info->interlaced = scan_format;
    } else { /* other scan_format values are reserved */
      info->interlaced = 0;
    }
  } else { /* no custom scan_format, use the preset value */
    info->interlaced = dirac_source_sampling[video_format];
  }
  /* field order is set by video_format and cannot be custom */
  info->top_field_first = dirac_top_field_first[video_format];

  info->fps_numerator = dirac_frate_tbl[dirac_vidfmt_frate[video_format]].fps_numerator;
  info->fps_denominator = dirac_frate_tbl[dirac_vidfmt_frate[video_format]].fps_denominator;
  if (dirac_bool( &bs )) {
    ogg_uint32_t frame_rate_index = dirac_uint( &bs );
    info->fps_numerator = dirac_frate_tbl[frame_rate_index].fps_numerator;
    info->fps_denominator = dirac_frate_tbl[frame_rate_index].fps_denominator;
    if (frame_rate_index == 0) {
      info->fps_numerator = dirac_uint( &bs );
      info->fps_denominator = dirac_uint( &bs );
    }
  }

  return 0;
}
