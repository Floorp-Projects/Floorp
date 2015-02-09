Managing lists of tests
=======================

.. py:currentmodule:: manifestparser

We don't always want to run all tests, all the time. Sometimes a test
may be broken, in other cases we only want to run a test on a specific
platform or build of Mozilla. To handle these cases (and more), we
created a python library to create and use test "manifests", which
codify this information.

:mod:`manifestparser` --- Create and manage test manifests
-----------------------------------------------------------

manifestparser lets you easily create and use test manifests, to
control which tests are run under what circumstances.

What manifestparser gives you:

* manifests are ordered lists of tests
* tests may have an arbitrary number of key, value pairs
* the parser returns an ordered list of test data structures, which
  are just dicts with some keys.  For example, a test with no
  user-specified metadata looks like this:

.. code-block:: text

    [{'expected': 'pass',
      'path': '/home/mozilla/mozmill/src/manifestparser/manifestparser/tests/testToolbar/testBackForwardButtons.js',
      'relpath': 'testToolbar/testBackForwardButtons.js',
      'name': 'testBackForwardButtons.js',
      'here': '/home/mozilla/mozmill/src/manifestparser/manifestparser/tests',
      'manifest': '/home/mozilla/mozmill/src/manifestparser/manifestparser/tests/manifest.ini',}]

The keys displayed here (path, relpath, name, here, and manifest) are
reserved keys for manifestparser and any consuming APIs.  You can add
additional key, value metadata to each test.

Why have test manifests?
````````````````````````

It is desirable to have a unified format for test manifests for testing
[mozilla-central](http://hg.mozilla.org/mozilla-central), etc.

* It is desirable to be able to selectively enable or disable tests based on platform or other conditions. This should be easy to do. Currently, since many of the harnesses just crawl directories, there is no effective way of disabling a test except for removal from mozilla-central
* It is desriable to do this in a universal way so that enabling and disabling tests as well as other tasks are easily accessible to a wider audience than just those intimately familiar with the specific test framework.
* It is desirable to have other metadata on top of the test. For instance, let's say a test is marked as skipped. It would be nice to give the reason why.


Most Mozilla test harnesses work by crawling a directory structure.
While this is straight-forward, manifests offer several practical
advantages:

* ability to turn a test off easily: if a test is broken on m-c
  currently, the only way to turn it off, generally speaking, is just
  removing the test.  Often this is undesirable, as if the test should
  be dismissed because other people want to land and it can't be
  investigated in real time (is it a failure? is the test bad? is no
  one around that knows the test?), then backing out a test is at best
  problematic.  With a manifest, a test may be disabled without
  removing it from the tree and a bug filed with the appropriate
  reason:

.. code-block:: text

     [test_broken.js]
     disabled = https://bugzilla.mozilla.org/show_bug.cgi?id=123456

* ability to run different (subsets of) tests on different
  platforms. Traditionally, we've done a bit of magic or had the test
  know what platform it would or would not run on. With manifests, you
  can mark what platforms a test will or will not run on and change
  these without changing the test.

.. code-block:: text

     [test_works_on_windows_only.js]
     run-if = os == 'win'

* ability to markup tests with metadata. We have a large, complicated,
  and always changing infrastructure.  key, value metadata may be used
  as an annotation to a test and appropriately curated and mined.  For
  instance, we could mark certain tests as randomorange with a bug
  number, if it were desirable.

* ability to have sane and well-defined test-runs. You can keep
  different manifests for different test runs and ``[include:]``
  (sub)manifests as appropriate to your needs.

Manifest Format
````````

Manifests are .ini file with the section names denoting the path
relative to the manifest:

.. code-block:: text

    [foo.js]
    [bar.js]
    [fleem.js]

The sections are read in order. In addition, tests may include
arbitrary key, value metadata to be used by the harness.  You may also
have a `[DEFAULT]` section that will give key, value pairs that will
be inherited by each test unless overridden:

.. code-block:: text

    [DEFAULT]
    type = restart

    [lilies.js]
    color = white

    [daffodils.js]
    color = yellow
    type = other
    # override type from DEFAULT

    [roses.js]
    color = red

You can also include other manifests:

.. code-block:: text

    [include:subdir/anothermanifest.ini]

And reference parent manifests to inherit keys and values from the DEFAULT
section, without adding possible included tests.

.. code-block:: text

    [parent:../manifest.ini]

Manifests are included relative to the directory of the manifest with
the `[include:]` directive unless they are absolute paths.

By default you can use both '#' and ';' as comment characters. Comments
must start on a new line, inline comments are not supported.

.. code-block:: text

    [roses.js]
    # a valid comment
    ; another valid comment
    color = red # not a valid comment

In the example above, the 'color' property will have the value 'red #
not a valid comment'.

Special variable server-root
````````````````````````````
There is a special variable called `server-root` used for paths on the system.
This variable is deemed a path and will be expanded into its absolute form.

Because of the inheritant nature of the key/value pairs, if one requires a
system path, it must be absolute for it to be of any use in any included file.

.. code-block:: text

    [DEFAULTS]
    server-root = ../data

    [test1.js]
    server-root = test1/data

Manifest Conditional Expressions
````````````````````````````````
The conditional expressions used in manifests are parsed using the *ExpressionParser* class.

.. autoclass:: manifestparser.ExpressionParser

Consumers of this module are expected to pass in a value dictionary
for evaluating conditional expressions. A common pattern is to pass
the dictionary from the :mod:`mozinfo` module.

Data
````

Manifest Destiny gives tests as a list of dictionaries (in python
terms).

* path: full path to the test
* relpath: relative path starting from the root manifest location
* name: file name of the test
* here: the parent directory of the manifest
* manifest: the path to the manifest containing the test

This data corresponds to a one-line manifest:

.. code-block:: text

    [testToolbar/testBackForwardButtons.js]

If additional key, values were specified, they would be in this dict
as well.

Outside of the reserved keys, the remaining key, values
are up to convention to use.  There is a (currently very minimal)
generic integration layer in manifestparser for use of all harnesses,
`manifestparser.TestManifest`.
For instance, if the 'disabled' key is present, you can get the set of
tests without disabled (various other queries are doable as well).

Since the system is convention-based, the harnesses may do whatever
they want with the data.  They may ignore it completely, they may use
the provided integration layer, or they may provide their own
integration layer.  This should allow whatever sort of logic is
desired.  For instance, if in yourtestharness you wanted to run only on
mondays for a certain class of tests:

.. code-block:: text

    tests = []
    for test in manifests.tests:
        if 'runOnDay' in test:
           if calendar.day_name[calendar.weekday(*datetime.datetime.now().timetuple()[:3])].lower() == test['runOnDay'].lower():
               tests.append(test)
        else:
           tests.append(test)

To recap:
* the manifests allow you to specify test data
* the parser gives you this data
* you can use it however you want or process it further as you need

Tests are denoted by sections in an .ini file (see
http://hg.mozilla.org/automation/manifestparser/file/tip/manifestparser/tests/mozmill-example.ini).

Additional manifest files may be included with an `[include:]` directive:

.. code-block:: text

    [include:path-to-additional-file.manifest]

The path to included files is relative to the current manifest.

The `[DEFAULT]` section contains variables that all tests inherit from.

Included files will inherit the top-level variables but may override
in their own `[DEFAULT]` section.

manifestparser Architecture
````````````````````````````

There is a two- or three-layered approach to the manifestparser
architecture, depending on your needs:

1. ManifestParser: this is a generic parser for .ini manifests that
facilitates the `[include:]` logic and the inheritence of
metadata. Despite the internal variable being called `self.tests`
(an oversight), this layer has nothing in particular to do with tests.

2. TestManifest: this is a harness-agnostic integration layer that is
test-specific. TestManifest faciliates `skip-if` and `run-if` logic.

3. Optionally, a harness will have an integration layer than inherits
from TestManifest if more harness-specific customization is desired at
the manifest level.

See the source code at https://github.com/mozilla/mozbase/tree/master/manifestparser
and
https://github.com/mozilla/mozbase/blob/master/manifestparser/manifestparser.py
in particular.

Filtering Manifests
```````````````````

After creating a `TestManifest` object, all manifest files are read and a list
of test objects can be accessed via `TestManifest.tests`. However this list contains
all test objects, whether they should be run or not. Normally they need to be
filtered down only to the set of tests that should be run by the test harness.

To do this, a test harness can call `TestManifest.active_tests`:

.. code-block:: python

    tests = manifest.active_tests(exists=True, disabled=True, **tags)

By default, `active_tests` runs the filters found in
:attr:`~.DEFAULT_FILTERS`. It also accepts two convenience arguments:

1. `exists`: if True (default), filter out tests that do not exist on the local file system.
2. `disabled`: if True (default), do not filter out tests containing the 'disabled' key
   (which can be set by `skip-if`, `run-if` or manually).

This works for simple cases, but there are other built-in filters, or even custom filters
that can be applied to the `TestManifest`. To do so, add the filter to `TestManifest.filters`:

.. code-block:: python

    from manifestparser.filters import subsuite
    import mozinfo

    filters = [subsuite('devtools')]
    tests = manifest.active_tests(filters=filters, **mozinfo.info)

.. automodule:: manifestparser.filters
    :members:
    :exclude-members: filterlist,InstanceFilter,DEFAULT_FILTERS

.. autodata::  manifestparser.filters.DEFAULT_FILTERS
    :annotation:

For example, suppose we want to introduce a new key called `timeout-if` that adds a
'timeout' property to a test if a certain condition is True. The syntax in the manifest
files will look like this:

.. code-block:: text

    [test_foo.py]
    timeout-if = 300, os == 'win'

The value is <timeout>, <condition> where condition is the same format as the one in
`skip-if`. In the above case, if os == 'win', a timeout of 300 seconds will be
applied. Otherwise, no timeout will be applied. All we need to do is define the filter
and add it:

.. code-block:: python

    from manifestparser.expression import parse
    import mozinfo

    def timeout_if(tests, values):
        for test in tests:
            if 'timeout-if' in test:
                timeout, condition = test['timeout-if'].split(',', 1)
                if parse(condition, **values):
                    test['timeout'] = timeout
            yield test

    tests = manifest.active_tests(filters=[timeout_if], **mozinfo.info)

Creating Manifests
``````````````````

manifestparser comes with a console script, `manifestparser create`, that
may be used to create a seed manifest structure from a directory of
files.  Run `manifestparser help create` for usage information.

Copying Manifests
`````````````````

To copy tests and manifests from a source:

.. code-block:: text

    manifestparser [options] copy from_manifest to_directory -tag1 -tag2 `key1=value1 key2=value2 ...

Updating Tests
``````````````

To update the tests associated with with a manifest from a source
directory:

.. code-block:: text

    manifestparser [options] update manifest from_directory -tag1 -tag2 `key1=value1 `key2=value2 ...

Usage example
`````````````

Here is an example of how to create manifests for a directory tree and
update the tests listed in the manifests from an external source.

Creating Manifests
``````````````````

Let's say you want to make a series of manifests for a given directory structure containing `.js` test files:

.. code-block:: text

    testing/mozmill/tests/firefox/
    testing/mozmill/tests/firefox/testAwesomeBar/
    testing/mozmill/tests/firefox/testPreferences/
    testing/mozmill/tests/firefox/testPrivateBrowsing/
    testing/mozmill/tests/firefox/testSessionStore/
    testing/mozmill/tests/firefox/testTechnicalTools/
    testing/mozmill/tests/firefox/testToolbar/
    testing/mozmill/tests/firefox/restartTests

You can use `manifestparser create` to do this:

.. code-block:: text

    $ manifestparser help create
    Usage: manifestparser.py [options] create directory <directory> <...>

         create a manifest from a list of directories

    Options:
      -p PATTERN, `pattern=PATTERN
                            glob pattern for files
      -i IGNORE, `ignore=IGNORE
                            directories to ignore
      -w IN_PLACE, --in-place=IN_PLACE
                            Write .ini files in place; filename to write to

We only want `.js` files and we want to skip the `restartTests` directory.
We also want to write a manifest per directory, so I use the `--in-place`
option to write the manifests:

.. code-block:: text

    manifestparser create . -i restartTests -p '*.js' -w manifest.ini

This creates a manifest.ini per directory that we care about with the JS test files:

.. code-block:: text

    testing/mozmill/tests/firefox/manifest.ini
    testing/mozmill/tests/firefox/testAwesomeBar/manifest.ini
    testing/mozmill/tests/firefox/testPreferences/manifest.ini
    testing/mozmill/tests/firefox/testPrivateBrowsing/manifest.ini
    testing/mozmill/tests/firefox/testSessionStore/manifest.ini
    testing/mozmill/tests/firefox/testTechnicalTools/manifest.ini
    testing/mozmill/tests/firefox/testToolbar/manifest.ini

The top-level `manifest.ini` merely has `[include:]` references to the sub manifests:

.. code-block:: text

    [include:testAwesomeBar/manifest.ini]
    [include:testPreferences/manifest.ini]
    [include:testPrivateBrowsing/manifest.ini]
    [include:testSessionStore/manifest.ini]
    [include:testTechnicalTools/manifest.ini]
    [include:testToolbar/manifest.ini]

Each sub-level manifest contains the (`.js`) test files relative to it.

Updating the tests from manifests
`````````````````````````````````

You may need to update tests as given in manifests from a different source directory.
`manifestparser update` was made for just this purpose:

.. code-block:: text

    Usage: manifestparser [options] update manifest directory -tag1 -tag2 `key1=value1 --key2=value2 ...

        update the tests as listed in a manifest from a directory

To update from a directory of tests in `~/mozmill/src/mozmill-tests/firefox/` run:

.. code-block:: text

    manifestparser update manifest.ini ~/mozmill/src/mozmill-tests/firefox/

Tests
`````

manifestparser includes a suite of tests:

https://github.com/mozilla/mozbase/tree/master/manifestparsery/tests

`test_manifest.txt` is a doctest that may be helpful in figuring out
how to use the API.  Tests are run via `python test.py`.

Bugs
````

Please file any bugs or feature requests at

https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=ManifestParser

Or contact jhammel @mozilla.org or in #ateam on irc.mozilla.org

CLI
```

Run `manifestparser help` for usage information.

To create a manifest from a set of directories:

.. code-block:: text

    manifestparser [options] create directory <directory> <...> [create-options]

To output a manifest of tests:

.. code-block:: text

    manifestparser [options] write manifest <manifest> <...> -tag1 -tag2 --key1=value1 --key2=value2 ...

To copy tests and manifests from a source:

.. code-block:: text

    manifestparser [options] copy from_manifest to_manifest -tag1 -tag2 `key1=value1 key2=value2 ...

To update the tests associated with with a manifest from a source
directory:

.. code-block:: text

    manifestparser [options] update manifest from_directory -tag1 -tag2 --key1=value1 --key2=value2 ...

Design Considerations
`````````````````````

Contrary to some opinion, manifestparser.py and the associated .ini
format were not magically plucked from the sky but were descended upon
through several design considerations.

* test manifests should be ordered.  While python 2.6 and greater has
  a ConfigParser that can use an ordered dictionary, it is a
  requirement that we support python 2.4 for the build + testing
  environment.  To that end, a `read_ini` function was implemented
  in manifestparser.py that should be the equivalent of the .ini
  dialect used by ConfigParser.

* the manifest format should be easily human readable/writable.  While
  there was initially some thought of using JSON, there was pushback
  that JSON was not easily editable.  An ideal manifest format would
  degenerate to a line-separated list of files.  While .ini format
  requires an additional `[]` per line, and while there have been
  complaints about this, hopefully this is good enough.

* python does not have an in-built YAML parser.  Since it was
  undesirable for manifestparser.py to have any dependencies, YAML was
  dismissed as a format.

* we could have used a proprietary format but decided against it.
  Everyone knows .ini and there are good tools to deal with it.
  However, since read_ini is the only function that transforms a
  manifest to a list of key, value pairs, while the implications for
  changing the format impacts downstream code, doing so should be
  programmatically simple.

* there should be a single file that may easily be
  transported. Traditionally, test harnesses have lived in
  mozilla-central. This is less true these days and it is increasingly
  likely that more tests will not live in mozilla-central going
  forward.  So `manifestparser.py` should be highly consumable. To
  this end, it is a single file, as appropriate to mozilla-central,
  which is also a working python package deployed to PyPI for easy
  installation.

Historical Reference
````````````````````

Date-ordered list of links about how manifests came to be where they are today::

* https://wiki.mozilla.org/Auto-tools/Projects/UniversalManifest
* http://alice.nodelman.net/blog/post/2010/05/
* http://alice.nodelman.net/blog/post/universal-manifest-for-unit-tests-a-proposal/
* https://elvis314.wordpress.com/2010/07/05/improving-personal-hygiene-by-adjusting-mochitests/
* https://elvis314.wordpress.com/2010/07/27/types-of-data-we-care-about-in-a-manifest/
* https://bugzilla.mozilla.org/show_bug.cgi?id=585106
* http://elvis314.wordpress.com/2011/05/20/converting-xpcshell-from-listing-directories-to-a-manifest/
* https://bugzilla.mozilla.org/show_bug.cgi?id=616999
* https://developer.mozilla.org/en/Writing_xpcshell-based_unit_tests#Adding_your_tests_to_the_xpcshell_manifest
