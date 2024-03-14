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
