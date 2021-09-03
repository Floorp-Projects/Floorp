# coding: utf-8

"""
Exposes a get_spec_tests() function for the project's test harness.

Creates a unittest.TestCase for the tests defined in the mustache spec.

"""

# TODO: this module can be cleaned up somewhat.
# TODO: move all of this code to pystache/tests/spectesting.py and
#   have it expose a get_spec_tests(spec_test_dir) function.

FILE_ENCODING = 'utf-8'  # the encoding of the spec test files.

yaml = None

try:
    # We try yaml first since it is more convenient when adding and modifying
    # test cases by hand (since the YAML is human-readable and is the master
    # from which the JSON format is generated).
    import yaml
except ImportError:
    try:
        import json
    except:
        # The module json is not available prior to Python 2.6, whereas
        # simplejson is.  The simplejson package dropped support for Python 2.4
        # in simplejson v2.1.0, so Python 2.4 requires a simplejson install
        # older than the most recent version.
        try:
            import simplejson as json
        except ImportError:
            # Raise an error with a type different from ImportError as a hack around
            # this issue:
            #   http://bugs.python.org/issue7559
            from sys import exc_info
            ex_type, ex_value, tb = exc_info()
            new_ex = Exception("%s: %s" % (ex_type.__name__, ex_value))
            raise new_ex.__class__, new_ex, tb
    file_extension = 'json'
    parser = json
else:
    file_extension = 'yml'
    parser = yaml


import codecs
import glob
import os.path
import unittest

import pystache
from pystache import common
from pystache.renderer import Renderer
from pystache.tests.common import AssertStringMixin


def get_spec_tests(spec_test_dir):
    """
    Return a list of unittest.TestCase instances.

    """
    # TODO: use logging module instead.
    print "pystache: spec tests: using %s" % _get_parser_info()

    cases = []

    # Make this absolute for easier diagnosis in case of error.
    spec_test_dir = os.path.abspath(spec_test_dir)
    spec_paths = glob.glob(os.path.join(spec_test_dir, '*.%s' % file_extension))

    for path in spec_paths:
        new_cases = _read_spec_tests(path)
        cases.extend(new_cases)

    # Store this as a value so that CheckSpecTestsFound is not checking
    # a reference to cases that contains itself.
    spec_test_count = len(cases)

    # This test case lets us alert the user that spec tests are missing.
    class CheckSpecTestsFound(unittest.TestCase):

        def runTest(self):
            if spec_test_count > 0:
                return
            raise Exception("Spec tests not found--\n  in %s\n"
                " Consult the README file on how to add the Mustache spec tests." % repr(spec_test_dir))

    case = CheckSpecTestsFound()
    cases.append(case)

    return cases


def _get_parser_info():
    return "%s (version %s)" % (parser.__name__, parser.__version__)


def _read_spec_tests(path):
    """
    Return a list of unittest.TestCase instances.

    """
    b = common.read(path)
    u = unicode(b, encoding=FILE_ENCODING)
    spec_data = parse(u)
    tests = spec_data['tests']

    cases = []
    for data in tests:
        case = _deserialize_spec_test(data, path)
        cases.append(case)

    return cases


# TODO: simplify the implementation of this function.
def _convert_children(node):
    """
    Recursively convert to functions all "code strings" below the node.

    This function is needed only for the json format.

    """
    if not isinstance(node, (list, dict)):
        # Then there is nothing to iterate over and recurse.
        return

    if isinstance(node, list):
        for child in node:
            _convert_children(child)
        return
    # Otherwise, node is a dict, so attempt the conversion.

    for key in node.keys():
        val = node[key]

        if not isinstance(val, dict) or val.get('__tag__') != 'code':
            _convert_children(val)
            continue
        # Otherwise, we are at a "leaf" node.

        val = eval(val['python'])
        node[key] = val
        continue


def _deserialize_spec_test(data, file_path):
    """
    Return a unittest.TestCase instance representing a spec test.

    Arguments:

      data: the dictionary of attributes for a single test.

    """
    context = data['data']
    description = data['desc']
    # PyYAML seems to leave ASCII strings as byte strings.
    expected = unicode(data['expected'])
    # TODO: switch to using dict.get().
    partials = data.has_key('partials') and data['partials'] or {}
    template = data['template']
    test_name = data['name']

    _convert_children(context)

    test_case = _make_spec_test(expected, template, context, partials, description, test_name, file_path)

    return test_case


def _make_spec_test(expected, template, context, partials, description, test_name, file_path):
    """
    Return a unittest.TestCase instance representing a spec test.

    """
    file_name  = os.path.basename(file_path)
    test_method_name = "Mustache spec (%s): %s" % (file_name, repr(test_name))

    # We subclass SpecTestBase in order to control the test method name (for
    # the purposes of improved reporting).
    class SpecTest(SpecTestBase):
        pass

    def run_test(self):
        self._runTest()

    # TODO: should we restore this logic somewhere?
    # If we don't convert unicode to str, we get the following error:
    #   "TypeError: __name__ must be set to a string object"
    # test.__name__ = str(name)
    setattr(SpecTest, test_method_name, run_test)
    case = SpecTest(test_method_name)

    case._context = context
    case._description = description
    case._expected = expected
    case._file_path = file_path
    case._partials = partials
    case._template = template
    case._test_name = test_name

    return case


def parse(u):
    """
    Parse the contents of a spec test file, and return a dict.

    Arguments:

      u: a unicode string.

    """
    # TODO: find a cleaner mechanism for choosing between the two.
    if yaml is None:
        # Then use json.

        # The only way to get the simplejson module to return unicode strings
        # is to pass it unicode.  See, for example--
        #
        #   http://code.google.com/p/simplejson/issues/detail?id=40
        #
        # and the documentation of simplejson.loads():
        #
        #   "If s is a str then decoded JSON strings that contain only ASCII
        #    characters may be parsed as str for performance and memory reasons.
        #    If your code expects only unicode the appropriate solution is
        #    decode s to unicode prior to calling loads."
        #
        return json.loads(u)
    # Otherwise, yaml.

    def code_constructor(loader, node):
        value = loader.construct_mapping(node)
        return eval(value['python'], {})

    yaml.add_constructor(u'!code', code_constructor)
    return yaml.load(u)


class SpecTestBase(unittest.TestCase, AssertStringMixin):

    def _runTest(self):
        context = self._context
        description = self._description
        expected = self._expected
        file_path = self._file_path
        partials = self._partials
        template = self._template
        test_name = self._test_name

        renderer = Renderer(partials=partials)
        actual = renderer.render(template, context)

        # We need to escape the strings that occur in our format string because
        # they can contain % symbols, for example (in delimiters.yml)--
        #
        #   "template: '{{=<% %>=}}(<%text%>)'"
        #
        def escape(s):
            return s.replace("%", "%%")

        parser_info = _get_parser_info()
        subs = [repr(test_name), description, os.path.abspath(file_path),
                template, repr(context), parser_info]
        subs = tuple([escape(sub) for sub in subs])
        # We include the parsing module version info to help with troubleshooting
        # yaml/json/simplejson issues.
        message = """%s: %s

  File: %s

  Template: \"""%s\"""

  Context: %s

  %%s

  [using %s]
  """ % subs

        self.assertString(actual, expected, format=message)
