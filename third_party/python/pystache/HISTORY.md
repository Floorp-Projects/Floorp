History
=======

**Note:** Official support for Python 2.4 will end with Pystache version 0.6.0.

0.5.4 (2014-07-11)
------------------

-   Bugfix: made test with filenames OS agnostic (issue \#162).

0.5.3 (2012-11-03)
------------------

-   Added ability to customize string coercion (e.g. to have None render as
    `''`) (issue \#130).
-   Added Renderer.render_name() to render a template by name (issue \#122).
-   Added TemplateSpec.template_path to specify an absolute path to a
    template (issue \#41).
-   Added option of raising errors on missing tags/partials:
    `Renderer(missing_tags='strict')` (issue \#110).
-   Added support for finding and loading templates by file name in
    addition to by template name (issue \#127). [xgecko]
-   Added a `parse()` function that yields a printable, pre-compiled
    parse tree.
-   Added support for rendering pre-compiled templates.
-   Added Python 3.3 to the list of supported versions.
-   Added support for [PyPy](http://pypy.org/) (issue \#125).
-   Added support for [Travis CI](http://travis-ci.org) (issue \#124).
    [msabramo]
-   Bugfix: `defaults.DELIMITERS` can now be changed at runtime (issue \#135).
    [bennoleslie]
-   Bugfix: exceptions raised from a property are no longer swallowed
    when getting a key from a context stack (issue \#110).
-   Bugfix: lambda section values can now return non-ascii, non-unicode
    strings (issue \#118).
-   Bugfix: allow `test_pystache.py` and `tox` to pass when run from a
    downloaded sdist (i.e. without the spec test directory).
-   Convert HISTORY and README files from reST to Markdown.
-   More robust handling of byte strings in Python 3.
-   Added Creative Commons license for David Phillips's logo.

0.5.2 (2012-05-03)
------------------

-   Added support for dot notation and version 1.1.2 of the spec (issue
    \#99). [rbp]
-   Missing partials now render as empty string per latest version of
    spec (issue \#115).
-   Bugfix: falsey values now coerced to strings using str().
-   Bugfix: lambda return values for sections no longer pushed onto
    context stack (issue \#113).
-   Bugfix: lists of lambdas for sections were not rendered (issue
    \#114).

0.5.1 (2012-04-24)
------------------

-   Added support for Python 3.1 and 3.2.
-   Added tox support to test multiple Python versions.
-   Added test script entry point: pystache-test.
-   Added \_\_version\_\_ package attribute.
-   Test harness now supports both YAML and JSON forms of Mustache spec.
-   Test harness no longer requires nose.

0.5.0 (2012-04-03)
------------------

This version represents a major rewrite and refactoring of the code base
that also adds features and fixes many bugs. All functionality and
nearly all unit tests have been preserved. However, some backwards
incompatible changes to the API have been made.

Below is a selection of some of the changes (not exhaustive).

Highlights:

-   Pystache now passes all tests in version 1.0.3 of the [Mustache
    spec](https://github.com/mustache/spec). [pvande]
-   Removed View class: it is no longer necessary to subclass from View
    or from any other class to create a view.
-   Replaced Template with Renderer class: template rendering behavior
    can be modified via the Renderer constructor or by setting
    attributes on a Renderer instance.
-   Added TemplateSpec class: template rendering can be specified on a
    per-view basis by subclassing from TemplateSpec.
-   Introduced separation of concerns and removed circular dependencies
    (e.g. between Template and View classes, cf. [issue
    \#13](https://github.com/defunkt/pystache/issues/13)).
-   Unicode now used consistently throughout the rendering process.
-   Expanded test coverage: nosetests now runs doctests and \~105 test
    cases from the Mustache spec (increasing the number of tests from 56
    to \~315).
-   Added a rudimentary benchmarking script to gauge performance while
    refactoring.
-   Extensive documentation added (e.g. docstrings).

Other changes:

-   Added a command-line interface. [vrde]
-   The main rendering class now accepts a custom partial loader (e.g. a
    dictionary) and a custom escape function.
-   Non-ascii characters in str strings are now supported while
    rendering.
-   Added string encoding, file encoding, and errors options for
    decoding to unicode.
-   Removed the output encoding option.
-   Removed the use of markupsafe.

Bug fixes:

-   Context values no longer processed as template strings.
    [jakearchibald]
-   Whitespace surrounding sections is no longer altered, per the spec.
    [heliodor]
-   Zeroes now render correctly when using PyPy. [alex]
-   Multline comments now permitted. [fczuardi]
-   Extensionless template files are now supported.
-   Passing `**kwargs` to `Template()` no longer modifies the context.
-   Passing `**kwargs` to `Template()` with no context no longer raises
    an exception.

0.4.1 (2012-03-25)
------------------

-   Added support for Python 2.4. [wangtz, jvantuyl]

0.4.0 (2011-01-12)
------------------

-   Add support for nested contexts (within template and view)
-   Add support for inverted lists
-   Decoupled template loading

0.3.1 (2010-05-07)
------------------

-   Fix package

0.3.0 (2010-05-03)
------------------

-   View.template\_path can now hold a list of path
-   Add {{& blah}} as an alias for {{{ blah }}}
-   Higher Order Sections
-   Inverted sections

0.2.0 (2010-02-15)
------------------

-   Bugfix: Methods returning False or None are not rendered
-   Bugfix: Don't render an empty string when a tag's value is 0.
    [enaeseth]
-   Add support for using non-callables as View attributes.
    [joshthecoder]
-   Allow using View instances as attributes. [joshthecoder]
-   Support for Unicode and non-ASCII-encoded bytestring output.
    [enaeseth]
-   Template file encoding awareness. [enaeseth]

0.1.1 (2009-11-13)
------------------

-   Ensure we're dealing with strings, always
-   Tests can be run by executing the test file directly

0.1.0 (2009-11-12)
------------------

-   First release
