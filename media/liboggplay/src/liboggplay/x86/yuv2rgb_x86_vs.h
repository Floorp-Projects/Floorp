#ifndef __OGGPLAY_YUV2RGB_VS_H__
#define __OGGPLAY_YUV2RGB_VS_H__

#define ATTR_ALIGN(_align) __declspec(align(_align))

#define emms() __asm emms
#define MMX_MOVNTQ movq
#define SSE_MOVNTQ movntq
#define SSE2_MOVNTQ movdqu

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

#endif

