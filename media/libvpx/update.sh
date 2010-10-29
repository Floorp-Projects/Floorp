#!/bin/bash
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla code.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Chris Pearce <chris@pearce.org.nz>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

if [ $# -lt 1 ]; then
  echo Usage: update.sh /path/to/libvpx/
  echo The libvpx dir must contain a directory "objdir" with the following directories configured in it:
  echo   * objdir/x86-win32-vs8
  echo   * objdir/x86-linux-gcc
  echo   * objdir/generic-gnu
  echo   * objdir/x86-darwin9-gcc
  echo   * objdir/x86_64-darwin9-gcc
  echo You can configure these from objdir/$target with the following command:
  echo $ ..configure --target=$target --disable-vp8-encoder --disable-examples --disable-install-docs
  echo On Mac, you also need --enable-pic
  exit -1
fi

# These are relative to SDK source dir.
commonFiles=(
  vp8/common/alloccommon.c
  vp8/common/blockd.c
  vp8/common/debugmodes.c
  vp8/common/entropy.c
  vp8/common/entropymode.c
  vp8/common/entropymv.c
  vp8/common/extend.c
  vp8/common/filter_c.c
  vp8/common/findnearmv.c
  vp8/common/generic/systemdependent.c
  vp8/common/idctllm.c
  vp8/common/invtrans.c
  vp8/common/loopfilter.c
  vp8/common/loopfilter_filters.c
  vp8/common/mbpitch.c
  vp8/common/modecont.c
  vp8/common/modecontext.c
  vp8/common/postproc.c
  vp8/common/predictdc.c
  vp8/common/quant_common.c
  vp8/common/recon.c
  vp8/common/reconinter.c
  vp8/common/reconintra4x4.c
  vp8/common/reconintra.c
  vp8/common/setupintrarecon.c
  vp8/common/swapyv12buffer.c
  vp8/common/textblit.c
  vp8/common/treecoder.c
  vp8/common/x86/loopfilter_x86.c
  vp8/common/x86/vp8_asm_stubs.c
  vp8/common/x86/x86_systemdependent.c
  vp8/decoder/dboolhuff.c
  vp8/decoder/decodemv.c
  vp8/decoder/decodframe.c
  vp8/decoder/demode.c
  vp8/decoder/dequantize.c
  vp8/decoder/detokenize.c
  vp8/decoder/generic/dsystemdependent.c
  vp8/decoder/idct_blk.c
  vp8/decoder/onyxd_if.c
  vp8/decoder/threading.c
  vp8/decoder/x86/idct_blk_mmx.c
  vp8/decoder/x86/idct_blk_sse2.c
  vp8/decoder/x86/x86_dsystemdependent.c
  vp8/vp8_dx_iface.c
  vpx/src/vpx_codec.c
  vpx/src/vpx_decoder.c
  vpx/src/vpx_decoder_compat.c
  vpx/src/vpx_encoder.c
  vpx/src/vpx_image.c
  vpx_mem/vpx_mem.c
  vpx_scale/generic/gen_scalers.c
  vpx_scale/generic/scalesystemdependant.c
  vpx_scale/generic/vpxscale.c
  vpx_scale/generic/yv12config.c
  vpx_scale/generic/yv12extend.c
  vp8/common/alloccommon.h
  vp8/common/blockd.h
  vp8/common/coefupdateprobs.h
  vp8/common/common.h
  vp8/common/common_types.h
  vp8/common/defaultcoefcounts.h
  vp8/common/entropy.h
  vp8/common/entropymode.h
  vp8/common/entropymv.h
  vp8/common/extend.h
  vp8/common/findnearmv.h
  vp8/common/g_common.h
  vp8/common/header.h
  vp8/common/idct.h
  vp8/common/invtrans.h
  vp8/common/loopfilter.h
  vp8/common/modecont.h
  vp8/common/mv.h
  vp8/common/onyxc_int.h
  vp8/common/onyxd.h
  vp8/common/onyx.h
  vp8/common/postproc.h
  vp8/common/ppflags.h
  vp8/common/pragmas.h
  vp8/common/predictdc.h
  vp8/common/preproc.h
  vp8/common/quant_common.h
  vp8/common/recon.h
  vp8/common/reconinter.h
  vp8/common/reconintra4x4.h
  vp8/common/reconintra.h
  vp8/common/segmentation_common.h
  vp8/common/setupintrarecon.h
  vp8/common/subpixel.h
  vp8/common/swapyv12buffer.h
  vp8/common/systemdependent.h
  vp8/common/threading.h
  vp8/common/treecoder.h
  vp8/common/type_aliases.h
  vp8/common/vpxerrors.h
  vp8/common/x86/idct_x86.h
  vp8/common/x86/loopfilter_x86.h
  vp8/common/x86/postproc_x86.h
  vp8/common/x86/recon_x86.h
  vp8/common/x86/subpixel_x86.h
  vp8/decoder/dboolhuff.h
  vp8/decoder/decodemv.h
  vp8/decoder/decoderthreading.h
  vp8/decoder/demode.h
  vp8/decoder/dequantize.h
  vp8/decoder/detokenize.h
  vp8/decoder/onyxd_int.h
  vp8/decoder/treereader.h
  vp8/decoder/x86/dequantize_x86.h
  vpx/internal/vpx_codec_internal.h
  vpx/vp8cx.h
  vpx/vp8dx.h
  vpx/vp8e.h
  vpx/vp8.h
  vpx/vpx_codec.h
  vpx/vpx_codec_impl_bottom.h
  vpx/vpx_codec_impl_top.h
  vpx/vpx_decoder_compat.h
  vpx/vpx_decoder.h
  vpx/vpx_encoder.h
  vpx/vpx_image.h
  vpx/vpx_integer.h
  vpx_mem/include/vpx_mem_intrnl.h
  vpx_mem/vpx_mem.h
  vpx_ports/config.h
  vpx_ports/mem.h
  vpx_ports/vpx_timer.h
  vpx_ports/x86.h
  vpx_scale/scale_mode.h
  vpx_scale/vpxscale.h
  vpx_scale/yv12config.h
  vpx_scale/yv12extend.h
  vp8/common/x86/idctllm_mmx.asm
  vp8/common/x86/idctllm_sse2.asm
  vp8/common/x86/iwalsh_mmx.asm
  vp8/common/x86/iwalsh_sse2.asm
  vp8/common/x86/loopfilter_mmx.asm
  vp8/common/x86/loopfilter_sse2.asm
  vp8/common/x86/postproc_mmx.asm
  vp8/common/x86/postproc_sse2.asm
  vp8/common/x86/recon_mmx.asm
  vp8/common/x86/recon_sse2.asm
  vp8/common/x86/subpixel_mmx.asm
  vp8/common/x86/subpixel_sse2.asm
  vp8/common/x86/subpixel_ssse3.asm
  vp8/decoder/x86/dequantize_mmx.asm
  vpx_ports/emms.asm
  vpx_ports/x86_abi_support.asm
  LICENSE
  PATENTS
)

# configure files specific to x86-win32-vs8
cp $1/objdir/x86-win32-vs8/vpx_config.c vpx_config_x86-win32-vs8.c
cp $1/objdir/x86-win32-vs8/vpx_config.asm vpx_config_x86-win32-vs8.asm
cp $1/objdir/x86-win32-vs8/vpx_config.h vpx_config_x86-win32-vs8.h

# Should be same for all platforms...
cp $1/objdir/x86-win32-vs8/vpx_version.h vpx_version.h

# Config files for x86-linux-gcc and Solaris x86
cp $1/objdir/x86-linux-gcc/vpx_config.c vpx_config_x86-linux-gcc.c
cp $1/objdir/x86-linux-gcc/vpx_config.asm vpx_config_x86-linux-gcc.asm
cp $1/objdir/x86-linux-gcc/vpx_config.h vpx_config_x86-linux-gcc.h

# Config files for x86_64-linux-gcc and Solaris x86_64
cp $1/objdir/x86_64-linux-gcc/vpx_config.c vpx_config_x86-linux-gcc.c
cp $1/objdir/x86_64-linux-gcc/vpx_config.asm vpx_config_x86-linux-gcc.asm
cp $1/objdir/x86_64-linux-gcc/vpx_config.h vpx_config_x86-linux-gcc.h

# Copy config files for mac...
cp $1/objdir/x86-darwin9-gcc/vpx_config.c vpx_config_x86-darwin9-gcc.c
cp $1/objdir/x86-darwin9-gcc/vpx_config.asm vpx_config_x86-darwin9-gcc.asm
cp $1/objdir/x86-darwin9-gcc/vpx_config.h vpx_config_x86-darwin9-gcc.h

# Copy config files for Mac64
cp $1/objdir/x86_64-darwin9-gcc/vpx_config.c vpx_config_x86_64-darwin9-gcc.c
cp $1/objdir/x86_64-darwin9-gcc/vpx_config.asm vpx_config_x86_64-darwin9-gcc.asm
cp $1/objdir/x86_64-darwin9-gcc/vpx_config.h vpx_config_x86_64-darwin9-gcc.h

# Config files for generic-gnu
cp $1/objdir/generic-gnu/vpx_config.c vpx_config_generic-gnu.c
cp $1/objdir/generic-gnu/vpx_config.h vpx_config_generic-gnu.h

# Copy common source files into mozilla tree.
for f in ${commonFiles[@]}
do
  mkdir -p -v `dirname $f`
  cp -v $1/$f $f
done

# Patch to reduce compiler warnings, so we can compile with -Werror in mozilla.
patch -p3 < reduce-warnings-1.patch
patch -p3 < subpixel-qword.patch
# Patch to compile with Sun Studio on Solaris
patch -p3 < solaris.patch
# Patch to fix frame buffer reference counting and parition length overflow
#  checks.
patch -p3 < frame_buf_ref.patch
