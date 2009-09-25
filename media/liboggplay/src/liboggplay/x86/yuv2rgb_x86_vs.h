#ifndef __OGGPLAY_YUV2RGB_VS_H__
#define __OGGPLAY_YUV2RGB_VS_H__

#define ATTR_ALIGN(_align) __declspec(align(_align))

#define emms() __asm emms
#define MMX_MOVNTQ movq
#define SSE_MOVNTQ movntq
#define SSE2_MOVNTQ movdqu

#if defined(_M_IX86)
#define LOAD_YUV_PLANAR_2(mov_instr, reg_type)		\
	__asm {								\
		__asm mov	eax, py					\
		__asm mov	edx, pu					\
		__asm mov_instr	reg_type##6, [eax]			\
		__asm mov_instr	reg_type##0, [edx]			\
		__asm mov	eax, pv					\
		__asm mov_instr	reg_type##1, [eax]			\
		__asm pxor	reg_type##4, reg_type##4		\
	}
#elif defined(_M_AMD64)
#define LOAD_YUV_PLANAR_2(mov_instr, reg_type)		\
	{								\
		xmm6 = _mm_loadu_si128((__m128i*)py);			\
		xmm0 = _mm_loadu_si128((__m128i*)pu);			\
		xmm1 = _mm_loadu_si128((__m128i*)pv);			\
		xmm4 = _mm_setzero_si128();				\
	}
#endif

#if defined(_M_IX86)
#define OUTPUT_RGBA_32(mov_instr, reg_type, offset0, offset1, offset2) \
	__asm {								\
		__asm mov	eax, dst				\
		__asm mov_instr	reg_type##3, [simd_table+128]		\
		__asm mov_instr reg_type##4, reg_type##1		\
		__asm mov_instr reg_type##5, reg_type##0		\
		__asm punpcklbw reg_type##1, reg_type##2		\
		__asm punpcklbw reg_type##0, reg_type##3		\
		__asm punpckhbw reg_type##4, reg_type##2		\
		__asm punpckhbw reg_type##5, reg_type##3		\
		__asm mov_instr reg_type##6, reg_type##1		\
		__asm mov_instr reg_type##7, reg_type##4		\
		__asm punpcklwd reg_type##1, reg_type##0                \
		__asm punpckhwd reg_type##6, reg_type##0                \
		__asm punpcklwd reg_type##4, reg_type##5                \
		__asm punpckhwd reg_type##7, reg_type##5                \
		__asm MOVNTQ	[eax], reg_type##1			\
		__asm MOVNTQ	[eax+offset0], reg_type##6		\
		__asm MOVNTQ	[eax+offset1], reg_type##4		\
		__asm MOVNTQ	[eax+offset2], reg_type##7		\
	}
#elif defined(_M_AMD64)
#define OUTPUT_RGBA_32(mov_instr, reg_type, offset0, offset1, offset2) \
	{								\
		xmm3 = _mm_load_si128((__m128i*)simd_table+8);		\
		xmm4 = _mm_unpackhi_epi8(xmm1, xmm2);			\
		xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);			\
		xmm5 = _mm_unpackhi_epi8(xmm0, xmm3);			\
		xmm0 = _mm_unpacklo_epi8(xmm0, xmm3);			\
		xmm6 = _mm_unpackhi_epi8(xmm1, xmm0);			\
		xmm1 = _mm_unpacklo_epi8(xmm1, xmm0);			\
		xmm7 = _mm_unpackhi_epi8(xmm4, xmm5);			\
		xmm4 = _mm_unpacklo_epi8(xmm4, xmm5);			\
		_mm_storeu_si128(dst, xmm1);				\
		_mm_storeu_si128(dst + offset0, xmm6);			\
		_mm_storeu_si128(dst + offset1, xmm4);			\
		_mm_storeu_si128(dst + offset2, xmm7);			\
	}
#endif

#if defined(_M_IX86)
#define OUTPUT_ARGB_32(mov_instr, reg_type, offset0, offset1, offset2) \
	__asm {								\
		__asm mov	eax, dst				\
		__asm mov_instr	reg_type##3, [simd_table+128]		\
		__asm mov_instr reg_type##4, reg_type##3		\
		__asm mov_instr reg_type##5, reg_type##2		\
		__asm punpcklbw reg_type##2, reg_type##0		\
		__asm punpcklbw reg_type##3, reg_type##1		\
		__asm punpckhbw reg_type##5, reg_type##0		\
		__asm punpckhbw reg_type##4, reg_type##1		\
		__asm mov_instr reg_type##0, reg_type##3		\
		__asm mov_instr reg_type##1, reg_type##4		\
		__asm punpcklwd reg_type##3, reg_type##2                \
		__asm punpckhwd reg_type##0, reg_type##2                \
		__asm punpcklwd reg_type##4, reg_type##5                \
		__asm punpckhwd reg_type##1, reg_type##5                \
		__asm MOVNTQ	[eax], reg_type##3			\
		__asm MOVNTQ	[eax+offset0], reg_type##0		\
		__asm MOVNTQ	[eax+offset1], reg_type##4		\
		__asm MOVNTQ	[eax+offset2], reg_type##1		\
	}
#elif defined(_M_AMD64)
#define OUTPUT_ARGB_32(mov_instr, reg_type, offset0, offset1, offset2) \
	{								\
		xmm3 = _mm_load_si128((__m128i*)simd_table+8);		\
		xmm5 = _mm_unpackhi_epi8(xmm2, xmm0);			\
		xmm2 = _mm_unpacklo_epi8(xmm2, xmm0);			\
		xmm4 = _mm_unpackhi_epi8(xmm3, xmm1);			\
		xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);			\
		xmm0 = _mm_unpackhi_epi16(xmm3, xmm2);			\
		xmm3 = _mm_unpacklo_epi16(xmm3, xmm2);			\
		xmm1 = _mm_unpackhi_epi16(xmm4, xmm5);			\
		xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);			\
		_mm_storeu_si128(dst, xmm3);				\
		_mm_storeu_si128(dst + offset0, xmm0);			\
		_mm_storeu_si128(dst + offset1, xmm4);			\
		_mm_storeu_si128(dst + offset2, xmm1);			\
	}
#endif

#if defined(_M_IX86)
#define OUTPUT_BGRA_32(mov_instr, reg_type, offset0, offset1, offset2) \
	__asm {								\
		__asm mov	eax, dst				\
		__asm mov_instr	reg_type##3, [simd_table+128]		\
		__asm mov_instr reg_type##4, reg_type##0		\
		__asm mov_instr reg_type##5, reg_type##1		\
		__asm punpcklbw reg_type##0, reg_type##2		\
		__asm punpcklbw reg_type##1, reg_type##3		\
		__asm punpckhbw reg_type##4, reg_type##2		\
		__asm punpckhbw reg_type##5, reg_type##3		\
		__asm mov_instr reg_type##6, reg_type##0		\
		__asm mov_instr reg_type##7, reg_type##4		\
		__asm punpcklwd reg_type##0, reg_type##1                \
		__asm punpckhwd reg_type##6, reg_type##1                \
		__asm punpcklwd reg_type##4, reg_type##5                \
		__asm punpckhwd reg_type##7, reg_type##5                \
		__asm MOVNTQ	[eax], reg_type##0			\
		__asm MOVNTQ	[eax+offset0], reg_type##6		\
		__asm MOVNTQ	[eax+offset1], reg_type##4		\
		__asm MOVNTQ	[eax+offset2], reg_type##7		\
	}
#elif defined(_M_AMD64)
#define OUTPUT_BGRA_32(mov_instr, reg_type, offset0, offset1, offset2) \
	{								\
		xmm3 = _mm_load_si128((__m128i*)simd_table+8);		\
		xmm4 = _mm_unpackhi_epi8(xmm0, xmm2);			\
		xmm0 = _mm_unpacklo_epi8(xmm0, xmm2);			\
		xmm5 = _mm_unpackhi_epi8(xmm1, xmm3);			\
		xmm1 = _mm_unpacklo_epi8(xmm1, xmm3);			\
		xmm6 = _mm_unpackhi_epi8(xmm0, xmm1);			\
		xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);			\
		xmm7 = _mm_unpackhi_epi8(xmm4, xmm5);			\
		xmm4 = _mm_unpacklo_epi8(xmm4, xmm5);			\
		_mm_storeu_si128(dst, xmm0);				\
		_mm_storeu_si128(dst + offset0, xmm6);			\
		_mm_storeu_si128(dst + offset1, xmm4);			\
		_mm_storeu_si128(dst + offset2, xmm7);			\
	}
#endif

#if defined(_M_IX86)
#define YUV_2_RGB(mov_instr, reg_type) \
	__asm {											\
		__asm punpcklbw reg_type##0, reg_type##4	/* mm0 = u3 u2 u1 u0 */\
		__asm punpcklbw reg_type##1, reg_type##4	/* mm1 = v3 v2 v1 v0 */\
		__asm psubsw	reg_type##0, [simd_table]	/* u -= 128 */\
		__asm psubsw	reg_type##1, [simd_table]	/* v -= 128 */\
		__asm psllw	reg_type##0, 3			/* promote precision */\
		__asm psllw	reg_type##1, 3			/* promote precision */\
		__asm mov_instr reg_type##2, reg_type##0	/* mm2 = u3 u2 u1 u0 */\
		__asm mov_instr reg_type##3, reg_type##1	/* mm3 = v3 v2 v1 v0 */\
		__asm pmulhw	reg_type##2, [simd_table+16]	/* mm2 = u * u_green */\
		__asm pmulhw	reg_type##3, [simd_table+32]	/* mm3 = v * v_green */\
		__asm pmulhw	reg_type##0, [simd_table+48]	/* mm0 = chroma_b */\
		__asm pmulhw	reg_type##1, [simd_table+64]	/* mm1 = chroma_r */\
		__asm paddsw	reg_type##2, reg_type##3	/* mm2 = chroma_g */\
		__asm psubusb	reg_type##6, [simd_table+80]	/* Y -= 16  */\
		__asm mov_instr reg_type##7, reg_type##6	/* mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
		__asm pand	reg_type##6, [simd_table+112]	/* mm6 =    Y6    Y4    Y2    Y0 */\
		__asm psrlw	reg_type##7, 8			/* mm7 =    Y7    Y5    Y3    Y1 */\
		__asm psllw	reg_type##6, 3			/* promote precision */\
		__asm psllw	reg_type##7, 3			/* promote precision */\
		__asm pmulhw	reg_type##6, [simd_table+96]	/* mm6 = luma_rgb even */\
		__asm pmulhw	reg_type##7, [simd_table+96]	/* mm7 = luma_rgb odd */\
		__asm mov_instr reg_type##3, reg_type##0	/* mm3 = chroma_b */\
		__asm mov_instr reg_type##4, reg_type##1	/* mm4 = chroma_r */\
		__asm mov_instr reg_type##5, reg_type##2	/* mm5 = chroma_g */\
		__asm paddsw	reg_type##0, reg_type##6	/* mm0 = B6 B4 B2 B0 */\
		__asm paddsw	reg_type##3, reg_type##7	/* mm3 = B7 B5 B3 B1 */\
		__asm paddsw	reg_type##1, reg_type##6	/* mm1 = R6 R4 R2 R0 */\
		__asm paddsw	reg_type##4, reg_type##7	/* mm4 = R7 R5 R3 R1 */\
		__asm paddsw	reg_type##2, reg_type##6	/* mm2 = G6 G4 G2 G0 */\
		__asm paddsw	reg_type##5, reg_type##7	/* mm5 = G7 G5 G3 G1 */\
		__asm packuswb	reg_type##0, reg_type##0	/* saturate to 0-255 */\
		__asm packuswb	reg_type##1, reg_type##1	/* saturate to 0-255 */\
		__asm packuswb	reg_type##2, reg_type##2	/* saturate to 0-255 */\
		__asm packuswb	reg_type##3, reg_type##3	/* saturate to 0-255 */\
		__asm packuswb	reg_type##4, reg_type##4	/* saturate to 0-255 */\
		__asm packuswb	reg_type##5, reg_type##5	/* saturate to 0-255 */\
		__asm punpcklbw	reg_type##0, reg_type##3	/* mm0 = B7 B6 B5 B4 B3 B2 B1 B0 */\
		__asm punpcklbw	reg_type##1, reg_type##4	/* mm1 = R7 R6 R5 R4 R3 R2 R1 R0 */\
		__asm punpcklbw	reg_type##2, reg_type##5	/* mm2 = G7 G6 G5 G4 G3 G2 G1 G0 */\
	}
#elif defined(_M_AMD64)
#define YUV_2_RGB(mov_instr, reg_type) \
	{									\
		xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);					/* mm0 = u3 u2 u1 u0 */\
		xmm1 = _mm_unpacklo_epi8(xmm1, xmm4);					/* mm1 = v3 v2 v1 v0 */\
		xmm0 = _mm_subs_epi16(xmm0, _mm_load_si128((__m128i*)simd_table));	/* u -= 128 */\
		xmm1 = _mm_subs_epi16(xmm1, _mm_load_si128((__m128i*)simd_table));	/* v -= 128 */\
		xmm0 = _mm_slli_epi16(xmm0, 3);						/* promote precision */\
		xmm1 = _mm_slli_epi16(xmm1, 3);						/* promote precision */\
		xmm2 = _mm_mulhi_epi16(xmm0, _mm_load_si128((__m128i*)simd_table+1));	/* mm2 = u * u_green */\
		xmm3 = _mm_mulhi_epi16(xmm1, _mm_load_si128((__m128i*)simd_table+2));	/* mm3 = v * v_green */\
		xmm0 = _mm_mulhi_epi16(xmm0, _mm_load_si128((__m128i*)simd_table+3));	/* mm0 = chroma_b */\
		xmm1 = _mm_mulhi_epi16(xmm1, _mm_load_si128((__m128i*)simd_table+4));	/* mm1 = chroma_r */\
		xmm2 = _mm_adds_epi16(xmm2, xmm3);					/* mm2 = chroma_g */\
		xmm6 = _mm_subs_epu8(xmm6, _mm_load_si128((__m128i*)simd_table+5));		/* Y -= 16  */\
		xmm7 = xmm6;								/* mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
		xmm6 = _mm_and_si128(xmm6, _mm_load_si128((__m128i*)simd_table+7);	/* mm6 =    Y6    Y4    Y2    Y0 */\
		xmm7 = _mm_srli_epi16(xmm7, 8);						/* mm7 =    Y7    Y5    Y3    Y1 */\
		xmm6 = _mm_slli_epi16(xmm6, 3);						/* promote precision */\
		xmm7 = _mm_slli_epi16(xmm7, 3);						/* promote precision */\
		xmm6 = _mm_mulhi_epi16(xmm6, _mm_load_si128((__m128i*)simd_table+6));	/* mm6 = luma_rgb even */\
		xmm7 = _mm_mulhi_epi16(xmm7, _mm_load_si128((__m128i*)simd_table+6));	/* mm7 = luma_rgb odd */\
		xmm3 = xmm0;								/* mm3 = chroma_b */\
		xmm4 = xmm1;								/* mm4 = chroma_r */\
		xmm5 = xmm2;								/* mm5 = chroma_g */\
		xmm0 = _mm_adds_epi16(xmm0, xmm6);					/* mm0 = B6 B4 B2 B0 */\
		xmm3 = _mm_adds_epi16(xmm3, xmm7);					/* mm3 = B7 B5 B3 B1 */\
		xmm1 = _mm_adds_epi16(xmm1, xmm6);					/* mm1 = R6 R4 R2 R0 */\
		xmm4 = _mm_adds_epi16(xmm4, xmm7);					/* mm4 = R7 R5 R3 R1 */\
		xmm2 = _mm_adds_epi16(xmm2, xmm6);					/* mm2 = G6 G4 G2 G0 */\
		xmm5 = _mm_adds_epi16(xmm5, xmm7);					/* mm5 = G7 G5 G3 G1 */\
		xmm0 = _mm_packus_epi16(xmm0, xmm0);					/* saturate to 0-255 */\
		xmm1 = _mm_packus_epi16(xmm1, xmm1);					/* saturate to 0-255 */\
		xmm2 = _mm_packus_epi16(xmm2, xmm2);					/* saturate to 0-255 */\
		xmm3 = _mm_packus_epi16(xmm3, xmm3);					/* saturate to 0-255 */\
		xmm4 = _mm_packus_epi16(xmm4, xmm4);					/* saturate to 0-255 */\
		xmm5 = _mm_packus_epi16(xmm5, xmm5);					/* saturate to 0-255 */\
		xmm0 = _mm_unpacklo_epi8(xmm0, xmm3);					/* mm0 = B7 B6 B5 B4 B3 B2 B1 B0 */\
		xmm1 = _mm_unpacklo_epi8(xmm1, xmm4);					/* mm1 = R7 R6 R5 R4 R3 R2 R1 R0 */\
		xmm2 = _mm_unpacklo_epi8(xmm2, xmm5);					/* mm2 = G7 G6 G5 G4 G3 G2 G1 G0 */\
	}
#endif

#endif

