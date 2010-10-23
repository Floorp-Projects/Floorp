# Usage: cp $1/update.sh <tremor_src_directory>
#
# Copies the needed files from a directory containing the original
# libtremor source that we need for the Mozilla HTML5 media support.
cp $1/asm_arm.h ./lib/asm_arm.h
cp $1/backends.h ./lib/backends.h
cp $1/block.c ./lib/tremor_block.c
cp $1/block.h ./lib/block.h
cp $1/codebook.c ./lib/tremor_codebook.c
cp $1/codebook.h ./lib/codebook.h
cp $1/codec_internal.h ./lib/codec_internal.h
cp $1/floor0.c ./lib/tremor_floor0.c
cp $1/floor1.c ./lib/tremor_floor1.c
cp $1/info.c ./lib/tremor_info.c
cp $1/lsp_lookup.h ./lib/lsp_lookup.h
cp $1/mapping0.c ./lib/tremor_mapping0.c
cp $1/mdct_lookup.h ./lib/mdct_lookup.h
cp $1/mdct.c ./lib/tremor_mdct.c
cp $1/mdct.h ./lib/mdct.h
cp $1/misc.h ./lib/misc.h
cp $1/registry.c ./lib/tremor_registry.c
cp $1/registry.h ./lib/registry.h
cp $1/res012.c ./lib/tremor_res012.c
cp $1/sharedbook.c ./lib/tremor_sharedbook.c
cp $1/synthesis.c ./lib/tremor_synthesis.c
cp $1/window_lookup.h ./lib/window_lookup.h
cp $1/window.c ./lib/tremor_window.c
cp $1/window.h ./lib/window.h
cp $1/ivorbiscodec.h ./include/tremor/ivorbiscodec.h
cp $1/os.h ./lib/os.h
cp $1/COPYING ./COPYING
cp $1/README ./README
