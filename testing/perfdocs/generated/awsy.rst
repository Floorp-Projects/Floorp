====
AWSY
====

Are We Slim Yet project (commonly known as AWSY) tracks memory usage across builds.

On treeherder, the AWSY builds are listed in subgroups of `SY`.

AWSY tests consist of three types: TP5*, TP6, and Base Memory Usage.

*\*TP5 tests are out of date and no longer maintained. These tests are scheduled to be removed:* https://bugzilla.mozilla.org/show_bug.cgi?id=1712406

Awsy tests
----------


.. dropdown:: base (FF)
   :container: + anchor-id-base-Awsy-tests

   **Owner**: :mccr8 and Perftest Team

   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * awsy-base: None
            * awsy-base-dmd: None

      * test-linux1804-64-qr/opt
            * awsy-base: None
            * awsy-base-dmd: None

      * test-linux1804-64-shippable-qr/opt
            * awsy-base: integration, mozilla-beta, mozilla-central, mozilla-release
            * awsy-base-dmd: None

      * test-macosx1015-64-shippable-qr/opt
            * awsy-base: integration, mozilla-beta, mozilla-central, mozilla-release
            * awsy-base-dmd: None

      * test-windows10-32-2004-qr/opt
            * awsy-base: None
            * awsy-base-dmd: None

      * test-windows10-32-2004-shippable-qr/opt
            * awsy-base: None
            * awsy-base-dmd: None

      * test-windows10-64-2004-qr/opt
            * awsy-base: None
            * awsy-base-dmd: None

      * test-windows10-64-2004-shippable-qr/opt
            * awsy-base: integration, mozilla-beta, mozilla-central, mozilla-release
            * awsy-base-dmd: None


.. dropdown:: dmd (FF)
   :container: + anchor-id-dmd-Awsy-tests

   **Owner**: :mccr8 and Perftest Team

   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-linux1804-64-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-linux1804-64-shippable-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-macosx1015-64-shippable-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-windows10-32-2004-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-windows10-32-2004-shippable-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-windows10-64-2004-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None

      * test-windows10-64-2004-shippable-qr/opt
            * awsy-base-dmd: None
            * awsy-dmd: None


.. dropdown:: tp5 (FF)
   :container: + anchor-id-tp5-Awsy-tests

   **Owner**: :mccr8 and Perftest Team

   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt

      * test-linux1804-64-qr/opt

      * test-linux1804-64-shippable-qr/opt

      * test-macosx1015-64-shippable-qr/opt

      * test-windows10-32-2004-qr/opt

      * test-windows10-32-2004-shippable-qr/opt

      * test-windows10-64-2004-qr/opt

      * test-windows10-64-2004-shippable-qr/opt


.. dropdown:: tp6 (FF)
   :container: + anchor-id-tp6-Awsy-tests

   **Owner**: :mccr8 and Perftest Team

   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * awsy-tp6: None

      * test-linux1804-64-qr/opt
            * awsy-tp6: None

      * test-linux1804-64-shippable-qr/opt
            * awsy-tp6: integration, mozilla-beta, mozilla-central, mozilla-release

      * test-macosx1015-64-shippable-qr/opt
            * awsy-tp6: integration, mozilla-beta, mozilla-central, mozilla-release

      * test-windows10-32-2004-qr/opt
            * awsy-tp6: None

      * test-windows10-32-2004-shippable-qr/opt
            * awsy-tp6: None

      * test-windows10-64-2004-qr/opt
            * awsy-tp6: None

      * test-windows10-64-2004-shippable-qr/opt
            * awsy-tp6: integration, mozilla-beta, mozilla-central, mozilla-release




Running AWSY Locally
*********************

Running tests locally is most likely only useful for debugging what is going on in a test,
as the test output is only reported as raw JSON. The CLI is documented via:

.. code-block:: bash

    ./mach awsy-test --help

*Currently all tests will download TP5 even if it is not used, see:* https://bugzilla.mozilla.org/show_bug.cgi?id=1683920

TP5 tests
=========

.. code-block:: bash

    ./mach awsy-test

TP6 tests
=========

.. code-block:: bash

    ./mach awsy-test --tp6

Base Memory Usage tests
========================

.. code-block:: bash

    ./mach awsy-test testing/awsy/awsy/test_base_memory_usage.py

Running AWSY on Try
********************

AWSY runs can be generated through the mach try fuzzy finder:

.. code-block:: bash

    ./mach try fuzzy

A query for "awsy" will return all AWSY tests. The default test is TP5, TP6 and Base test names will contain `tp6` and `base`, respectively.

The following documents all tests we currently run for AWSY.
*The following content was migrated from* https://wiki.mozilla.org/AWSY/Tests *and will be updated to clarify TP5/TP6 tests vs Base tests:* https://bugzilla.mozilla.org/show_bug.cgi?id=1714600


TP5/TP6 Tests
**************

The following tests exist for both TP5 and TP6. Running `./mach awsy-test` by default will run TP5 tests*.
To run TP6 tests, add the `--tp6` flag: `./mach awsy-test --tp6`

*\*TP5 tests are out of date and no longer maintained. These tests are scheduled to be removed:* https://bugzilla.mozilla.org/show_bug.cgi?id=1712406


Explicit Memory
================

* This is memory explicitly reported by a memory reporter. It includes all the memory allocated via explicit calls to heap allocation functions (such as malloc and new), and some (only that covered by memory reporters) of the memory allocated via explicit calls to non-heap allocations functions (such as mmap and VirtualAlloc).

**Possible regression causes**

* A regression in this usually means a new feature is using or retaining more memory and should be looked at. These are easier to diagnose as we can compare memory reports.

See the `about:memory` `mdn page <https://developer.mozilla.org/docs/Mozilla/Performance/about:memory#Explicit_Allocations>`__ for more details.


Heap Unclassified
==================

*to do: add test definition*


Images
=======

* This is a subset of the "explicit" measurement that focuses on memory used to render images.

**Possible regression causes**

* A regressions in this can indicate leaks or poor memory usage in the image subsystem. In the past this was persistent problem.


JS
====

*to do: add test definition*


Resident Memory
================

* This is a higher level measurement provided by the operating system. We sum the "resident" memory (`RSS <https://en.wikipedia.org/wiki/Resident_set_size>`_) with the `resident-unique <https://en.wikipedia.org/wiki/Unique_set_size>`_ memory of the content processes. It's pretty noisy and large so it's not very useful in detecting smaller regressions.

**Possible regression causes**

* Regressions in this often track regressions in explicit and heap unclassified. If we see a regression in resident, but not in other reports this can indicate we are leaking untracked memory (perhaps through shared memory, graphics allocations, file handles, etc).


Base Content Tests
*******************

* An updated test focused on supporting Fission. This measures the base overhead of an empty content process. It tracks resident unique, heap unclassified, JS, and explicit memory metrics as well as storing full memory reports as artifacts. The median value for each metric is used from across all content processes. It has much lower thresholds for alerting and is recorded in `Perfherder <https://wiki.mozilla.org/EngineeringProductivity/Projects/Perfherder>`_.


Base Content Explicit
======================

**Possible regression causes**

A change has caused more JavaScript to load at startup or into blank pages

* **Common solution**: lazily load any new modules you rely on
* **Common solution**: Split your code out to only load what is minimally needed initially. You modified the JS engine and it's using more memory
* **Common solution**: Attempt to reduce your object size for the common case, these tend to add up! You implemented a new feature in JavaScript
* **Common solution**: Write the majority (or all of it) in compiled code (C++/Rust). This will reduce overhead and generally improve performance.


Base Content Heap Unclassified
===============================

* The "heap-unclassified" value represents heap-allocated memory that is not measured by any memory reporter. This is typically 10--20% of "explicit".


**Possible regression causes**

* A regression in this can indicate that we're leaking memory or that additional memory reporters should be added.
* An improvement can indicate that leaks have been fixed or that we added new memory reporters.

See the `about:memory` `mdn page <https://developer.mozilla.org/docs/Mozilla/Performance/about:memory#Explicit_Allocations>`__ for more details.


Base Content JS
================

* This is the "js-main-runtime/" value in `about:memory` which is all the memory attributed to the javascript engine.

**Possible regression causes**

* A regression in this number can indicate leaks in the JS engine, optimizations that take performance into consideration at the expense of more memory, or problems with the garbage collector.


Base Content Resident Unique Memory
====================================

*to do: add test definition*


Other references
-----------------

`Are We Slim Yet MDN web docs <https://developer.mozilla.org/en-US/docs/Mozilla/Performance/AWSY>`_
