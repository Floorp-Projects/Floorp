# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for reading metadata from the build system into
# data structures.

r"""Read build frontend files into data structures.

In terms of code architecture, the main interface is BuildReader. BuildReader
starts with a root mozbuild file. It creates a new execution environment for
this file, which is represented by the Sandbox class. The Sandbox class is used
to fill a Context, representing the output of an individual mozbuild file. The

The BuildReader contains basic logic for traversing a tree of mozbuild files.
It does this by examining specific variables populated during execution.
"""

from __future__ import absolute_import, print_function, unicode_literals

import ast
import inspect
import logging
import os
import sys
import textwrap
import time
import traceback
import types

from collections import (
    defaultdict,
    OrderedDict,
)
from io import StringIO
from multiprocessing import cpu_count

from mozbuild.util import (
    EmptyValue,
    HierarchicalStringList,
    memoize,
    ReadOnlyDefaultDict,
)

from mozbuild.testing import (
    TEST_MANIFESTS,
    REFTEST_FLAVORS,
    WEB_PLATFORM_TESTS_FLAVORS,
)

from mozbuild.backend.configenvironment import ConfigEnvironment

from mozpack.files import FileFinder
import mozpack.path as mozpath

from .data import (
    AndroidEclipseProjectData,
    JavaJarData,
)

from .sandbox import (
    default_finder,
    SandboxError,
    SandboxExecutionError,
    SandboxLoadError,
    Sandbox,
)

from .context import (
    Context,
    ContextDerivedValue,
    Files,
    FUNCTIONS,
    VARIABLES,
    DEPRECATION_HINTS,
    SourcePath,
    SPECIAL_VARIABLES,
    SUBCONTEXTS,
    SubContext,
    TemplateContext,
)

from mozbuild.base import ExecutionSummary
from concurrent.futures.process import ProcessPoolExecutor



if sys.version_info.major == 2:
    text_type = unicode
    type_type = types.TypeType
else:
    text_type = str
    type_type = type


def log(logger, level, action, params, formatter):
    logger.log(level, formatter, extra={'action': action, 'params': params})


class EmptyConfig(object):
    """A config object that is empty.

    This config object is suitable for using with a BuildReader on a vanilla
    checkout, without any existing configuration. The config is simply
    bootstrapped from a top source directory path.
    """
    class PopulateOnGetDict(ReadOnlyDefaultDict):
        """A variation on ReadOnlyDefaultDict that populates during .get().

        This variation is needed because CONFIG uses .get() to access members.
        Without it, None (instead of our EmptyValue types) would be returned.
        """
        def get(self, key, default=None):
            return self[key]

    def __init__(self, topsrcdir):
        self.topsrcdir = topsrcdir
        self.topobjdir = ''

        self.substs = self.PopulateOnGetDict(EmptyValue, {
            # These 2 variables are used semi-frequently and it isn't worth
            # changing all the instances.
            b'MOZ_APP_NAME': b'empty',
            b'MOZ_CHILD_PROCESS_NAME': b'empty',
            # Set manipulations are performed within the moz.build files. But
            # set() is not an exposed symbol, so we can't create an empty set.
            b'NECKO_PROTOCOLS': set(),
            # Needed to prevent js/src's config.status from loading.
            b'JS_STANDALONE': b'1',
        })
        udict = {}
        for k, v in self.substs.items():
            if isinstance(v, str):
                udict[k.decode('utf-8')] = v.decode('utf-8')
            else:
                udict[k] = v
        self.substs_unicode = self.PopulateOnGetDict(EmptyValue, udict)
        self.defines = self.substs
        self.external_source_dir = None
        self.error_is_fatal = False


def is_read_allowed(path, config):
    """Whether we are allowed to load a mozbuild file at the specified path.

    This is used as cheap security to ensure the build is isolated to known
    source directories.

    We are allowed to read from the main source directory and any defined
    external source directories. The latter is to allow 3rd party applications
    to hook into our build system.
    """
    assert os.path.isabs(path)
    assert os.path.isabs(config.topsrcdir)

    path = mozpath.normpath(path)
    topsrcdir = mozpath.normpath(config.topsrcdir)

    if mozpath.basedir(path, [topsrcdir]):
        return True

    if config.external_source_dir:
        external_dir = os.path.normcase(config.external_source_dir)
        norm_path = os.path.normcase(path)
        if mozpath.basedir(norm_path, [external_dir]):
            return True

    return False


class SandboxCalledError(SandboxError):
    """Represents an error resulting from calling the error() function."""

    def __init__(self, file_stack, message):
        SandboxError.__init__(self, file_stack)
        self.message = message


class MozbuildSandbox(Sandbox):
    """Implementation of a Sandbox tailored for mozbuild files.

    We expose a few useful functions and expose the set of variables defining
    Mozilla's build system.

    context is a Context instance.

    metadata is a dict of metadata that can be used during the sandbox
    evaluation.
    """
    def __init__(self, context, metadata={}, finder=default_finder):
        assert isinstance(context, Context)

        Sandbox.__init__(self, context, finder=finder)

        self._log = logging.getLogger(__name__)

        self.metadata = dict(metadata)
        exports = self.metadata.get('exports', {})
        self.exports = set(exports.keys())
        context.update(exports)
        self.templates = self.metadata.setdefault('templates', {})
        self.special_variables = self.metadata.setdefault('special_variables',
                                                          SPECIAL_VARIABLES)
        self.functions = self.metadata.setdefault('functions', FUNCTIONS)
        self.subcontext_types = self.metadata.setdefault('subcontexts',
                                                         SUBCONTEXTS)

    def __getitem__(self, key):
        if key in self.special_variables:
            return self.special_variables[key][0](self._context)
        if key in self.functions:
            return self._create_function(self.functions[key])
        if key in self.subcontext_types:
            return self._create_subcontext(self.subcontext_types[key])
        if key in self.templates:
            return self._create_template_wrapper(self.templates[key])
        return Sandbox.__getitem__(self, key)

    def __contains__(self, key):
        if any(key in d for d in (self.special_variables, self.functions,
                                  self.subcontext_types, self.templates)):
            return True

        return Sandbox.__contains__(self, key)

    def __setitem__(self, key, value):
        if key in self.special_variables and value is self[key]:
            return
        if key in self.special_variables or key in self.functions or key in self.subcontext_types:
            raise KeyError('Cannot set "%s" because it is a reserved keyword'
                           % key)
        if key in self.exports:
            self._context[key] = value
            self.exports.remove(key)
            return
        Sandbox.__setitem__(self, key, value)

    def exec_file(self, path):
        """Override exec_file to normalize paths and restrict file loading.

        Paths will be rejected if they do not fall under topsrcdir or one of
        the external roots.
        """

        # realpath() is needed for true security. But, this isn't for security
        # protection, so it is omitted.
        if not is_read_allowed(path, self._context.config):
            raise SandboxLoadError(self._context.source_stack,
                sys.exc_info()[2], illegal_path=path)

        Sandbox.exec_file(self, path)

    def _add_java_jar(self, name):
        """Add a Java JAR build target."""
        if not name:
            raise Exception('Java JAR cannot be registered without a name')

        if '/' in name or '\\' in name or '.jar' in name:
            raise Exception('Java JAR names must not include slashes or'
                ' .jar: %s' % name)

        if name in self['JAVA_JAR_TARGETS']:
            raise Exception('Java JAR has already been registered: %s' % name)

        jar = JavaJarData(name)
        self['JAVA_JAR_TARGETS'][name] = jar
        return jar

    # Not exposed to the sandbox.
    def add_android_eclipse_project_helper(self, name):
        """Add an Android Eclipse project target."""
        if not name:
            raise Exception('Android Eclipse project cannot be registered without a name')

        if name in self['ANDROID_ECLIPSE_PROJECT_TARGETS']:
            raise Exception('Android Eclipse project has already been registered: %s' % name)

        data = AndroidEclipseProjectData(name)
        self['ANDROID_ECLIPSE_PROJECT_TARGETS'][name] = data
        return data

    def _add_android_eclipse_project(self, name, manifest):
        if not manifest:
            raise Exception('Android Eclipse project must specify a manifest')

        data = self.add_android_eclipse_project_helper(name)
        data.manifest = manifest
        data.is_library = False
        return data

    def _add_android_eclipse_library_project(self, name):
        data = self.add_android_eclipse_project_helper(name)
        data.manifest = None
        data.is_library = True
        return data

    def _export(self, varname):
        """Export the variable to all subdirectories of the current path."""

        exports = self.metadata.setdefault('exports', dict())
        if varname in exports:
            raise Exception('Variable has already been exported: %s' % varname)

        try:
            # Doing a regular self._context[varname] causes a set as a side
            # effect. By calling the dict method instead, we don't have any
            # side effects.
            exports[varname] = dict.__getitem__(self._context, varname)
        except KeyError:
            self.last_name_error = KeyError('global_ns', 'get_unknown', varname)
            raise self.last_name_error

    def recompute_exports(self):
        """Recompute the variables to export to subdirectories with the current
        values in the subdirectory."""

        if 'exports' in self.metadata:
            for key in self.metadata['exports']:
                self.metadata['exports'][key] = self[key]

    def _include(self, path):
        """Include and exec another file within the context of this one."""

        # path is a SourcePath
        self.exec_file(path.full_path)

    def _warning(self, message):
        # FUTURE consider capturing warnings in a variable instead of printing.
        print('WARNING: %s' % message, file=sys.stderr)

    def _error(self, message):
        if self._context.error_is_fatal:
            raise SandboxCalledError(self._context.source_stack, message)
        else:
            self._warning(message)

    def _template_decorator(self, func):
        """Registers a template function."""

        if not inspect.isfunction(func):
            raise Exception('`template` is a function decorator. You must '
                'use it as `@template` preceding a function declaration.')

        name = func.func_name

        if name in self.templates:
            raise KeyError(
                'A template named "%s" was already declared in %s.' % (name,
                self.templates[name].path))

        if name.islower() or name.isupper() or name[0].islower():
            raise NameError('Template function names must be CamelCase.')

        self.templates[name] = TemplateFunction(func, self)

    @memoize
    def _create_subcontext(self, cls):
        """Return a function object that creates SubContext instances."""
        def fn(*args, **kwargs):
            return cls(self._context, *args, **kwargs)

        return fn

    @memoize
    def _create_function(self, function_def):
        """Returns a function object for use within the sandbox for the given
        function definition.

        The wrapper function does type coercion on the function arguments
        """
        func, args_def, doc = function_def
        def function(*args):
            def coerce(arg, type):
                if not isinstance(arg, type):
                    if issubclass(type, ContextDerivedValue):
                        arg = type(self._context, arg)
                    else:
                        arg = type(arg)
                return arg
            args = [coerce(arg, type) for arg, type in zip(args, args_def)]
            return func(self)(*args)

        return function

    @memoize
    def _create_template_wrapper(self, template):
        """Returns a function object for use within the sandbox for the given
        TemplateFunction instance..

        When a moz.build file contains a reference to a template call, the
        sandbox needs a function to execute. This is what this method returns.
        That function creates a new sandbox for execution of the template.
        After the template is executed, the data from its execution is merged
        with the context of the calling sandbox.
        """
        def template_wrapper(*args, **kwargs):
            context = TemplateContext(
                template=template.name,
                allowed_variables=self._context._allowed_variables,
                config=self._context.config)
            context.add_source(self._context.current_path)
            for p in self._context.all_paths:
                context.add_source(p)

            sandbox = MozbuildSandbox(context, metadata={
                # We should arguably set these defaults to something else.
                # Templates, for example, should arguably come from the state
                # of the sandbox from when the template was declared, not when
                # it was instantiated. Bug 1137319.
                'functions': self.metadata.get('functions', {}),
                'special_variables': self.metadata.get('special_variables', {}),
                'subcontexts': self.metadata.get('subcontexts', {}),
                'templates': self.metadata.get('templates', {})
            }, finder=self._finder)

            template.exec_in_sandbox(sandbox, *args, **kwargs)

            # This is gross, but allows the merge to happen. Eventually, the
            # merging will go away and template contexts emitted independently.
            klass = self._context.__class__
            self._context.__class__ = TemplateContext
            # The sandbox will do all the necessary checks for these merges.
            for key, value in context.items():
                if isinstance(value, dict):
                    self[key].update(value)
                elif isinstance(value, (list, HierarchicalStringList)):
                    self[key] += value
                else:
                    self[key] = value
            self._context.__class__ = klass

            for p in context.all_paths:
                self._context.add_source(p)

        return template_wrapper


class TemplateFunction(object):
    def __init__(self, func, sandbox):
        self.path = func.func_code.co_filename
        self.name = func.func_name

        code = func.func_code
        firstlineno = code.co_firstlineno
        lines = sandbox._current_source.splitlines(True)
        lines = inspect.getblock(lines[firstlineno - 1:])

        # The code lines we get out of inspect.getsourcelines look like
        #   @template
        #   def Template(*args, **kwargs):
        #       VAR = 'value'
        #       ...
        func_ast = ast.parse(''.join(lines), self.path)
        # Remove decorators
        func_ast.body[0].decorator_list = []
        # Adjust line numbers accordingly
        ast.increment_lineno(func_ast, firstlineno - 1)

        # When using a custom dictionary for function globals/locals, Cpython
        # actually never calls __getitem__ and __setitem__, so we need to
        # modify the AST so that accesses to globals are properly directed
        # to a dict.
        self._global_name = b'_data' # AST wants str for this, not unicode
        # In case '_data' is a name used for a variable in the function code,
        # prepend more underscores until we find an unused name.
        while (self._global_name in code.co_names or
                self._global_name in code.co_varnames):
            self._global_name += '_'
        func_ast = self.RewriteName(sandbox, self._global_name).visit(func_ast)

        # Execute the rewritten code. That code now looks like:
        #   def Template(*args, **kwargs):
        #       _data['VAR'] = 'value'
        #       ...
        # The result of executing this code is the creation of a 'Template'
        # function object in the global namespace.
        glob = {'__builtins__': sandbox._builtins}
        func = types.FunctionType(
            compile(func_ast, self.path, 'exec'),
            glob,
            self.name,
            func.func_defaults,
            func.func_closure,
        )
        func()

        self._func = glob[self.name]

    def exec_in_sandbox(self, sandbox, *args, **kwargs):
        """Executes the template function in the given sandbox."""
        # Create a new function object associated with the execution sandbox
        glob = {
            self._global_name: sandbox,
            '__builtins__': sandbox._builtins
        }
        func = types.FunctionType(
            self._func.func_code,
            glob,
            self.name,
            self._func.func_defaults,
            self._func.func_closure
        )
        sandbox.exec_function(func, args, kwargs, self.path,
                              becomes_current_path=False)

    class RewriteName(ast.NodeTransformer):
        """AST Node Transformer to rewrite variable accesses to go through
        a dict.
        """
        def __init__(self, sandbox, global_name):
            self._sandbox = sandbox
            self._global_name = global_name

        def visit_Str(self, node):
            # String nodes we got from the AST parser are str, but we want
            # unicode literals everywhere, so transform them.
            node.s = unicode(node.s)
            return node

        def visit_Name(self, node):
            # Modify uppercase variable references and names known to the
            # sandbox as if they were retrieved from a dict instead.
            if not node.id.isupper() and node.id not in self._sandbox:
                return node

            def c(new_node):
                return ast.copy_location(new_node, node)

            return c(ast.Subscript(
                value=c(ast.Name(id=self._global_name, ctx=ast.Load())),
                slice=c(ast.Index(value=c(ast.Str(s=node.id)))),
                ctx=node.ctx
            ))


class SandboxValidationError(Exception):
    """Represents an error encountered when validating sandbox results."""
    def __init__(self, message, context):
        Exception.__init__(self, message)
        self.context = context

    def __str__(self):
        s = StringIO()

        delim = '=' * 30
        s.write('\n%s\nERROR PROCESSING MOZBUILD FILE\n%s\n\n' % (delim, delim))

        s.write('The error occurred while processing the following file or ')
        s.write('one of the files it includes:\n')
        s.write('\n')
        s.write('    %s/moz.build\n' % self.context.srcdir)
        s.write('\n')

        s.write('The error occurred when validating the result of ')
        s.write('the execution. The reported error is:\n')
        s.write('\n')
        s.write(''.join('    %s\n' % l
                        for l in self.message.splitlines()))
        s.write('\n')

        return s.getvalue()


class BuildReaderError(Exception):
    """Represents errors encountered during BuildReader execution.

    The main purpose of this class is to facilitate user-actionable error
    messages. Execution errors should say:

      - Why they failed
      - Where they failed
      - What can be done to prevent the error

    A lot of the code in this class should arguably be inside sandbox.py.
    However, extraction is somewhat difficult given the additions
    MozbuildSandbox has over Sandbox (e.g. the concept of included files -
    which affect error messages, of course).
    """
    def __init__(self, file_stack, trace, sandbox_exec_error=None,
        sandbox_load_error=None, validation_error=None, other_error=None,
        sandbox_called_error=None):

        self.file_stack = file_stack
        self.trace = trace
        self.sandbox_called_error = sandbox_called_error
        self.sandbox_exec = sandbox_exec_error
        self.sandbox_load = sandbox_load_error
        self.validation_error = validation_error
        self.other = other_error

    @property
    def main_file(self):
        return self.file_stack[-1]

    @property
    def actual_file(self):
        # We report the file that called out to the file that couldn't load.
        if self.sandbox_load is not None:
            if len(self.sandbox_load.file_stack) > 1:
                return self.sandbox_load.file_stack[-2]

            if len(self.file_stack) > 1:
                return self.file_stack[-2]

        if self.sandbox_error is not None and \
            len(self.sandbox_error.file_stack):
            return self.sandbox_error.file_stack[-1]

        return self.file_stack[-1]

    @property
    def sandbox_error(self):
        return self.sandbox_exec or self.sandbox_load or \
            self.sandbox_called_error

    def __str__(self):
        s = StringIO()

        delim = '=' * 30
        s.write('\n%s\nERROR PROCESSING MOZBUILD FILE\n%s\n\n' % (delim, delim))

        s.write('The error occurred while processing the following file:\n')
        s.write('\n')
        s.write('    %s\n' % self.actual_file)
        s.write('\n')

        if self.actual_file != self.main_file and not self.sandbox_load:
            s.write('This file was included as part of processing:\n')
            s.write('\n')
            s.write('    %s\n' % self.main_file)
            s.write('\n')

        if self.sandbox_error is not None:
            self._print_sandbox_error(s)
        elif self.validation_error is not None:
            s.write('The error occurred when validating the result of ')
            s.write('the execution. The reported error is:\n')
            s.write('\n')
            s.write(''.join('    %s\n' % l
                            for l in self.validation_error.message.splitlines()))
            s.write('\n')
        else:
            s.write('The error appears to be part of the %s ' % __name__)
            s.write('Python module itself! It is possible you have stumbled ')
            s.write('across a legitimate bug.\n')
            s.write('\n')

            for l in traceback.format_exception(type(self.other), self.other,
                self.trace):
                s.write(unicode(l))

        return s.getvalue()

    def _print_sandbox_error(self, s):
        # Try to find the frame of the executed code.
        script_frame = None

        # We don't currently capture the trace for SandboxCalledError.
        # Therefore, we don't get line numbers from the moz.build file.
        # FUTURE capture this.
        trace = getattr(self.sandbox_error, 'trace', None)
        frames = []
        if trace:
            frames = traceback.extract_tb(trace)
        for frame in frames:
            if frame[0] == self.actual_file:
                script_frame = frame

            # Reset if we enter a new execution context. This prevents errors
            # in this module from being attributes to a script.
            elif frame[0] == __file__ and frame[2] == 'exec_function':
                script_frame = None

        if script_frame is not None:
            s.write('The error was triggered on line %d ' % script_frame[1])
            s.write('of this file:\n')
            s.write('\n')
            s.write('    %s\n' % script_frame[3])
            s.write('\n')

        if self.sandbox_called_error is not None:
            self._print_sandbox_called_error(s)
            return

        if self.sandbox_load is not None:
            self._print_sandbox_load_error(s)
            return

        self._print_sandbox_exec_error(s)

    def _print_sandbox_called_error(self, s):
        assert self.sandbox_called_error is not None

        s.write('A moz.build file called the error() function.\n')
        s.write('\n')
        s.write('The error it encountered is:\n')
        s.write('\n')
        s.write('    %s\n' % self.sandbox_called_error.message)
        s.write('\n')
        s.write('Correct the error condition and try again.\n')

    def _print_sandbox_load_error(self, s):
        assert self.sandbox_load is not None

        if self.sandbox_load.illegal_path is not None:
            s.write('The underlying problem is an illegal file access. ')
            s.write('This is likely due to trying to access a file ')
            s.write('outside of the top source directory.\n')
            s.write('\n')
            s.write('The path whose access was denied is:\n')
            s.write('\n')
            s.write('    %s\n' % self.sandbox_load.illegal_path)
            s.write('\n')
            s.write('Modify the script to not access this file and ')
            s.write('try again.\n')
            return

        if self.sandbox_load.read_error is not None:
            if not os.path.exists(self.sandbox_load.read_error):
                s.write('The underlying problem is we referenced a path ')
                s.write('that does not exist. That path is:\n')
                s.write('\n')
                s.write('    %s\n' % self.sandbox_load.read_error)
                s.write('\n')
                s.write('Either create the file if it needs to exist or ')
                s.write('do not reference it.\n')
            else:
                s.write('The underlying problem is a referenced path could ')
                s.write('not be read. The trouble path is:\n')
                s.write('\n')
                s.write('    %s\n' % self.sandbox_load.read_error)
                s.write('\n')
                s.write('It is possible the path is not correct. Is it ')
                s.write('pointing to a directory? It could also be a file ')
                s.write('permissions issue. Ensure that the file is ')
                s.write('readable.\n')

            return

        # This module is buggy if you see this.
        raise AssertionError('SandboxLoadError with unhandled properties!')

    def _print_sandbox_exec_error(self, s):
        assert self.sandbox_exec is not None

        inner = self.sandbox_exec.exc_value

        if isinstance(inner, SyntaxError):
            s.write('The underlying problem is a Python syntax error ')
            s.write('on line %d:\n' % inner.lineno)
            s.write('\n')
            s.write('    %s\n' % inner.text)
            if inner.offset:
                s.write((' ' * (inner.offset + 4)) + '^\n')
            s.write('\n')
            s.write('Fix the syntax error and try again.\n')
            return

        if isinstance(inner, KeyError):
            self._print_keyerror(inner, s)
        elif isinstance(inner, ValueError):
            self._print_valueerror(inner, s)
        else:
            self._print_exception(inner, s)

    def _print_keyerror(self, inner, s):
        if not inner.args or inner.args[0] not in ('global_ns', 'local_ns'):
            self._print_exception(inner, s)
            return

        if inner.args[0] == 'global_ns':
            import difflib

            verb = None
            if inner.args[1] == 'get_unknown':
                verb = 'read'
            elif inner.args[1] == 'set_unknown':
                verb = 'write'
            elif inner.args[1] == 'reassign':
                s.write('The underlying problem is an attempt to reassign ')
                s.write('a reserved UPPERCASE variable.\n')
                s.write('\n')
                s.write('The reassigned variable causing the error is:\n')
                s.write('\n')
                s.write('    %s\n' % inner.args[2])
                s.write('\n')
                s.write('Maybe you meant "+=" instead of "="?\n')
                return
            else:
                raise AssertionError('Unhandled global_ns: %s' % inner.args[1])

            s.write('The underlying problem is an attempt to %s ' % verb)
            s.write('a reserved UPPERCASE variable that does not exist.\n')
            s.write('\n')
            s.write('The variable %s causing the error is:\n' % verb)
            s.write('\n')
            s.write('    %s\n' % inner.args[2])
            s.write('\n')
            close_matches = difflib.get_close_matches(inner.args[2],
                                                      VARIABLES.keys(), 2)
            if close_matches:
                s.write('Maybe you meant %s?\n' % ' or '.join(close_matches))
                s.write('\n')

            if inner.args[2] in DEPRECATION_HINTS:
                s.write('%s\n' %
                    textwrap.dedent(DEPRECATION_HINTS[inner.args[2]]).strip())
                return

            s.write('Please change the file to not use this variable.\n')
            s.write('\n')
            s.write('For reference, the set of valid variables is:\n')
            s.write('\n')
            s.write(', '.join(sorted(VARIABLES.keys())) + '\n')
            return

        s.write('The underlying problem is a reference to an undefined ')
        s.write('local variable:\n')
        s.write('\n')
        s.write('    %s\n' % inner.args[2])
        s.write('\n')
        s.write('Please change the file to not reference undefined ')
        s.write('variables and try again.\n')

    def _print_valueerror(self, inner, s):
        if not inner.args or inner.args[0] not in ('global_ns', 'local_ns'):
            self._print_exception(inner, s)
            return

        assert inner.args[1] == 'set_type'

        s.write('The underlying problem is an attempt to write an illegal ')
        s.write('value to a special variable.\n')
        s.write('\n')
        s.write('The variable whose value was rejected is:\n')
        s.write('\n')
        s.write('    %s' % inner.args[2])
        s.write('\n')
        s.write('The value being written to it was of the following type:\n')
        s.write('\n')
        s.write('    %s\n' % type(inner.args[3]).__name__)
        s.write('\n')
        s.write('This variable expects the following type(s):\n')
        s.write('\n')
        if type(inner.args[4]) == type_type:
            s.write('    %s\n' % inner.args[4].__name__)
        else:
            for t in inner.args[4]:
                s.write( '    %s\n' % t.__name__)
        s.write('\n')
        s.write('Change the file to write a value of the appropriate type ')
        s.write('and try again.\n')

    def _print_exception(self, e, s):
        s.write('An error was encountered as part of executing the file ')
        s.write('itself. The error appears to be the fault of the script.\n')
        s.write('\n')
        s.write('The error as reported by Python is:\n')
        s.write('\n')
        s.write('    %s\n' % traceback.format_exception_only(type(e), e))


class BuildReader(object):
    """Read a tree of mozbuild files into data structures.

    This is where the build system starts. You give it a tree configuration
    (the output of configuration) and it executes the moz.build files and
    collects the data they define.

    The reader can optionally call a callable after each sandbox is evaluated
    but before its evaluated content is processed. This gives callers the
    opportunity to modify contexts before side-effects occur from their
    content. This callback receives the ``Context`` containing the result of
    each sandbox evaluation. Its return value is ignored.
    """

    def __init__(self, config, finder=default_finder):
        self.config = config

        self._log = logging.getLogger(__name__)
        self._read_files = set()
        self._execution_stack = []
        self._finder = finder

        max_workers = cpu_count()
        self._gyp_worker_pool = ProcessPoolExecutor(max_workers=max_workers)
        self._gyp_processors = []
        self._execution_time = 0.0
        self._file_count = 0
        self._gyp_execution_time = 0.0
        self._gyp_file_count = 0

    def summary(self):
        return ExecutionSummary(
            'Finished reading {file_count:d} moz.build files in '
            '{execution_time:.2f}s',
            file_count=self._file_count,
            execution_time=self._execution_time)

    def gyp_summary(self):
        return ExecutionSummary(
            'Read {file_count:d} gyp files in parallel contributing '
            '{execution_time:.2f}s to total wall time',
            file_count=self._gyp_file_count,
            execution_time=self._gyp_execution_time)

    def read_topsrcdir(self):
        """Read the tree of linked moz.build files.

        This starts with the tree's top-most moz.build file and descends into
        all linked moz.build files until all relevant files have been evaluated.

        This is a generator of Context instances. As each moz.build file is
        read, a new Context is created and emitted.
        """
        path = mozpath.join(self.config.topsrcdir, 'moz.build')
        for r in self.read_mozbuild(path, self.config):
            yield r
        all_gyp_paths = set()
        for g in self._gyp_processors:
            for gyp_context in g.results:
                all_gyp_paths |= gyp_context.all_paths
                yield gyp_context
            self._gyp_execution_time += g.execution_time
        self._gyp_file_count += len(all_gyp_paths)
        self._gyp_worker_pool.shutdown()

    def all_mozbuild_paths(self):
        """Iterator over all available moz.build files.

        This method has little to do with the reader. It should arguably belong
        elsewhere.
        """
        # In the future, we may traverse moz.build files by looking
        # for DIRS references in the AST, even if a directory is added behind
        # a conditional. For now, just walk the filesystem.
        ignore = {
            # Ignore fake moz.build files used for testing moz.build.
            'python/mozbuild/mozbuild/test',

            # Ignore object directories.
            'obj*',
        }

        finder = FileFinder(self.config.topsrcdir, find_executables=False,
            ignore=ignore)

        # The root doesn't get picked up by FileFinder.
        yield 'moz.build'

        for path, f in finder.find('**/moz.build'):
            yield path

    def find_sphinx_variables(self):
        """This function finds all assignments of Sphinx documentation variables.

        This is a generator of tuples of (moz.build path, var, key, value). For
        variables that assign to keys in objects, key will be defined.

        With a little work, this function could be made more generic. But if we
        end up writing a lot of ast code, it might be best to import a
        high-level AST manipulation library into the tree.
        """
        # This function looks for assignments to SPHINX_TREES and
        # SPHINX_PYTHON_PACKAGE_DIRS variables.
        #
        # SPHINX_TREES is a dict. Keys and values should both be strings. The
        # target of the assignment should be a Subscript node. The value
        # assigned should be a Str node. e.g.
        #
        #  SPHINX_TREES['foo'] = 'bar'
        #
        # This is an Assign node with a Subscript target. The Subscript's value
        # is a Name node with id "SPHINX_TREES." The slice of this target
        # is an Index node and its value is a Str with value "foo."
        #
        # SPHINX_PYTHON_PACKAGE_DIRS is a simple list. The target of the
        # assignment should be a Name node. Values should be a List node, whose
        # elements are Str nodes. e.g.
        #
        #  SPHINX_PYTHON_PACKAGE_DIRS += ['foo']
        #
        # This is an AugAssign node with a Name target with id
        # "SPHINX_PYTHON_PACKAGE_DIRS." The value is a List node containing 1
        # Str elt whose value is "foo."
        relevant = [
            'SPHINX_TREES',
            'SPHINX_PYTHON_PACKAGE_DIRS',
        ]

        def assigned_variable(node):
            # This is not correct, but we don't care yet.
            if hasattr(node, 'targets'):
                # Nothing in moz.build does multi-assignment (yet). So error if
                # we see it.
                assert len(node.targets) == 1

                target = node.targets[0]
            else:
                target = node.target

            if isinstance(target, ast.Subscript):
                if not isinstance(target.value, ast.Name):
                    return None, None
                name = target.value.id
            elif isinstance(target, ast.Name):
                name = target.id
            else:
                return None, None

            if name not in relevant:
                return None, None

            key = None
            if isinstance(target, ast.Subscript):
                assert isinstance(target.slice, ast.Index)
                assert isinstance(target.slice.value, ast.Str)
                key = target.slice.value.s

            return name, key

        def assigned_values(node):
            value = node.value
            if isinstance(value, ast.List):
                for v in value.elts:
                    assert isinstance(v, ast.Str)
                    yield v.s
            else:
                assert isinstance(value, ast.Str)
                yield value.s

        assignments = []

        class Visitor(ast.NodeVisitor):
            def helper(self, node):
                name, key = assigned_variable(node)
                if not name:
                    return

                for v in assigned_values(node):
                    assignments.append((name, key, v))

            def visit_Assign(self, node):
                self.helper(node)

            def visit_AugAssign(self, node):
                self.helper(node)

        for p in self.all_mozbuild_paths():
            assignments[:] = []
            full = os.path.join(self.config.topsrcdir, p)

            with open(full, 'rb') as fh:
                source = fh.read()

            tree = ast.parse(source, full)
            Visitor().visit(tree)

            for name, key, value in assignments:
                yield p, name, key, value

    def read_mozbuild(self, path, config, descend=True, metadata={}):
        """Read and process a mozbuild file, descending into children.

        This starts with a single mozbuild file, executes it, and descends into
        other referenced files per our traversal logic.

        The traversal logic is to iterate over the *DIRS variables, treating
        each element as a relative directory path. For each encountered
        directory, we will open the moz.build file located in that
        directory in a new Sandbox and process it.

        If descend is True (the default), we will descend into child
        directories and files per variable values.

        Arbitrary metadata in the form of a dict can be passed into this
        function. This feature is intended to facilitate the build reader
        injecting state and annotations into moz.build files that is
        independent of the sandbox's execution context.

        Traversal is performed depth first (for no particular reason).
        """
        self._execution_stack.append(path)
        try:
            for s in self._read_mozbuild(path, config, descend=descend,
                                         metadata=metadata):
                yield s

        except BuildReaderError as bre:
            raise bre

        except SandboxCalledError as sce:
            raise BuildReaderError(list(self._execution_stack),
                sys.exc_info()[2], sandbox_called_error=sce)

        except SandboxExecutionError as se:
            raise BuildReaderError(list(self._execution_stack),
                sys.exc_info()[2], sandbox_exec_error=se)

        except SandboxLoadError as sle:
            raise BuildReaderError(list(self._execution_stack),
                sys.exc_info()[2], sandbox_load_error=sle)

        except SandboxValidationError as ve:
            raise BuildReaderError(list(self._execution_stack),
                sys.exc_info()[2], validation_error=ve)

        except Exception as e:
            raise BuildReaderError(list(self._execution_stack),
                sys.exc_info()[2], other_error=e)

    def _read_mozbuild(self, path, config, descend, metadata):
        path = mozpath.normpath(path)
        log(self._log, logging.DEBUG, 'read_mozbuild', {'path': path},
            'Reading file: {path}')

        if path in self._read_files:
            log(self._log, logging.WARNING, 'read_already', {'path': path},
                'File already read. Skipping: {path}')
            return

        self._read_files.add(path)

        time_start = time.time()

        topobjdir = config.topobjdir

        if not mozpath.basedir(path, [config.topsrcdir]):
            external = config.external_source_dir
            if external and mozpath.basedir(path, [external]):
                config = ConfigEnvironment.from_config_status(
                    mozpath.join(topobjdir, 'config.status'))
                config.topsrcdir = external
                config.external_source_dir = None

        relpath = mozpath.relpath(path, config.topsrcdir)
        reldir = mozpath.dirname(relpath)

        if mozpath.dirname(relpath) == 'js/src' and \
                not config.substs.get('JS_STANDALONE'):
            config = ConfigEnvironment.from_config_status(
                mozpath.join(topobjdir, reldir, 'config.status'))
            config.topobjdir = topobjdir
            config.external_source_dir = None

        context = Context(VARIABLES, config, self._finder)
        sandbox = MozbuildSandbox(context, metadata=metadata,
                                  finder=self._finder)
        sandbox.exec_file(path)
        self._execution_time += time.time() - time_start
        self._file_count += len(context.all_paths)

        # Yield main context before doing any processing. This gives immediate
        # consumers an opportunity to change state before our remaining
        # processing is performed.
        yield context

        # We need the list of directories pre-gyp processing for later.
        dirs = list(context.get('DIRS', []))

        curdir = mozpath.dirname(path)

        for target_dir in context.get('GYP_DIRS', []):
            gyp_dir = context['GYP_DIRS'][target_dir]
            for v in ('input', 'variables'):
                if not getattr(gyp_dir, v):
                    raise SandboxValidationError('Missing value for '
                        'GYP_DIRS["%s"].%s' % (target_dir, v), context)

            # The make backend assumes contexts for sub-directories are
            # emitted after their parent, so accumulate the gyp contexts.
            # We could emit the parent context before processing gyp
            # configuration, but we need to add the gyp objdirs to that context
            # first.
            from .gyp_reader import GypProcessor
            non_unified_sources = set()
            for s in gyp_dir.non_unified_sources:
                source = SourcePath(context, s)
                if not self._finder.get(source.full_path):
                    raise SandboxValidationError('Cannot find %s.' % source,
                        context)
                non_unified_sources.add(source)
            action_overrides = {}
            for action, script in gyp_dir.action_overrides.iteritems():
                action_overrides[action] = SourcePath(context, script)

            gyp_processor = GypProcessor(context.config,
                                         gyp_dir,
                                         mozpath.join(curdir, gyp_dir.input),
                                         mozpath.join(context.objdir,
                                                      target_dir),
                                         self._gyp_worker_pool,
                                         action_overrides,
                                         non_unified_sources)
            self._gyp_processors.append(gyp_processor)

        for subcontext in sandbox.subcontexts:
            yield subcontext

        # Traverse into referenced files.

        # It's very tempting to use a set here. Unfortunately, the recursive
        # make backend needs order preserved. Once we autogenerate all backend
        # files, we should be able to convert this to a set.
        recurse_info = OrderedDict()
        for d in dirs:
            if d in recurse_info:
                raise SandboxValidationError(
                    'Directory (%s) registered multiple times' % (
                        mozpath.relpath(d.full_path, context.srcdir)),
                    context)

            recurse_info[d] = {}
            for key in sandbox.metadata:
                if key == 'exports':
                    sandbox.recompute_exports()

                recurse_info[d][key] = dict(sandbox.metadata[key])

        for path, child_metadata in recurse_info.items():
            child_path = path.join('moz.build').full_path

            # Ensure we don't break out of the topsrcdir. We don't do realpath
            # because it isn't necessary. If there are symlinks in the srcdir,
            # that's not our problem. We're not a hosted application: we don't
            # need to worry about security too much.
            if not is_read_allowed(child_path, context.config):
                raise SandboxValidationError(
                    'Attempting to process file outside of allowed paths: %s' %
                        child_path, context)

            if not descend:
                continue

            for res in self.read_mozbuild(child_path, context.config,
                                          metadata=child_metadata):
                yield res

        self._execution_stack.pop()

    def _find_relevant_mozbuilds(self, paths):
        """Given a set of filesystem paths, find all relevant moz.build files.

        We assume that a moz.build file in the directory ancestry of a given path
        is relevant to that path. Let's say we have the following files on disk::

           moz.build
           foo/moz.build
           foo/baz/moz.build
           foo/baz/file1
           other/moz.build
           other/file2

        If ``foo/baz/file1`` is passed in, the relevant moz.build files are
        ``moz.build``, ``foo/moz.build``, and ``foo/baz/moz.build``. For
        ``other/file2``, the relevant moz.build files are ``moz.build`` and
        ``other/moz.build``.

        Returns a dict of input paths to a list of relevant moz.build files.
        The root moz.build file is first and the leaf-most moz.build is last.
        """
        root = self.config.topsrcdir
        result = {}

        @memoize
        def exists(path):
            return self._finder.get(path) is not None

        def itermozbuild(path):
            subpath = ''
            yield 'moz.build'
            for part in mozpath.split(path):
                subpath = mozpath.join(subpath, part)
                yield mozpath.join(subpath, 'moz.build')

        for path in sorted(paths):
            path = mozpath.normpath(path)
            if os.path.isabs(path):
                if not mozpath.basedir(path, [root]):
                    raise Exception('Path outside topsrcdir: %s' % path)
                path = mozpath.relpath(path, root)

            result[path] = [p for p in itermozbuild(path)
                              if exists(mozpath.join(root, p))]

        return result

    def read_relevant_mozbuilds(self, paths):
        """Read and process moz.build files relevant for a set of paths.

        For an iterable of relative-to-root filesystem paths ``paths``,
        find all moz.build files that may apply to them based on filesystem
        hierarchy and read those moz.build files.

        The return value is a 2-tuple. The first item is a dict mapping each
        input filesystem path to a list of Context instances that are relevant
        to that path. The second item is a list of all Context instances. Each
        Context instance is in both data structures.
        """
        relevants = self._find_relevant_mozbuilds(paths)

        topsrcdir = self.config.topsrcdir

        # Source moz.build file to directories to traverse.
        dirs = defaultdict(set)
        # Relevant path to absolute paths of relevant contexts.
        path_mozbuilds = {}

        # There is room to improve this code (and the code in
        # _find_relevant_mozbuilds) to better handle multiple files in the same
        # directory. Bug 1136966 tracks.
        for path, mbpaths in relevants.items():
            path_mozbuilds[path] = [mozpath.join(topsrcdir, p) for p in mbpaths]

            for i, mbpath in enumerate(mbpaths[0:-1]):
                source_dir = mozpath.dirname(mbpath)
                target_dir = mozpath.dirname(mbpaths[i + 1])

                d = mozpath.normpath(mozpath.join(topsrcdir, mbpath))
                dirs[d].add(mozpath.relpath(target_dir, source_dir))

        # Exporting doesn't work reliably in tree traversal mode. Override
        # the function to no-op.
        functions = dict(FUNCTIONS)
        def export(sandbox):
            return lambda varname: None
        functions['export'] = tuple([export] + list(FUNCTIONS['export'][1:]))

        metadata = {
            'functions': functions,
        }

        contexts = defaultdict(list)
        all_contexts = []
        for context in self.read_mozbuild(mozpath.join(topsrcdir, 'moz.build'),
                                          self.config, metadata=metadata):
            # Explicitly set directory traversal variables to override default
            # traversal rules.
            if not isinstance(context, SubContext):
                for v in ('DIRS', 'GYP_DIRS'):
                    context[v][:] = []

                context['DIRS'] = sorted(dirs[context.main_path])

            contexts[context.main_path].append(context)
            all_contexts.append(context)

        result = {}
        for path, paths in path_mozbuilds.items():
            result[path] = reduce(lambda x, y: x + y, (contexts[p] for p in paths), [])

        return result, all_contexts

    def files_info(self, paths):
        """Obtain aggregate data from Files for a set of files.

        Given a set of input paths, determine which moz.build files may
        define metadata for them, evaluate those moz.build files, and
        apply file metadata rules defined within to determine metadata
        values for each file requested.

        Essentially, for each input path:

        1. Determine the set of moz.build files relevant to that file by
           looking for moz.build files in ancestor directories.
        2. Evaluate moz.build files starting with the most distant.
        3. Iterate over Files sub-contexts.
        4. If the file pattern matches the file we're seeking info on,
           apply attribute updates.
        5. Return the most recent value of attributes.
        """
        paths, _ = self.read_relevant_mozbuilds(paths)

        r = {}

        for path, ctxs in paths.items():
            flags = Files(Context())

            for ctx in ctxs:
                if not isinstance(ctx, Files):
                    continue

                relpath = mozpath.relpath(path, ctx.relsrcdir)
                pattern = ctx.pattern

                # Only do wildcard matching if the '*' character is present.
                # Otherwise, mozpath.match will match directories, which we've
                # arbitrarily chosen to not allow.
                if pattern == relpath or \
                        ('*' in pattern and mozpath.match(relpath, pattern)):
                    flags += ctx

            if not any([flags.test_tags, flags.test_files, flags.test_flavors]):
                flags += self.test_defaults_for_path(ctxs)

            r[path] = flags

        return r

    def test_defaults_for_path(self, ctxs):
        # This names the context keys that will end up emitting a test
        # manifest.
        test_manifest_contexts = set(
            ['%s_MANIFESTS' % key for key in TEST_MANIFESTS] +
            ['%s_MANIFESTS' % flavor.upper() for flavor in REFTEST_FLAVORS] +
            ['%s_MANIFESTS' % flavor.upper().replace('-', '_') for flavor in WEB_PLATFORM_TESTS_FLAVORS]
        )

        result_context = Files(Context())
        for ctx in ctxs:
            for key in ctx:
                if key not in test_manifest_contexts:
                    continue
                for paths, obj in ctx[key]:
                    if isinstance(paths, tuple):
                        path, tests_root = paths
                        tests_root = mozpath.join(ctx.relsrcdir, tests_root)
                        for t in (mozpath.join(tests_root, path) for path, _ in obj):
                            result_context.test_files.add(mozpath.dirname(t) + '/**')
                    else:
                        for t in obj.tests:
                            if isinstance(t, tuple):
                                path, _ = t
                                relpath = mozpath.relpath(path,
                                                          self.config.topsrcdir)
                            else:
                                relpath = t['relpath']
                            result_context.test_files.add(mozpath.dirname(relpath) + '/**')
        return result_context
