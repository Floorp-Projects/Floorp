# coding: utf-8
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import tempfile
import unittest


from mozunit import main
import mozbuild.vendor.rewrite_mozbuild as mu

SAMPLE_PIXMAN_MOZBUILD = """
if CONFIG['OS_ARCH'] != 'Darwin' and CONFIG['CC_TYPE'] in ('clang', 'gcc'):
    if CONFIG['HAVE_ARM_NEON']:
        SOURCES += [
            "pixman-arm-neon-asm-bilinear.S",
            "pixman-arm-neon-asm.S",
        ]
    if CONFIG['HAVE_ARM_SIMD']:
        SOURCES += [
            'pixman-arm-simd-asm-scaled.S',
            'pixman-arm-simd-asm.S']

SOURCES += ['pixman-region32.c',
    'pixman-solid-fill.c',
    'pixman-trap.c',
    'pixman-utils.c',
    'pixman-x86.c',
    'pixman.c',
]

if use_sse2:
    DEFINES['USE_SSE'] = True
    DEFINES['USE_SSE2'] = True
    SOURCES += ['pixman-sse2.c']
    SOURCES['pixman-sse2.c'].flags += CONFIG['SSE_FLAGS'] + CONFIG['SSE2_FLAGS']
    if CONFIG['CC_TYPE'] in ('clang', 'gcc'):
        SOURCES['pixman-sse2.c'].flags += ['-Winline']
"""

SAMPLE_DAV1D_MOZBUILD = """
SOURCES += [
    '../../third_party/dav1d/src/cdf.c',
    '../../third_party/dav1d/src/cpu.c',
    ]
EXPORTS = [
    '../../third_party/dav1d/src/header1.h',
    '../../third_party/dav1d/src/header2.h',
    ]
"""


SAMPLE_JPEGXL_MOZBUILD = """
SOURCES += [
    "/third_party/jpeg-xl/lib/jxl/ac_strategy.cc",
    "/third_party/jpeg-xl/lib/jxl/alpha.cc",
    "/third_party/jpeg-xl/lib/jxl/ans_common.cc",
    "/third_party/jpeg-xl/lib/jxl/aux_out.cc",
    ]
EXPORTS.bob.carol = [
    "/third_party/jpeg-xl/lib/jxl/header1.hpp",
    "/third_party/jpeg-xl/lib/jxl/header2.h",
]
"""


def _make_mozbuild_directory_structure(mozbuild_path, contents):
    d = tempfile.TemporaryDirectory()
    os.makedirs(os.path.join(d.name, os.path.split(mozbuild_path)[0]))

    arcconfig = open(os.path.join(d.name, ".arcconfig"), mode="w")
    arcconfig.close()

    mozbuild = open(os.path.join(d.name, mozbuild_path), mode="w")
    mozbuild.write(contents)
    mozbuild.close()

    return d


class TestUtils(unittest.TestCase):
    def test_normalize_filename(self):
        self.assertEqual(mu.normalize_filename("foo/bar/moz.build", "/"), "/")
        self.assertEqual(
            mu.normalize_filename("foo/bar/moz.build", "a.c"), "foo/bar/a.c"
        )
        self.assertEqual(
            mu.normalize_filename("foo/bar/moz.build", "baz/a.c"), "foo/bar/baz/a.c"
        )
        self.assertEqual(mu.normalize_filename("foo/bar/moz.build", "/a.c"), "/a.c")

    def test_unnormalize_filename(self):
        test_vectors = [
            ("foo/bar/moz.build", "/"),
            ("foo/bar/moz.build", "a.c"),
            ("foo/bar/moz.build", "baz/a.c"),
            ("foo/bar/moz.build", "/a.c"),
        ]

        for vector in test_vectors:
            mozbuild, file = vector
            self.assertEqual(
                mu.unnormalize_filename(
                    mozbuild, mu.normalize_filename(mozbuild, file)
                ),
                file,
            )

    def test_find_all_posible_assignments_from_filename(self):
        test_vectors = [
            # (
            # target_filename_normalized
            # source_assignments
            # expected
            # )
            (
                "root/dir/asm/blah.S",
                {
                    "> SOURCES": ["root/dir/main.c"],
                    "> if conditional > SOURCES": ["root/dir/asm/blah.S"],
                },
                {"> if conditional > SOURCES": ["root/dir/asm/blah.S"]},
            ),
            (
                "root/dir/dostuff.c",
                {
                    "> SOURCES": ["root/dir/main.c"],
                    "> if conditional > SOURCES": ["root/dir/asm/blah.S"],
                },
                {"> SOURCES": ["root/dir/main.c"]},
            ),
        ]

        for vector in test_vectors:
            target_filename_normalized, source_assignments, expected = vector
            actual = mu.find_all_posible_assignments_from_filename(
                source_assignments, target_filename_normalized
            )
            self.assertEqual(actual, expected)

    def test_filenames_directory_is_in_filename_list(self):
        test_vectors = [
            # (
            # normalized filename
            # list of normalized_filenames
            # expected
            # )
            ("foo/bar/a.c", ["foo/b.c"], False),
            ("foo/bar/a.c", ["foo/b.c", "foo/bar/c.c"], True),
            ("foo/bar/a.c", ["foo/b.c", "foo/bar/baz/d.c"], False),
        ]
        for vector in test_vectors:
            normalized_filename, list_of_normalized_filesnames, expected = vector
            actual = mu.filenames_directory_is_in_filename_list(
                normalized_filename, list_of_normalized_filesnames
            )
            self.assertEqual(actual, expected)

    def test_guess_best_assignment(self):
        test_vectors = [
            # (
            # filename_normalized
            # source_assignments
            # expected
            # )
            (
                "foo/asm_arm.c",
                {
                    "> SOURCES": ["foo/main.c", "foo/all_utility.c"],
                    "> if ASM > SOURCES": ["foo/asm_x86.c"],
                },
                "> if ASM > SOURCES",
            )
        ]
        for vector in test_vectors:
            normalized_filename, source_assignments, expected = vector
            actual, _ = mu.guess_best_assignment(
                source_assignments, normalized_filename
            )
            self.assertEqual(actual, expected)

    def test_mozbuild_removing(self):
        test_vectors = [
            (
                "media/dav1d/moz.build",
                SAMPLE_DAV1D_MOZBUILD,
                "third_party/dav1d/src/cdf.c",
                "media/dav1d/",
                "third-party/dav1d/",
                "    '../../third_party/dav1d/src/cdf.c',\n",
            ),
            (
                "media/dav1d/moz.build",
                SAMPLE_DAV1D_MOZBUILD,
                "third_party/dav1d/src/header1.h",
                "media/dav1d/",
                "third-party/dav1d/",
                "    '../../third_party/dav1d/src/header1.h',\n",
            ),
            (
                "media/jxl/moz.build",
                SAMPLE_JPEGXL_MOZBUILD,
                "third_party/jpeg-xl/lib/jxl/alpha.cc",
                "media/jxl/",
                "third-party/jpeg-xl/",
                '    "/third_party/jpeg-xl/lib/jxl/alpha.cc",\n',
            ),
            (
                "media/jxl/moz.build",
                SAMPLE_JPEGXL_MOZBUILD,
                "third_party/jpeg-xl/lib/jxl/header1.hpp",
                "media/jxl/",
                "third-party/jpeg-xl/",
                '    "/third_party/jpeg-xl/lib/jxl/header1.hpp",\n',
            ),
        ]

        for vector in test_vectors:
            (
                mozbuild_path,
                mozbuild_contents,
                file_to_remove,
                moz_yaml_dir,
                vendoring_dir,
                replace_str,
            ) = vector

            startdir = os.getcwd()
            try:
                mozbuild_dir = _make_mozbuild_directory_structure(
                    mozbuild_path, mozbuild_contents
                )
                os.chdir(mozbuild_dir.name)

                mu.remove_file_from_moz_build_file(
                    file_to_remove,
                    moz_yaml_dir=moz_yaml_dir,
                    vendoring_dir=vendoring_dir,
                )

                with open(os.path.join(mozbuild_dir.name, mozbuild_path)) as file:
                    contents = file.read()

                expected_output = mozbuild_contents.replace(replace_str, "")
                if contents != expected_output:
                    print("File to remove:", file_to_remove)
                    print("Contents:")
                    print("-------------------")
                    print(contents)
                    print("-------------------")
                    print("Expected:")
                    print("-------------------")
                    print(expected_output)
                    print("-------------------")
                self.assertEqual(contents, expected_output)
            finally:
                os.chdir(startdir)

    def test_mozbuild_adding(self):
        test_vectors = [
            (
                "media/dav1d/moz.build",
                SAMPLE_DAV1D_MOZBUILD,
                "third_party/dav1d/src/cdf2.c",
                "media/dav1d/",
                "third-party/dav1d/",
                "cdf.c',\n",
                "cdf.c',\n    '../../third_party/dav1d/src/cdf2.c',\n",
            ),
            (
                "media/dav1d/moz.build",
                SAMPLE_DAV1D_MOZBUILD,
                "third_party/dav1d/src/header3.h",
                "media/dav1d/",
                "third-party/dav1d/",
                "header2.h',\n",
                "header2.h',\n    '../../third_party/dav1d/src/header3.h',\n",
            ),
            (
                "media/jxl/moz.build",
                SAMPLE_JPEGXL_MOZBUILD,
                "third_party/jpeg-xl/lib/jxl/alpha2.cc",
                "media/jxl/",
                "third-party/jpeg-xl/",
                'alpha.cc",\n',
                'alpha.cc",\n    "/third_party/jpeg-xl/lib/jxl/alpha2.cc",\n',
            ),
            (
                "media/jxl/moz.build",
                SAMPLE_JPEGXL_MOZBUILD,
                "third_party/jpeg-xl/lib/jxl/header3.hpp",
                "media/jxl/",
                "third-party/jpeg-xl/",
                'header2.h",\n',
                'header2.h",\n    "/third_party/jpeg-xl/lib/jxl/header3.hpp",\n',
            ),
        ]

        for vector in test_vectors:
            (
                mozbuild_path,
                mozbuild_contents,
                file_to_add,
                moz_yaml_dir,
                vendoring_dir,
                search_str,
                replace_str,
            ) = vector

            startdir = os.getcwd()
            try:
                mozbuild_dir = _make_mozbuild_directory_structure(
                    mozbuild_path, mozbuild_contents
                )
                os.chdir(mozbuild_dir.name)

                mu.add_file_to_moz_build_file(
                    file_to_add, moz_yaml_dir=moz_yaml_dir, vendoring_dir=vendoring_dir
                )

                with open(os.path.join(mozbuild_dir.name, mozbuild_path)) as file:
                    contents = file.read()

                expected_output = mozbuild_contents.replace(search_str, replace_str)
                if contents != expected_output:
                    print("File to add:", file_to_add)
                    print("Contents:")
                    print("-------------------")
                    print(contents)
                    print("-------------------")
                    print("Expected:")
                    print("-------------------")
                    print(expected_output)
                    print("-------------------")
                self.assertEqual(contents, expected_output)
            finally:
                os.chdir(startdir)

    # This test is legacy. I'm keeping it around, but new test vectors should be added to the
    # non-internal test to exercise the public API.
    def test_mozbuild_adding_internal(self):
        test_vectors = [
            # (
            # mozbuild_contents
            # unnormalized_filename_to_add,
            # unnormalized_list_of_files
            # expected_output
            # )
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-sse2-more.c",
                ["pixman-sse2.c"],
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    "SOURCES += ['pixman-sse2.c']",
                    "SOURCES += ['pixman-sse2-more.c','pixman-sse2.c']",
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-trap-more.c",
                [
                    "pixman-region32.c",
                    "pixman-solid-fill.c",
                    "pixman-trap.c",
                    "pixman-utils.c",
                    "pixman-x86.c",
                    "pixman.c",
                ],
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    "'pixman-trap.c',", "'pixman-trap-more.c',\n    'pixman-trap.c',"
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-arm-neon-asm-more.S",
                ["pixman-arm-neon-asm-bilinear.S", "pixman-arm-neon-asm.S"],
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    '"pixman-arm-neon-asm.S"',
                    '"pixman-arm-neon-asm-more.S",\n            "pixman-arm-neon-asm.S"',
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-arm-simd-asm-smore.S",
                ["pixman-arm-simd-asm-scaled.S", "pixman-arm-simd-asm.S"],
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    "'pixman-arm-simd-asm.S'",
                    "'pixman-arm-simd-asm-smore.S',\n            'pixman-arm-simd-asm.S'",
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-arm-simd-asn.S",
                ["pixman-arm-simd-asm-scaled.S", "pixman-arm-simd-asm.S"],
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    "'pixman-arm-simd-asm.S'",
                    "'pixman-arm-simd-asm.S',\n            'pixman-arm-simd-asn.S'",
                ),
            ),
        ]

        for vector in test_vectors:
            (
                mozbuild_contents,
                unnormalized_filename_to_add,
                unnormalized_list_of_files,
                expected_output,
            ) = vector

            fd, filename = tempfile.mkstemp(text=True)
            os.close(fd)
            file = open(filename, mode="w")
            file.write(mozbuild_contents)
            file.close()

            mu.edit_moz_build_file_to_add_file(
                filename, unnormalized_filename_to_add, unnormalized_list_of_files
            )

            with open(filename) as file:
                contents = file.read()
            os.remove(filename)

            if contents != expected_output:
                print("File to add:", unnormalized_filename_to_add)
                print("Contents:")
                print("-------------------")
                print(contents)
                print("-------------------")
                print("Expected:")
                print("-------------------")
                print(expected_output)
                print("-------------------")
            self.assertEqual(contents, expected_output)

    # This test is legacy. I'm keeping it around, but new test vectors should be added to the
    # non-internal test to exercise the public API.
    def test_mozbuild_removing_internal(self):
        test_vectors = [
            # (
            # mozbuild_contents
            # unnormalized_filename_to_add
            # expected_output
            # )
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-sse2.c",
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    "SOURCES += ['pixman-sse2.c']", "SOURCES += []"
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-trap.c",
                SAMPLE_PIXMAN_MOZBUILD.replace("    'pixman-trap.c',\n", ""),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-arm-neon-asm.S",
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    '            "pixman-arm-neon-asm.S",\n', ""
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-arm-simd-asm.S",
                SAMPLE_PIXMAN_MOZBUILD.replace(
                    "            'pixman-arm-simd-asm.S'", "            "
                ),
            ),
            (
                SAMPLE_PIXMAN_MOZBUILD,
                "pixman-region32.c",
                SAMPLE_PIXMAN_MOZBUILD.replace("'pixman-region32.c',", ""),
            ),
        ]

        for vector in test_vectors:
            (
                mozbuild_contents,
                unnormalized_filename_to_remove,
                expected_output,
            ) = vector

            fd, filename = tempfile.mkstemp(text=True)
            os.close(fd)
            file = open(filename, mode="w")
            file.write(mozbuild_contents)
            file.close()

            mu.edit_moz_build_file_to_remove_file(
                filename, unnormalized_filename_to_remove
            )

            with open(filename) as file:
                contents = file.read()
            os.remove(filename)

            if contents != expected_output:
                print("File to remove:", unnormalized_filename_to_remove)
                print("Contents:")
                print("-------------------")
                print(contents)
                print("-------------------")
                print("Expected:")
                print("-------------------")
                print(expected_output)
                print("-------------------")
            self.assertEqual(contents, expected_output)


if __name__ == "__main__":
    main()
