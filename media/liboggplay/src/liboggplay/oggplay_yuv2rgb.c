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

/* although we use cpu runtime detection, we still need these
 * macros as there's no way e.g. we could compile a x86 asm code 
 * on a ppc machine and vica-versa
 */
#if defined(i386) || defined(__x86__) || defined(__x86_64__) || defined(_M_IX86)
#include "oggplay_yuv2rgb_x86.c"
#elif defined(__ppc__) || defined(__ppc64__)
//altivec intristics only working with -maltivec gcc flag, 
//but we want runtime altivec detection, hence this has to be
//fixed!
//#include "oggplay_yuv2rgb_altivec.c"
#endif

static int yuv_initialized;
static ogg_uint32_t cpu_features;

/**
 * vanilla implementation of YUV-to-RGB conversion.
 *
 *  - using table-lookups instead of multiplication
 *  - avoid CLAMPing by incorporating 
 *
 */

#define CLAMP(v)    ((v) > 255 ? 255 : (v) < 0 ? 0 : (v))

#define prec 15 
static const int CoY	= (int)(1.164 * (1 << prec) + 0.5);
static const int CoRV	= (int)(1.596 * (1 << prec) + 0.5);
static const int CoGU	= (int)(0.391 * (1 << prec) + 0.5);
static const int CoGV	= (int)(0.813 * (1 << prec) + 0.5);
static const int CoBU	= (int)(2.018 * (1 << prec) + 0.5);

static int CoefsGU[256];
static int CoefsGV[256]; 
static int CoefsBU[256]; 
static int CoefsRV[256];
static int CoefsY[256];

/**
 * Initialize the lookup-table for vanilla yuv to rgb conversion
 * and the cpu_features global.
 */
static void
init_yuv_converters()
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

	cpu_features = oc_cpu_flags_get();
	yuv_initialized = 1;
}

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

/* yuv420p -> */
#define LOOKUP_COEFFS int ruv = CoefsRV[*pv]; 			\
		      int guv = CoefsGU[*pu] + CoefsGV[*pv]; 	\
		      int buv = CoefsBU[*pu]; 			\
                      int r, g, b;

#define CONVERT(OUTPUT_FUNC) LOOKUP_COEFFS				 \
			     VANILLA_YUV2RGB_PIXEL(py[0], ruv, guv, buv);\
			     OUTPUT_FUNC(dst, r, g, b);			 \
			     VANILLA_YUV2RGB_PIXEL(py[1], ruv, guv, buv);\
			     OUTPUT_FUNC((dst+4), r, g, b);

#define CLEANUP

YUV_CONVERT(yuv420_to_rgba_vanilla, CONVERT(VANILLA_RGBA_OUT), 2, 8, 2, 1)
YUV_CONVERT(yuv420_to_bgra_vanilla, CONVERT(VANILLA_BGRA_OUT), 2, 8, 2, 1)
YUV_CONVERT(yuv420_to_abgr_vanilla, CONVERT(VANILLA_ABGR_OUT), 2, 8, 2, 1)
YUV_CONVERT(yuv420_to_argb_vanilla, CONVERT(VANILLA_ARGB_OUT), 2, 8, 2, 1)

#undef CONVERT
#undef CLEANUP

void
oggplay_yuv2rgba(const OggPlayYUVChannels* yuv, OggPlayRGBChannels* rgb)
{
	if (!yuv_initialized)
		init_yuv_converters();

#if defined(i386) || defined(__x86__) || defined(__x86_64__) || defined(_M_IX86)
#if defined(_MSC_VER) || (defined(ATTRIBUTE_ALIGNED_MAX) && ATTRIBUTE_ALIGNED_MAX >= 16)
	if (yuv->y_width % 16 == 0 && cpu_features & OC_CPU_X86_SSE2)
		return yuv420_to_rgba_sse2(yuv, rgb);
#endif
	if (yuv->y_width % 8 == 0 && cpu_features & OC_CPU_X86_MMX)
		return yuv420_to_rgba_mmx(yuv, rgb);
#elif defined(__ppc__) || defined(__ppc64__)
	if (yuv->y_width % 16 == 0 && yuv->y_height % 2 == 0 && cpu_features & OC_CPU_PPC_ALTIVEC)
		return yuv420_to_abgr_vanilla(yuv, rgb);
#endif

#if WORDS_BIGENDIAN || IS_BIG_ENDIAN 
	return yuv420_to_abgr_vanilla(yuv, rgb);
#else
	return yuv420_to_rgba_vanilla(yuv, rgb);
#endif
}

void 
oggplay_yuv2bgra(const OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb)
{
	if (!yuv_initialized)
		init_yuv_converters();

#if defined(i386) || defined(__x86__) || defined(__x86_64__) || defined(_M_IX86)
#if defined(_MSC_VER) || (defined(ATTRIBUTE_ALIGNED_MAX) && ATTRIBUTE_ALIGNED_MAX >= 16)
	if (yuv->y_width % 16 == 0 && cpu_features & OC_CPU_X86_SSE2)
		return yuv420_to_bgra_sse2(yuv, rgb);
#endif
	if (yuv->y_width % 8 == 0 && cpu_features & OC_CPU_X86_MMX)
		return yuv420_to_bgra_mmx(yuv, rgb);
#elif defined(__ppc__) || defined(__ppc64__)
	if (yuv->y_width % 16 == 0 && yuv->y_height % 2 == 0 && cpu_features & OC_CPU_PPC_ALTIVEC)
		return yuv420_to_argb_vanilla(yuv, rgb);
#endif

#if WORDS_BIGENDIAN || IS_BIG_ENDIAN 
	return yuv420_to_argb_vanilla(yuv, rgb);
#else
	return yuv420_to_bgra_vanilla(yuv, rgb);
#endif
}

void 
oggplay_yuv2argb(const OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb)
{
	if (!yuv_initialized)
		init_yuv_converters();

#if defined(i386) || defined(__x86__) || defined(__x86_64__) || defined(_M_IX86)
#if defined(_MSC_VER) || (defined(ATTRIBUTE_ALIGNED_MAX) && ATTRIBUTE_ALIGNED_MAX >= 16)
	if (yuv->y_width % 16 == 0 && cpu_features & OC_CPU_X86_SSE2)
		return yuv420_to_argb_sse2(yuv, rgb);
#endif
	if (yuv->y_width % 8 == 0 && cpu_features & OC_CPU_X86_MMX)
		return yuv420_to_argb_mmx(yuv, rgb);
#elif defined(__ppc__) || defined(__ppc64__)
	if (yuv->y_width % 16 == 0 && yuv->y_height % 2 == 0 && cpu_features & OC_CPU_PPC_ALTIVEC)
		return yuv420_to_bgra_vanilla(yuv, rgb);
#endif

#if WORDS_BIGENDIAN || IS_BIG_ENDIAN 
	return yuv420_to_bgra_vanilla(yuv, rgb);
#else
	return yuv420_to_argb_vanilla(yuv, rgb);
#endif
}

