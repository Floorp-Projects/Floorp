package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Unused on Bazel builds, where this is not defined/known; Copybara replaces
# usages with an empty list.
COMPAT = [
    "//buildenv/target:non_prod",  # includes mobile/vendor.
]

cc_library(
    name = "vqsort",
    srcs = [
        # Split into separate files to reduce MSVC build time.
        "vqsort.cc",
        "vqsort_i16a.cc",
        "vqsort_i16d.cc",
        "vqsort_u16a.cc",
        "vqsort_u16d.cc",
        "vqsort_f32a.cc",
        "vqsort_f32d.cc",
        "vqsort_i32a.cc",
        "vqsort_i32d.cc",
        "vqsort_u32a.cc",
        "vqsort_u32d.cc",
        "vqsort_f64a.cc",
        "vqsort_f64d.cc",
        "vqsort_i64a.cc",
        "vqsort_i64d.cc",
        "vqsort_u64a.cc",
        "vqsort_u64d.cc",
        "vqsort_128a.cc",
        "vqsort_128d.cc",
    ],
    hdrs = [
        "disabled_targets.h",
        "vqsort.h",  # public interface
    ],
    compatible_with = [],
    textual_hdrs = [
        "shared-inl.h",
        "sorting_networks-inl.h",
        "traits-inl.h",
        "traits128-inl.h",
        "vqsort-inl.h",
    ],
    deps = [
        # Only if VQSORT_SECURE_RNG is set.
        # "//third_party/absl/random",
        "//:hwy",
    ],
)

# -----------------------------------------------------------------------------
# Internal-only targets

cc_library(
    name = "helpers",
    testonly = 1,
    textual_hdrs = [
        "algo-inl.h",
        "result-inl.h",
    ],
    deps = [
        ":vqsort",
        "//:nanobenchmark",
        # Required for HAVE_PDQSORT, but that is unused and this is
        # unavailable to Bazel builds, hence commented out.
        # "//third_party/boost/allowed",
        # Avoid ips4o and thus TBB to work around hwloc build failure.
    ],
)

cc_binary(
    name = "print_network",
    testonly = 1,
    srcs = ["print_network.cc"],
    deps = [
        ":helpers",
        ":vqsort",
        "//:hwy",
    ],
)

cc_test(
    name = "sort_test",
    size = "medium",
    srcs = ["sort_test.cc"],
    features = ["fully_static_link"],
    linkstatic = True,
    local_defines = ["HWY_IS_TEST"],
    # for test_suite.
    tags = ["hwy_ops_test"],
    deps = [
        ":helpers",
        ":vqsort",
        "@com_google_googletest//:gtest_main",
        "//:hwy",
        "//:hwy_test_util",
    ],
)

cc_binary(
    name = "bench_sort",
    testonly = 1,
    srcs = ["bench_sort.cc"],
    features = ["fully_static_link"],
    linkstatic = True,
    local_defines = ["HWY_IS_TEST"],
    deps = [
        ":helpers",
        ":vqsort",
        "@com_google_googletest//:gtest_main",
        "//:hwy",
        "//:hwy_test_util",
    ],
)

cc_binary(
    name = "bench_parallel",
    testonly = 1,
    srcs = ["bench_parallel.cc"],
    features = ["fully_static_link"],
    linkstatic = True,
    local_defines = ["HWY_IS_TEST"],
    deps = [
        ":helpers",
        ":vqsort",
        "@com_google_googletest//:gtest_main",
        "//:hwy",
        "//:hwy_test_util",
    ],
)
