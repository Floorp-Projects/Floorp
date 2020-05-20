2.2.0 (2020-04-08)

- Include column information in error messages
- Fix ``@overload`` detection with other decorators and in non-global scopes
- Fix return-type annotation being a class member
- Fix assignment to ``_`` in doctests with existing ``_`` name
- Namespace attributes which are attached to ast nodes with ``_pyflakes_`` to
  avoid conflicts with other libraries (notably bandit)
- Add check for f-strings without placeholders
- Add check for unused/extra/invalid ``'string literal'.format(...)``
- Add check for unused/extra/invalid ``'string literal % ...``
- Improve python shebang detection
- Allow type ignore to be followed by a code ``# type: ignore[attr-defined]``
- Add support for assignment expressions (PEP 572)
- Support ``@overload`` detection from ``typing_extensions`` as well
- Fix ``@overload`` detection for async functions
- Allow ``continue`` inside ``finally`` in python 3.8+
- Fix handling of annotations in positional-only arguments
- Make pyflakes more resistant to future syntax additions
- Fix false positives in partially quoted type annotations
- Warn about ``is`` comparison to tuples
- Fix ``Checker`` usage with async function subtrees
- Add check for ``if`` of non-empty tuple
- Switch from ``optparse`` to ``argparse``
- Fix false positives in partially quoted type annotations in unusual contexts
- Be more cautious when identifying ``Literal`` type expressions

2.1.1 (2019-02-28)

- Fix reported line number for type comment errors
- Fix typing.overload check to only check imported names

2.1.0 (2019-01-23)

- Allow intentional assignment to variables named ``_``
- Recognize ``__module__`` as a valid name in class scope
- ``pyflakes.checker.Checker`` supports checking of partial ``ast`` trees
- Detect assign-before-use for local variables which shadow builtin names
- Detect invalid ``print`` syntax using ``>>`` operator
- Treat ``async for`` the same as a ``for`` loop for introducing variables
- Add detection for list concatenation in ``__all__``
- Exempt ``@typing.overload`` from duplicate function declaration
- Importing a submodule of an ``as``-aliased ``import``-import is marked as
  used
- Report undefined names from ``__all__`` as possibly coming from a ``*``
  import
- Add support for changes in Python 3.8-dev
- Add support for PEP 563 (``from __future__ import annotations``)
- Include Python version and platform information in ``pyflakes --version``
- Recognize ``__annotations__`` as a valid magic global in Python 3.6+
- Mark names used in PEP 484 ``# type: ...`` comments as used
- Add check for use of ``is`` operator with ``str``, ``bytes``, and ``int``
  literals

2.0.0 (2018-05-20)

- Drop support for EOL Python <2.7 and 3.2-3.3
- Check for unused exception binding in ``except:`` block
- Handle string literal type annotations
- Ignore redefinitions of ``_``, unless originally defined by import
- Support ``__class__`` without ``self`` in Python 3
- Issue an error for ``raise NotImplemented(...)``

1.6.0 (2017-08-03)

- Process function scope variable annotations for used names
- Find Python files without extensions by their shebang

1.5.0 (2017-01-09)

- Enable support for PEP 526 annotated assignments

1.4.0 (2016-12-30):

- Change formatting of ImportStarMessage to be consistent with other errors
- Support PEP 498 "f-strings"

1.3.0 (2016-09-01):

- Fix PyPy2 Windows IntegrationTests
- Check for duplicate dictionary keys
- Fix TestMain tests on Windows
- Fix "continue" and "break" checks ignoring py3.5's "async for" loop

1.2.3 (2016-05-12):

- Fix TypeError when processing relative imports

1.2.2 (2016-05-06):

- Avoid traceback when exception is del-ed in except

1.2.1 (2015-05-05):

- Fix false RedefinedWhileUnused for submodule imports

1.2.0 (2016-05-03):

- Warn against reusing exception names after the except: block on Python 3
- Improve the error messages for imports

1.1.0 (2016-03-01):

- Allow main() to accept arguments.
- Support @ matrix-multiplication operator
- Validate ``__future__`` imports
- Fix doctest scope testing
- Warn for tuple assertions which are always true
- Warn for "import \*" not at module level on Python 3
- Catch many more kinds of SyntaxErrors
- Check PEP 498 f-strings
- (and a few more sundry bugfixes)

1.0.0 (2015-09-20):

- Python 3.5 support. async/await statements in particular.
- Fix test_api.py on Windows.
- Eliminate a false UnusedImport warning when the name has been
  declared "global"

0.9.2 (2015-06-17):

- Fix a traceback when a global is defined in one scope, and used in another.

0.9.1 (2015-06-09):

- Update NEWS.txt to include 0.9.0, which had been forgotten.

0.9.0 (2015-05-31):

- Exit gracefully, not with a traceback, on SIGINT and SIGPIPE.
- Fix incorrect report of undefined name when using lambda expressions in
  generator expressions.
- Don't crash on DOS line endings on Windows and Python 2.6.
- Don't report an undefined name if the 'del' which caused a name to become
  undefined is only conditionally executed.
- Properly handle differences in list comprehension scope in Python 3.
- Improve handling of edge cases around 'global' defined variables.
- Report an error for 'return' outside a function.

0.8.1 (2014-03-30):

- Detect the declared encoding in Python 3.
- Do not report redefinition of import in a local scope, if the
  global name is used elsewhere in the module.
- Catch undefined variable in loop generator when it is also used as
  loop variable.
- Report undefined name for ``(a, b) = (1, 2)`` but not for the general
  unpacking ``(a, b) = func()``.
- Correctly detect when an imported module is used in default arguments
  of a method, when the method and the module use the same name.
- Distribute a universal wheel file.

0.8.0 (2014-03-22):

- Adapt for the AST in Python 3.4.
- Fix caret position on SyntaxError.
- Fix crash on Python 2.x with some doctest SyntaxError.
- Add tox.ini.
- The ``PYFLAKES_NODOCTEST`` environment variable has been replaced with the
  ``PYFLAKES_DOCTEST`` environment variable (with the opposite meaning).
  Doctest checking is now disabled by default; set the environment variable
  to enable it.
- Correctly parse incremental ``__all__ += [...]``.
- Catch return with arguments inside a generator (Python <= 3.2).
- Do not complain about ``_`` in doctests.
- Drop deprecated methods ``pushFunctionScope`` and ``pushClassScope``.

0.7.3 (2013-07-02):

- Do not report undefined name for generator expression and dict or
  set comprehension at class level.
- Deprecate ``Checker.pushFunctionScope`` and ``Checker.pushClassScope``:
  use ``Checker.pushScope`` instead.
- Remove dependency on Unittest2 for the tests.

0.7.2 (2013-04-24):

- Fix computation of ``DoctestSyntaxError.lineno`` and ``col``.
- Add boolean attribute ``Checker.withDoctest`` to ignore doctests.
- If environment variable ``PYFLAKES_NODOCTEST`` is set, skip doctests.
- Environment variable ``PYFLAKES_BUILTINS`` accepts a comma-separated
  list of additional built-in names.

0.7.1 (2013-04-23):

- File ``bin/pyflakes`` was missing in tarball generated with distribute.
- Fix reporting errors in non-ASCII filenames (Python 2.x).

0.7.0 (2013-04-17):

- Add --version and --help options.
- Support ``python -m pyflakes`` (Python 2.7 and Python 3.x).
- Add attribute ``Message.col`` to report column offset.
- Do not report redefinition of variable for a variable used in a list
  comprehension in a conditional.
- Do not report redefinition of variable for generator expressions and
  set or dict comprehensions.
- Do not report undefined name when the code is protected with a
  ``NameError`` exception handler.
- Do not report redefinition of variable when unassigning a module imported
  for its side-effect.
- Support special locals like ``__tracebackhide__`` for py.test.
- Support checking doctests.
- Fix issue with Turkish locale where ``'i'.upper() == 'i'`` in Python 2.

0.6.1 (2013-01-29):

- Fix detection of variables in augmented assignments.

0.6.0 (2013-01-29):

- Support Python 3 up to 3.3, based on the pyflakes3k project.
- Preserve compatibility with Python 2.5 and all recent versions of Python.
- Support custom reporters in addition to the default Reporter.
- Allow function redefinition for modern property construction via
  property.setter/deleter.
- Fix spurious redefinition warnings in conditionals.
- Do not report undefined name in ``__all__`` if import * is used.
- Add WindowsError as a known built-in name on all platforms.
- Support specifying additional built-ins in the ``Checker`` constructor.
- Don't issue Unused Variable warning when using locals() in current scope.
- Handle problems with the encoding of source files.
- Remove dependency on Twisted for the tests.
- Support ``python setup.py test`` and ``python setup.py develop``.
- Create script using setuptools ``entry_points`` to support all platforms,
  including Windows.

0.5.0 (2011-09-02):

- Convert pyflakes to use newer _ast infrastructure rather than compiler.
- Support for new syntax in 2.7 (including set literals, set comprehensions,
  and dictionary comprehensions).
- Make sure class names don't get bound until after class definition.

0.4.0 (2009-11-25):

- Fix reporting for certain SyntaxErrors which lack line number
  information.
- Check for syntax errors more rigorously.
- Support checking names used with the class decorator syntax in versions
  of Python which have it.
- Detect local variables which are bound but never used.
- Handle permission errors when trying to read source files.
- Handle problems with the encoding of source files.
- Support importing dotted names so as not to incorrectly report them as
  redefined unused names.
- Support all forms of the with statement.
- Consider static ``__all__`` definitions and avoid reporting unused names
  if the names are listed there.
- Fix incorrect checking of class names with respect to the names of their
  bases in the class statement.
- Support the ``__path__`` global in ``__init__.py``.

0.3.0 (2009-01-30):

- Display more informative SyntaxError messages.
- Don't hang flymake with unmatched triple quotes (only report a single
  line of source for a multiline syntax error).
- Recognize ``__builtins__`` as a defined name.
- Improve pyflakes support for python versions 2.3-2.5
- Support for if-else expressions and with statements.
- Warn instead of error on non-existent file paths.
- Check for ``__future__`` imports after other statements.
- Add reporting for some types of import shadowing.
- Improve reporting of unbound locals
