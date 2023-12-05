#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Modified from chromium/src/third_party/libvpx/generate_gni.sh

# This script is used to generate sources.mozbuild and files in the
# config/platform directories needed to build libvpx.
# Every time libvpx source code is updated just run this script.
#
# Usage:
# $ ./generate_sources_mozbuild.sh

export LC_ALL=C
BASE_DIR=$(pwd)
LIBVPX_SRC_DIR="libvpx"
LIBVPX_CONFIG_DIR="config"
DISABLE_AVX="--disable-avx512"

# Print license header.
# $1 - Output base name
function write_license {
  echo "# This file is generated. Do not edit." >> $1
  echo "" >> $1
}

# Search for source files with the same basename in vp8, vp9, and vpx_dsp. The
# build does not support duplicate file names.
function find_duplicates {
  local readonly duplicate_file_names=$(find \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp \
    -type f -name \*.c  | xargs -I {} basename {} | sort | uniq -d \
  )

  if [ -n "${duplicate_file_names}" ]; then
    echo "ERROR: DUPLICATE FILES FOUND"
    for file in  ${duplicate_file_names}; do
      find \
        $BASE_DIR/$LIBVPX_SRC_DIR/vp8 \
        $BASE_DIR/$LIBVPX_SRC_DIR/vp9 \
        $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp \
        -name $file
    done
    exit 1
  fi
}

# Generate sources.mozbuild with a list of source files.
# $1 - Array name for file list. This is processed with 'declare' below to
#      regenerate the array locally.
# $2 - Variable name.
# $3 - Output file.
function write_sources {
  # Convert the first argument back in to an array.
  declare -a file_list=("${!1}")

  echo "  '$2': [" >> "$3"
  for f in $file_list
  do
    echo "    'libvpx/$f'," >> "$3"
  done
  echo "]," >> "$3"
}

# Convert a list of source files into sources.mozbuild.
# $1 - Input file.
# $2 - Output prefix.
function convert_srcs_to_project_files {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 3. Convert .asm.s to .asm because moz.build will do the conversion.

  local source_list=$(grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1)

  # Remove vpx_config.c.
  # The platform-specific vpx_config.c will be added into in moz.build later.
  source_list=$(echo "$source_list" | grep -v 'vpx_config\.c')

  # Remove include-only asm files (no object code emitted)
  source_list=$(echo "$source_list" | grep -v 'x86_abi_support\.asm')
  source_list=$(echo "$source_list" | grep -v 'config\.asm')

  # The actual ARM files end in .asm. We have rules to translate them to .S
  source_list=$(echo "$source_list" | sed s/\.asm\.s$/.asm/)

  # Exports - everything in vpx, vpx_mem, vpx_ports, vpx_scale
  local exports_list=$(echo "$source_list" | \
    egrep '^(vpx|vpx_mem|vpx_ports|vpx_scale)/.*h$')
  # but not anything in one level down, like 'internal'
  exports_list=$(echo "$exports_list" | egrep -v '/(internal|src)/')
  # or any of the other internal-ish header files.
  exports_list=$(echo "$exports_list" | egrep -v '/(emmintrin_compat.h|mem_.*|msvc.h|vpx_once.h)$')

  # Remove these files from the main list.
  source_list=$(comm -23 <(echo "$source_list") <(echo "$exports_list"))

  # Write a single file that includes all source files for all archs.
  local c_sources=$(echo "$source_list" | egrep '.(asm|c)$')
  local exports_sources=$(echo "$exports_list" | egrep '.h$')

  write_sources exports_sources ${2}_EXPORTS "$BASE_DIR/sources.mozbuild"
  write_sources c_sources ${2}_SOURCES "$BASE_DIR/sources.mozbuild"
}

# Clean files from previous make.
function make_clean {
  make clean > /dev/null
  rm -f libvpx_srcs.txt
}

# Print the configuration.
# $1 - Header file directory.
function print_config {
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
}

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
# $3 - Optional - any additional arguments to pass through.
function gen_rtcd_header {
  echo "Generate $LIBVPX_CONFIG_DIR/$1/*_rtcd.h files."

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm \
    -o $BASE_DIR/$TEMP_DIR/libvpx.config

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp8_rtcd $DISABLE_AVX $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8/common/rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp8_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp9_rtcd $DISABLE_AVX $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9/common/vp9_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp9_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_scale_rtcd $DISABLE_AVX $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_scale/vpx_scale_rtcd.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_scale_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_dsp_rtcd $DISABLE_AVX $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp/vpx_dsp_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_dsp_rtcd.h

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
}

# Generate Config files. "--enable-external-build" must be set to skip
# detection of capabilities on specific targets.
# $1 - Header file directory.
# $2 - Config command line.
function gen_config_files {
  ./configure $2 > /dev/null

  # Disable HAVE_UNISTD_H.
  ( echo '/HAVE_UNISTD_H'; echo 'd' ; echo 'w' ; echo 'q' ) | ed -s vpx_config.h

  local ASM_CONV=ads2gas.pl

  # Generate vpx_config.asm.
  if [[ "$1" == *x64* ]] || [[ "$1" == *ia32* ]]; then
    egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print "%define " $2 " " $3}' > vpx_config.asm
  else
    egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print $2 " EQU " $3}' | perl $BASE_DIR/$LIBVPX_SRC_DIR/build/make/$ASM_CONV > vpx_config.asm
  fi

  cp vpx_config.* $BASE_DIR/$LIBVPX_CONFIG_DIR/$1
  make_clean
  rm -rf vpx_config.*
}

find_duplicates

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

echo "Generate config files."
all_platforms="--enable-external-build --disable-examples --disable-install-docs --disable-unit-tests"
all_platforms="${all_platforms} --enable-multi-res-encoding --size-limit=8192x4608 --enable-pic"
all_platforms="${all_platforms} --disable-avx512"
x86_platforms="--enable-postproc --enable-vp9-postproc --as=yasm"
arm_platforms="--enable-runtime-cpu-detect --enable-realtime-only"
arm64_platforms="--enable-realtime-only"

gen_config_files linux/x64 "--target=x86_64-linux-gcc ${all_platforms} ${x86_platforms}"
gen_config_files linux/ia32 "--target=x86-linux-gcc ${all_platforms} ${x86_platforms}"
gen_config_files mac/x64 "--target=x86_64-darwin9-gcc ${all_platforms} ${x86_platforms}"
gen_config_files mac/ia32 "--target=x86-darwin9-gcc ${all_platforms} ${x86_platforms}"
gen_config_files win/x64 "--target=x86_64-win64-vs15 ${all_platforms} ${x86_platforms}"
gen_config_files win/ia32 "--target=x86-win32-gcc ${all_platforms} ${x86_platforms}"

gen_config_files linux/arm "--target=armv7-linux-gcc ${all_platforms} ${arm_platforms}"
gen_config_files linux/arm64 "--target=arm64-linux-gcc ${all_platforms} ${arm64_platforms}"
gen_config_files win/aarch64 "--target=arm64-win64-vs15 ${all_platforms} ${arm64_platforms}"

gen_config_files generic "--target=generic-gnu ${all_platforms}"

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

gen_rtcd_header linux/x64 x86_64
gen_rtcd_header linux/ia32 x86
gen_rtcd_header mac/x64 x86_64
gen_rtcd_header mac/ia32 x86
gen_rtcd_header win/x64 x86_64
gen_rtcd_header win/ia32 x86

gen_rtcd_header linux/arm armv7
gen_rtcd_header linux/arm64 arm64
gen_rtcd_header win/aarch64 arm64

gen_rtcd_header generic generic

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make_clean

# Remove existing source files.
rm -rf $BASE_DIR/sources.mozbuild
write_license $BASE_DIR/sources.mozbuild
echo "files = {" >> $BASE_DIR/sources.mozbuild

echo "Generate X86_64 source list."
config=$(print_config linux/x64)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libvpx_srcs.txt X64

# Copy vpx_version.h once. The file is the same for all platforms.
cp vpx_version.h $BASE_DIR/$LIBVPX_CONFIG_DIR

echo "Generate IA32 source list."
config=$(print_config linux/ia32)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libvpx_srcs.txt IA32

echo "Generate ARM source list."
config=$(print_config linux/arm)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libvpx_srcs.txt ARM

echo "Generate ARM64 source list."
config=$(print_config linux/arm64)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libvpx_srcs.txt ARM64

echo "Generate generic source list."
config=$(print_config generic)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libvpx_srcs.txt GENERIC

echo "}" >> $BASE_DIR/sources.mozbuild

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

cd $BASE_DIR/$LIBVPX_SRC_DIR

cd $BASE_DIR
