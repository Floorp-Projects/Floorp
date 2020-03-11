
*pathspec*: Path Specification
==============================

*pathspec* is a utility library for pattern matching of file paths. So
far this only includes Git's wildmatch pattern matching which itself is
derived from Rsync's wildmatch. Git uses wildmatch for its `gitignore`_
files.

.. _`gitignore`: http://git-scm.com/docs/gitignore


Tutorial
--------

Say you have a "Projects" directory and you want to back it up, but only
certain files, and ignore others depending on certain conditions::

	>>> import pathspec
	>>> # The gitignore-style patterns for files to select, but we're including
	>>> # instead of ignoring.
	>>> spec = """
	...
	... # This is a comment because the line begins with a hash: "#"
	...
	... # Include several project directories (and all descendants) relative to
	... # the current directory. To reference a directory you must end with a
	... # slash: "/"
	... /project-a/
	... /project-b/
	... /project-c/
	...
	... # Patterns can be negated by prefixing with exclamation mark: "!"
	...
	... # Ignore temporary files beginning or ending with "~" and ending with
	... # ".swp".
	... !~*
	... !*~
	... !*.swp
	...
	... # These are python projects so ignore compiled python files from
	... # testing.
	... !*.pyc
	...
	... # Ignore the build directories but only directly under the project
	... # directories.
	... !/*/build/
	...
	... """

We want to use the ``GitWildMatchPattern`` class to compile our patterns. The
``PathSpec`` class provides an interface around pattern implementations::

	>>> spec = pathspec.PathSpec.from_lines(pathspec.patterns.GitWildMatchPattern, spec.splitlines())

That may be a mouthful but it allows for additional patterns to be implemented
in the future without them having to deal with anything but matching the paths
sent to them. ``GitWildMatchPattern`` is the implementation of the actual
pattern which internally gets converted into a regular expression.
``PathSpec`` is a simple wrapper around a list of compiled patterns.

To make things simpler, we can use the registered name for a pattern class
instead of always having to provide a reference to the class itself. The
``GitWildMatchPattern`` class is registered as **gitwildmatch**::

	>>> spec = pathspec.PathSpec.from_lines('gitwildmatch', spec.splitlines())

If we wanted to manually compile the patterns we can just do the following::

	>>> patterns = map(pathspec.patterns.GitWildMatchPattern, spec.splitlines())
	>>> spec = PathSpec(patterns)

``PathSpec.from_lines()`` is simply a class method which does just that.

If you want to load the patterns from file, you can pass the file instance
directly as well::

	>>> with open('patterns.list', 'r') as fh:
	>>>     spec = pathspec.PathSpec.from_lines('gitwildmatch', fh)

You can perform matching on a whole directory tree with::

	>>> matches = spec.match_tree('path/to/directory')

Or you can perform matching on a specific set of file paths with::

	>>> matches = spec.match_files(file_paths)

Or check to see if an individual file matches::

	>>> is_matched = spec.match_file(file_path)


License
-------

*pathspec* is licensed under the `Mozilla Public License Version 2.0`_. See
`LICENSE`_ or the `FAQ`_ for more information.

In summary, you may use *pathspec* with any closed or open source project
without affecting the license of the larger work so long as you:

- give credit where credit is due,

- and release any custom changes made to *pathspec*.

.. _`Mozilla Public License Version 2.0`: http://www.mozilla.org/MPL/2.0
.. _`LICENSE`: LICENSE
.. _`FAQ`: http://www.mozilla.org/MPL/2.0/FAQ.html


Source
------

The source code for *pathspec* is available from the GitHub repo
`cpburnz/python-path-specification`_.

.. _`cpburnz/python-path-specification`: https://github.com/cpburnz/python-path-specification


Installation
------------

*pathspec* requires the following packages:

- `setuptools`_

*pathspec* can be installed from source with::

	python setup.py install

*pathspec* is also available for install through `PyPI`_::

	pip install pathspec

.. _`setuptools`: https://pypi.python.org/pypi/setuptools
.. _`PyPI`: http://pypi.python.org/pypi/pathspec


Documentation
-------------

Documentation for *pathspec* is available on `Read the Docs`_.

.. _`Read the Docs`: http://python-path-specification.readthedocs.io


Other Languages
---------------

*pathspec* is also available as a `Ruby gem`_.

.. _`Ruby gem`: https://github.com/highb/pathspec-ruby
