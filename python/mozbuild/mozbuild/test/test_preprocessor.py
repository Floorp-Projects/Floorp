# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import unittest

import os
import shutil
from six import StringIO

from tempfile import mkdtemp

from mozunit import main, MockedOpen

from mozbuild.preprocessor import Preprocessor


class TestPreprocessor(unittest.TestCase):
    """
    Unit tests for the Context class
    """

    def setUp(self):
        self.pp = Preprocessor()
        self.pp.out = StringIO()

    def do_include_compare(self, content_lines, expected_lines):
        content = "%s" % "\n".join(content_lines)
        expected = "%s".rstrip() % "\n".join(expected_lines)

        with MockedOpen({"dummy": content}):
            self.pp.do_include("dummy")
            self.assertEqual(self.pp.out.getvalue().rstrip("\n"), expected)

    def do_include_pass(self, content_lines):
        self.do_include_compare(content_lines, ["PASS"])

    def test_conditional_if_0(self):
        self.do_include_pass(
            [
                "#if 0",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_no_marker(self):
        lines = [
            "#if 0",
            "PASS",
            "#endif",
        ]
        self.pp.setMarker(None)
        self.do_include_compare(lines, lines)

    def test_string_value(self):
        self.do_include_compare(
            [
                "#define FOO STRING",
                "#if FOO",
                "string value is true",
                "#else",
                "string value is false",
                "#endif",
            ],
            ["string value is false"],
        )

    def test_number_value(self):
        self.do_include_compare(
            [
                "#define FOO 1",
                "#if FOO",
                "number value is true",
                "#else",
                "number value is false",
                "#endif",
            ],
            ["number value is true"],
        )

    def test_conditional_if_0_elif_1(self):
        self.do_include_pass(
            [
                "#if 0",
                "#elif 1",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_conditional_if_1(self):
        self.do_include_pass(
            [
                "#if 1",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_conditional_if_0_or_1(self):
        self.do_include_pass(
            [
                "#if 0 || 1",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_conditional_if_1_elif_1_else(self):
        self.do_include_pass(
            [
                "#if 1",
                "PASS",
                "#elif 1",
                "FAIL",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_conditional_if_1_if_1(self):
        self.do_include_pass(
            [
                "#if 1",
                "#if 1",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_conditional_not_0(self):
        self.do_include_pass(
            [
                "#if !0",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_conditional_not_0_and_1(self):
        self.do_include_pass(
            [
                "#if !0 && !1",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_conditional_not_1(self):
        self.do_include_pass(
            [
                "#if !1",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_conditional_not_emptyval(self):
        self.do_include_compare(
            [
                "#define EMPTYVAL",
                "#ifndef EMPTYVAL",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
                "#ifdef EMPTYVAL",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ],
            ["PASS", "PASS"],
        )

    def test_conditional_not_nullval(self):
        self.do_include_pass(
            [
                "#define NULLVAL 0",
                "#if !NULLVAL",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_indentation(self):
        self.do_include_pass(
            [
                "         #define NULLVAL 0",
                " #if !NULLVAL",
                "PASS",
                "           #else",
                "FAIL",
                "     #endif",
            ]
        )

    def test_expand(self):
        self.do_include_pass(
            [
                "#define ASVAR AS",
                "#expand P__ASVAR__S",
            ]
        )

    def test_undef_defined(self):
        self.do_include_compare(
            [
                "#define BAR",
                "#undef BAR",
                "BAR",
            ],
            ["BAR"],
        )

    def test_undef_undefined(self):
        self.do_include_compare(
            [
                "#undef BAR",
            ],
            [],
        )

    def test_filter_attemptSubstitution(self):
        self.do_include_compare(
            [
                "#filter attemptSubstitution",
                "@PASS@",
                "#unfilter attemptSubstitution",
            ],
            ["@PASS@"],
        )

    def test_filter_emptyLines(self):
        self.do_include_compare(
            [
                "lines with a",
                "",
                "blank line",
                "#filter emptyLines",
                "lines with",
                "",
                "no blank lines",
                "#unfilter emptyLines",
                "yet more lines with",
                "",
                "blank lines",
            ],
            [
                "lines with a",
                "",
                "blank line",
                "lines with",
                "no blank lines",
                "yet more lines with",
                "",
                "blank lines",
            ],
        )

    def test_filter_dumbComments(self):
        self.do_include_compare(
            [
                "#filter dumbComments",
                "PASS//PASS  // PASS",
                "  //FAIL",
                "//     FAIL",
                "PASS  //",
                "PASS  //    FAIL",
                "//",
                "",
                "#unfilter dumbComments",
                "// PASS",
            ],
            [
                "PASS//PASS  // PASS",
                "",
                "",
                "PASS  //",
                "PASS  //    FAIL",
                "",
                "",
                "// PASS",
            ],
        )

    def test_filter_dumbComments_and_emptyLines(self):
        self.do_include_compare(
            [
                "#filter dumbComments emptyLines",
                "PASS//PASS  // PASS",
                "  //FAIL",
                "//     FAIL",
                "PASS  //",
                "PASS  //    FAIL",
                "//",
                "",
                "#unfilter dumbComments emptyLines",
                "",
                "// PASS",
            ],
            [
                "PASS//PASS  // PASS",
                "PASS  //",
                "PASS  //    FAIL",
                "",
                "// PASS",
            ],
        )

    def test_filter_substitution(self):
        self.do_include_pass(
            [
                "#define VAR ASS",
                "#filter substitution",
                "P@VAR@",
                "#unfilter substitution",
            ]
        )

    def test_error(self):
        with MockedOpen({"f": "#error spit this message out\n"}):
            with self.assertRaises(Preprocessor.Error) as e:
                self.pp.do_include("f")
                self.assertEqual(e.args[0][-1], "spit this message out")

    def test_javascript_line(self):
        # The preprocessor is reading the filename from somewhere not caught
        # by MockedOpen.
        tmpdir = mkdtemp()
        try:
            full = os.path.join(tmpdir, "javascript_line.js.in")
            with open(full, "w") as fh:
                fh.write(
                    "\n".join(
                        [
                            "// Line 1",
                            "#if 0",
                            "// line 3",
                            "#endif",
                            "// line 5",
                            "# comment",
                            "// line 7",
                            "// line 8",
                            "// line 9",
                            "# another comment",
                            "// line 11",
                            "#define LINE 1",
                            "// line 13, given line number overwritten with 2",
                            "",
                        ]
                    )
                )

            self.pp.do_include(full)
            out = "\n".join(
                [
                    "// Line 1",
                    '//@line 5 "CWDjavascript_line.js.in"',
                    "// line 5",
                    '//@line 7 "CWDjavascript_line.js.in"',
                    "// line 7",
                    "// line 8",
                    "// line 9",
                    '//@line 11 "CWDjavascript_line.js.in"',
                    "// line 11",
                    '//@line 2 "CWDjavascript_line.js.in"',
                    "// line 13, given line number overwritten with 2",
                    "",
                ]
            )
            out = out.replace("CWD", tmpdir + os.path.sep)
            self.assertEqual(self.pp.out.getvalue(), out)
        finally:
            shutil.rmtree(tmpdir)

    def test_literal(self):
        self.do_include_pass(
            [
                "#literal PASS",
            ]
        )

    def test_var_directory(self):
        self.do_include_pass(
            [
                "#ifdef DIRECTORY",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_file(self):
        self.do_include_pass(
            [
                "#ifdef FILE",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_if_0(self):
        self.do_include_pass(
            [
                "#define VAR 0",
                "#if VAR",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_var_if_0_elifdef(self):
        self.do_include_pass(
            [
                "#if 0",
                "#elifdef FILE",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_if_0_elifndef(self):
        self.do_include_pass(
            [
                "#if 0",
                "#elifndef VAR",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_ifdef_0(self):
        self.do_include_pass(
            [
                "#define VAR 0",
                "#ifdef VAR",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_ifdef_1_or_undef(self):
        self.do_include_pass(
            [
                "#define FOO 1",
                "#if defined(FOO) || defined(BAR)",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_ifdef_undef(self):
        self.do_include_pass(
            [
                "#define VAR 0",
                "#undef VAR",
                "#ifdef VAR",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_var_ifndef_0(self):
        self.do_include_pass(
            [
                "#define VAR 0",
                "#ifndef VAR",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_var_ifndef_0_and_undef(self):
        self.do_include_pass(
            [
                "#define FOO 0",
                "#if !defined(FOO) && !defined(BAR)",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_var_ifndef_undef(self):
        self.do_include_pass(
            [
                "#define VAR 0",
                "#undef VAR",
                "#ifndef VAR",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_var_line(self):
        self.do_include_pass(
            [
                "#ifdef LINE",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_filterDefine(self):
        self.do_include_pass(
            [
                "#filter substitution",
                "#define VAR AS",
                "#define VAR2 P@VAR@",
                "@VAR2@S",
            ]
        )

    def test_number_value_equals(self):
        self.do_include_pass(
            [
                "#define FOO 1000",
                "#if FOO == 1000",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_default_defines(self):
        self.pp.handleCommandLine(["-DFOO"])
        self.do_include_pass(
            [
                "#if FOO == 1",
                "PASS",
                "#else",
                "FAIL",
            ]
        )

    def test_number_value_equals_defines(self):
        self.pp.handleCommandLine(["-DFOO=1000"])
        self.do_include_pass(
            [
                "#if FOO == 1000",
                "PASS",
                "#else",
                "FAIL",
            ]
        )

    def test_octal_value_equals(self):
        self.do_include_pass(
            [
                "#define FOO 0100",
                "#if FOO == 0100",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_octal_value_equals_defines(self):
        self.pp.handleCommandLine(["-DFOO=0100"])
        self.do_include_pass(
            [
                "#if FOO == 0100",
                "PASS",
                "#else",
                "FAIL",
                "#endif",
            ]
        )

    def test_value_quoted_expansion(self):
        """
        Quoted values on the commandline don't currently have quotes stripped.
        Pike says this is for compat reasons.
        """
        self.pp.handleCommandLine(['-DFOO="ABCD"'])
        self.do_include_compare(
            [
                "#filter substitution",
                "@FOO@",
            ],
            ['"ABCD"'],
        )

    def test_octal_value_quoted_expansion(self):
        self.pp.handleCommandLine(['-DFOO="0100"'])
        self.do_include_compare(
            [
                "#filter substitution",
                "@FOO@",
            ],
            ['"0100"'],
        )

    def test_number_value_not_equals_quoted_defines(self):
        self.pp.handleCommandLine(['-DFOO="1000"'])
        self.do_include_pass(
            [
                "#if FOO == 1000",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_octal_value_not_equals_quoted_defines(self):
        self.pp.handleCommandLine(['-DFOO="0100"'])
        self.do_include_pass(
            [
                "#if FOO == 0100",
                "FAIL",
                "#else",
                "PASS",
                "#endif",
            ]
        )

    def test_undefined_variable(self):
        with MockedOpen({"f": "#filter substitution\n@foo@"}):
            with self.assertRaises(Preprocessor.Error) as e:
                self.pp.do_include("f")
                self.assertEqual(e.key, "UNDEFINED_VAR")

    def test_include(self):
        files = {
            "foo/test": "\n".join(
                [
                    "#define foo foobarbaz",
                    "#include @inc@",
                    "@bar@",
                    "",
                ]
            ),
            "bar": "\n".join(
                [
                    "#define bar barfoobaz",
                    "@foo@",
                    "",
                ]
            ),
            "f": "\n".join(
                [
                    "#filter substitution",
                    "#define inc ../bar",
                    "#include foo/test",
                    "",
                ]
            ),
        }

        with MockedOpen(files):
            self.pp.do_include("f")
            self.assertEqual(self.pp.out.getvalue(), "foobarbaz\nbarfoobaz\n")

    def test_include_line(self):
        files = {
            "srcdir/test.js": "\n".join(
                [
                    "#define foo foobarbaz",
                    "#include @inc@",
                    "@bar@",
                    "",
                ]
            ),
            "srcdir/bar.js": "\n".join(
                [
                    "#define bar barfoobaz",
                    "@foo@",
                    "",
                ]
            ),
            "srcdir/foo.js": "\n".join(
                [
                    "bazfoobar",
                    "#include bar.js",
                    "bazbarfoo",
                    "",
                ]
            ),
            "objdir/baz.js": "baz\n",
            "srcdir/f.js": "\n".join(
                [
                    "#include foo.js",
                    "#filter substitution",
                    "#define inc bar.js",
                    "#include test.js",
                    "#include ../objdir/baz.js",
                    "fin",
                    "",
                ]
            ),
        }

        preprocessed = (
            '//@line 1 "$SRCDIR/foo.js"\n'
            "bazfoobar\n"
            '//@line 2 "$SRCDIR/bar.js"\n'
            "@foo@\n"
            '//@line 3 "$SRCDIR/foo.js"\n'
            "bazbarfoo\n"
            '//@line 2 "$SRCDIR/bar.js"\n'
            "foobarbaz\n"
            '//@line 3 "$SRCDIR/test.js"\n'
            "barfoobaz\n"
            '//@line 1 "$OBJDIR/baz.js"\n'
            "baz\n"
            '//@line 6 "$SRCDIR/f.js"\n'
            "fin\n"
        )

        # Try with separate srcdir/objdir
        with MockedOpen(files):
            self.pp.topsrcdir = os.path.abspath("srcdir")
            self.pp.topobjdir = os.path.abspath("objdir")
            self.pp.do_include("srcdir/f.js")
            self.assertEqual(self.pp.out.getvalue(), preprocessed)

        # Try again with relative objdir
        self.setUp()
        files["srcdir/objdir/baz.js"] = files["objdir/baz.js"]
        del files["objdir/baz.js"]
        files["srcdir/f.js"] = files["srcdir/f.js"].replace("../", "")
        with MockedOpen(files):
            self.pp.topsrcdir = os.path.abspath("srcdir")
            self.pp.topobjdir = os.path.abspath("srcdir/objdir")
            self.pp.do_include("srcdir/f.js")
            self.assertEqual(self.pp.out.getvalue(), preprocessed)

    def test_include_missing_file(self):
        with MockedOpen({"f": "#include foo\n"}):
            with self.assertRaises(Preprocessor.Error) as e:
                self.pp.do_include("f")
            self.assertEqual(e.exception.key, "FILE_NOT_FOUND")

    def test_include_undefined_variable(self):
        with MockedOpen({"f": "#filter substitution\n#include @foo@\n"}):
            with self.assertRaises(Preprocessor.Error) as e:
                self.pp.do_include("f")
            self.assertEqual(e.exception.key, "UNDEFINED_VAR")

    def test_include_literal_at(self):
        files = {
            "@foo@": "#define foo foobarbaz\n",
            "f": "#include @foo@\n#filter substitution\n@foo@\n",
        }

        with MockedOpen(files):
            self.pp.do_include("f")
            self.assertEqual(self.pp.out.getvalue(), "foobarbaz\n")

    def test_command_line_literal_at(self):
        with MockedOpen({"@foo@.in": "@foo@\n"}):
            self.pp.handleCommandLine(["-Fsubstitution", "-Dfoo=foobarbaz", "@foo@.in"])
            self.assertEqual(self.pp.out.getvalue(), "foobarbaz\n")

    def test_invalid_ifdef(self):
        with MockedOpen({"dummy": "#ifdef FOO == BAR\nPASS\n#endif"}):
            with self.assertRaises(Preprocessor.Error) as e:
                self.pp.do_include("dummy")
            self.assertEqual(e.exception.key, "INVALID_VAR")

        with MockedOpen({"dummy": "#ifndef FOO == BAR\nPASS\n#endif"}):
            with self.assertRaises(Preprocessor.Error) as e:
                self.pp.do_include("dummy")
            self.assertEqual(e.exception.key, "INVALID_VAR")

        # Trailing whitespaces, while not nice, shouldn't be an error.
        self.do_include_pass(
            [
                "#ifndef  FOO ",
                "PASS",
                "#endif",
            ]
        )


if __name__ == "__main__":
    main()
