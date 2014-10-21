#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Mozilla universal manifest parser
"""

__all__ = ['read_ini', # .ini reader
           'ManifestParser', 'TestManifest', 'convert', # manifest handling
           'parse', 'ParseError', 'ExpressionParser'] # conditional expression parser

import json
import fnmatch
import os
import re
import shutil
import sys

from optparse import OptionParser
from StringIO import StringIO

relpath = os.path.relpath
string = (basestring,)


# expr.py
# from:
# http://k0s.org/mozilla/hg/expressionparser
# http://hg.mozilla.org/users/tmielczarek_mozilla.com/expressionparser

# Implements a top-down parser/evaluator for simple boolean expressions.
# ideas taken from http://effbot.org/zone/simple-top-down-parsing.htm
#
# Rough grammar:
# expr := literal
#       | '(' expr ')'
#       | expr '&&' expr
#       | expr '||' expr
#       | expr '==' expr
#       | expr '!=' expr
# literal := BOOL
#          | INT
#          | STRING
#          | IDENT
# BOOL   := true|false
# INT    := [0-9]+
# STRING := "[^"]*"
# IDENT  := [A-Za-z_]\w*

# Identifiers take their values from a mapping dictionary passed as the second
# argument.

# Glossary (see above URL for details):
# - nud: null denotation
# - led: left detonation
# - lbp: left binding power
# - rbp: right binding power

class ident_token(object):
    def __init__(self, scanner, value):
        self.value = value
    def nud(self, parser):
        # identifiers take their value from the value mappings passed
        # to the parser
        return parser.value(self.value)

class literal_token(object):
    def __init__(self, scanner, value):
        self.value = value
    def nud(self, parser):
        return self.value

class eq_op_token(object):
    "=="
    def led(self, parser, left):
        return left == parser.expression(self.lbp)

class neq_op_token(object):
    "!="
    def led(self, parser, left):
        return left != parser.expression(self.lbp)

class not_op_token(object):
    "!"
    def nud(self, parser):
        return not parser.expression(100)

class and_op_token(object):
    "&&"
    def led(self, parser, left):
        right = parser.expression(self.lbp)
        return left and right

class or_op_token(object):
    "||"
    def led(self, parser, left):
        right = parser.expression(self.lbp)
        return left or right

class lparen_token(object):
    "("
    def nud(self, parser):
        expr = parser.expression()
        parser.advance(rparen_token)
        return expr

class rparen_token(object):
    ")"

class end_token(object):
    """always ends parsing"""

### derived literal tokens

class bool_token(literal_token):
    def __init__(self, scanner, value):
        value = {'true':True, 'false':False}[value]
        literal_token.__init__(self, scanner, value)

class int_token(literal_token):
    def __init__(self, scanner, value):
        literal_token.__init__(self, scanner, int(value))

class string_token(literal_token):
    def __init__(self, scanner, value):
        literal_token.__init__(self, scanner, value[1:-1])

precedence = [(end_token, rparen_token),
              (or_op_token,),
              (and_op_token,),
              (eq_op_token, neq_op_token),
              (lparen_token,),
              ]
for index, rank in enumerate(precedence):
    for token in rank:
        token.lbp = index # lbp = lowest left binding power

class ParseError(Exception):
    """error parsing conditional expression"""

class ExpressionParser(object):
    """
    A parser for a simple expression language.

    The expression language can be described as follows::

        EXPRESSION ::= LITERAL | '(' EXPRESSION ')' | '!' EXPRESSION | EXPRESSION OP EXPRESSION
        OP ::= '==' | '!=' | '&&' | '||'
        LITERAL ::= BOOL | INT | IDENT | STRING
        BOOL ::= 'true' | 'false'
        INT ::= [0-9]+
        IDENT ::= [a-zA-Z_]\w*
        STRING ::= '"' [^\"] '"' | ''' [^\'] '''

    At its core, expressions consist of booleans, integers, identifiers and.
    strings. Booleans are one of *true* or *false*. Integers are a series
    of digits. Identifiers are a series of English letters and underscores.
    Strings are a pair of matching quote characters (single or double) with
    zero or more characters inside.

    Expressions can be combined with operators: the equals (==) and not
    equals (!=) operators compare two expressions and produce a boolean. The
    and (&&) and or (||) operators take two expressions and produce the logical
    AND or OR value of them, respectively. An expression can also be prefixed
    with the not (!) operator, which produces its logical negation.

    Finally, any expression may be contained within parentheses for grouping.

    Identifiers take their values from the mapping provided.
    """

    scanner = None

    def __init__(self, text, valuemapping, strict=False):
        """
        Initialize the parser
        :param text: The expression to parse as a string.
        :param valuemapping: A dict mapping identifier names to values.
        :param strict: If true, referencing an identifier that was not
                       provided in :valuemapping: will raise an error.
        """
        self.text = text
        self.valuemapping = valuemapping
        self.strict = strict

    def _tokenize(self):
        """
        Lex the input text into tokens and yield them in sequence.
        """
        if not ExpressionParser.scanner:
            ExpressionParser.scanner = re.Scanner([
                # Note: keep these in sync with the class docstring above.
                (r"true|false", bool_token),
                (r"[a-zA-Z_]\w*", ident_token),
                (r"[0-9]+", int_token),
                (r'("[^"]*")|(\'[^\']*\')', string_token),
                (r"==", eq_op_token()),
                (r"!=", neq_op_token()),
                (r"\|\|", or_op_token()),
                (r"!", not_op_token()),
                (r"&&", and_op_token()),
                (r"\(", lparen_token()),
                (r"\)", rparen_token()),
                (r"\s+", None), # skip whitespace
            ])
        tokens, remainder = ExpressionParser.scanner.scan(self.text)
        for t in tokens:
            yield t
        yield end_token()

    def value(self, ident):
        """
        Look up the value of |ident| in the value mapping passed in the
        constructor.
        """
        if self.strict:
            return self.valuemapping[ident]
        else:
            return self.valuemapping.get(ident, None)

    def advance(self, expected):
        """
        Assert that the next token is an instance of |expected|, and advance
        to the next token.
        """
        if not isinstance(self.token, expected):
            raise Exception, "Unexpected token!"
        self.token = self.iter.next()

    def expression(self, rbp=0):
        """
        Parse and return the value of an expression until a token with
        right binding power greater than rbp is encountered.
        """
        t = self.token
        self.token = self.iter.next()
        left = t.nud(self)
        while rbp < self.token.lbp:
            t = self.token
            self.token = self.iter.next()
            left = t.led(self, left)
        return left

    def parse(self):
        """
        Parse and return the value of the expression in the text
        passed to the constructor. Raises a ParseError if the expression
        could not be parsed.
        """
        try:
            self.iter = self._tokenize()
            self.token = self.iter.next()
            return self.expression()
        except:
            raise ParseError("could not parse: %s; variables: %s" % (self.text, self.valuemapping))

    __call__ = parse

def parse(text, **values):
    """
    Parse and evaluate a boolean expression.
    :param text: The expression to parse, as a string.
    :param values: A dict containing a name to value mapping for identifiers
                   referenced in *text*.
    :rtype: the final value of the expression.
    :raises: :py:exc::ParseError: will be raised if parsing fails.
    """
    return ExpressionParser(text, values).parse()


### path normalization

def normalize_path(path):
    """normalize a relative path"""
    if sys.platform.startswith('win'):
        return path.replace('/', os.path.sep)
    return path

def denormalize_path(path):
    """denormalize a relative path"""
    if sys.platform.startswith('win'):
        return path.replace(os.path.sep, '/')
    return path


### .ini reader

def read_ini(fp, variables=None, default='DEFAULT',
             comments=';#', separators=('=', ':'),
             strict=True):
    """
    read an .ini file and return a list of [(section, values)]
    - fp : file pointer or path to read
    - variables : default set of variables
    - default : name of the section for the default section
    - comments : characters that if they start a line denote a comment
    - separators : strings that denote key, value separation in order
    - strict : whether to be strict about parsing
    """

    # variables
    variables = variables or {}
    sections = []
    key = value = None
    section_names = set()
    if isinstance(fp, basestring):
        fp = file(fp)

    # read the lines
    for (linenum, line) in enumerate(fp.readlines(), start=1):

        stripped = line.strip()

        # ignore blank lines
        if not stripped:
            # reset key and value to avoid continuation lines
            key = value = None
            continue

        # ignore comment lines
        if stripped[0] in comments:
            continue

        # check for a new section
        if len(stripped) > 2 and stripped[0] == '[' and stripped[-1] == ']':
            section = stripped[1:-1].strip()
            key = value = None

            # deal with DEFAULT section
            if section.lower() == default.lower():
                if strict:
                    assert default not in section_names
                section_names.add(default)
                current_section = variables
                continue

            if strict:
                # make sure this section doesn't already exist
                assert section not in section_names, "Section '%s' already found in '%s'" % (section, section_names)

            section_names.add(section)
            current_section = {}
            sections.append((section, current_section))
            continue

        # if there aren't any sections yet, something bad happen
        if not section_names:
            raise Exception('No sections found')

        # (key, value) pair
        for separator in separators:
            if separator in stripped:
                key, value = stripped.split(separator, 1)
                key = key.strip()
                value = value.strip()

                if strict:
                    # make sure this key isn't already in the section or empty
                    assert key
                    if current_section is not variables:
                        assert key not in current_section

                current_section[key] = value
                break
        else:
            # continuation line ?
            if line[0].isspace() and key:
                value = '%s%s%s' % (value, os.linesep, stripped)
                current_section[key] = value
            else:
                # something bad happened!
                if hasattr(fp, 'name'):
                    filename = fp.name
                else:
                    filename = 'unknown'
                raise Exception("Error parsing manifest file '%s', line %s" %
                                (filename, linenum))

    # interpret the variables
    def interpret_variables(global_dict, local_dict):
        variables = global_dict.copy()
        if 'skip-if' in local_dict and 'skip-if' in variables:
            local_dict['skip-if'] = "(%s) || (%s)" % (variables['skip-if'].split('#')[0], local_dict['skip-if'].split('#')[0])
        variables.update(local_dict)

        # server-root is an os path declared relative to the manifest file.
        # inheritance demands we expand it as absolute
        if 'server-root' in variables:
            root = os.path.join(os.path.dirname(fp.name),
                                variables['server-root'])
            variables['server-root'] = os.path.abspath(root)

        return variables

    sections = [(i, interpret_variables(variables, j)) for i, j in sections]
    return sections


### objects for parsing manifests

class ManifestParser(object):
    """read .ini manifests"""

    def __init__(self, manifests=(), defaults=None, strict=True):
        self._defaults = defaults or {}
        self._ancestor_defaults = {}
        self.tests = []
        self.manifest_defaults = {}
        self.strict = strict
        self.rootdir = None
        self.relativeRoot = None
        if manifests:
            self.read(*manifests)

    def getRelativeRoot(self, root):
        return root

    ### methods for reading manifests

    def _read(self, root, filename, defaults, defaults_only=False):
        """
        Internal recursive method for reading and parsing manifests.
        Stores all found tests in self.tests
        :param root: The base path
        :param filename: File object or string path for the base manifest file
        :param defaults: Options that apply to all items
        :param defaults_only: If True will only gather options, not include
                              tests. Used for upstream parent includes
                              (default False)
        """
        def read_file(type):
            include_file = section.split(type, 1)[-1]
            include_file = normalize_path(include_file)
            if not os.path.isabs(include_file):
                include_file = os.path.join(self.getRelativeRoot(here), include_file)
            if not os.path.exists(include_file):
                message = "Included file '%s' does not exist" % include_file
                if self.strict:
                    raise IOError(message)
                else:
                    sys.stderr.write("%s\n" % message)
                    return
            return include_file

        # get directory of this file if not file-like object
        if isinstance(filename, string):
            filename = os.path.abspath(filename)
            fp = open(filename)
            here = os.path.dirname(filename)
        else:
            fp = filename
            filename = here = None
        defaults['here'] = here

        # Rootdir is needed for relative path calculation. Precompute it for
        # the microoptimization used below.
        if self.rootdir is None:
            rootdir = ""
        else:
            assert os.path.isabs(self.rootdir)
            rootdir = self.rootdir + os.path.sep

        # read the configuration
        sections = read_ini(fp=fp, variables=defaults, strict=self.strict)
        self.manifest_defaults[filename] = defaults

        # get the tests
        for section, data in sections:
            subsuite = ''
            if 'subsuite' in data:
                subsuite = data['subsuite']

            # read the parent manifest if specified
            if section.startswith('parent:'):
                include_file = read_file('parent:')
                if include_file:
                    self._read(root, include_file, {}, True)
                continue

            # If this is a parent include we only load the defaults into ancestor
            if defaults_only:
                self._ancestor_defaults = dict(data.items() + self._ancestor_defaults.items())
                break

            # a file to include
            # TODO: keep track of included file structure:
            # self.manifests = {'manifest.ini': 'relative/path.ini'}
            if section.startswith('include:'):
                include_file = read_file('include:')
                if include_file:
                    include_defaults = data.copy()
                    self._read(root, include_file, include_defaults)
                continue

            # otherwise an item
            # apply ancestor defaults, while maintaining current file priority
            data = dict(self._ancestor_defaults.items() + data.items())

            test = data
            test['name'] = section

            # Will be None if the manifest being read is a file-like object.
            test['manifest'] = filename

            # determine the path
            path = test.get('path', section)
            _relpath = path
            if '://' not in path: # don't futz with URLs
                path = normalize_path(path)
                if here and not os.path.isabs(path):
                    path = os.path.normpath(os.path.join(here, path))

                # Microoptimization, because relpath is quite expensive.
                # We know that rootdir is an absolute path or empty. If path
                # starts with rootdir, then path is also absolute and the tail
                # of the path is the relative path (possibly non-normalized,
                # when here is unknown).
                # For this to work rootdir needs to be terminated with a path
                # separator, so that references to sibling directories with
                # a common prefix don't get misscomputed (e.g. /root and
                # /rootbeer/file).
                # When the rootdir is unknown, the relpath needs to be left
                # unchanged. We use an empty string as rootdir in that case,
                # which leaves relpath unchanged after slicing.
                if path.startswith(rootdir):
                    _relpath = path[len(rootdir):]
                else:
                    _relpath = relpath(path, rootdir)

            test['subsuite'] = subsuite
            test['path'] = path
            test['relpath'] = _relpath

            # append the item
            self.tests.append(test)

    def read(self, *filenames, **defaults):
        """
        read and add manifests from file paths or file-like objects

        filenames -- file paths or file-like objects to read as manifests
        defaults -- default variables
        """

        # ensure all files exist
        missing = [filename for filename in filenames
                   if isinstance(filename, string) and not os.path.exists(filename) ]
        if missing:
            raise IOError('Missing files: %s' % ', '.join(missing))

        # default variables
        _defaults = defaults.copy() or self._defaults.copy()
        _defaults.setdefault('here', None)

        # process each file
        for filename in filenames:
            # set the per file defaults
            defaults = _defaults.copy()
            here = None
            if isinstance(filename, string):
                here = os.path.dirname(os.path.abspath(filename))
                defaults['here'] = here # directory of master .ini file

            if self.rootdir is None:
                # set the root directory
                # == the directory of the first manifest given
                self.rootdir = here

            self._read(here, filename, defaults)


    ### methods for querying manifests

    def query(self, *checks, **kw):
        """
        general query function for tests
        - checks : callable conditions to test if the test fulfills the query
        """
        tests = kw.get('tests', None)
        if tests is None:
            tests = self.tests
        retval = []
        for test in tests:
            for check in checks:
                if not check(test):
                    break
            else:
                retval.append(test)
        return retval

    def get(self, _key=None, inverse=False, tags=None, tests=None, **kwargs):
        # TODO: pass a dict instead of kwargs since you might hav
        # e.g. 'inverse' as a key in the dict

        # TODO: tags should just be part of kwargs with None values
        # (None == any is kinda weird, but probably still better)

        # fix up tags
        if tags:
            tags = set(tags)
        else:
            tags = set()

        # make some check functions
        if inverse:
            has_tags = lambda test: not tags.intersection(test.keys())
            def dict_query(test):
                for key, value in kwargs.items():
                    if test.get(key) == value:
                        return False
                return True
        else:
            has_tags = lambda test: tags.issubset(test.keys())
            def dict_query(test):
                for key, value in kwargs.items():
                    if test.get(key) != value:
                        return False
                return True

        # query the tests
        tests = self.query(has_tags, dict_query, tests=tests)

        # if a key is given, return only a list of that key
        # useful for keys like 'name' or 'path'
        if _key:
            return [test[_key] for test in tests]

        # return the tests
        return tests

    def manifests(self, tests=None):
        """
        return manifests in order in which they appear in the tests
        """
        if tests is None:
            # Make sure to return all the manifests, even ones without tests.
            return self.manifest_defaults.keys()

        manifests = []
        for test in tests:
            manifest = test.get('manifest')
            if not manifest:
                continue
            if manifest not in manifests:
                manifests.append(manifest)
        return manifests

    def paths(self):
        return [i['path'] for i in self.tests]


    ### methods for auditing

    def missing(self, tests=None):
        """return list of tests that do not exist on the filesystem"""
        if tests is None:
            tests = self.tests
        return [test for test in tests
                if not os.path.exists(test['path'])]

    def check_missing(self, tests=None):
        missing = self.missing(tests=tests)
        if missing:
            missing_paths = [test['path'] for test in missing]
            if self.strict:
                raise IOError("Strict mode enabled, test paths must exist. "
                              "The following test(s) are missing: %s" %
                              json.dumps(missing_paths, indent=2))
            print >> sys.stderr, "Warning: The following test(s) are missing: %s" % \
                                  json.dumps(missing_paths, indent=2)
        return missing

    def verifyDirectory(self, directories, pattern=None, extensions=None):
        """
        checks what is on the filesystem vs what is in a manifest
        returns a 2-tuple of sets:
        (missing_from_filesystem, missing_from_manifest)
        """

        files = set([])
        if isinstance(directories, basestring):
            directories = [directories]

        # get files in directories
        for directory in directories:
            for dirpath, dirnames, filenames in os.walk(directory, topdown=True):

                # only add files that match a pattern
                if pattern:
                    filenames = fnmatch.filter(filenames, pattern)

                # only add files that have one of the extensions
                if extensions:
                    filenames = [filename for filename in filenames
                                 if os.path.splitext(filename)[-1] in extensions]

                files.update([os.path.join(dirpath, filename) for filename in filenames])

        paths = set(self.paths())
        missing_from_filesystem = paths.difference(files)
        missing_from_manifest = files.difference(paths)
        return (missing_from_filesystem, missing_from_manifest)


    ### methods for output

    def write(self, fp=sys.stdout, rootdir=None,
              global_tags=None, global_kwargs=None,
              local_tags=None, local_kwargs=None):
        """
        write a manifest given a query
        global and local options will be munged to do the query
        globals will be written to the top of the file
        locals (if given) will be written per test
        """

        # open file if `fp` given as string
        close = False
        if isinstance(fp, string):
            fp = file(fp, 'w')
            close = True

        # root directory
        if rootdir is None:
            rootdir = self.rootdir

        # sanitize input
        global_tags = global_tags or set()
        local_tags = local_tags or set()
        global_kwargs = global_kwargs or {}
        local_kwargs = local_kwargs or {}

        # create the query
        tags = set([])
        tags.update(global_tags)
        tags.update(local_tags)
        kwargs = {}
        kwargs.update(global_kwargs)
        kwargs.update(local_kwargs)

        # get matching tests
        tests = self.get(tags=tags, **kwargs)

        # print the .ini manifest
        if global_tags or global_kwargs:
            print >> fp, '[DEFAULT]'
            for tag in global_tags:
                print >> fp, '%s =' % tag
            for key, value in global_kwargs.items():
                print >> fp, '%s = %s' % (key, value)
            print >> fp

        for test in tests:
            test = test.copy() # don't overwrite

            path = test['name']
            if not os.path.isabs(path):
                path = test['path']
                if self.rootdir:
                    path = relpath(test['path'], self.rootdir)
                path = denormalize_path(path)
            print >> fp, '[%s]' % path

            # reserved keywords:
            reserved = ['path', 'name', 'here', 'manifest', 'relpath']
            for key in sorted(test.keys()):
                if key in reserved:
                    continue
                if key in global_kwargs:
                    continue
                if key in global_tags and not test[key]:
                    continue
                print >> fp, '%s = %s' % (key, test[key])
            print >> fp

        if close:
            # close the created file
            fp.close()

    def __str__(self):
        fp = StringIO()
        self.write(fp=fp)
        value = fp.getvalue()
        return value

    def copy(self, directory, rootdir=None, *tags, **kwargs):
        """
        copy the manifests and associated tests
        - directory : directory to copy to
        - rootdir : root directory to copy to (if not given from manifests)
        - tags : keywords the tests must have
        - kwargs : key, values the tests must match
        """
        # XXX note that copy does *not* filter the tests out of the
        # resulting manifest; it just stupidly copies them over.
        # ideally, it would reread the manifests and filter out the
        # tests that don't match *tags and **kwargs

        # destination
        if not os.path.exists(directory):
            os.path.makedirs(directory)
        else:
            # sanity check
            assert os.path.isdir(directory)

        # tests to copy
        tests = self.get(tags=tags, **kwargs)
        if not tests:
            return # nothing to do!

        # root directory
        if rootdir is None:
            rootdir = self.rootdir

        # copy the manifests + tests
        manifests = [relpath(manifest, rootdir) for manifest in self.manifests()]
        for manifest in manifests:
            destination = os.path.join(directory, manifest)
            dirname = os.path.dirname(destination)
            if not os.path.exists(dirname):
                os.makedirs(dirname)
            else:
                # sanity check
                assert os.path.isdir(dirname)
            shutil.copy(os.path.join(rootdir, manifest), destination)

        missing = self.check_missing(tests)
        tests = [test for test in tests if test not in missing]
        for test in tests:
            if os.path.isabs(test['name']):
                continue
            source = test['path']
            destination = os.path.join(directory, relpath(test['path'], rootdir))
            shutil.copy(source, destination)
            # TODO: ensure that all of the tests are below the from_dir

    def update(self, from_dir, rootdir=None, *tags, **kwargs):
        """
        update the tests as listed in a manifest from a directory
        - from_dir : directory where the tests live
        - rootdir : root directory to copy to (if not given from manifests)
        - tags : keys the tests must have
        - kwargs : key, values the tests must match
        """

        # get the tests
        tests = self.get(tags=tags, **kwargs)

        # get the root directory
        if not rootdir:
            rootdir = self.rootdir

        # copy them!
        for test in tests:
            if not os.path.isabs(test['name']):
                _relpath = relpath(test['path'], rootdir)
                source = os.path.join(from_dir, _relpath)
                if not os.path.exists(source):
                    message = "Missing test: '%s' does not exist!"
                    if self.strict:
                        raise IOError(message)
                    print >> sys.stderr, message + " Skipping."
                    continue
                destination = os.path.join(rootdir, _relpath)
                shutil.copy(source, destination)

    ### directory importers

    @classmethod
    def _walk_directories(cls, directories, function, pattern=None, ignore=()):
        """
        internal function to import directories
        """

        class FilteredDirectoryContents(object):
            """class to filter directory contents"""

            sort = sorted

            def __init__(self, pattern=pattern, ignore=ignore, cache=None):
                if pattern is None:
                    pattern = set()
                if isinstance(pattern, basestring):
                    pattern = [pattern]
                self.patterns = pattern
                self.ignore = set(ignore)

                # cache of (dirnames, filenames) keyed on directory real path
                # assumes volume is frozen throughout scope
                self._cache = cache or {}

            def __call__(self, directory):
                """returns 2-tuple: dirnames, filenames"""
                directory = os.path.realpath(directory)
                if directory not in self._cache:
                    dirnames, filenames = self.contents(directory)

                    # filter out directories without progeny
                    # XXX recursive: should keep track of seen directories
                    dirnames = [ dirname for dirname in dirnames
                                 if not self.empty(os.path.join(directory, dirname)) ]

                    self._cache[directory] = (tuple(dirnames), filenames)

                # return cached values
                return self._cache[directory]

            def empty(self, directory):
                """
                returns if a directory and its descendents are empty
                """
                return self(directory) == ((), ())

            def contents(self, directory, sort=None):
                """
                return directory contents as (dirnames, filenames)
                with `ignore` and `pattern` applied
                """

                if sort is None:
                    sort = self.sort

                # split directories and files
                dirnames = []
                filenames = []
                for item in os.listdir(directory):
                    path = os.path.join(directory, item)
                    if os.path.isdir(path):
                        dirnames.append(item)
                    else:
                        # XXX not sure what to do if neither a file or directory
                        # (if anything)
                        assert os.path.isfile(path)
                        filenames.append(item)

                # filter contents;
                # this could be done in situ re the above for loop
                # but it is really disparate in intent
                # and could conceivably go to a separate method
                dirnames = [dirname for dirname in dirnames
                            if dirname not in self.ignore]
                filenames = set(filenames)
                # we use set functionality to filter filenames
                if self.patterns:
                    matches = set()
                    matches.update(*[fnmatch.filter(filenames, pattern)
                                     for pattern in self.patterns])
                    filenames = matches

                if sort is not None:
                    # sort dirnames, filenames
                    dirnames = sort(dirnames)
                    filenames = sort(filenames)

                return (tuple(dirnames), tuple(filenames))

        # make a filtered directory object
        directory_contents = FilteredDirectoryContents(pattern=pattern, ignore=ignore)

        # walk the directories, generating manifests
        for index, directory in enumerate(directories):

            for dirpath, dirnames, filenames in os.walk(directory):

                # get the directory contents from the caching object
                _dirnames, filenames = directory_contents(dirpath)
                # filter out directory names
                dirnames[:] = _dirnames

                # call callback function
                function(directory, dirpath, dirnames, filenames)

    @classmethod
    def populate_directory_manifests(cls, directories, filename, pattern=None, ignore=(), overwrite=False):
        """
        walks directories and writes manifests of name `filename` in-place; returns `cls` instance populated
        with the given manifests

        filename -- filename of manifests to write
        pattern -- shell pattern (glob) or patterns of filenames to match
        ignore -- directory names to ignore
        overwrite -- whether to overwrite existing files of given name
        """

        manifest_dict = {}

        if os.path.basename(filename) != filename:
            raise IOError("filename should not include directory name")

        # no need to hit directories more than once
        _directories = directories
        directories = []
        for directory in _directories:
            if directory not in directories:
                directories.append(directory)

        def callback(directory, dirpath, dirnames, filenames):
            """write a manifest for each directory"""

            manifest_path = os.path.join(dirpath, filename)
            if (dirnames or filenames) and not (os.path.exists(manifest_path) and overwrite):
                with file(manifest_path, 'w') as manifest:
                    for dirname in dirnames:
                        print >> manifest, '[include:%s]' % os.path.join(dirname, filename)
                    for _filename in filenames:
                        print >> manifest, '[%s]' % _filename

                # add to list of manifests
                manifest_dict.setdefault(directory, manifest_path)

        # walk the directories to gather files
        cls._walk_directories(directories, callback, pattern=pattern, ignore=ignore)
        # get manifests
        manifests = [manifest_dict[directory] for directory in _directories]

        # create a `cls` instance with the manifests
        return cls(manifests=manifests)

    @classmethod
    def from_directories(cls, directories, pattern=None, ignore=(), write=None, relative_to=None):
        """
        convert directories to a simple manifest; returns ManifestParser instance

        pattern -- shell pattern (glob) or patterns of filenames to match
        ignore -- directory names to ignore
        write -- filename or file-like object of manifests to write;
                 if `None` then a StringIO instance will be created
        relative_to -- write paths relative to this path;
                       if false then the paths are absolute
        """


        # determine output
        opened_manifest_file = None # name of opened manifest file
        absolute = not relative_to # whether to output absolute path names as names
        if isinstance(write, string):
            opened_manifest_file = write
            write = file(write, 'w')
        if write is None:
            write = StringIO()

        # walk the directories, generating manifests
        def callback(directory, dirpath, dirnames, filenames):

            # absolute paths
            filenames = [os.path.join(dirpath, filename)
                         for filename in filenames]
            # ensure new manifest isn't added
            filenames = [filename for filename in filenames
                         if filename != opened_manifest_file]
            # normalize paths
            if not absolute and relative_to:
                filenames = [relpath(filename, relative_to)
                             for filename in filenames]

            # write to manifest
            print >> write, '\n'.join(['[%s]' % denormalize_path(filename)
                                               for filename in filenames])


        cls._walk_directories(directories, callback, pattern=pattern, ignore=ignore)

        if opened_manifest_file:
            # close file
            write.close()
            manifests = [opened_manifest_file]
        else:
            # manifests/write is a file-like object;
            # rewind buffer
            write.flush()
            write.seek(0)
            manifests = [write]


        # make a ManifestParser instance
        return cls(manifests=manifests)

convert = ManifestParser.from_directories


class TestManifest(ManifestParser):
    """
    apply logic to manifests;  this is your integration layer :)
    specific harnesses may subclass from this if they need more logic
    """

    def filter(self, values, tests):
        """
        filter on a specific list tag, e.g.:
        run-if = os == win linux
        skip-if = os == mac
        """

        # tags:
        run_tag = 'run-if'
        skip_tag = 'skip-if'
        fail_tag = 'fail-if'

        cache = {}

        def _parse(cond):
            if '#' in cond:
                cond = cond[:cond.index('#')]
            cond = cond.strip()
            if cond in cache:
                ret = cache[cond]
            else:
                ret = parse(cond, **values)
                cache[cond] = ret
            return ret

        # loop over test
        for test in tests:
            reason = None # reason to disable

            # tagged-values to run
            if run_tag in test:
                condition = test[run_tag]
                if not _parse(condition):
                    reason = '%s: %s' % (run_tag, condition)

            # tagged-values to skip
            if skip_tag in test:
                condition = test[skip_tag]
                if _parse(condition):
                    reason = '%s: %s' % (skip_tag, condition)

            # mark test as disabled if there's a reason
            if reason:
                test.setdefault('disabled', reason)

            # mark test as a fail if so indicated
            if fail_tag in test:
                condition = test[fail_tag]
                if _parse(condition):
                    test['expected'] = 'fail'

    def active_tests(self, exists=True, disabled=True, options=None, **values):
        """
        - exists : return only existing tests
        - disabled : whether to return disabled tests
        - options: an optparse or argparse options object, used for subsuites
        - values : keys and values to filter on (e.g. `os = linux mac`)
        """
        tests = [i.copy() for i in self.tests] # shallow copy

        # Conditional subsuites are specified using:
        #    subsuite = foo,condition
        # where 'foo' is the subsuite name, and 'condition' is the same type of
        # condition used for skip-if.  If the condition doesn't evaluate to true,
        # the subsuite designation will be removed from the test.
        #
        # Look for conditional subsuites, and replace them with the subsuite itself
        # (if the condition is true), or nothing.
        for test in tests:
            subsuite = test.get('subsuite', '')
            if ',' in subsuite:
                try:
                    subsuite, condition = subsuite.split(',')
                except ValueError:
                    raise ParseError("subsuite condition can't contain commas")
                # strip any comments from the condition
                condition = condition.split('#')[0]
                matched = parse(condition, **values)
                if matched:
                    test['subsuite'] = subsuite
                else:
                    test['subsuite'] = ''

        # Filter on current subsuite
        if options:
            if hasattr(options, 'subsuite') and options.subsuite:
                tests = [test for test in tests if options.subsuite == test['subsuite']]
            else:
                tests = [test for test in tests if not test['subsuite']]

        # mark all tests as passing unless indicated otherwise
        for test in tests:
            test['expected'] = test.get('expected', 'pass')

        # ignore tests that do not exist
        if exists:
            missing = self.check_missing(tests)
            tests = [test for test in tests if test not in missing]

        # filter by tags
        self.filter(values, tests)

        # ignore disabled tests if specified
        if not disabled:
            tests = [test for test in tests
                     if not 'disabled' in test]

        # return active tests
        return tests

    def test_paths(self):
        return [test['path'] for test in self.active_tests()]


### command line attributes

class ParserError(Exception):
  """error for exceptions while parsing the command line"""

def parse_args(_args):
    """
    parse and return:
    --keys=value (or --key value)
    -tags
    args
    """

    # return values
    _dict = {}
    tags = []
    args = []

    # parse the arguments
    key = None
    for arg in _args:
        if arg.startswith('---'):
            raise ParserError("arguments should start with '-' or '--' only")
        elif arg.startswith('--'):
            if key:
                raise ParserError("Key %s still open" % key)
            key = arg[2:]
            if '=' in key:
                key, value = key.split('=', 1)
                _dict[key] = value
                key = None
                continue
        elif arg.startswith('-'):
            if key:
                raise ParserError("Key %s still open" % key)
            tags.append(arg[1:])
            continue
        else:
            if key:
                _dict[key] = arg
                continue
            args.append(arg)

    # return values
    return (_dict, tags, args)


### classes for subcommands

class CLICommand(object):
    usage = '%prog [options] command'
    def __init__(self, parser):
      self._parser = parser # master parser
    def parser(self):
      return OptionParser(usage=self.usage, description=self.__doc__,
                          add_help_option=False)

class Copy(CLICommand):
    usage = '%prog [options] copy manifest directory -tag1 -tag2 --key1=value1 --key2=value2 ...'
    def __call__(self, options, args):
      # parse the arguments
      try:
        kwargs, tags, args = parse_args(args)
      except ParserError, e:
        self._parser.error(e.message)

      # make sure we have some manifests, otherwise it will
      # be quite boring
      if not len(args) == 2:
        HelpCLI(self._parser)(options, ['copy'])
        return

      # read the manifests
      # TODO: should probably ensure these exist here
      manifests = ManifestParser()
      manifests.read(args[0])

      # print the resultant query
      manifests.copy(args[1], None, *tags, **kwargs)


class CreateCLI(CLICommand):
    """
    create a manifest from a list of directories
    """
    usage = '%prog [options] create directory <directory> <...>'

    def parser(self):
        parser = CLICommand.parser(self)
        parser.add_option('-p', '--pattern', dest='pattern',
                          help="glob pattern for files")
        parser.add_option('-i', '--ignore', dest='ignore',
                          default=[], action='append',
                          help='directories to ignore')
        parser.add_option('-w', '--in-place', dest='in_place',
                          help='Write .ini files in place; filename to write to')
        return parser

    def __call__(self, _options, args):
        parser = self.parser()
        options, args = parser.parse_args(args)

        # need some directories
        if not len(args):
            parser.print_usage()
            return

        # add the directories to the manifest
        for arg in args:
            assert os.path.exists(arg)
            assert os.path.isdir(arg)
            manifest = convert(args, pattern=options.pattern, ignore=options.ignore,
                               write=options.in_place)
        if manifest:
            print manifest


class WriteCLI(CLICommand):
    """
    write a manifest based on a query
    """
    usage = '%prog [options] write manifest <manifest> -tag1 -tag2 --key1=value1 --key2=value2 ...'
    def __call__(self, options, args):

        # parse the arguments
        try:
            kwargs, tags, args = parse_args(args)
        except ParserError, e:
            self._parser.error(e.message)

        # make sure we have some manifests, otherwise it will
        # be quite boring
        if not args:
            HelpCLI(self._parser)(options, ['write'])
            return

        # read the manifests
        # TODO: should probably ensure these exist here
        manifests = ManifestParser()
        manifests.read(*args)

        # print the resultant query
        manifests.write(global_tags=tags, global_kwargs=kwargs)


class HelpCLI(CLICommand):
    """
    get help on a command
    """
    usage = '%prog [options] help [command]'

    def __call__(self, options, args):
        if len(args) == 1 and args[0] in commands:
            commands[args[0]](self._parser).parser().print_help()
        else:
            self._parser.print_help()
            print '\nCommands:'
            for command in sorted(commands):
                print '  %s : %s' % (command, commands[command].__doc__.strip())

class UpdateCLI(CLICommand):
    """
    update the tests as listed in a manifest from a directory
    """
    usage = '%prog [options] update manifest directory -tag1 -tag2 --key1=value1 --key2=value2 ...'

    def __call__(self, options, args):
        # parse the arguments
        try:
            kwargs, tags, args = parse_args(args)
        except ParserError, e:
            self._parser.error(e.message)

        # make sure we have some manifests, otherwise it will
        # be quite boring
        if not len(args) == 2:
            HelpCLI(self._parser)(options, ['update'])
            return

        # read the manifests
        # TODO: should probably ensure these exist here
        manifests = ManifestParser()
        manifests.read(args[0])

        # print the resultant query
        manifests.update(args[1], None, *tags, **kwargs)


# command -> class mapping
commands = { 'create': CreateCLI,
             'help': HelpCLI,
             'update': UpdateCLI,
             'write': WriteCLI }

def main(args=sys.argv[1:]):
    """console_script entry point"""

    # set up an option parser
    usage = '%prog [options] [command] ...'
    description = "%s. Use `help` to display commands" % __doc__.strip()
    parser = OptionParser(usage=usage, description=description)
    parser.add_option('-s', '--strict', dest='strict',
                      action='store_true', default=False,
                      help='adhere strictly to errors')
    parser.disable_interspersed_args()

    options, args = parser.parse_args(args)

    if not args:
        HelpCLI(parser)(options, args)
        parser.exit()

    # get the command
    command = args[0]
    if command not in commands:
        parser.error("Command must be one of %s (you gave '%s')" % (', '.join(sorted(commands.keys())), command))

    handler = commands[command](parser)
    handler(options, args[1:])

if __name__ == '__main__':
    main()
