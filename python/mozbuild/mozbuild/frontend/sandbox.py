# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

r"""Python sandbox implementation for build files.

This module contains classes for Python sandboxes that execute in a
highly-controlled environment.

The main class is `Sandbox`. This provides an execution environment for Python
code and is used to fill a Context instance for the takeaway information from
the execution.

Code in this module takes a different approach to exception handling compared
to what you'd see elsewhere in Python. Arguments to built-in exceptions like
KeyError are machine parseable. This machine-friendly data is used to present
user-friendly error messages in the case of errors.
"""

from __future__ import unicode_literals

import copy
import os
import sys
import weakref

from contextlib import contextmanager

from mozbuild.util import ReadOnlyDict
from context import Context


def alphabetical_sorted(iterable, cmp=None, key=lambda x: x.lower(),
                        reverse=False):
    """sorted() replacement for the sandbox, ordering alphabetically by
    default.
    """
    return sorted(iterable, cmp, key, reverse)


class SandboxError(Exception):
    def __init__(self, file_stack):
        self.file_stack = file_stack


class SandboxExecutionError(SandboxError):
    """Represents errors encountered during execution of a Sandbox.

    This is a simple container exception. It's purpose is to capture state
    so something else can report on it.
    """
    def __init__(self, file_stack, exc_type, exc_value, trace):
        SandboxError.__init__(self, file_stack)

        self.exc_type = exc_type
        self.exc_value = exc_value
        self.trace = trace


class SandboxLoadError(SandboxError):
    """Represents errors encountered when loading a file for execution.

    This exception represents errors in a Sandbox that occurred as part of
    loading a file. The error could have occurred in the course of executing
    a file. If so, the file_stack will be non-empty and the file that caused
    the load will be on top of the stack.
    """
    def __init__(self, file_stack, trace, illegal_path=None, read_error=None):
        SandboxError.__init__(self, file_stack)

        self.trace = trace
        self.illegal_path = illegal_path
        self.read_error = read_error


class Sandbox(dict):
    """Represents a sandbox for executing Python code.

    This class provides a sandbox for execution of a single mozbuild frontend
    file. The results of that execution is stored in the Context instance given
    as the ``context`` argument.

    Sandbox is effectively a glorified wrapper around compile() + exec(). You
    point it at some Python code and it executes it. The main difference from
    executing Python code like normal is that the executed code is very limited
    in what it can do: the sandbox only exposes a very limited set of Python
    functionality. Only specific types and functions are available. This
    prevents executed code from doing things like import modules, open files,
    etc.

    Sandbox instances act as global namespace for the sandboxed execution
    itself. They shall not be used to access the results of the execution.
    Those results are available in the given Context instance after execution.

    The Sandbox itself is responsible for enforcing rules such as forbidding
    reassignment of variables.

    Implementation note: Sandbox derives from dict because exec() insists that
    what it is given for namespaces is a dict.
    """
    # The default set of builtins.
    BUILTINS = ReadOnlyDict({
        # Only real Python built-ins should go here.
        'None': None,
        'False': False,
        'True': True,
        'sorted': alphabetical_sorted,
        'int': int,
    })

    def __init__(self, context, builtins=None):
        """Initialize a Sandbox ready for execution.
        """
        self._builtins = builtins or self.BUILTINS
        dict.__setitem__(self, '__builtins__', self._builtins)

        assert isinstance(self._builtins, ReadOnlyDict)
        assert isinstance(context, Context)

        # Contexts are modeled as a stack because multiple context managers
        # may be active.
        self._active_contexts = [context]

        # Seen sub-contexts. Will be populated with other Context instances
        # that were related to execution of this instance.
        self.subcontexts = []

        # We need to record this because it gets swallowed as part of
        # evaluation.
        self._last_name_error = None

    @property
    def _context(self):
        return self._active_contexts[-1]

    def exec_file(self, path):
        """Execute code at a path in the sandbox.

        The path must be absolute.
        """
        assert os.path.isabs(path)

        source = None

        try:
            with open(path, 'rt') as fd:
                source = fd.read()
        except Exception as e:
            raise SandboxLoadError(self._context.source_stack,
                sys.exc_info()[2], read_error=path)

        self.exec_source(source, path)

    def exec_source(self, source, path='', becomes_current_path=True):
        """Execute Python code within a string.

        The passed string should contain Python code to be executed. The string
        will be compiled and executed.

        You should almost always go through exec_file() because exec_source()
        does not perform extra path normalization. This can cause relative
        paths to behave weirdly.
        """
        if path and becomes_current_path:
            self._context.push_source(path)

        old_sandbox = self._context._sandbox
        self._context._sandbox = weakref.ref(self)

        # We don't have to worry about bytecode generation here because we are
        # too low-level for that. However, we could add bytecode generation via
        # the marshall module if parsing performance were ever an issue.

        try:
            # compile() inherits the __future__ from the module by default. We
            # do want Unicode literals.
            code = compile(source, path, 'exec')
            # We use ourself as the global namespace for the execution. There
            # is no need for a separate local namespace as moz.build execution
            # is flat, namespace-wise.
            exec(code, self)
        except SandboxError as e:
            raise e
        except NameError as e:
            # A NameError is raised when a variable could not be found.
            # The original KeyError has been dropped by the interpreter.
            # However, we should have it cached in our instance!

            # Unless a script is doing something wonky like catching NameError
            # itself (that would be silly), if there is an exception on the
            # global namespace, that's our error.
            actual = e

            if self._last_name_error is not None:
                actual = self._last_name_error
            source_stack = self._context.source_stack
            if not becomes_current_path:
                # Add current file to the stack because it wasn't added before
                # sandbox execution.
                source_stack.append(path)
            raise SandboxExecutionError(source_stack, type(actual), actual,
                                        sys.exc_info()[2])

        except Exception as e:
            # Need to copy the stack otherwise we get a reference and that is
            # mutated during the finally.
            exc = sys.exc_info()
            source_stack = self._context.source_stack
            if not becomes_current_path:
                # Add current file to the stack because it wasn't added before
                # sandbox execution.
                source_stack.append(path)
            raise SandboxExecutionError(source_stack, exc[0], exc[1], exc[2])
        finally:
            self._context._sandbox = old_sandbox
            if path:
                self._context.pop_source()

    def push_subcontext(self, context):
        """Push a SubContext onto the execution stack.

        When called, the active context will be set to the specified context,
        meaning all variable accesses will go through it. We also record this
        SubContext as having been executed as part of this sandbox.
        """
        self._active_contexts.append(context)
        if context not in self.subcontexts:
            self.subcontexts.append(context)

    def pop_subcontext(self, context):
        """Pop a SubContext off the execution stack.

        SubContexts must be pushed and popped in opposite order. This is
        validated as part of the function call to ensure proper consumer API
        use.
        """
        popped = self._active_contexts.pop()
        assert popped == context

    def __getitem__(self, key):
        if key.isupper():
            try:
                return self._context[key]
            except Exception as e:
                self._last_name_error = e
                raise

        return dict.__getitem__(self, key)

    def __setitem__(self, key, value):
        if key in self._builtins or key == '__builtins__':
            raise KeyError('Cannot reassign builtins')

        if key.isupper():
            # Forbid assigning over a previously set value. Interestingly, when
            # doing FOO += ['bar'], python actually does something like:
            #   foo = namespace.__getitem__('FOO')
            #   foo.__iadd__(['bar'])
            #   namespace.__setitem__('FOO', foo)
            # This means __setitem__ is called with the value that is already
            # in the dict, when doing +=, which is permitted.
            if key in self._context and self._context[key] is not value:
                raise KeyError('global_ns', 'reassign', key)

            self._context[key] = value
        else:
            dict.__setitem__(self, key, value)

    def get(self, key, default=None):
        raise NotImplementedError('Not supported')

    def __len__(self):
        raise NotImplementedError('Not supported')

    def __iter__(self):
        raise NotImplementedError('Not supported')

    def __contains__(self, key):
        raise NotImplementedError('Not supported')
