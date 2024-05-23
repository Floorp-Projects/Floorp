# Mn Python tests

_Marionette_ is the codename of a [remote protocol] built in to
Firefox as well as the name of a functional test framework for
automating user interface tests.

The in-tree test framework supports tests written in Python, using
Python’s [unittest] library.  Test cases are written as a subclass
of `MarionetteTestCase`, with child tests belonging to instance
methods that have a name starting with `test_`.

You can additionally define [`setUp`] and [`tearDown`] instance
methods to execute code before and after child tests, and
[`setUpClass`]/[`tearDownClass`] for the parent test.  When you use
these, it is important to remember calling the `MarionetteTestCase`
superclass’ own [`setUp`]/[`tearDown`] methods since they handle
setup/cleanup of the session.

The test structure is illustrated here:

```python
from marionette_harness import MarionetteTestCase

class TestSomething(MarionetteTestCase):
    def setUp(self):
        # code to execute before any tests are run
        MarionetteTestCase.setUp(self)

    def test_foo(self):
        # run test for 'foo'

    def test_bar(self):
        # run test for 'bar'

    def tearDown(self):
        # code to execute after all tests are run
        MarionetteTestCase.tearDown(self)
```

[remote protocol]: Protocol.md
[unittest]: https://docs.python.org/3/library/unittest.html
[`setUp`]: https://docs.python.org/3/library/unittest.html#unittest.TestCase.setUp
[`setUpClass`]: https://docs.python.org/3/library/unittest.html#unittest.TestCase.setUpClass
[`tearDown`]: https://docs.python.org/3/library/unittest.html#unittest.TestCase.tearDown
[`tearDownClass`]: https://docs.python.org/3/library/unittest.html#unittest.TestCase.tearDownClass

## Test assertions

Assertions are provided courtesy of [unittest].  For example:

```python
from marionette_harness import MarionetteTestCase

class TestSomething(MarionetteTestCase):
    def test_foo(self):
        self.assertEqual(9, 3 * 3, '3 x 3 should be 9')
        self.assertTrue(type(2) == int, '2 should be an integer')
```

## The API

The full API documentation is found [here], but the key objects are:

* `MarionetteTestCase`: a subclass for `unittest.TestCase`
  used as a base class for all tests to run.

* {class}`Marionette <marionette_driver.marionette.Marionette>`: client that speaks to Firefox

[here]: /python/marionette_driver.rst

## Registering Test Manifests

To run Marionette Python tests locally via `mach` or as part of the `Mn` tests jobs
in CI they need to be registered. This happens by adding a manifest file to the tree,
which includes a reference to the test files and expectations for results.

Such a manifest file can look like the following and is stored with the extension `.toml`:

```ini
[DEFAULT]

["test_expected_fail.py"]
expected = "fail"

["test_not_on_windows.py"]
skip-if = ["os == 'win'"]
```

The registration of such a manifest file is done in two different ways:

1. To run the tests locally via `./mach test` or `./mach marionette-test` the
created Marionette manifest file needs to be referenced in the folder's related
`moz.build` file by adding it to the `MARIONETTE_MANIFESTS` variable like:

    MARIONETTE_MANIFESTS += ["test/marionette/manifest.toml"]

2. To run the tests in CI the manifest file also needs to be included in the
Marionette's own [master manifest file]. This ensures that the test packaging step
will find the tests and include them as well in the test package.

[master manifest file]: https://searchfox.org/mozilla-central/source/testing/marionette/harness/marionette_harness/tests/unit-tests.toml
