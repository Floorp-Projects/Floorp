##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
if (NOT AOM_BUILD_CMAKE_AOM_EXPERIMENT_DEPS_CMAKE_)
set(AOM_BUILD_CMAKE_AOM_EXPERIMENT_DEPS_CMAKE_ 1)

# Adjusts CONFIG_* CMake variables to address conflicts between active AV1
# experiments.
macro (fix_experiment_configs)
  if (CONFIG_ANALYZER)
    if (NOT CONFIG_INSPECTION)
      change_config_and_warn(CONFIG_INSPECTION 1 CONFIG_ANALYZER)
    endif ()
  endif ()

  if (CONFIG_VAR_TX_NO_TX_MODE AND NOT CONFIG_VAR_TX)
     change_config_and_warn(CONFIG_VAR_TX 1 CONFIG_VAR_TX_NO_TX_MODE)
  endif ()

  if (CONFIG_CHROMA_2X2)
    change_config_and_warn(CONFIG_CHROMA_SUB8X8 0 CONFIG_CHROMA_2X2)
  endif ()

  if (CONFIG_DAALA_TX)
     set(CONFIG_DAALA_DCT4 1)
     set(CONFIG_DAALA_DCT8 1)
     set(CONFIG_DAALA_DCT16 1)
     set(CONFIG_DAALA_DCT32 1)
     set(CONFIG_DAALA_DCT64 1)
  endif ()

  if (CONFIG_DAALA_DCT64)
    if (NOT CONFIG_TX64X64)
      set(CONFIG_DAALA_DCT64 0)
      message("--- DAALA_DCT64 requires TX64X64: disabled DAALA_DCT64")
    endif ()
  endif ()

  if (CONFIG_DAALA_DCT4 OR CONFIG_DAALA_DCT8 OR CONFIG_DAALA_DCT16 OR
      CONFIG_DAALA_DCT32 OR CONFIG_DAALA_DCT64)
    if (CONFIG_LGT)
      change_config_and_warn(CONFIG_LGT 0 CONFIG_DAALA_DCTx)
    endif ()
    if (NOT CONFIG_LOWBITDEPTH)
      change_config_and_warn(CONFIG_LOWBITDEPTH 1 CONFIG_DAALA_DCTx)
    endif ()
  endif ()

  if (CONFIG_TXK_SEL)
    if (NOT CONFIG_LV_MAP)
      change_config_and_warn(CONFIG_LV_MAP 1 CONFIG_TXK_SEL)
    endif ()
  endif ()

  if (CONFIG_CTX1D)
    if (NOT CONFIG_LV_MAP)
      change_config_and_warn(CONFIG_LV_MAP 1 CONFIG_CTX1D)
    endif ()
    if (NOT CONFIG_EXT_TX)
      change_config_and_warn(CONFIG_EXT_TX 1 CONFIG_CTX1D)
    endif ()
  endif ()

  if (CONFIG_EXT_COMP_REFS)
    if (NOT CONFIG_EXT_REFS)
      change_config_and_warn(CONFIG_EXT_REFS 1 CONFIG_EXT_COMP_REFS)
    endif ()
  endif ()

  if (CONFIG_STRIPED_LOOP_RESTORATION)
    if (NOT CONFIG_LOOP_RESTORATION)
      change_config_and_warn(CONFIG_LOOP_RESTORATION 1
                             CONFIG_STRIPED_LOOP_RESTORATION)
    endif ()
  endif ()

  if (CONFIG_MFMV)
    if (NOT CONFIG_FRAME_MARKER)
      change_config_and_warn(CONFIG_FRAME_MARKER 1 CONFIG_MFMV)
    endif ()
  endif ()

  if (CONFIG_NEW_MULTISYMBOL)
    if (NOT CONFIG_RESTRICT_COMPRESSED_HDR)
      change_config_and_warn(CONFIG_RESTRICT_COMPRESSED_HDR 1
                             CONFIG_NEW_MULTISYMBOL)
    endif ()
  endif ()

  if (CONFIG_EXT_PARTITION_TYPES)
    if (CONFIG_SUPERTX)
      change_config_and_warn(CONFIG_SUPERTX 0
                             CONFIG_EXT_PARTITION_TYPES)
    endif ()
  endif ()

  if (CONFIG_JNT_COMP)
    if (NOT CONFIG_FRAME_MARKER)
      change_config_and_warn(CONFIG_FRAME_MARKER 1 CONFIG_JNT_COMP)
    endif ()
  endif ()

  if (CONFIG_AMVR)
    change_config_and_warn(CONFIG_HASH_ME 1 CONFIG_AMVR)
  endif ()

  if (CONFIG_PVQ)
    if (CONFIG_EXT_TX)
      change_config_and_warn(CONFIG_EXT_TX 0 CONFIG_PVQ)
    endif ()
    if (CONFIG_HIGHBITDEPTH)
      change_config_and_warn(CONFIG_HIGHBITDEPTH 0 CONFIG_PVQ)
    endif ()
    if (CONFIG_PALETTE_THROUGHPUT)
      change_config_and_warn(CONFIG_PALETTE_THROUGHPUT 0 CONFIG_PVQ)
    endif ()
    if (CONFIG_RECT_TX)
      change_config_and_warn(CONFIG_RECT_TX 0 CONFIG_PVQ)
    endif ()
    if (CONFIG_VAR_TX)
      change_config_and_warn(CONFIG_VAR_TX 0 CONFIG_PVQ)
    endif ()
  endif ()

  if (CONFIG_HORZONLY_FRAME_SUPERRES)
    if (NOT CONFIG_FRAME_SUPERRES)
      change_config_and_warn(CONFIG_FRAME_SUPERRES 1 CONFIG_HORZONLY_FRAME_SUPERRES)
    endif ()
  endif ()
endmacro ()

endif ()  # AOM_BUILD_CMAKE_AOM_EXPERIMENT_DEPS_CMAKE_
