#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import re
import shutil
import sys
import subprocess
from pprint import pprint
from StringIO import StringIO

PLATFORMS= [
  'x86-win32-vs8',
  'x86_64-win64-vs8',
  'x86-linux-gcc',
  'x86_64-linux-gcc',
  'generic-gnu',
  'x86-darwin9-gcc',
  'x86_64-darwin9-gcc',
  'armv7-android-gcc',
  'x86-win32-gcc',
  'x86_64-win64-gcc',
]


mk_files = [
    'vp8/vp8_common.mk',
    'vp8/vp8cx_arm.mk',
    'vp8/vp8cx.mk',
    'vp8/vp8dx.mk',
    'vp9/vp9_common.mk',
    'vp9/vp9cx.mk',
    'vp9/vp9dx.mk',
    'vpx_mem/vpx_mem.mk',
    'vpx_ports/vpx_ports.mk',
    'vpx_scale/vpx_scale.mk',
    'vpx/vpx_codec.mk',
]

extensions = ['.asm', '.c', '.h']

MODULES = {
    'UNIFIED_SOURCES': [
        'API_DOC_SRCS-$(CONFIG_VP8_DECODER)',
        'API_DOC_SRCS-yes',
        'API_EXPORTS',
        'API_SRCS-$(CONFIG_VP8_DECODER)',
        'API_SRCS-yes',
        'MEM_SRCS-yes',
        'PORTS_SRCS-yes',
        'SCALE_SRCS-$(CONFIG_SPATIAL_RESAMPLING)',
        'SCALE_SRCS-no',
        'SCALE_SRCS-yes',
        'VP8_COMMON_SRCS-yes',
        'VP8_DX_EXPORTS',
        'VP8_DX_SRCS-$(CONFIG_MULTITHREAD)',
        'VP8_DX_SRCS-no',
        'VP8_DX_SRCS_REMOVE-no',
        'VP8_DX_SRCS_REMOVE-yes',
        'VP8_DX_SRCS-yes',
        'VP9_COMMON_SRCS-yes',
        'VP9_DX_EXPORTS',
        'VP9_DX_SRCS-no',
        'VP9_DX_SRCS_REMOVE-no',
        'VP9_DX_SRCS_REMOVE-yes',
        'VP9_DX_SRCS-yes',
        'API_DOC_SRCS-$(CONFIG_VP8_ENCODER)',
        'API_SRCS-$(BUILD_LIBVPX)',
        'API_SRCS-$(CONFIG_VP8_ENCODER)',
        'API_SRCS-$(CONFIG_VP9_ENCODER)',
        'VP8_CX_EXPORTS',
        'VP8_CX_SRCS-$(CONFIG_MULTI_RES_ENCODING)',
        'VP8_CX_SRCS-$(CONFIG_MULTITHREAD)',
        'VP8_CX_SRCS-$(CONFIG_TEMPORAL_DENOISING)',
        'VP8_CX_SRCS-no',
        'VP8_CX_SRCS_REMOVE-no',
        'VP8_CX_SRCS_REMOVE-yes',
        'VP8_CX_SRCS-yes',
        'VP9_CX_EXPORTS',
        'VP9_CX_SRCS-no',
        'VP9_CX_SRCS_REMOVE-no',
        'VP9_CX_SRCS_REMOVE-yes',
        'VP9_CX_SRCS-yes',
    ],
    'X86_ASM': [
        'PORTS_SRCS-$(BUILD_LIBVPX)',
        'VP8_COMMON_SRCS-$(ARCH_X86)$(ARCH_X86_64)',
        'VP8_COMMON_SRCS-$(HAVE_MMX)',
        'VP8_COMMON_SRCS-$(HAVE_SSE2)',
        'VP8_COMMON_SRCS-$(HAVE_SSE3)',
        'VP8_COMMON_SRCS-$(HAVE_SSE4_1)',
        'VP8_COMMON_SRCS-$(HAVE_SSSE3)',
        'VP9_COMMON_SRCS-$(ARCH_X86)$(ARCH_X86_64)',
        'VP9_COMMON_SRCS-$(HAVE_MMX)',
        'VP9_COMMON_SRCS-$(HAVE_SSE2)',
        'VP9_COMMON_SRCS-$(HAVE_SSSE3)',
        'VP8_CX_SRCS-$(ARCH_X86)$(ARCH_X86_64)',
        'VP8_CX_SRCS-$(HAVE_MMX)',
        'VP8_CX_SRCS-$(HAVE_SSE2)',
        'VP8_CX_SRCS-$(HAVE_SSE4_1)',
        'VP8_CX_SRCS-$(HAVE_SSSE3)',
        'VP8_CX_SRCS_REMOVE-$(HAVE_SSE2)',
        'VP9_CX_SRCS-$(ARCH_X86)$(ARCH_X86_64)',
        'VP9_CX_SRCS-$(HAVE_MMX)',
        'VP9_CX_SRCS-$(HAVE_SSE2)',
        'VP9_CX_SRCS-$(HAVE_SSE3)',
        'VP9_CX_SRCS-$(HAVE_SSE4_1)',
        'VP9_CX_SRCS-$(HAVE_SSSE3)',
    ],
    'X86-64_ASM': [
        'VP8_CX_SRCS-$(ARCH_X86_64)',
        'VP9_CX_SRCS-$(ARCH_X86_64)',
    ],
    'ARM_ASM': [
        'PORTS_SRCS-$(ARCH_ARM)',
        'SCALE_SRCS-$(HAVE_NEON)',
        'VP8_COMMON_SRCS-$(ARCH_ARM)',
        'VP8_COMMON_SRCS-$(HAVE_MEDIA)',
        'VP8_COMMON_SRCS-$(HAVE_NEON)',
        'VP9_COMMON_SRCS-$(HAVE_NEON)',
        'VP9_COMMON_SRCS-$(HAVE_NEON_ASM)',
        'VP8_CX_SRCS-$(ARCH_ARM)',
        'VP8_CX_SRCS-$(HAVE_EDSP)',
        'VP8_CX_SRCS-$(HAVE_MEDIA)',
        'VP8_CX_SRCS-$(HAVE_NEON)',
        'VP8_CX_SRCS-$(HAVE_NEON_ASM)',
        'VP9_CX_SRCS-$(HAVE_NEON)',
    ],
    'ERROR_CONCEALMENT': [
        'VP8_DX_SRCS-$(CONFIG_ERROR_CONCEALMENT)',
    ],
    'AVX2': [
        'VP9_COMMON_SRCS-$(HAVE_AVX2)',
        'VP9_CX_SRCS-$(HAVE_AVX2)',
    ],
    'VP8_POSTPROC': [
        'VP8_COMMON_SRCS-$(CONFIG_POSTPROC)',
    ],
    'VP9_POSTPROC': [
        'VP9_COMMON_SRCS-$(CONFIG_VP9_POSTPROC)',
    ]
}

DISABLED_MODULES = [
    'API_SRCS-$(CONFIG_SPATIAL_SVC)',
    'MEM_SRCS-$(CONFIG_MEM_MANAGER)',
    'MEM_SRCS-$(CONFIG_MEM_TRACKER)',
    'VP8_COMMON_SRCS-$(CONFIG_POSTPROC_VISUALIZER)',
    'VP9_COMMON_SRCS-$(CONFIG_POSTPROC_VISUALIZER)',
    'VP8_CX_SRCS-$(CONFIG_INTERNAL_STATS)',
    'VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS)',
    'VP9_CX_SRCS-$(CONFIG_VP9_TEMPORAL_DENOISING)',

    # mips files are also ignored via ignored_folders
    'SCALE_SRCS-$(HAVE_DSPR2)',
    'VP8_COMMON_SRCS-$(HAVE_DSPR2)',
    'VP9_COMMON_SRCS-$(HAVE_DSPR2)',
    'VP8_CX_SRCS_REMOVE-$(HAVE_EDSP)',
]

libvpx_files = [
    'build/make/obj_int_extract.c',
    'build/make/ads2gas.pl',
    'build/make/thumb.pm',
    'LICENSE',
    'PATENTS',
]

ignore_files = [
    'vp8/common/context.c',
    'vp8/common/textblit.c',
    'vp8/encoder/ssim.c',
    'vp8/encoder/x86/ssim_opt.asm',
    'vp9/common/vp9_textblit.c',
    'vp9/common/vp9_textblit.h',
    'vp9/encoder/vp9_ssim.c',
    'vp9/encoder/x86/vp9_ssim_opt.asm',
    'vpx_mem/vpx_mem_tracker.c',
    'vpx_scale/generic/bicubic_scaler.c',
    'vpx_scale/win32/scaleopt.c',
    'vpx_scale/win32/scalesystemdependent.c',
]

ignore_folders = [
    'examples/',
    'googletest/',
    'libmkv/',
    'libyuv/',
    'mips/',
    'nestegg/',
    'objdir/',
    'ppc/',
    'test/',
    'vpx_mem/memory_manager/',
]
files = {
    'EXPORTS': [
        'vpx_mem/include/vpx_mem_intrnl.h',
        'vpx_mem/vpx_mem.h',
        'vpx_ports/arm.h',
        'vpx_ports/mem.h',
        'vpx_ports/vpx_timer.h',
        'vpx_ports/x86.h',
        'vpx_scale/vpx_scale.h',
        'vpx_scale/yv12config.h',
        'vpx/vp8cx.h',
        'vpx/vp8dx.h',
        'vpx/vp8.h',
        'vpx/vpx_codec.h',
        'vpx/vpx_decoder.h',
        'vpx/vpx_encoder.h',
        'vpx/vpx_frame_buffer.h',
        'vpx/vpx_image.h',
        'vpx/vpx_integer.h',
    ],
    'X86-64_ASM': [
        'third_party/x86inc/x86inc.asm',
        'vp8/common/x86/loopfilter_block_sse2_x86_64.asm',
        'vp9/encoder/x86/vp9_quantize_ssse3_x86_64.asm',
    ],
    'SOURCES': [
        'vp8/common/rtcd.c',
        'vp8/common/sad_c.c',
        'vp8/encoder/bitstream.c',
        'vp8/encoder/onyx_if.c',
        'vp8/vp8_dx_iface.c',
        'vp9/common/vp9_alloccommon.c',
        'vp9/common/vp9_blockd.c',
        'vp9/common/vp9_common_data.c',
        'vp9/common/vp9_convolve.c',
        'vp9/common/vp9_debugmodes.c',
        'vp9/common/vp9_entropy.c',
        'vp9/common/vp9_entropymode.c',
        'vp9/common/vp9_entropymv.c',
        'vp9/common/vp9_filter.c',
        'vp9/common/vp9_frame_buffers.c',
        'vp9/common/vp9_idct.c',
        'vp9/common/vp9_loopfilter.c',
        'vp9/common/vp9_loopfilter_filters.c',
        'vp9/common/vp9_mvref_common.c',
        'vp9/common/vp9_pred_common.c',
        'vp9/common/vp9_prob.c',
        'vp9/common/vp9_quant_common.c',
        'vp9/common/vp9_reconinter.c',
        'vp9/common/vp9_reconintra.c',
        'vp9/common/vp9_rtcd.c',
        'vp9/common/vp9_scale.c',
        'vp9/common/vp9_scan.c',
        'vp9/common/vp9_seg_common.c',
        'vp9/common/vp9_thread.c',
        'vp9/common/vp9_tile_common.c',
        'vp9/decoder/vp9_decodeframe.c',
        'vp9/decoder/vp9_decodemv.c',
        'vp9/decoder/vp9_decoder.c',
        'vp9/decoder/vp9_detokenize.c',
        'vp9/decoder/vp9_dsubexp.c',
        'vp9/decoder/vp9_dthread.c',
        'vp9/decoder/vp9_reader.c',
        'vp9/encoder/vp9_bitstream.c',
        'vp9/encoder/vp9_aq_complexity.c',
        'vp9/encoder/vp9_aq_cyclicrefresh.c',
        'vp9/encoder/vp9_aq_variance.c',
        'vp9/encoder/vp9_context_tree.c',
        'vp9/encoder/vp9_cost.c',
        'vp9/encoder/vp9_dct.c',
        'vp9/encoder/vp9_encodeframe.c',
        'vp9/encoder/vp9_encodemb.c',
        'vp9/encoder/vp9_encodemv.c',
        'vp9/encoder/vp9_encoder.c',
        'vp9/encoder/vp9_extend.c',
        'vp9/encoder/vp9_firstpass.c',
        'vp9/encoder/vp9_lookahead.c',
        'vp9/encoder/vp9_mbgraph.c',
        'vp9/encoder/vp9_mcomp.c',
        'vp9/encoder/vp9_picklpf.c',
        'vp9/encoder/vp9_pickmode.c',
        'vp9/encoder/vp9_quantize.c',
        'vp9/encoder/vp9_ratectrl.c',
        'vp9/encoder/vp9_rd.c',
        'vp9/encoder/vp9_rdopt.c',
        'vp9/encoder/vp9_resize.c',
        'vp9/encoder/vp9_sad.c',
        'vp9/encoder/vp9_segmentation.c',
        'vp9/encoder/vp9_speed_features.c',
        'vp9/encoder/vp9_subexp.c',
        'vp9/encoder/vp9_svc_layercontext.c',
        'vp9/encoder/vp9_temporal_filter.c',
        'vp9/encoder/vp9_tokenize.c',
        'vp9/encoder/vp9_treewriter.c',
        'vp9/encoder/vp9_variance.c',
        'vp9/encoder/vp9_write_bit_buffer.c',
        'vp9/encoder/vp9_writer.c',
        'vp9/vp9_cx_iface.c',
        'vp9/vp9_dx_iface.c',
        'vpx/src/svc_encodeframe.c',
        'vpx/src/vpx_encoder.c',
        'vpx_mem/vpx_mem.c',
        'vpx_scale/vpx_scale_rtcd.c',
        'vpx_scale/generic/yv12config.c',
        'vpx_scale/generic/yv12extend.c',
    ]
}

manual = [
    # special case in moz.build
    'vp8/encoder/boolhuff.c',

    # 64bit only
    'vp8/common/x86/loopfilter_block_sse2_x86_64.asm',
    'vp9/encoder/x86/vp9_quantize_ssse3_x86_64.asm',

    # offsets are special cased in Makefile.in
    'vp8/encoder/vp8_asm_enc_offsets.c',
    'vpx_scale/vpx_scale_asm_offsets.c',

    # ignore while vp9 postproc is not enabled
    'vp9/common/x86/vp9_postproc_mmx.asm',
    'vp9/common/x86/vp9_postproc_sse2.asm',

    # ssim_opt is not enabled
    'vp8/encoder/x86/ssim_opt.asm',
    'vp9/encoder/x86/vp9_ssim_opt.asm',

    # asm includes
    'vpx_ports/x86_abi_support.asm',
]

platform_files = [
    'vp8_rtcd.h',
    'vp9_rtcd.h',
    'vpx_config.asm',
    'vpx_config.h',
    'vpx_scale_rtcd.h',
]

def prepare_upstream(prefix, commit=None):
    if os.path.exists(prefix):
        print "Please remove '%s' folder before running %s" % (prefix, sys.argv[0])
        sys.exit(1)

    upstream_url = 'https://gerrit.chromium.org/gerrit/webm/libvpx'
    subprocess.call(['git', 'clone', upstream_url, prefix])
    if commit:
        os.chdir(prefix)
        subprocess.call(['git', 'checkout', commit])
    else:
        os.chdir(prefix)
        p = subprocess.Popen(['git', 'rev-parse', 'HEAD'], stdout=subprocess.PIPE)
        stdout, stderr = p.communicate()
        commit = stdout.strip()

    for target in PLATFORMS:
        target_objdir = os.path.join(prefix, 'objdir', target)
        os.makedirs(target_objdir)
        os.chdir(target_objdir)
        configure = ['../../configure', '--target=%s' % target,
            '--disable-examples', '--disable-install-docs',
            '--enable-multi-res-encoding',
        ]

        if 'darwin9' in target:
            configure += ['--enable-pic']
        if 'linux' in target:
            configure += ['--enable-pic']
            configure += ['--disable-avx2']
        # x86inc.asm is not compatible with pic 32bit builds
        if target == 'x86-linux-gcc':
            configure += ['--disable-use-x86inc']

        if target == 'armv7-android-gcc':
            configure += ['--sdk-path=%s' % ndk_path]

        subprocess.call(configure)
        make_targets = [f for f in platform_files if not os.path.exists(f)]
        if make_targets:
            subprocess.call(['make'] + make_targets)
        for f in make_targets:
            if not os.path.exists(f):
                print "%s missing from %s, check toolchain" % (f, target)
                sys.exit(1)

    os.chdir(base)
    return commit

def cleanup_upstream():
    shutil.rmtree(os.path.join(base, 'upstream'))

def get_module(key):
    for module in MODULES:
        if key in MODULES[module]:
            return module

def get_libvpx_files(prefix):
    for root, folders, files in os.walk(prefix):
        for f in files:
            f = os.path.join(root, f)[len(prefix):]
            if os.path.splitext(f)[-1] in extensions \
              and os.sep in f \
              and f not in ignore_files \
              and not any(folder in f for folder in ignore_folders):
                libvpx_files.append(f)
    return libvpx_files

def get_sources(prefix):
    source = {}
    unknown = {}
    disabled = {}

    for mk in mk_files:
        with open(os.path.join(prefix, mk)) as f:
            base = os.path.dirname(mk)
            for l in f:
                if '+=' in l:
                    l = l.split('+=')
                    key = l[0].strip()
                    value = l[1].strip().replace('$(ASM)', '.asm')
                    value = os.path.join(base, value)
                    if not key.startswith('#') and os.path.splitext(value)[-1] in extensions:
                        if key not in source:
                            source[key] = []
                        source[key].append(value)

    for key in source:
        for f in source[key]:
            if key.endswith('EXPORTS') and f.endswith('.h'):
                files['EXPORTS'].append(f)
            if os.path.splitext(f)[-1] in ('.c', '.asm') and not f in manual:
                module = get_module(key)
                if module:
                    if not module in files:
                        files[module] = []
                    t = files[module]
                elif key in DISABLED_MODULES:
                    if not key in disabled:
                        disabled[key] = []
                    t = disabled[key]
                else:
                    if not key in unknown:
                        unknown[key] = []
                    t = unknown[key]
                t.append(f)

    files['UNIFIED_SOURCES'] = [f for f in files['UNIFIED_SOURCES'] if f not in files['SOURCES']]

    for key in files:
        files[key] = list(sorted(set(files[key])))

    return source, files, disabled, unknown

def update_sources_mozbuild(files, sources_mozbuild):
    f = StringIO()
    pprint(files, stream=f)
    sources_mozbuild_new = "files = {\n %s\n}\n" % f.getvalue().strip()[1:-1]
    if sources_mozbuild != sources_mozbuild_new:
        print 'updating sources.mozbuild'
        with open('sources.mozbuild', 'w') as f:
            f.write(sources_mozbuild_new)

def get_current_files():
    current_files = []
    for root, folders, files in os.walk('.'):
        for f in files:
            f = os.path.join(root, f)[len('.%s'%os.sep):]
            if 'upstream%s'%os.sep in f or not os.sep in f:
                continue
            if os.path.splitext(f)[-1] in extensions:
                current_files.append(f)
    return current_files

def is_new(a, b):
    return not os.path.exists(a) \
        or not os.path.exists(b) \
        or open(a).read() != open(b).read()

def get_sources_mozbuild():
    with open('sources.mozbuild') as f:
        sources_mozbuild = f.read()
    exec(sources_mozbuild)
    return sources_mozbuild, files

def update_and_remove_files(prefix, libvpx_files, files):
    current_files = get_current_files()

    def copy(src, dst):
        print '    ', dst
        shutil.copy(src, dst)

    # Update files
    first = True
    for f in libvpx_files:
        fdir = os.path.dirname(f)
        if fdir and not os.path.exists(fdir):
            os.makedirs(fdir)
        s = os.path.join(prefix, f)
        if is_new(f, s):
            if first:
                print "Copy files:"
                first = False
            copy(s, f)

    # Copy configuration files for each platform
    for target in PLATFORMS:
        first = True
        for f in platform_files:
            t = os.path.splitext(f)
            t = '%s_%s%s' % (t[0], target, t[1])
            f = os.path.join(prefix, 'objdir', target, f)
            if is_new(f, t):
                if first:
                    print "Copy files for %s:" % target
                    first = False
                copy(f, t)

    # Copy vpx_version.h from one of the build targets
    s = os.path.join(prefix, 'objdir/x86-linux-gcc/vpx_version.h')
    f = 'vpx_version.h'
    if is_new(s, f):
        copy(s, f)

    # Remove unknown files from tree
    removed_files = [f for f in current_files if f not in libvpx_files]
    if removed_files:
        print "Remove files:"
        for f in removed_files:
            os.unlink(f)
            print '    ', f

def apply_patches():
    # Patch to permit vpx users to specify their own <stdint.h> types.
    os.system("patch -p0 < stdint.patch")
    # Patch to allow older versions of Apple's clang to build libvpx.
    os.system("patch -p3 < apple-clang.patch")

def update_readme(commit):
    with open('README_MOZILLA') as f:
        readme = f.read()

    if 'The git commit ID used was' in readme:
        new_readme = re.sub('The git commit ID used was [a-f0-9]+',
            'The git commit ID used was %s' % commit, readme)
    else:
        new_readme = "%s\n\nThe git commit ID used was %s\n" % (readme, commit)

    if readme != new_readme:
        with open('README_MOZILLA', 'w') as f:
            f.write(new_readme)

def print_info(source, files, disabled, unknown, moz_build_files):
    for key in moz_build_files:
        if key not in files:
            print key, 'MISSING'
        else:
            gone = set(moz_build_files[key]) - set(files[key])
            new = set(files[key]) - set(moz_build_files[key])
            if gone:
                print key, 'GONE:'
                print '    '+ '\n    '.join(gone)
            if new:
                print key, 'NEW:'
                print '    '+ '\n    '.join(new)

    if unknown:
        print "Please update this script, the following modules are unknown"
        pprint(unknown)

    if DEBUG:
        print "===== SOURCE"
        pprint(source)
        print "===== FILES"
        pprint(files)
        print "===== DISABLED"
        pprint(disabled)
        print "===== UNKNOWN"
        pprint(unknown)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='''This script only works on Mac OS X since the OS X Toolchain is not available on other platforms.
In addition you need XCode and the Android NDK installed.
If commit hash is not provided, current git master is used.''')
    parser.add_argument('--debug', dest='debug', action="store_true")
    parser.add_argument('--ndk', dest='ndk', type=str)
    parser.add_argument('--commit', dest='commit', type=str, default=None)

    args = parser.parse_args()

    if sys.platform != 'darwin' or not args.ndk:
        parser.print_help()
        sys.exit(1)

    ndk_path = args.ndk
    commit = args.commit
    DEBUG = args.debug

    base = os.path.abspath(os.curdir)
    prefix = os.path.join(base, 'upstream/')

    commit = prepare_upstream(prefix, commit)

    libvpx_files = get_libvpx_files(prefix)
    source, files, disabled, unknown = get_sources(prefix)

    sources_mozbuild, moz_build_files = get_sources_mozbuild()

    print_info(source, files, disabled, unknown, moz_build_files)
    update_sources_mozbuild(files, sources_mozbuild)
    update_and_remove_files(prefix, libvpx_files, files)
    apply_patches()
    update_readme(commit)

    cleanup_upstream()
