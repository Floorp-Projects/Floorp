load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

workspace(name = "libjxl")

local_repository(
    name = "highway",
    path = "third_party/highway",
)

local_repository(
    name = "brotli",
    path = "third_party/brotli",
)

new_local_repository(
    name = "skcms",
    build_file_content = """
cc_library(
    name = "skcms",
    srcs = [
        "skcms.cc",
        "src/skcms_internals.h",
        "src/skcms_Transform.h",
        "src/Transform_inl.h",
    ],
    hdrs = ["skcms.h"],
    visibility = ["//visibility:public"],
)
    """,
    path = "third_party/skcms",
)

new_git_repository(
    name = "libjpeg_turbo",
    build_file_content = """
load("@bazel_skylib//rules:expand_template.bzl", "expand_template")
SUBSTITUTIONS = {
    "@BUILD@" : "20230208",
    "@CMAKE_PROJECT_NAME@" : "libjpeg-turbo",
    "@COPYRIGHT_YEAR@" : "2023",
    "@INLINE@" : "__inline__",
    "@JPEG_LIB_VERSION@" : "62",
    "@LIBJPEG_TURBO_VERSION_NUMBER@" : "2001091",
    "@SIZE_T@" : "8",
    "@THREAD_LOCAL@" : "__thread",
    "@VERSION@" : "2.1.91",
}
YES_DEFINES = [
    "C_ARITH_CODING_SUPPORTED", "D_ARITH_CODING_SUPPORTED",
    "HAVE_BUILTIN_CTZL", "MEM_SRCDST_SUPPORTED"
]
NO_DEFINES = [
    "WITH_SIMD", "RIGHT_SHIFT_IS_UNSIGNED", "HAVE_INTRIN_H"
]
SUBSTITUTIONS.update({
    "#cmakedefine " + key : "#define " + key for key in YES_DEFINES
})
SUBSTITUTIONS.update({
    "#cmakedefine " + key : "// #define " + key for key in NO_DEFINES
})
[
    expand_template(
        name = "expand_" + src,
        template = src + ".in",
        out = src,
        substitutions = SUBSTITUTIONS,
        visibility = ["//visibility:public"],
    ) for src in ["jconfig.h", "jconfigint.h", "jversion.h"]
]
JPEG16_SOURCES = [
    "jccolor.c",
    "jcdiffct.c",
    "jclossls.c",
    "jcmainct.c",
    "jcprepct.c",
    "jcsample.c",
    "jdcolor.c",
    "jddiffct.c",
    "jdlossls.c",
    "jdmainct.c",
    "jdmerge.c",
    "jdpostct.c",
    "jdsample.c",
    "jquant1.c",
    "jquant2.c",
    "jutils.c",
]
JPEG12_SOURCES = JPEG16_SOURCES + [
    "jccoefct.c",
    "jcdctmgr.c",
    "jdcoefct.c",
    "jddctmgr.c",
    "jfdctfst.c",
    "jfdctint.c",
    "jidctflt.c",
    "jidctfst.c",
    "jidctint.c",
    "jidctred.c",
]
JPEG_SOURCES = JPEG12_SOURCES + [
    "jaricom.c",
    "jcapimin.c",
    "jcapistd.c",
    "jcarith.c",
    "jchuff.c",
    "jcicc.c",
    "jcinit.c",
    "jclhuff.c",
    "jcmarker.c",
    "jcmaster.c",
    "jcomapi.c",
    "jcparam.c",
    "jcphuff.c",
    "jdapimin.c",
    "jdapistd.c",
    "jdarith.c",
    "jdatadst.c",
    "jdatasrc.c",
    "jdhuff.c",
    "jdicc.c",
    "jdinput.c",
    "jdlhuff.c",
    "jdmarker.c",
    "jdmaster.c",
    "jdphuff.c",
    "jdtrans.c",
    "jerror.c",
    "jfdctflt.c",
    "jmemmgr.c",
    "jmemnobs.c",
]
JPEG_HEADERS = [
    "jccolext.c",
    "jchuff.h",
    "jcmaster.h",
    "jconfig.h",
    "jconfigint.h",
    "jdcoefct.h",
    "jdcol565.c",
    "jdcolext.c",
    "jdct.h",
    "jdhuff.h",
    "jdmainct.h",
    "jdmaster.h",
    "jdmerge.h",
    "jdmrg565.c",
    "jdmrgext.c",
    "jdsample.h",
    "jerror.h",
    "jinclude.h",
    "jlossls.h",
    "jmemsys.h",
    "jmorecfg.h",
    "jpeg_nbits_table.h",
    "jpegapicomp.h",
    "jpegint.h",
    "jpeglib.h",
    "jsamplecomp.h",
    "jsimd.h",
    "jsimddct.h",
    "jstdhuff.c",
    "jversion.h",
]
cc_library(
    name = "jpeg16",
    srcs = JPEG16_SOURCES,
    hdrs = JPEG_HEADERS,
    local_defines = ["BITS_IN_JSAMPLE=16"],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "jpeg12",
    srcs = JPEG12_SOURCES,
    hdrs = JPEG_HEADERS,
    local_defines = ["BITS_IN_JSAMPLE=12"],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "jpeg",
    srcs = JPEG_SOURCES,
    hdrs = JPEG_HEADERS,
    deps = [":jpeg16", ":jpeg12"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
exports_files([
    "jmorecfg.h",
    "jpeglib.h",
])
    """,
    remote = "https://github.com/libjpeg-turbo/libjpeg-turbo.git",
    tag = "2.1.91",
)

http_archive(
    name = "gif",
    build_file_content = """
cc_library(
    name = "gif",
    srcs = [
        "dgif_lib.c", "egif_lib.c", "gifalloc.c", "gif_err.c", "gif_font.c",
        "gif_hash.c", "openbsd-reallocarray.c", "gif_hash.h",
        "gif_lib_private.h"
    ],
    hdrs = ["gif_lib.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
    """,
    sha256 = "31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd",
    strip_prefix = "giflib-5.2.1",
    url = "https://netcologne.dl.sourceforge.net/project/giflib/giflib-5.2.1.tar.gz",
)
