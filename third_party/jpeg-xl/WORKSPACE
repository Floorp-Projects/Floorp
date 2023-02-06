load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

http_archive(
    name = "bazel_skylib",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
)
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

local_repository(
    name = "highway",
    path = "third_party/highway",
)

local_repository(
    name = "brotli",
    path = "third_party/brotli",
)

new_local_repository(
    name = "googletest",
    path = "third_party/googletest",
    build_file = "third_party/googletest/BUILD.bazel",
)

new_local_repository(
    name = "skcms",
    path = "third_party/skcms",
    build_file_content = """
cc_library(
    name = "skcms",
    srcs = [
        "skcms.cc",
        "skcms_internal.h",
        "src/Transform_inl.h",
    ],
    hdrs = ["skcms.h"],
    visibility = ["//visibility:public"],
)
    """,
)

# TODO(eustas): zconf.h might be deleted after CMake build; consider fetching
#               pristine sources with hash taken from `deps.sh`.
new_local_repository(
    name = "zlib",
    path = "third_party/zlib",
    build_file_content = """
cc_library(
    name = "zlib",
    defines = ["HAVE_UNISTD_H"],
    srcs = [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "crc32.h",
        "deflate.c",
        "deflate.h",
        "gzclose.c",
        "gzguts.h",
        "gzlib.c",
        "gzread.c",
        "gzwrite.c",
        "infback.c",
        "inffast.c",
        "inffast.h",
        "inffixed.h",
        "inflate.c",
        "inflate.h",
        "inftrees.c",
        "inftrees.h",
        "trees.c",
        "trees.h",
        "uncompr.c",
        "zconf.h",
        "zutil.c",
        "zutil.h",
    ],
    hdrs = ["zlib.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
    """,
)

new_local_repository(
    name = "png",
    path = "third_party/libpng",
    build_file_content = """
genrule(
    name = "pnglibconf",
    srcs = ["scripts/pnglibconf.h.prebuilt"],
    outs = ["pnglibconf.h"],
    cmd = "cp -f $< $@",
)
cc_library(
    name = "png",
    srcs = [
        "png.c",
        "pngconf.h",
        "pngdebug.h",
        "pngerror.c",
        "pngget.c",
        "pnginfo.h",
        ":pnglibconf",
        "pngmem.c",
        "pngpread.c",
        "pngpriv.h",
        "pngread.c",
        "pngrio.c",
        "pngrtran.c",
        "pngrutil.c",
        "pngset.c",
        "pngstruct.h",
        "pngtrans.c",
        "pngwio.c",
        "pngwrite.c",
        "pngwtran.c",
        "pngwutil.c",
    ],
    hdrs = ["png.h"],
    includes = ["."],
    linkopts = ["-lm"],
    visibility = ["//visibility:public"],
    deps = ["@zlib//:zlib"],
)
    """,
)

new_git_repository(
    name = "libjpeg_turbo",
    remote = "https://github.com/libjpeg-turbo/libjpeg-turbo.git",
    tag = "2.1.4",
    build_file_content = """
load("@bazel_skylib//rules:expand_template.bzl", "expand_template")
SUBSTITUTIONS = {
    "@BITS_IN_JSAMPLE@" : "8",
    "@BUILD@" : "20230125",
    "@CMAKE_PROJECT_NAME@" : "libjpeg-turbo",
    "@COPYRIGHT_YEAR@" : "2023",
    "@INLINE@" : "__inline__",
    "@JPEG_LIB_VERSION@" : "62",
    "@LIBJPEG_TURBO_VERSION_NUMBER@" : "2001005",
    "@SIZE_T@" : "8",
    "@THREAD_LOCAL@" : "__thread",
    "@VERSION@" : "2.1.5",
}
YES_DEFINES = [
    "C_ARITH_CODING_SUPPORTED", "D_ARITH_CODING_SUPPORTED",
    "MEM_SRCDST_SUPPORTED", "HAVE_LOCALE_H", "HAVE_STDDEF_H", "HAVE_STDLIB_H",
    "NEED_SYS_TYPES_H", "HAVE_UNSIGNED_CHAR", "HAVE_UNSIGNED_SHORT",
    "HAVE_BUILTIN_CTZL"
]
NO_DEFINES = [
    "WITH_SIMD", "NEED_BSD_STRINGS", "INCOMPLETE_TYPES_BROKEN",
    "RIGHT_SHIFT_IS_UNSIGNED", "HAVE_INTRIN_H"
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
    ) for src in ["jconfig.h", "jconfigint.h", "jversion.h"]
]
cc_library(
    name = "jpeg",
    srcs = [
        "jaricom.c",
        "jcapimin.c",
        "jcapistd.c",
        "jcarith.c",
        "jccoefct.c",
        "jccolor.c",
        "jcdctmgr.c",
        "jchuff.c",
        "jchuff.h",
        "jcicc.c",
        "jcinit.c",
        "jcmainct.c",
        "jcmarker.c",
        "jcmaster.c",
        "jcomapi.c",
        "jconfig.h",
        "jconfigint.h",
        "jcparam.c",
        "jcphuff.c",
        "jcprepct.c",
        "jcsample.c",
        "jctrans.c",
        "jdapimin.c",
        "jdapistd.c",
        "jdarith.c",
        "jdatadst.c",
        "jdatasrc.c",
        "jdcoefct.c",
        "jdcoefct.h",
        "jdcolor.c",
        "jdct.h",
        "jddctmgr.c",
        "jdhuff.c",
        "jdhuff.h",
        "jdicc.c",
        "jdinput.c",
        "jdmainct.c",
        "jdmainct.h",
        "jdmarker.c",
        "jdmaster.c",
        "jdmaster.h",
        "jdmerge.c",
        "jdmerge.h",
        "jdphuff.c",
        "jdpostct.c",
        "jdsample.c",
        "jdsample.h",
        "jdtrans.c",
        "jerror.c",
        "jerror.h",
        "jfdctflt.c",
        "jfdctfst.c",
        "jfdctint.c",
        "jidctflt.c",
        "jidctfst.c",
        "jidctint.c",
        "jidctred.c",
        "jinclude.h",
        "jmemmgr.c",
        "jmemnobs.c",
        "jmemsys.h",
        "jmorecfg.h",
        "jpeg_nbits_table.h",
        "jpegcomp.h",
        "jpegint.h",
        "jpeglib.h",
        "jquant1.c",
        "jquant2.c",
        "jsimd_none.c",
        "jsimd.h",
        "jsimddct.h",
        "jutils.c",
        "jversion.h",
    ],
    hdrs = [
        "jccolext.c",
        "jdcol565.c",
        "jdcolext.c",
        "jdmrg565.c",
        "jdmrgext.c",
        "jerror.h",
        "jinclude.h",
        "jpeglib.h",
        "jstdhuff.c",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
    """
)

http_archive(
    name = "gif",
    url = "https://netcologne.dl.sourceforge.net/project/giflib/giflib-5.2.1.tar.gz",
    sha256 = "31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd",
    strip_prefix = "giflib-5.2.1",
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
    """
)
