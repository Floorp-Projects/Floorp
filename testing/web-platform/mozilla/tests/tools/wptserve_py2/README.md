Python 2 compatible version of wptserve.

This should be removed as soon as Python 2 is no longer required by
any code that uses wptserve (notably marionette-harness based tests).

When removing it reverting the commit that added it should ensure that
we return to the previous configuration where wptserve is used from
the wpt import.
