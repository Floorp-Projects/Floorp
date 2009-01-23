# Usage: ./update.sh <theora_src_directory>
#
# Copies the needed files from a directory containing the original
# libtheora source that we need for the Mozilla HTML5 media support.
sed s/\#define\ OC_X86ASM//g $1/config.h >./lib/config.h
sed s/\#define\ USE_ASM//g ./lib/config.h >./lib/config.h2
sed s/\#define\ THEORA_DISABLE_ENCODE//g ./lib/config.h2 >./lib/config.h
rm ./lib/config.h2
cp ./lib/config.h ./include/theora/config.h
cp $1/LICENSE ./LICENSE
cp $1/CHANGES ./CHANGES
cp $1/COPYING ./COPYING
cp $1/README ./README
cp $1/AUTHORS ./AUTHORS
cp $1/lib/cpu.c ./lib/cpu.c
cp $1/lib/cpu.h ./lib/cpu.h
cp $1/lib/dec/ocintrin.h ./lib/dec/ocintrin.h
cp $1/lib/dec/huffdec.c ./lib/dec/huffdec.c
cp $1/lib/dec/apiwrapper.h ./lib/dec/apiwrapper.h
cp $1/lib/dec/x86/mmxfrag.c ./lib/dec/x86/mmxfrag.c
cp $1/lib/dec/x86/x86state.c ./lib/dec/x86/x86state.c
cp $1/lib/dec/x86/x86int.h ./lib/dec/x86/x86int.h
cp $1/lib/dec/x86/mmxstate.c ./lib/dec/x86/mmxstate.c
cp $1/lib/dec/x86/mmxidct.c ./lib/dec/x86/mmxidct.c
cp $1/lib/dec/x86_vc/mmxfrag.c ./lib/dec/x86_vc/mmxfrag.c
cp $1/lib/dec/x86_vc/mmxidct.c ./lib/dec/x86_vc/mmxidct.c
cp $1/lib/dec/x86_vc/mmxloopfilter.c ./lib/dec/x86_vc/mmxloopfilter.c
cp $1/lib/dec/x86_vc/mmxstate.c ./lib/dec/x86_vc/mmxstate.c
cp $1/lib/dec/x86_vc/x86int.h ./lib/dec/x86_vc/x86int.h
cp $1/lib/dec/x86_vc/x86state.c ./lib/dec/x86_vc/x86state.c
cp $1/lib/dec/bitpack.h ./lib/dec/bitpack.h
cp $1/lib/dec/quant.c ./lib/dec/quant.c
cp $1/lib/dec/bitpack.c ./lib/dec/bitpack.c
cp $1/lib/dec/internal.c ./lib/dec/internal.c
cp $1/lib/dec/huffdec.h ./lib/dec/huffdec.h
cp $1/lib/dec/dct.h ./lib/dec/dct.h
cp $1/lib/dec/idct.h ./lib/dec/idct.h
cp $1/lib/dec/decinfo.c ./lib/dec/decinfo.c
cp $1/lib/dec/decapiwrapper.c ./lib/dec/decapiwrapper.c
cp $1/lib/dec/idct.c ./lib/dec/idct.c
cp $1/lib/dec/state.c ./lib/dec/state.c
cp $1/lib/dec/info.c ./lib/dec/info.c
cp $1/lib/dec/huffman.h ./lib/dec/huffman.h
cp $1/lib/dec/decint.h ./lib/dec/decint.h
cp $1/lib/dec/fragment.c ./lib/dec/fragment.c
cp $1/lib/dec/apiwrapper.c ./lib/dec/apiwrapper.c
cp $1/lib/dec/decode.c ./lib/dec/decode.c
cp $1/lib/dec/dequant.c ./lib/dec/dequant.c
cp $1/lib/dec/quant.h ./lib/dec/quant.h
cp $1/lib/dec/dequant.h ./lib/dec/dequant.h
cp $1/lib/internal.h ./lib/internal.h
cp $1/include/theora/theora.h ./include/theora/theora.h
cp $1/include/theora/theoradec.h ./include/theora/theoradec.h
cp $1/include/theora/codec.h ./include/theora/codec.h
patch -p3 <455357_wince_local_variable_macro_clash_patch
