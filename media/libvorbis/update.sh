# Usage: /bin/sh update.sh <vorbis_src_directory>
#
# Copies the needed files from a directory containing the original
# libvorbis source that we need for the Mozilla HTML5 media support.
cp $1/lib/envelope.h ./lib/envelope.h
cp $1/lib/lpc.h ./lib/lpc.h
cp $1/lib/highlevel.h ./lib/highlevel.h
cp $1/lib/floor0.c ./lib/vorbis_floor0.c
cp $1/lib/lookup_data.h ./lib/lookup_data.h
cp $1/lib/psy.c ./lib/vorbis_psy.c
cp $1/lib/window.c ./lib/vorbis_window.c
cp $1/lib/info.c ./lib/vorbis_info.c
cp $1/lib/res0.c ./lib/vorbis_res0.c
cp $1/lib/lookup.h ./lib/lookup.h
cp $1/lib/lookup.c ./lib/vorbis_lookup.c
sed s/lookup\.c/vorbis_lookup\.c/g $1/lib/lsp.c >./lib/vorbis_lsp.c
cp $1/lib/codebook.h ./lib/codebook.h
cp $1/lib/registry.c ./lib/vorbis_registry.c
cp $1/lib/smallft.h ./lib/smallft.h
cp $1/lib/synthesis.c ./lib/vorbis_synthesis.c
cp $1/lib/masking.h ./lib/masking.h
cp $1/lib/window.h ./lib/window.h
cp $1/lib/scales.h ./lib/scales.h
cp $1/lib/lsp.h ./lib/lsp.h
cp $1/lib/analysis.c ./lib/vorbis_analysis.c
cp $1/lib/misc.h ./lib/misc.h
cp $1/lib/floor1.c ./lib/vorbis_floor1.c
cp $1/lib/lpc.c ./lib/vorbis_lpc.c
cp $1/lib/backends.h ./lib/backends.h
cp $1/lib/sharedbook.c ./lib/vorbis_sharedbook.c
cp $1/lib/mapping0.c ./lib/vorbis_mapping0.c
cp $1/lib/smallft.c ./lib/vorbis_smallft.c
cp $1/lib/psy.h ./lib/psy.h
cp $1/lib/bitrate.h ./lib/bitrate.h
cp $1/lib/envelope.c ./lib/vorbis_envelope.c
cp $1/lib/os.h ./lib/os.h
cp $1/lib/mdct.c ./lib/vorbis_mdct.c
cp $1/lib/codec_internal.h ./lib/codec_internal.h
cp $1/lib/mdct.h ./lib/mdct.h
cp $1/lib/registry.h ./lib/registry.h
cp $1/lib/codebook.c ./lib/vorbis_codebook.c
cp $1/lib/bitrate.c ./lib/vorbis_bitrate.c
cp $1/lib/block.c ./lib/vorbis_block.c
cp $1/include/vorbis/codec.h ./include/vorbis/codec.h
cp $1/todo.txt ./todo.txt
cp $1/COPYING ./COPYING
cp $1/README ./README
cp $1/AUTHORS ./AUTHORS

patch -p3 < ./alloca.diff
