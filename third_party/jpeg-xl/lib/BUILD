package(default_visibility = ["//:__subpackages__"])

load('lib.bzl',
    DEC_SRCS = "libjxl_dec_sources",
    ENC_SRCS = "libjxl_enc_sources",
    EXTRAS_SRCS = "libjxl_extras_sources",
    # profiler.h is referenced in DEC_SRCS, and it is enough when it is off.
    # PROFILER_SRCS = "libjxl_profiler_sources",
    PUBLIC_HDRS = "libjxl_public_headers",
    TESTLIB_FILES = "libjxl_testlib_sources",
    TESTS_SRCS = "libjxl_tests_sources",
    THREADS_PUBLIC_HDRS = "libjxl_threads_public_headers",
    THREADS_SRCS = "libjxl_threads_sources",
    VERSION_DEFS = "libjxl_version_defines",
)

"""
libjxl_gbench_sources
libjxl_tests_sources
"""

load("@bazel_skylib//rules:expand_template.bzl", "expand_template")


EXPORT_TEMPLATE = """
#ifndef @_EXPORT_H
#define @_EXPORT_H

#define @_EXPORT
#define @_NO_EXPORT

#ifndef @_DEPRECATED
#  define @_DEPRECATED __attribute__ ((__deprecated__))
#endif

#endif
"""

JXL_EXPORT_H = "include/jxl/jxl_export.h"
genrule(
    name = "create_jxl_export",
    outs = [JXL_EXPORT_H],
    cmd = "echo \"" + EXPORT_TEMPLATE.replace('@', 'JXL') + "\" > $@",
)

JXL_THREADS_EXPORT_H = "include/jxl/jxl_threads_export.h"
genrule(
    name = "create_jxl_threads_export",
    outs = [JXL_THREADS_EXPORT_H],
    cmd = "echo \"" + EXPORT_TEMPLATE.replace('@', 'JXL_THREADS') + "\" > $@",
)

JXL_VERSION_H = "include/jxl/version.h"

VERSION_PARTS = { "@" + entry.partition("=")[0] + "@" : entry.partition("=")[2] for entry in VERSION_DEFS}

# TODO(eustas): extract version from CMake file
expand_template(
    name = "expand_jxl_version",
    template = "jxl/version.h.in",
    out = JXL_VERSION_H,
    substitutions = VERSION_PARTS,
)

cc_library(
    name = "jpegxl_version",
    hdrs = [JXL_VERSION_H],
    strip_include_prefix = "include",
)

cc_library(
    name = "jpegxl_private",
    srcs = DEC_SRCS + ENC_SRCS,
    hdrs = PUBLIC_HDRS + [JXL_EXPORT_H],
    strip_include_prefix = "include",
    defines = ["JPEGXL_ENABLE_SKCMS=1"],
    deps = [
        ":jpegxl_version",
        "@brotli//:brotlidec",
        "@brotli//:brotlienc",
        "@highway//:hwy",
        "@skcms//:skcms",
    ],
)

cc_library(
    name = "jpegxl_threads",
    srcs = THREADS_SRCS,
    hdrs = THREADS_PUBLIC_HDRS + [JXL_THREADS_EXPORT_H],
    strip_include_prefix = "include",
    # TODO(eustas): used only for [memory_manager.h, profiler.h, parallel_runner.h]
    deps = [":jpegxl_private"],
)

# TODO: move to lib.bzl
SUPPORTED_DECODERS = ["apng", "gif", "jpg"]
SUPPORTED_ENCODERS = ["apng", "jpg"]

EXTRAS_DEC_SRCS = ["extras/dec/" + FMT + ".cc" for FMT in SUPPORTED_DECODERS]
EXTRAS_DEC_HDRS = ["extras/dec/" + FMT + ".h" for FMT in SUPPORTED_DECODERS]

EXTRAS_ENC_SRCS = ["extras/enc/" + FMT + ".cc" for FMT in SUPPORTED_ENCODERS]
EXTRAS_ENC_HDRS = ["extras/enc/" + FMT + ".h" for FMT in SUPPORTED_ENCODERS]

cc_library(
    name = "jpegxl_extras",
    srcs = EXTRAS_SRCS + EXTRAS_DEC_SRCS + EXTRAS_ENC_SRCS,
    hdrs = EXTRAS_DEC_HDRS + EXTRAS_ENC_HDRS,
    defines = [
        "JPEGXL_ENABLE_APNG=1",
        "JPEGXL_ENABLE_EXR=0", # TODO(eustas): add
        "JPEGXL_ENABLE_GIF=1",
        "JPEGXL_ENABLE_JPEG=1",
    ],
    deps = [
        ":jpegxl_private",
        ":jpegxl_threads",
        ":jpegxl_version",
        "@gif//:gif",
        "@libjpeg_turbo//:jpeg",
        "@png//:png",
    ],
)

cc_library(
    name = "test_utils",
    testonly = 1,
    srcs = [src for src in TESTLIB_FILES if not src.endswith(".h")],
    hdrs = [src for src in TESTLIB_FILES if src.endswith(".h")],
    deps = [
        ":jpegxl_extras",
        ":jpegxl_private",
        "@bazel_tools//tools/cpp/runfiles",
    ],
)

TESTS = [src.partition("/")[2].partition(".")[0] for src in TESTS_SRCS]

# TODO(eustas): move to lib.bzl?
TEST_SHARDS = {
    "ans_test" : 2,
    "linalg_test" : 2,
    "modular_test" : 4,
    "roundtrip_test" : 4,
    "xorshift128plus_test" : 2,
    "ac_strategy_test" : 10, # TODO(eustas): separate heavy shard
    "dct_test" : 32,
    "decode_test" : 10, # TODO(eustas): separate heavy shard
    "fast_dct_test" : 8, # TODO(eustas): separate ultra-heavy shard
    "fast_math_test" : 10, # TODO(eustas): separate heavy shard
    "jxl_test" : 10, # TODO(eustas): separate heavy shard
    "render_pipeline/render_pipeline_test" : 10, # ? ->
}

# TODO(eustas): move to lib.bzl?
TEST_TIMEOUTS = {
    "fast_dct_test" : "long",
    "dct_test" : "long",
}

[
    cc_test(
        name = test,
        srcs = ["jxl/" + test + ".cc"],
        data = ["//:testdata"],
        timeout = TEST_TIMEOUTS.get(test, "moderate"),
        shard_count = TEST_SHARDS.get(test, 1),
        deps = [
            ":jpegxl_extras",
            ":jpegxl_private",
            ":jpegxl_threads",  # Some tests use the public threads API.
            ":test_utils",
            "@googletest//:gtest_main",
            "@highway//:hwy_test_util",
            "@highway//:nanobenchmark",
            "//tools:box",
        ],
    ) for test in TESTS
]
