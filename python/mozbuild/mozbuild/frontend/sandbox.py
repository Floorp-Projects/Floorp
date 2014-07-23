# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

r"""Python sandbox implementation for build files.

This module contains classes for Python sandboxes that execute in a
highly-controlled environment.

The main class is `Sandbox`. This provides an execution environment for Python
code.

The behavior inside sandboxes is mostly regulated by the `GlobalNamespace` and
`LocalNamespace` classes. These represent the global and local namespaces in
the sandbox, respectively.

Code in this module takes a different approach to exception handling compared
to what you'd see elsewhere in Python. Arguments to built-in exceptions like
KeyError are machine parseable. This machine-friendly data is used to present
user-friendly error messages in the case of errors.
"""

from __future__ import unicode_literals

import copy
import os
import sys

from contextlib import contextmanager

from mozbuild.util import ReadOnlyDict


class SandboxDerivedValue(object):
    """Classes deriving from this one receive a special treatment in a
    sandbox GlobalNamespace. See GlobalNamespace documentation.
    """


def alphabetical_sorted(iterable, cmp=None, key=lambda x: x.lower(),
                        reverse=False):
    """sorted() replacement for the sandbox, ordering alphabetically by
    default.
    """
    return sorted(iterable, cmp, key, reverse)


class GlobalNamespace(dict):
    """Represents the globals namespace in a sandbox.

    This is a highly specialized dictionary employing light magic.

    At the crux we have the concept of a restricted keys set. Only very
    specific keys may be retrieved or mutated. The rules are as follows:

        - The '__builtins__' key is hardcoded and is read-only.
        - The set of variables that can be assigned or accessed during
          execution is passed into the constructor.

    When variables are assigned to, we verify assignment is allowed. Assignment
    is allowed if the variable is known (set defined at constructor time) and
    if the value being assigned is the expected type (also defined at
    constructor time).

    When variables are read, we first try to read the existing value. If a
    value is not found and it is defined in the allowed variables set, we
    return a new instance of the class for that variable. We don't assign
    default instances until they are accessed because this makes debugging
    the end-result much simpler. Instead of a data structure with lots of
    empty/default values, you have a data structure with only the values
    that were read or touched.

    Instances of variables classes are created by invoking class_name(),
    except when class_name derives from SandboxDerivedValue, in which
    case class_name(instance_of_the_global_namespace) is invoked.
    A value is added to those calls when instances are created during
    assignment (setitem).

    Instantiators of this class are given a backdoor to perform setting of
    arbitrary values. e.g.

        ns = GlobalNamespace()
        with ns.allow_all_writes():
            ns['foo'] = True

        ns['bar'] = True  # KeyError raised.
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

    def __init__(self, allowed_variables=None, builtins=None):
        """Create a new global namespace having specific variables.

        allowed_variables is a dict of the variables that can be queried and
        mutated. Keys in this dict are the strings representing keys in this
        namespace which are valid. Values are tuples of stored type, assigned
        type, default value, and a docstring describing the purpose of the variable.

        builtins is the value to use for the special __builtins__ key. If not
        defined, the BUILTINS constant attached to this class is used. The
        __builtins__ object is read-only.
        """
        builtins = builtins or self.BUILTINS

        assert isinstance(builtins, ReadOnlyDict)

        dict.__init__(self, {'__builtins__': builtins})

        self._allowed_variables = allowed_variables or {}

        # We need to record this because it gets swallowed as part of
        # evaluation.
        self.last_name_error = None

        self._allow_all_writes = False

        self._allow_one_mutation = set()

    def __getitem__(self, name):
        try:
            return dict.__getitem__(self, name)
        except KeyError:
            pass

        # The variable isn't present yet. Fall back to VARIABLES.
        default = self._allowed_variables.get(name, None)
        if default is None:
            self.last_name_error = KeyError('global_ns', 'get_unknown', name)
            raise self.last_name_error

        # If the default is specifically a lambda (or, rather, any function--but
        # not a class that can be called), then it is actually a rule to
        # generate the default that should be used.
        default = default[0]
        if issubclass(default, SandboxDerivedValue):
            value = default(self)
        else:
            value = default()

        dict.__setitem__(self, name, value)
        return dict.__getitem__(self, name)

    def __setitem__(self, name, value):
        if self._allow_all_writes:
            dict.__setitem__(self, name, value)
            self._allow_one_mutation.add(name)
            return

        # Forbid assigning over a previously set value. Interestingly, when
        # doing FOO += ['bar'], python actually does something like:
        #   foo = namespace.__getitem__('FOO')
        #   foo.__iadd__(['bar'])
        #   namespace.__setitem__('FOO', foo)
        # This means __setitem__ is called with the value that is already
        # in the dict, when doing +=, which is permitted.
        if name in self._allow_one_mutation:
            self._allow_one_mutation.remove(name)
        elif name in self and dict.__getitem__(self, name) is not value:
            raise Exception('Reassigning %s is forbidden' % name)

        # We don't need to check for name.isupper() here because LocalNamespace
        # only sends variables our way if isupper() is True.
        stored_type, input_type, docs, tier = \
            self._allowed_variables.get(name, (None, None, None, None))

        # Variable is unknown.
        if stored_type is None:
            self.last_name_error = KeyError('global_ns', 'set_unknown', name,
                value)
            raise self.last_name_error

        # If the incoming value is not the type we store, we try to convert
        # it to that type. This relies on proper coercion rules existing. This
        # is the responsibility of whoever defined the symbols: a type should
        # not be in the allowed set if the constructor function for the stored
        # type does not accept an instance of that type.
        if not isinstance(value, stored_type):
            if not isinstance(value, input_type):
                self.last_name_error = ValueError('global_ns', 'set_type', name,
                    value, input_type)
                raise self.last_name_error

            if issubclass(stored_type, SandboxDerivedValue):
                value = stored_type(self, value)
            else:
                value = stored_type(value)

        dict.__setitem__(self, name, value)

    @contextmanager
    def allow_all_writes(self):
        """Allow any variable to be written to this instance.

        This is used as a context manager. When activated, all writes
        (__setitem__ calls) are allowed. When the context manager is exited,
        the instance goes back to its default behavior of only allowing
        whitelisted mutations.
        """
        self._allow_all_writes = True
        yield self
        self._allow_all_writes = False

    # dict.update doesn't call our __setitem__, so we have to override it.
    def update(self, other):
        for name, value in other.items():
            self.__setitem__(name, value)


class LocalNamespace(dict):
    """Represents the locals namespace in a Sandbox.

    This behaves like a dict except with some additional behavior tailored
    to our sandbox execution model.

    Under normal rules of exec(), doing things like += could have interesting
    consequences. Keep in mind that a += is really a read, followed by the
    creation of a new variable, followed by a write. If the read came from the
    global namespace, then the write would go to the local namespace, resulting
    in fragmentation. This is not desired.

    LocalNamespace proxies reads and writes for global-looking variables
    (read: UPPERCASE) to the global namespace. This means that attempting to
    read or write an unknown variable results in exceptions raised from the
    GlobalNamespace.
    """
    def __init__(self, global_ns):
        """Create a local namespace associated with a GlobalNamespace."""
        dict.__init__({})

        self._globals = global_ns
        self.last_name_error = None

    def __getitem__(self, name):
        if name.isupper():
            return self._globals[name]

        return dict.__getitem__(self, name)

    def __setitem__(self, name, value):
        if name.isupper():
            self._globals[name] = value
            return

        dict.__setitem__(self, name, value)


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


class Sandbox(object):
    """Represents a sandbox for executing Python code.

    This class both provides a sandbox for execution of a single mozbuild
    frontend file as well as an interface to the results of that execution.

    Sandbox is effectively a glorified wrapper around compile() + exec(). You
    point it at some Python code and it executes it. The main difference from
    executing Python code like normal is that the executed code is very limited
    in what it can do: the sandbox only exposes a very limited set of Python
    functionality. Only specific types and functions are available. This
    prevents executed code from doing things like import modules, open files,
    etc.

    Sandboxes are bound to a mozconfig instance. These objects are produced by
    the output of configure.

    Sandbox instances can be accessed like dictionaries to facilitate result
    retrieval. e.g. foo = sandbox['FOO']. Direct assignment is not allowed.

    Each sandbox has associated with it a GlobalNamespace and LocalNamespace.
    Only data stored in the GlobalNamespace is retrievable via the dict
    interface. This is because the local namespace should be irrelevant: it
    should only contain throwaway variables.
    """
    def __init__(self, allowed_variables=None, builtins=None):
        """Initialize a Sandbox ready for execution.

        The arguments are proxied to GlobalNamespace.__init__.
        """
        self._globals = GlobalNamespace(allowed_variables=allowed_variables,
            builtins=builtins)
        self._allowed_variables = allowed_variables
        self._locals = LocalNamespace(self._globals)
        self._execution_stack = []
        self.main_path = None
        self.all_paths = set()

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
            raise SandboxLoadError(list(self._execution_stack),
                sys.exc_info()[2], read_error=path)

        self.exec_source(source, path)

    def exec_source(self, source, path):
        """Execute Python code within a string.

        The passed string should contain Python code to be executed. The string
        will be compiled and executed.

        You should almost always go through exec_file() because exec_source()
        does not perform extra path normalization. This can cause relative
        paths to behave weirdly.
        """
        self._execution_stack.append(path)

        if self.main_path is None:
            self.main_path = path

        self.all_paths.add(path)

        # We don't have to worry about bytecode generation here because we are
        # too low-level for that. However, we could add bytecode generation via
        # the marshall module if parsing performance were ever an issue.

        try:
            # compile() inherits the __future__ from the module by default. We
            # do want Unicode literals.
            code = compile(source, path, 'exec')
            exec(code, self._globals, self._locals)
        except SandboxError as e:
            raise e
        except NameError as e:
            # A NameError is raised when a local or global could not be found.
            # The original KeyError has been dropped by the interpreter.
            # However, we should have it cached in our namespace instances!

            # Unless a script is doing something wonky like catching NameError
            # itself (that would be silly), if there is an exception on the
            # global namespace, that's our error.
            actual = e

            if self._globals.last_name_error is not None:
                actual = self._globals.last_name_error
            elif self._locals.last_name_error is not None:
                actual = self._locals.last_name_error

            raise SandboxExecutionError(list(self._execution_stack),
                type(actual), actual, sys.exc_info()[2])

        except Exception as e:
            # Need to copy the stack otherwise we get a reference and that is
            # mutated during the finally.
            exc = sys.exc_info()
            raise SandboxExecutionError(list(self._execution_stack), exc[0],
                exc[1], exc[2])
        finally:
            self._execution_stack.pop()

    # Dict interface proxies reads to global namespace.
    def __len__(self):
        return len(self._globals)

    def __getitem__(self, name):
        return self._globals[name]

    def __iter__(self):
        return iter(self._globals)

    def iterkeys(self):
        return self.__iter__()

    def __contains__(self, key):
        return key in self._globals

    def get(self, key, default=None):
        return self._globals.get(key, default)

    def get_affected_tiers(self):
        tiers = (self._allowed_variables[key][3] for key in self
                 if key in self._allowed_variables)
        return set(tier for tier in tiers if tier)
