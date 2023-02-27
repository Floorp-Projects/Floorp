package(default_visibility = ['//:__subpackages__'])

load('jxl_lists.bzl',
    'libjxl_codec_apng_sources',
    'libjxl_codec_gif_sources',
    'libjxl_codec_jpegli_sources',
    'libjxl_codec_jpg_sources',
    'libjxl_codec_jxl_sources',
    'libjxl_codec_npy_sources',
    'libjxl_codec_pgx_sources',
    'libjxl_codec_pnm_sources',
    'libjxl_dec_box_sources',
    'libjxl_dec_jpeg_sources',
    'libjxl_dec_sources',
    'libjxl_enc_sources',
    'libjxl_extras_for_tools_sources',
    'libjxl_extras_sources',
    'libjxl_jpegli_sources',
    'libjxl_jpegli_testlib_files',
    'libjxl_jpegli_tests',
    'libjxl_major_version',
    'libjxl_minor_version',
    'libjxl_patch_version',
    'libjxl_public_headers',
    'libjxl_testlib_files',
    'libjxl_tests',
    'libjxl_threads_public_headers',
    'libjxl_threads_sources',
    #'libjxl_codec_exr_sources',
    #'libjxl_gbench_sources',
    #'libjxl_profiler_sources',
)

load('jxl_vars.bzl',
    'libjxl_deps_brotli',
    'libjxl_deps_gif',
    'libjxl_deps_gtest',
    'libjxl_deps_hwy_nanobenchmark',
    'libjxl_deps_hwy_test_util',
    'libjxl_deps_hwy',
    'libjxl_deps_jpeg',
    'libjxl_deps_jxl_box',
    'libjxl_deps_png',
    'libjxl_deps_runfiles',
    'libjxl_deps_skcms',
    'libjxl_deps_testdata',
    'libjxl_root_package',
    'libjxl_test_shards',
    'libjxl_test_timeouts',
)

load('@bazel_skylib//rules:expand_template.bzl', 'expand_template')


EXPORT_TEMPLATE = '''
#ifndef @_EXPORT_H
#define @_EXPORT_H

#define @_EXPORT
#define @_NO_EXPORT

#ifndef @_DEPRECATED
#  define @_DEPRECATED __attribute__ ((__deprecated__))
#endif

#endif
'''

JXL_EXPORT_H = 'include/jxl/jxl_export.h'
genrule(
    name = 'create_jxl_export',
    outs = [JXL_EXPORT_H],
    cmd = 'echo \'' + EXPORT_TEMPLATE.replace('@', 'JXL') + '\' > $@',
)

JXL_THREADS_EXPORT_H = 'include/jxl/jxl_threads_export.h'
genrule(
    name = 'create_jxl_threads_export',
    outs = [JXL_THREADS_EXPORT_H],
    cmd = 'echo \'' + EXPORT_TEMPLATE.replace('@', 'JXL_THREADS') + '\' > $@',
)

JXL_VERSION_H = 'include/jxl/version.h'

# TODO(eustas): extract version from CMake file
expand_template(
    name = 'expand_jxl_version',
    template = 'jxl/version.h.in',
    out = JXL_VERSION_H,
    substitutions = {
        '@JPEGXL_MAJOR_VERSION@': str(libjxl_major_version),
        '@JPEGXL_MINOR_VERSION@': str(libjxl_minor_version),
        '@JPEGXL_PATCH_VERSION@': str(libjxl_patch_version),
    },
)

cc_library(
    name = 'jpegxl_version',
    hdrs = [JXL_VERSION_H],
    strip_include_prefix = 'include',
)

cc_library(
    name = 'jpegxl_private',
    srcs = libjxl_dec_sources + libjxl_dec_box_sources + libjxl_dec_jpeg_sources + libjxl_enc_sources,
    hdrs = libjxl_public_headers + [JXL_EXPORT_H],
    strip_include_prefix = 'include',
    defines = ['JPEGXL_ENABLE_SKCMS=1'],
    deps = [
        ':jpegxl_version',
    ] + libjxl_deps_brotli + libjxl_deps_hwy + libjxl_deps_skcms
)

cc_library(
    name = 'jpegxl_threads',
    srcs = libjxl_threads_sources,
    hdrs = libjxl_threads_public_headers + [JXL_THREADS_EXPORT_H],
    strip_include_prefix = 'include',
    # TODO(eustas): used only for [memory_manager.h, profiler.h, parallel_runner.h]
    deps = [':jpegxl_private'],
)

CODEC_FILES = libjxl_codec_apng_sources + libjxl_codec_gif_sources + libjxl_codec_jpegli_sources + libjxl_codec_jpg_sources + libjxl_codec_jxl_sources + libjxl_codec_npy_sources + libjxl_codec_pgx_sources + libjxl_codec_pnm_sources
CODEC_SRCS = [path for path in CODEC_FILES if path.endswith('.cc')]
CODEC_HDRS = [path for path in CODEC_FILES if path.endswith('.h')]

cc_library(
    name = 'jpegli',
    srcs = libjxl_jpegli_sources,
    hdrs = [
        'jpegli/common_internal.h', # TODO(eustas): should not be here
    ],
    deps = [
        ':jpegxl_private',
    ] + libjxl_deps_hwy + libjxl_deps_jpeg,
)

# TODO(eustas): build codecs separately?
cc_library(
    name = 'jpegxl_extras',
    srcs = libjxl_extras_sources + libjxl_extras_for_tools_sources + CODEC_SRCS,
    hdrs = CODEC_HDRS,
    defines = [
        'JPEGXL_ENABLE_APNG=1',
        'JPEGXL_ENABLE_EXR=0', # TODO(eustas): add
        'JPEGXL_ENABLE_GIF=1',
        'JPEGXL_ENABLE_JPEG=1',
        'JPEGXL_ENABLE_JPEGLI=1',
    ],
    deps = [
        ':jpegli',
        ':jpegxl_private',
        ':jpegxl_threads',
        ':jpegxl_version'
    ] + libjxl_deps_gif + libjxl_deps_jpeg + libjxl_deps_png
)

TESTLIB_FILES = libjxl_testlib_files + libjxl_jpegli_testlib_files

cc_library(
    name = 'test_utils',
    testonly = 1,
    srcs = [src for src in TESTLIB_FILES if not src.endswith('.h')],
    hdrs = [src for src in TESTLIB_FILES if src.endswith('.h')],
    defines = [
        'JPEGXL_ROOT_PACKAGE=\'"' + libjxl_root_package + '"\'',
    ],
    deps = [
        ':jpegxl_extras',
        ':jpegxl_private',
        ':jpegli',
    ] + libjxl_deps_runfiles
)

TESTS = [src.partition('.')[0] for src in libjxl_tests + libjxl_jpegli_tests]

[
    cc_test(
        name = test,
        srcs = [
            test + '.cc',
            'jxl/testing.h',
            'jpegli/testing.h',
        ],
        data = ['//:testdata'],
        timeout = libjxl_test_timeouts.get(test, 'moderate'),
        shard_count = libjxl_test_shards.get(test, 1),
        deps = [
            ':jpegxl_extras',
            ':jpegxl_private',
            ':jpegxl_threads',  # Some tests use the public threads API.
            ':test_utils',
        ] + libjxl_deps_gtest + libjxl_deps_hwy_test_util + libjxl_deps_hwy_nanobenchmark + libjxl_deps_jxl_box
    ) for test in TESTS
]
