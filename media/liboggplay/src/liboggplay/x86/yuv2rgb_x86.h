#ifndef __YUV2RGB_X86_H__
#define __YUV2RGB_X86_H__

# ifdef ATTRIBUTE_ALIGNED_MAX
#define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
# else
#define ATTR_ALIGN(align)
# endif

#define emms() __asm__ __volatile__ ( "emms;" );
#define MMX_MOVNTQ "movq"
#define SSE_MOVNTQ "movntq"
#define SSE2_MOVNTQ "movdqu"

#define YUV_2_RGB(mov_instr, reg_type) \
	__asm__ __volatile__ (		\
			"punpcklbw %%"#reg_type"4, %%"#reg_type"0;" 	/* mm0 = u3 u2 u1 u0 */\
			"punpcklbw %%"#reg_type"4, %%"#reg_type"1;"	/* mm1 = v3 v2 v1 v0 */\
			"psubsw (%0), %%"#reg_type"0;"			/* u -= 128 */\
			"psubsw (%0), %%"#reg_type"1;"			/* v -= 128 */\
			"psllw $3, %%"#reg_type"0;"			/* promote precision */\
			"psllw $3, %%"#reg_type"1;"			/* promote precision */\
			#mov_instr " %%"#reg_type"0, %%"#reg_type"2;"	/* mm2 = u3 u2 u1 u0 */\
			#mov_instr " %%"#reg_type"1, %%"#reg_type"3;"	/* mm3 = v3 v2 v1 v0 */\
			"pmulhw 16(%0), %%"#reg_type"2;"		/* mm2 = u * u_green */\
			"pmulhw 32(%0), %%"#reg_type"3;"		/* mm3 = v * v_green */\
			"pmulhw 48(%0), %%"#reg_type"0;"		/* mm0 = chroma_b */\
			"pmulhw 64(%0), %%"#reg_type"1;"		/* mm1 = chroma_r */\
			"paddsw %%"#reg_type"3, %%"#reg_type"2;"	/* mm2 = chroma_g */\
			"psubusb 80(%0), %%"#reg_type"6;"		/* Y -= 16  */\
			#mov_instr " %%"#reg_type"6, %%"#reg_type"7;"	/* mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
			"pand 112(%0), %%"#reg_type"6;"			/* mm6 =    Y6    Y4    Y2    Y0 */\
			"psrlw $8, %%"#reg_type"7;"			/* mm7 =    Y7    Y5    Y3    Y1 */\
			"psllw $3, %%"#reg_type"6;"			/* promote precision */\
			"psllw $3, %%"#reg_type"7;"			/* promote precision */\
			"pmulhw 96(%0), %%"#reg_type"6;"		/* mm6 = luma_rgb even */\
			"pmulhw 96(%0), %%"#reg_type"7;"		/* mm7 = luma_rgb odd */\
			#mov_instr " %%"#reg_type"0, %%"#reg_type"3;"	/* mm3 = chroma_b */\
			#mov_instr " %%"#reg_type"1, %%"#reg_type"4;"	/* mm4 = chroma_r */\
			#mov_instr " %%"#reg_type"2, %%"#reg_type"5;"	/* mm5 = chroma_g */\
			"paddsw %%"#reg_type"6, %%"#reg_type"0;"	/* mm0 = B6 B4 B2 B0 */\
			"paddsw %%"#reg_type"7, %%"#reg_type"3;"	/* mm3 = B7 B5 B3 B1 */\
			"paddsw %%"#reg_type"6, %%"#reg_type"1;"	/* mm1 = R6 R4 R2 R0 */\
			"paddsw %%"#reg_type"7, %%"#reg_type"4;"	/* mm4 = R7 R5 R3 R1 */\
			"paddsw %%"#reg_type"6, %%"#reg_type"2;"	/* mm2 = G6 G4 G2 G0 */\
			"paddsw %%"#reg_type"7, %%"#reg_type"5;"	/* mm5 = G7 G5 G3 G1 */\
			"packuswb %%"#reg_type"0, %%"#reg_type"0;"	/* saturate to 0-255 */\
			"packuswb %%"#reg_type"1, %%"#reg_type"1;"	/* saturate to 0-255 */\
			"packuswb %%"#reg_type"2, %%"#reg_type"2;"	/* saturate to 0-255 */\
			"packuswb %%"#reg_type"3, %%"#reg_type"3;"	/* saturate to 0-255 */\
			"packuswb %%"#reg_type"4, %%"#reg_type"4;"	/* saturate to 0-255 */\
			"packuswb %%"#reg_type"5, %%"#reg_type"5;"	/* saturate to 0-255 */\
			"punpcklbw %%"#reg_type"3, %%"#reg_type"0;"	/* mm0 = B7 B6 B5 B4 B3 B2 B1 B0 */\
			"punpcklbw %%"#reg_type"4, %%"#reg_type"1;"	/* mm1 = R7 R6 R5 R4 R3 R2 R1 R0 */\
			"punpcklbw %%"#reg_type"5, %%"#reg_type"2;"	/* mm2 = G7 G6 G5 G4 G3 G2 G1 G0 */\
			::"r" (simd_table));

#define OUTPUT_BGRA_32(mov_instr, reg_type, offset0, offset1, offset2) \
	__asm__ __volatile__ (				\
			/* r0=B, r1=R, r2=G */		\
			#mov_instr " 128(%1), %%"#reg_type"3;\n\t"\
			#mov_instr " %%"#reg_type"0, %%"#reg_type"4;\n\t"\
			#mov_instr " %%"#reg_type"1, %%"#reg_type"5;\n\t"\
			"punpcklbw %%"#reg_type"2, %%"#reg_type"0;\n\t" /* GB GB GB GB low  */\
			"punpcklbw %%"#reg_type"3, %%"#reg_type"1;\n\t" /* FR FR FR FR low  */\
			"punpckhbw %%"#reg_type"2, %%"#reg_type"4;\n\t" /* GB GB GB GB high */\
			"punpckhbw %%"#reg_type"3, %%"#reg_type"5;\n\t" /* FR FR FR FR high */\
			#mov_instr " %%"#reg_type"0, %%"#reg_type"6;\n\t"\
			#mov_instr " %%"#reg_type"4, %%"#reg_type"7;\n\t"\
			"punpcklwd %%"#reg_type"1, %%"#reg_type"0;\n\t" /* FRGB FRGB 0 */\
			"punpckhwd %%"#reg_type"1, %%"#reg_type"6;\n\t" /* FRGB FRGB 1 */\
			"punpcklwd %%"#reg_type"5, %%"#reg_type"4;\n\t" /* FRGB FRGB 2 */\
			"punpckhwd %%"#reg_type"5, %%"#reg_type"7;\n\t" /* FRGB FRGB 3 */\
			MOVNTQ " %%"#reg_type"0, (%0);\n\t"\
			MOVNTQ " %%"#reg_type"6, "#offset0"(%0);\n\t"\
			MOVNTQ " %%"#reg_type"4, "#offset1"(%0);\n\t"\
			MOVNTQ " %%"#reg_type"7, "#offset2"(%0);\n\t"\
			::  "r" (dst), "r" (simd_table));


#define OUTPUT_ARGB_32(mov_instr, reg_type, offset0, offset1, offset2) \
	__asm__ __volatile__ (				\
			/* r0=B, r1=R, r2=G */		\
			#mov_instr " 128(%1), %%"#reg_type"3;\n\t"\
			#mov_instr " %%"#reg_type"3, %%"#reg_type"4;\n\t"\
			#mov_instr " %%"#reg_type"2, %%"#reg_type"5;\n\t"\
			"punpcklbw %%"#reg_type"0, %%"#reg_type"2;\n\t" /* BG BG BG BG low  */\
			"punpcklbw %%"#reg_type"1, %%"#reg_type"3;\n\t" /* RF RF RF RF low  */\
			"punpckhbw %%"#reg_type"0, %%"#reg_type"5;\n\t" /* BG BG BG BG high */\
			"punpckhbw %%"#reg_type"1, %%"#reg_type"4;\n\t" /* RF RF RF RF high */\
			#mov_instr " %%"#reg_type"3, %%"#reg_type"0;\n\t"\
			#mov_instr " %%"#reg_type"4, %%"#reg_type"1;\n\t"\
			"punpcklwd %%"#reg_type"2, %%"#reg_type"3;\n\t" /* BGRF BGRF 0 */\
			"punpckhwd %%"#reg_type"2, %%"#reg_type"0;\n\t" /* BGRF BGRF 1 */\
			"punpcklwd %%"#reg_type"5, %%"#reg_type"4;\n\t" /* BGRF BGRF 2 */\
			"punpckhwd %%"#reg_type"5, %%"#reg_type"1;\n\t" /* BGRF BGRF 3 */\
			MOVNTQ " %%"#reg_type"3, (%0);\n\t"\
			MOVNTQ " %%"#reg_type"0, "#offset0"(%0);\n\t"\
			MOVNTQ " %%"#reg_type"4, "#offset1"(%0);\n\t"\
			MOVNTQ " %%"#reg_type"1, "#offset2"(%0);\n\t"\
			::  "r" (dst), "r" (simd_table));

#define OUTPUT_RGBA_32(mov_instr, reg_type, offset0, offset1, offset2) \
	__asm__ __volatile__ (				\
			/* r0=B, r1=R, r2=G */		\
			#mov_instr " 128(%1), %%"#reg_type"3;\n\t"\
			#mov_instr " %%"#reg_type"1, %%"#reg_type"4;\n\t"\
			#mov_instr " %%"#reg_type"0, %%"#reg_type"5;\n\t"\
			"punpcklbw %%"#reg_type"2, %%"#reg_type"1;\n\t" /* GR GR GR GR low  */\
			"punpcklbw %%"#reg_type"3, %%"#reg_type"0;\n\t" /* 0B 0B 0B 0B low  */\
			"punpckhbw %%"#reg_type"2, %%"#reg_type"4;\n\t" /* GR GR GR GR high */\
			"punpckhbw %%"#reg_type"3, %%"#reg_type"5;\n\t" /* 0B 0B 0B 0B high */\
			#mov_instr " %%"#reg_type"1, %%"#reg_type"6;\n\t"\
			#mov_instr " %%"#reg_type"4, %%"#reg_type"7;\n\t"\
			"punpcklwd %%"#reg_type"0, %%"#reg_type"1;\n\t" /* 0BGR 0BGR 0 */\
			"punpckhwd %%"#reg_type"0, %%"#reg_type"6;\n\t" /* 0BGR 0BGR 1 */\
			"punpcklwd %%"#reg_type"5, %%"#reg_type"4;\n\t" /* 0BGR 0BGR 2 */\
			"punpckhwd %%"#reg_type"5, %%"#reg_type"7;\n\t" /* 0BGR 0BGR 3 */\
			MOVNTQ " %%"#reg_type"1, (%0);\n\t"\
			MOVNTQ " %%"#reg_type"6, "#offset0"(%0);\n\t"\
			MOVNTQ " %%"#reg_type"4, "#offset1"(%0);\n\t"\
			MOVNTQ " %%"#reg_type"7, "#offset2"(%0);\n\t"\
			::  "r" (dst), "r" (simd_table));

#define LOAD_YUV_PLANAR_2(mov_instr, reg_type)				\
	__asm__ __volatile__ (						\
			#mov_instr " %0, %%"#reg_type"6;\n\t"		\
			#mov_instr " %1, %%"#reg_type"0;\n\t"		\
			#mov_instr " %2, %%"#reg_type"1;\n\t"		\
			"pxor %%"#reg_type"4, %%"#reg_type"4;\n\t"	\
			:: "m" (*py), "m" (*pu), "m" (*pv));


#endif /* __YUV2RGB_X86_H__ */

