Mn Python tests
===============

_Marionette_ is the codename of a [remote protocol] built in to
Firefox as well as the name of a functional test framework for
automating user interface tests.

The in-tree test framework supports tests written in Python, using
Python’s [unittest] library.  Test cases are written as a subclass
of [`MarionetteTestCase`], with child tests belonging to instance
methods that have a name starting with `test_`.

You can additionally define [`setUp`] and [`tearDown`] instance
methods to execute code before and after child tests, and
[`setUpClass`]/[`tearDownClass`] for the parent test.  When you use
these, it is important to remember calling the [`MarionetteTestCase`]
superclass’ own `setUp`/`tearDown` methods since they handle
setup/cleanup of the session.

The test structure is illustrated here:

	from marionette_test import MarionetteTestCase

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

[remote protocol]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/Protocol
[unittest]: https://docs.python.org/2.7/library/unittest.html
[`MarionetteTestCase`]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/MarionetteTestCase
[`setUp`]: https://docs.python.org/2.7/library/unittest.html#unittest.TestCase.setUp
[`setUpClass`]: https://docs.python.org/2.7/library/unittest.html#unittest.TestCase.setUpClass
[`tearDown`]: https://docs.python.org/2.7/library/unittest.html#unittest.TestCase.tearDown
[`tearDownClass`]: https://docs.python.org/2.7/library/unittest.html#unittest.TestCase.tearDownClass


Test assertions
---------------

Assertions are provided courtesy of [unittest].  For example:

	from marionette_test import MarionetteTestCase

	class TestSomething(MarionetteTestCase):
	    def test_foo(self):
	        self.assertEqual(9, 3 * 3, '3 x 3 should be 9')
	        self.assertTrue(type(2) == int, '2 should be an integer')


The API
-------

The full API documentation is found at
http://marionette-client.readthedocs.io/en/master/, but the key
objects are:

  * [`MarionetteTestCase`]: a subclass for `unittest.TestCase`
    used as a base class for all tests to run.

  * [`Marionette`]: client that speaks to Firefox.

[`Marionette`]: http://marionette-client.readthedocs.io/en/master/reference.html#marionette
