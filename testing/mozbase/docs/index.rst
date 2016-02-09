.. MozBase documentation master file, created by
   sphinx-quickstart on Mon Oct 22 14:02:17 2012.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

mozbase
=======

Mozbase is a set of easy-to-use Python packages forming a supplemental standard
library for Mozilla. It provides consistency and reduces redundancy in
automation and other system-level software. All of Mozilla's test harnesses use
mozbase to some degree, including Talos_, mochitest_, reftest_, Autophone_, and
Eideticker_.

.. _Talos: https://wiki.mozilla.org/Talos

.. _mochitest: https://developer.mozilla.org/en-US/docs/Mochitest

.. _reftest: https://developer.mozilla.org/en-US/docs/Creating_reftest-based_unit_tests

.. _Autophone: https://wiki.mozilla.org/Auto-tools/Projects/AutoPhone

.. _Eideticker: https://wiki.mozilla.org/Project_Eideticker

In the course of writing automated tests at Mozilla, we found that
the same tasks came up over and over, regardless of the specific nature of
what we were testing. We figured that consolidating this code into a set of
libraries would save us a good deal of time, and so we spent some effort
factoring out the best-of-breed automation code into something we named
"mozbase" (usually written all in lower case except at the beginning of a
sentence).

This is the main documentation for users of mozbase. There is also a
project_ wiki page with notes on development practices and administration.

.. _project: https://wiki.mozilla.org/Auto-tools/Projects/Mozbase

The documentation is organized by category, then by module. Figure out what you
want to do then dive in!

.. toctree::
   :maxdepth: 2

   manifestparser
   gettinginfo
   setuprunning
   loggingreporting
   devicemanagement

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

