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
 * @param CONVERT a macro that defines the actual conversion function
 * @param VANILLA_OUT 
 * @param NUM_PIXELS number of pixels processed in one iteration
 * @param OUT_SHIFT number of pixels to shift after one iteration in rgb data stream
 * @param Y_SHIFT number of pixels to shift after one iteration in Y data stream
 * @param UV_SHIFT
 * @param UV_VERT_SUB
 */
#define YUV_CONVERT(FUNC, CONVERT, VANILLA_OUT, NUM_PIXELS, OUT_SHIFT, Y_SHIFT, UV_SHIFT, UV_VERT_SUB)\
static void                                                     \
(FUNC)(const OggPlayYUVChannels* yuv, OggPlayRGBChannels* rgb)  \
{                                                               \
	int             i,j, w, h, r;                           \
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
	w = yuv->y_width / NUM_PIXELS;                          \
	h = yuv->y_height;                                      \
	r = yuv->y_width % NUM_PIXELS;				\
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
		/*						\
		 * the video frame is not the multiple of NUM_PIXELS, \
		 * thus we have to deal with remaning pixels using 	\
		 * vanilla implementation.				\
		 */						\
		if (r) { 					\
			if (r==1 && yuv->y_width&1) {		\
				pu -= 1; pv -= 1;               \
			}					\
								\
			for 					\
			( 					\
			  j=(yuv->y_width-r); j < yuv->y_width; \
			  ++j, 					\
			  dst += 4,				\
			  py += 1 				\
			) 					\
			{ 					\
				LOOKUP_COEFFS			\
				VANILLA_YUV2RGB_PIXEL(py[0], ruv, guv, buv) \
				VANILLA_OUT(dst, r, g, b)	\
				if 				\
				(				\
				  (j%2 && !(j+1==yuv->y_width-1 && yuv->y_width&1)) \
				  ||				\
				  UV_VERT_SUB==1		\
				) { 				\
					pu += 1; pv += 1;	\
				} 				\
			}					\
		} 						\
								\
		ptro += rgb->rgb_width * 4;                     \
		ptry += yuv->y_width;                           \
								\
		if 						\
		(						\
		  (i & 0x1 && !(i+1==h-1 && h&1))		\
		  ||						\
		  UV_VERT_SUB==1				\
		)		            			\
		{                                               \
			ptru += yuv->uv_width;                  \
			ptrv += yuv->uv_width;                  \
		}                                               \
	}                                                       \
	CLEANUP                                                 \
}  

#endif

