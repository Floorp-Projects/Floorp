-------------
Running Tests
-------------

Because of the rules below, you can run all the unit tests in this directory,
and only these tests, with:

  mach gtest "pkix*"

You can run just the tests for functions defined in filename pkixfoo.cpp with:

  mach gtest "pkixfoo*"

If you run "mach gtest" then you'll end up running every gtest in Gecko.



------------
Naming Files
------------

Name files containing tests according to one of the following patterns:

  * <filename>_tests.cpp
  * <filename>_<Function>_tests.cpp
  * <filename>_<category>_tests.cpp

  <filename> is the name of the file containing the definitions of the
             function(s) being tested by every test.
  <Function> is the name of the function that is being tested by every
             test.
  <category> describes the group of related functions that are being
             tested by every test.



------------------------------------------------
Always Use a Fixture Class: TEST_F(), not TEST()
------------------------------------------------

Many tests don't technically need a fixture, and so TEST() could technically
be used to define the test. However, when you use TEST_F() instead of TEST(),
the compiler will not allow you to make any typos in the test case name, but
if you use TEST() then the name of the test case is not checked.

See https://code.google.com/p/googletest/wiki/Primer#Test_Fixtures:_Using_the_Same_Data_Configuration_for_Multiple_Te
to learn more about test fixtures.

---------------
Naming Fixtures
---------------

When all tests in a file use the same fixture, use the base name of the file
without the "_tests" suffix as the name of the fixture class; e.g. tests in
"pkixocsp.cpp" should use a fixture "class pkixocsp" by default.

Sometimes tests in a file need separate fixtures. In this case, name the
fixture class according to the pattern <fixture_base>_<fixture_suffix>, where
<fixture_base> is the base name of the file without the "_tests" suffix, and
<fixture_suffix> is a descriptive name for the fixture class, e.g.
"class pkixocsp_DelegatedResponder".
