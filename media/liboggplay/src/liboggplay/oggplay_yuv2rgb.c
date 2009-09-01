/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * yuv2rgb.c
 *
 * YUV->RGB function, using platform-specific optimisations where possible.
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 * Marcin Lubonski
 * Viktor Gal
 */

#include "oggplay_private.h"
#include "oggplay_yuv2rgb_template.h"

/* cpu extension detection */
#include "cpu.c"

/**
 * yuv_convert_fptr type is a function pointer type for
 * the various yuv-rgb converters
 */
typedef void (*yuv_convert_fptr) (const OggPlayYUVChannels *yuv, 
					OggPlayRGBChannels *rgb);

/* it is useless to determine each YUV conversion run
 * the cpu type/featurs, thus we save the conversion function
 * pointers
 */
static struct OggPlayYUVConverters {
	yuv_convert_fptr yuv2rgba; /**< YUV420 to RGBA */
	yuv_convert_fptr yuv2bgra; /**< YUV420 to BGRA */
	yuv_convert_fptr yuv2argb; /**< YUV420 to ARGB */
} yuv_conv = {NULL, NULL, NULL};

/**
 * vanilla implementation of YUV-to-RGB conversion.
 *
 *  - using table-lookups instead of multiplication
 *  - avoid CLAMPing by incorporating 
 *
 */

#define prec 15 
static const int CoY	= (int)(1.164 * (1 << prec) + 0.5);
static const int CoRV	= (int)(1.596 * (1 << prec) + 0.5);
static const int CoGU	= (int)(0.391 * (1 << prec) + 0.5);
static const int CoGV	= (int)(0.813 * (1 << prec) + 0.5);
static const int CoBU	= (int)(2.018 * (1 << prec) + 0.5);

static int CoefsGU[256] = {0};
static int CoefsGV[256]; 
static int CoefsBU[256]; 
static int CoefsRV[256];
static int CoefsY[256];

#define CLAMP(v)    ((v) > 255 ? 255 : (v) < 0 ? 0 : (v))

#define VANILLA_YUV2RGB_PIXEL(y, ruv, guv, buv)	\
r = (CoefsY[y] + ruv) >> prec;	\
g = (CoefsY[y] + guv) >> prec;	\
b = (CoefsY[y] + buv) >> prec;	\

#define VANILLA_RGBA_OUT(out, r, g, b) \
out[0] = CLAMP(r); \
out[1] = CLAMP(g); \
out[2] = CLAMP(b); \
out[3] = 255;

#define VANILLA_BGRA_OUT(out, r, g, b) \
out[0] = CLAMP(b); \
out[1] = CLAMP(g); \
out[2] = CLAMP(r); \
out[3] = 255;

#define VANILLA_ARGB_OUT(out, r, g, b) \
out[0] = 255;	   \
out[1] = CLAMP(r); \
out[2] = CLAMP(g); \
out[3] = CLAMP(b);

#define VANILLA_ABGR_OUT(out, r, g, b) \
out[0] = 255;	   \
out[1] = CLAMP(b); \
out[2] = CLAMP(g); \
out[3] = CLAMP(r);

#define LOOKUP_COEFFS int ruv = CoefsRV[*pv]; 			\
		      int guv = CoefsGU[*pu] + CoefsGV[*pv]; 	\
		      int buv = CoefsBU[*pu]; 			\
                      int r, g, b;

/* yuv420p -> */
#define CONVERT(OUTPUT_FUNC) LOOKUP_COEFFS				 \
			     VANILLA_YUV2RGB_PIXEL(py[0], ruv, guv, buv) \
			     OUTPUT_FUNC(dst, r, g, b)  \
			     VANILLA_YUV2RGB_PIXEL(py[1], ruv, guv, buv) \
			     OUTPUT_FUNC((dst+4), r, g, b)

#define CLEANUP

YUV_CONVERT(yuv420_to_rgba_vanilla, CONVERT(VANILLA_RGBA_OUT), VANILLA_RGBA_OUT, 2, 8, 2, 1)
YUV_CONVERT(yuv420_to_bgra_vanilla, CONVERT(VANILLA_BGRA_OUT), VANILLA_BGRA_OUT, 2, 8, 2, 1)
YUV_CONVERT(yuv420_to_abgr_vanilla, CONVERT(VANILLA_ABGR_OUT), VANILLA_ABGR_OUT, 2, 8, 2, 1)
YUV_CONVERT(yuv420_to_argb_vanilla, CONVERT(VANILLA_ARGB_OUT), VANILLA_ARGB_OUT, 2, 8, 2, 1)

#undef CONVERT
#undef CLEANUP

/* although we use cpu runtime detection, we still need these
 * macros as there's no way e.g. we could compile a x86 asm code 
 * on a ppc machine and vica-versa
 */
#if defined(i386) || defined(__x86__) || defined(__x86_64__) || defined(_M_IX86)
#include "x86/oggplay_yuv2rgb_x86.c"
#elif defined(__ppc__) || defined(__ppc64__)
//altivec intristics only working with -maltivec gcc flag, 
//but we want runtime altivec detection, hence this has to be
//fixed!
//#include "oggplay_yuv2rgb_altivec.c"
#endif


/**
 * Initialize the lookup-table for vanilla yuv to rgb conversion.
 */
static void
init_vanilla_coeffs (void)
{
	int i;

	for(i = 0; i < 256; ++i)
	{
		CoefsGU[i] = -CoGU * (i - 128);
		CoefsGV[i] = -CoGV * (i - 128);
		CoefsBU[i] = CoBU * (i - 128);
		CoefsRV[i] = CoRV * (i - 128);
		CoefsY[i]  = CoY * (i - 16) + (prec/2);
	}
}

/**
 * Initialize the function pointers in yuv_conv.
 *
 * Initialize the function pointers in yuv_conv, based on the
 * the available CPU extensions.
 */
static void
init_yuv_converters(void)
{
	ogg_uint32_t features = 0;

	if ( yuv_conv.yuv2rgba == NULL )
	{
		init_vanilla_coeffs ();
		features = oc_cpu_flags_get(); 		
#if defined(i386) || defined(__x86__) || defined(__x86_64__) || defined(_M_IX86)
#if defined(_MSC_VER) || (defined(ATTRIBUTE_ALIGNED_MAX) && ATTRIBUTE_ALIGNED_MAX >= 16)
		if (features & OC_CPU_X86_SSE2) 
		{
			yuv_conv.yuv2rgba = yuv420_to_rgba_sse2;
			yuv_conv.yuv2bgra = yuv420_to_bgra_sse2;
			yuv_conv.yuv2argb = yuv420_to_argb_sse2;
			return;
		}
		else
#endif /* ATTRIBUTE_ALIGNED_MAX */
		if (features & OC_CPU_X86_MMXEXT)	
		{
			yuv_conv.yuv2rgba = yuv420_to_rgba_sse;
			yuv_conv.yuv2bgra = yuv420_to_bgra_sse;
			yuv_conv.yuv2argb = yuv420_to_argb_sse;
			return;
		}
		else if (features & OC_CPU_X86_MMX)
		{   
			yuv_conv.yuv2rgba = yuv420_to_rgba_mmx;
			yuv_conv.yuv2bgra = yuv420_to_bgra_mmx;
			yuv_conv.yuv2argb = yuv420_to_argb_mmx;
			return;
		}
#elif defined(__ppc__) || defined(__ppc64__)
		if (features & OC_CPU_PPC_ALTIVEC)
		{
			yuv_conv.yuv2rgba = yuv420_to_abgr_vanilla;
			yuv_conv.yuv2bgra = yuv420_to_argb_vanilla;
			yuv_conv.yuv2argb = yuv420_to_bgra_vanilla;
			return;
		}
#endif		
		/*
     * no CPU extension was found... using vanilla converter, with respect
     * to the endianness of the host
     */
#if WORDS_BIGENDIAN || IS_BIG_ENDIAN 
		yuv_conv.yuv2rgba = yuv420_to_abgr_vanilla;
		yuv_conv.yuv2bgra = yuv420_to_argb_vanilla;
		yuv_conv.yuv2argb = yuv420_to_bgra_vanilla;
#else
		yuv_conv.yuv2rgba = yuv420_to_rgba_vanilla;
		yuv_conv.yuv2bgra = yuv420_to_bgra_vanilla;
		yuv_conv.yuv2argb = yuv420_to_argb_vanilla;
#endif
	}
}


void
oggplay_yuv2rgba(const OggPlayYUVChannels* yuv, OggPlayRGBChannels* rgb)
{
	if (yuv_conv.yuv2rgba == NULL)
		init_yuv_converters();

	yuv_conv.yuv2rgba(yuv, rgb);
}

void 
oggplay_yuv2bgra(const OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb)
{
	if (yuv_conv.yuv2bgra == NULL)
		init_yuv_converters();

	yuv_conv.yuv2bgra(yuv, rgb);
}

void 
oggplay_yuv2argb(const OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb)
{
	if (yuv_conv.yuv2argb == NULL)
		init_yuv_converters();

	yuv_conv.yuv2argb(yuv, rgb);
}

