# Usage: ./update.sh <theora_src_directory>
#
# Copies the needed files from a directory containing the original
# libtheora source that we need for the Mozilla HTML5 media support.
sed \
 -e s/\#define\ OC_X86_ASM//g \
 -e s/\#define\ OC_X86_64_ASM//g \
 -e s/\#define\ OC_ARM_ASM_EDSP\ 1//g \
 -e s/\#define\ OC_ARM_ASM_MEDIA\ 1//g \
 -e s/\#define\ OC_ARM_ASM_NEON\ 1//g \
 -e s/\#define\ OC_ARM_ASM//g \
 -e s/\#define\ THEORA_DISABLE_ENCODE//g \
 $1/config.h > lib/config.h
sed \
 -e s/@HAVE_ARM_ASM_EDSP@/1/g \
 -e s/@HAVE_ARM_ASM_MEDIA@/1/g \
 -e s/@HAVE_ARM_ASM_NEON@/1/g \
 $1/lib/arm/armopts.s.in > lib/arm/armopts.s
cp $1/LICENSE ./LICENSE
cp $1/CHANGES ./CHANGES
cp $1/COPYING ./COPYING
cp $1/README ./README
cp $1/AUTHORS ./AUTHORS
cp $1/lib/apiwrapper.c ./lib/
cp $1/lib/apiwrapper.h ./lib/
cp $1/lib/bitpack.c ./lib/
cp $1/lib/bitpack.h ./lib/
cp $1/lib/dct.h ./lib/
cp $1/lib/decapiwrapper.c ./lib/
cp $1/lib/decinfo.c ./lib/
cp $1/lib/decint.h ./lib/
cp $1/lib/decode.c ./lib/
cp $1/lib/dequant.c ./lib/
cp $1/lib/dequant.h ./lib/
cp $1/lib/fragment.c ./lib/
cp $1/lib/huffdec.c ./lib/
cp $1/lib/huffdec.h ./lib/
cp $1/lib/huffman.h ./lib/
cp $1/lib/idct.c ./lib/
cp $1/lib/info.c ./lib/
cp $1/lib/internal.c ./lib/
cp $1/lib/internal.h ./lib/
cp $1/lib/mathops.h ./lib/
cp $1/lib/ocintrin.h ./lib/
cp $1/lib/quant.c ./lib/
cp $1/lib/quant.h ./lib/
cp $1/lib/state.c ./lib/
cp $1/lib/state.h ./lib/
cp $1/lib/arm/arm2gnu.pl ./lib/arm/
cp $1/lib/arm/armbits.h ./lib/arm/
cp $1/lib/arm/armbits.s ./lib/arm/
cp $1/lib/arm/armcpu.c ./lib/arm/
cp $1/lib/arm/armcpu.h ./lib/arm/
cp $1/lib/arm/armfrag.s ./lib/arm/
cp $1/lib/arm/armidct.s ./lib/arm/
cp $1/lib/arm/armint.h ./lib/arm/
cp $1/lib/arm/armloop.s ./lib/arm/
cp $1/lib/arm/armstate.c ./lib/arm/
cp $1/lib/x86/mmxfrag.c ./lib/x86/
cp $1/lib/x86/mmxidct.c ./lib/x86/
cp $1/lib/x86/mmxloop.h ./lib/x86/
cp $1/lib/x86/mmxstate.c ./lib/x86/
cp $1/lib/x86/sse2idct.c ./lib/x86/
cp $1/lib/x86/sse2trans.h ./lib/x86/
cp $1/lib/x86/x86cpu.c ./lib/x86/
cp $1/lib/x86/x86cpu.h ./lib/x86/
cp $1/lib/x86/x86int.h ./lib/x86/
cp $1/lib/x86/x86state.c ./lib/x86/
cp $1/lib/x86_vc/mmxfrag.c ./lib/x86_vc/
cp $1/lib/x86_vc/mmxidct.c ./lib/x86_vc/
cp $1/lib/x86_vc/mmxloop.h ./lib/x86_vc/
cp $1/lib/x86_vc/mmxstate.c ./lib/x86_vc/
cp $1/lib/x86_vc/x86cpu.c ./lib/x86_vc/
cp $1/lib/x86_vc/x86cpu.h ./lib/x86_vc/
cp $1/lib/x86_vc/x86int.h ./lib/x86_vc/
cp $1/lib/x86_vc/x86state.c ./lib/x86_vc/
cp $1/include/theora/theora.h ./include/theora/theora.h
cp $1/include/theora/theoradec.h ./include/theora/theoradec.h
cp $1/include/theora/theoraenc.h ./include/theora/theoraenc.h
cp $1/include/theora/codec.h ./include/theora/codec.h
