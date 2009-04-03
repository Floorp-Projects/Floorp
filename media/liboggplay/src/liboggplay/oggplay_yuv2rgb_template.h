#ifndef __OGGPLAY_YUV2RGB_TEMPLATE_H__
#define __OGGPLAY_YUV2RGB_TEMPLATE_H__

#if defined(WIN32)
#define restrict
#else
#ifndef restrict
#define restrict __restrict__
#endif
#endif

/**
 * Template for YUV to RGB conversion
 *
 * @param FUNC function name
 * @param CONVERT a macro that defines 
 * @param NUM_PIXELS number of pixels processed in one iteration
 * @param OUT_SHIFT number of pixels to shift after one iteration in rgb data stream
 * @param Y_SHIFT number of pixels to shift after one iteration in Y data stream
 * @param UV_SHIFT
 */
#define YUV_CONVERT(FUNC, CONVERT, NUM_PIXELS, OUT_SHIFT, Y_SHIFT, UV_SHIFT)\
static void                                                     \
(FUNC)(const OggPlayYUVChannels* yuv, OggPlayRGBChannels* rgb)  \
{                                                               \
	int             i,j, w, h;                              \
	unsigned char*  restrict ptry;                          \
	unsigned char*  restrict ptru;                          \
	unsigned char*  restrict ptrv;                          \
	unsigned char*  restrict ptro;                          \
	unsigned char   *dst, *py, *pu, *pv;                    \
								\
	ptro = rgb->ptro;                                       \
	ptry = yuv->ptry;                                       \
	ptru = yuv->ptru;                                       \
	ptrv = yuv->ptrv;                                       \
								\
	w = yuv->y_width/NUM_PIXELS;                            \
	h = yuv->y_height;                                      \
	for (i = 0; i < h; ++i)                                 \
	{                                                       \
		py  = ptry;                                     \
		pu  = ptru;                                     \
		pv  = ptrv;                                     \
		dst = ptro;                                     \
		for (j = 0; j < w; ++j,                         \
				dst += OUT_SHIFT,               \
				py += Y_SHIFT,                  \
				pu += UV_SHIFT,                 \
				pv += UV_SHIFT)                 \
		{                                               \
			/* use the given conversion function */ \
			CONVERT                                 \
		}                                               \
		ptro += rgb->rgb_width * 4;                     \
		ptry += yuv->y_width;                           \
								\
		if (i & 0x1)                                    \
		{                                               \
			ptru += yuv->uv_width;                  \
			ptrv += yuv->uv_width;                  \
		}                                               \
	}                                                       \
	CLEANUP                                                 \
}  

#endif

