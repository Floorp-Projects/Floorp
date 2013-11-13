import unittest

from StringIO import StringIO
import os
import sys
import os.path
from mozunit import main, MockedOpen

from mozbuild.preprocessor import Preprocessor

def NamedIO(name, content):
  with open(name, 'w') as f:
    f.write(content)
  return name

class TestPreprocessor(unittest.TestCase):
  """
  Unit tests for the Context class
  """

  def setUp(self):
    self.pp = Preprocessor()
    self.pp.out = StringIO()

  def test_conditional_if_0(self):
    f = NamedIO("conditional_if_0.in", """#if 0
FAIL
#else
PASS
#endif
""")
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")

  def test_no_marker(self):
    no_marker = """#if 0
PASS
#endif
"""
    f = NamedIO("no_marker.in", no_marker)
    self.pp.setMarker(None)
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), no_marker)

  def test_string_value(self):
    f = NamedIO("string_value.in", """#define FOO STRING
#if FOO
string value is true
#else
string value is false
#endif
""")
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "string value is false\n")
  
  def test_number_value(self):
    f = NamedIO("string_value.in", """#define FOO 1
#if FOO
number value is true
#else
number value is false
#endif
""")
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "number value is true\n")
  
  def test_conditional_if_0_elif_1(self):
    f = NamedIO('conditional_if_0_elif_1.in', '''#if 0
#elif 1
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_conditional_if_1(self):
    f = NamedIO('conditional_if_1.in', '''#if 1
PASS
#else
FAILE
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_conditional_if_1_elif_1_else(self):
    f = NamedIO('conditional_if_1_elif_1_else.in', '''#if 1
PASS
#elif 1
FAIL
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_conditional_if_1_if_1(self):
    f = NamedIO('conditional_if_1_if_1.in', '''#if 1
#if 1
PASS
#else
FAIL
#endif
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_conditional_not_0(self):
    f = NamedIO('conditional_not_0.in', '''#if !0
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_conditional_not_1(self):
    f = NamedIO('conditional_not_1.in', '''#if !1
FAIL
#else
PASS
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_conditional_not_emptyval(self):
    f = NamedIO('conditional_not_emptyval.in', '''#define EMPTYVAL
#if !EMPTYVAL
FAIL
#else
PASS
#endif
#if EMPTYVAL
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\nPASS\n")
  
  def test_conditional_not_nullval(self):
    f = NamedIO('conditional_not_nullval.in', '''#define NULLVAL 0
#if !NULLVAL
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_expand(self):
    f = NamedIO('expand.in', '''#define ASVAR AS
#expand P__ASVAR__S
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")

  def test_undef_defined(self):
    f = NamedIO('undef_defined.in', '''#define BAR
#undef BAR
BAR
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "BAR\n")

  def test_undef_undefined(self):
    f = NamedIO('undef_undefined.in', '''#undef VAR
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "")
  
  def test_filter_attemptSubstitution(self):
    f = NamedIO('filter_attemptSubstitution.in', '''#filter attemptSubstitution
@PASS@
#unfilter attemptSubstitution
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "@PASS@\n")
  
  def test_filter_emptyLines(self):
    f = NamedIO('filter_emptyLines.in', '''lines with a

blank line
#filter emptyLines
lines with

no blank lines
#unfilter emptyLines
yet more lines with

blank lines
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), '''lines with a

blank line
lines with
no blank lines
yet more lines with

blank lines
''')
  
  def test_filter_slashslash(self):
    f = NamedIO('filter_slashslash.in', '''#filter slashslash
PASS//FAIL  // FAIL
#unfilter slashslash
PASS // PASS
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\nPASS // PASS\n")
  
  def test_filter_spaces(self):
    f = NamedIO('filter_spaces.in', '''#filter spaces
You should see two nice ascii tables
 +-+-+-+
 | |   |     |
 +-+-+-+
#unfilter spaces
+-+---+
| |   |
+-+---+ 
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), """You should see two nice ascii tables
+-+-+-+
| | | |
+-+-+-+
+-+---+
| |   |
+-+---+ 
""")
  
  def test_filter_substitution(self):
    f = NamedIO('filter_substitution.in', '''#define VAR ASS
#filter substitution
P@VAR@
#unfilter substitution
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")

  def test_error(self):
    f = NamedIO('error.in', '''#error spit this message out
''')
    caught_msg = None
    try:
      self.pp.do_include(f)
    except Preprocessor.Error, e:
      caught_msg = e.args[0][-1]
    self.assertEqual(caught_msg, 'spit this message out')
  
  def test_javascript_line(self):
    f = NamedIO('javascript_line.js.in', '''// Line 1
#if 0
// line 3
#endif
// line 5
# comment
// line 7
// line 8
// line 9
# another comment
// line 11
#define LINE 1
// line 13, given line number overwritten with 2
''')
    self.pp.do_include(f)
    out = """// Line 1
//@line 5 "CWDjavascript_line.js.in"
// line 5
//@line 7 "CWDjavascript_line.js.in"
// line 7
// line 8
// line 9
//@line 11 "CWDjavascript_line.js.in"
// line 11
//@line 2 "CWDjavascript_line.js.in"
// line 13, given line number overwritten with 2
"""
    out = out.replace('CWD', os.getcwd() + os.path.sep)
    self.assertEqual(self.pp.out.getvalue(), out)
  
  def test_literal(self):
    f = NamedIO('literal.in', '''#literal PASS
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_directory(self):
    f = NamedIO('var_directory.in', '''#ifdef DIRECTORY
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_file(self):
    f = NamedIO('var_file.in', '''#ifdef FILE
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_if_0(self):
    f = NamedIO('var_if_0.in', '''#define VAR 0
#if VAR
FAIL
#else
PASS
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_if_0_elifdef(self):
    f = NamedIO('var_if_0_elifdef.in', '''#if 0
#elifdef FILE
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_if_0_elifndef(self):
    f = NamedIO('var_if_0_elifndef.in', '''#if 0
#elifndef VAR
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_ifdef_0(self):
    f = NamedIO('var_ifdef_0.in', '''#define VAR 0
#ifdef VAR
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_ifdef_undef(self):
    f = NamedIO('var_ifdef_undef.in', '''#define VAR 0
#undef VAR
#ifdef VAR
FAIL
#else
PASS
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_ifndef_0(self):
    f = NamedIO('var_ifndef_0.in', '''#define VAR 0
#ifndef VAR
FAIL
#else
PASS
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_ifndef_undef(self):
    f = NamedIO('var_ifndef_undef.in', '''#define VAR 0
#undef VAR
#ifndef VAR
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")
  
  def test_var_line(self):
    f = NamedIO('var_line.in', '''#ifdef LINE
PASS
#else
FAIL
#endif
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")

  def test_lineEndings(self):
    f = NamedIO('lineEndings.in', '''first
#literal second
''')
    self.pp.setLineEndings('cr')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "first\rsecond\r")

  def test_filterDefine(self):
    f = NamedIO('filterDefine.in', '''#filter substitution
#define VAR AS
#define VAR2 P@VAR@
@VAR2@S
''')
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "PASS\n")

  def test_number_value_equals(self):
    f = NamedIO("number_value_equals.in", """#define FOO 1000
#if FOO == 1000
number value is equal
#else
number value is not equal
#endif
""")
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "number value is equal\n")

  def test_number_value_equals_defines(self):
    f = NamedIO("number_value_equals_defines.in", """#if FOO == 1000
number value is equal
#else
number value is not equal
#endif
""")
    self.pp.handleCommandLine(["-DFOO=1000"])
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "number value is equal\n")

  def test_octal_value_equals(self):
    f = NamedIO("octal_value_equals.in", """#define FOO 0100
#if FOO == 0100
octal value is equal
#else
octal value is not equal
#endif
""")
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "octal value is equal\n")

  def test_octal_value_equals_defines(self):
    f = NamedIO("octal_value_equals_defines.in", """#if FOO == 0100
octal value is equal
#else
octal value is not equal
#endif
""")
    self.pp.handleCommandLine(["-DFOO=0100"])
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "octal value is equal\n")

  def test_value_quoted_expansion(self):
    """
    Quoted values on the commandline don't currently have quotes stripped.
    Pike says this is for compat reasons.
    """
    f = NamedIO("value_quoted_expansion.in", """#filter substitution
@FOO@
""")
    self.pp.handleCommandLine(['-DFOO="ABCD"'])
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), '"ABCD"\n')

  def test_octal_value_quoted_expansion(self):
    f = NamedIO("value_quoted_expansion.in", """#filter substitution
@FOO@
""")
    self.pp.handleCommandLine(['-DFOO="0100"'])
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), '"0100"\n')

  def test_number_value_not_equals_quoted_defines(self):
    f = NamedIO("number_value_not_equals_quoted_defines.in", """#if FOO == 1000
number value is equal
#else
number value is not equal
#endif
""")
    self.pp.handleCommandLine(['-DFOO="1000"'])
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "number value is not equal\n")

  def test_octal_value_not_equals_quoted_defines(self):
    f = NamedIO("octal_value_not_equals_quoted_defines.in", """#if FOO == 0100
octal value is equal
#else
octal value is not equal
#endif
""")
    self.pp.handleCommandLine(['-DFOO="0100"'])
    self.pp.do_include(f)
    self.assertEqual(self.pp.out.getvalue(), "octal value is not equal\n")

  def test_undefined_variable(self):
    f = NamedIO("undefined_variable.in", """#filter substitution
@foo@
""")
    try:
      self.pp.do_include(f)
    except Preprocessor.Error, exception:
      self.assertEqual(exception.key, 'UNDEFINED_VAR')
    else:
      self.fail("Expected a Preprocessor.Error")

  def test_include(self):
    with MockedOpen({"foo/test": """#define foo foobarbaz
#include @inc@
@bar@
""",
                     "bar": """#define bar barfoobaz
@foo@
"""}):
      f = NamedIO("include.in", """#filter substitution
#define inc ../bar
#include foo/test""")
      self.pp.do_include(f)
      self.assertEqual(self.pp.out.getvalue(), """foobarbaz
barfoobaz
""")

  def test_include_missing_file(self):
    f = NamedIO("include_missing_file.in", "#include foo")
    try:
      self.pp.do_include(f)
    except Preprocessor.Error, exception:
      self.assertEqual(exception.key, 'FILE_NOT_FOUND')
    else:
      self.fail("Expected a Preprocessor.Error")

  def test_include_undefined_variable(self):
    f = NamedIO("include_undefined_variable.in", """#filter substitution
#include @foo@
""")
    try:
      self.pp.do_include(f)
    except Preprocessor.Error, exception:
      self.assertEqual(exception.key, 'UNDEFINED_VAR')
    else:
      self.fail("Expected a Preprocessor.Error")

  def test_include_literal_at(self):
    with MockedOpen({"@foo@": "#define foo foobarbaz"}):
      f = NamedIO("include_literal_at.in", """#include @foo@
#filter substitution
@foo@
""")
      self.pp.do_include(f)
      self.assertEqual(self.pp.out.getvalue(), """foobarbaz
""")

  def test_command_line_literal_at(self):
    with MockedOpen({"@foo@.in": """@foo@
"""}):
      self.pp.handleCommandLine(['-Fsubstitution', '-Dfoo=foobarbaz', '@foo@.in'])
      self.assertEqual(self.pp.out.getvalue(), """foobarbaz
""")

if __name__ == '__main__':
  main()
