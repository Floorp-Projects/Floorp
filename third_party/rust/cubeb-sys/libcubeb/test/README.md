Notes on writing tests.

The googletest submodule is currently at 1.6 rather than the latest, and should
only be updated to track the version used in Gecko to make test compatibility
easier.

Always #include "gtest/gtest.h" before *anything* else.

All tests should be part of the "cubeb" test case, e.g. TEST(cubeb, my_test).

Tests are built stand-alone in cubeb, but built as a single unit in Gecko, so
you must use unique names for globally visible items in each test, e.g. rather
than state_cb use state_cb_my_test.
