========
AWSY
========
Are We Slim Yet project (commonly known as AWSY) tracks memory usage across builds.

On treeherder, the AWSY builds are listed in subgroups of `SY`.

==================
Test Descriptions
==================

----------------
Base Content JS
----------------

* An updated test focused on supporting fission. This measures the base overhead of an empty content process. It tracks resident unique, heap unclassified, JS, and explicit memory metrics as well as storing full memory reports as artifacts. The median value for each metric is used from across all content processes. It has much lower thresholds for alerting and is recorded in `Perfherder <https://wiki.mozilla.org/EngineeringProductivity/Projects/Perfherder>`_.

^^^^^^^^^^^^^^^^^^^^^^^^^^^
Possible regression causes
^^^^^^^^^^^^^^^^^^^^^^^^^^^

A change has caused more JavaScript to load at startup or into blank pages

* **Common solution**: lazily load any new modules you rely on
* **Common solution**: Split your code out to only load what is minimally needed initially. You modified the JS engine and it's using more memory
* **Common solution**: Attempt to reduce your object size for the common case, these tend to add up! You implemented a new feature in JavaScript
* **Common solution**: Write the majority (or all of it) in compiled code (C++/Rust). This will reduce overhead and generally improve performance.

------------------------
Explicit Memory summary
------------------------

* This is memory explicitly reported by a memory reporter. It includes all the memory allocated via explicit calls to heap allocation functions (such as malloc and new), and some (only that covered by memory reporters) of the memory allocated via explicit calls to non-heap allocations functions (such as mmap and VirtualAlloc).

^^^^^^^^^^^^^^^^^^^^^^^^^^^
Possible regression causes
^^^^^^^^^^^^^^^^^^^^^^^^^^^

* A regression in this usually means a new feature is using or retaining more memory and should be looked at. These are easier to diagnose as we can compare memory reports.

See the `about:memory` `mdn page <https://developer.mozilla.org/docs/Mozilla/Performance/about:memory#Explicit_Allocations>`_ for more details.

--------------------------
Heap Unclassified summary
--------------------------

* The "heap-unclassified" value represents heap-allocated memory that is not measured by any memory reporter. This is typically 10--20% of "explicit".


^^^^^^^^^^^^^^^^^^^^^^^^^^^
Possible regression causes
^^^^^^^^^^^^^^^^^^^^^^^^^^^
* A regression in this can indicate that we're leaking memory or that additional memory reporters should be added.
* An improvement can indicate that leaks have been fixed or that we added new memory reporters.

See the `about:memory` `mdn page <https://developer.mozilla.org/docs/Mozilla/Performance/about:memory#Explicit_Allocations>`_ for more details.

---------------
Images summary
---------------

* This is a subset of the "explicit" measurement that focuses on memory used to render images.

^^^^^^^^^^^^^^^^^^^^^^^^^^^
Possible regression causes
^^^^^^^^^^^^^^^^^^^^^^^^^^^

* A regressions in this can indicate leaks or poor memory usage in the image subsystem. In the past this was persistent problem.

---------------
JS summary
---------------

* This is the "js-main-runtime/" value in `about:memory` which is all the memory attributed to the javascript engine.

^^^^^^^^^^^^^^^^^^^^^^^^^^^
Possible regression causes
^^^^^^^^^^^^^^^^^^^^^^^^^^^

* A regression in this number can indicate leaks in the JS engine, optimizations that take performance into consideration at the expense of more memory, or problems with the garbage collector.

------------------------
Resident Memory summary
------------------------

* This is a higher level measurement provided by the operating system. We sum the "resident" memory (`RSS <https://en.wikipedia.org/wiki/Resident_set_size>`_) with the `resident-unique <https://en.wikipedia.org/wiki/Unique_set_size>`_ memory of the content processes. It's pretty noisy and large so it's not very useful in detecting smaller regressions.

^^^^^^^^^^^^^^^^^^^^^^^^^^^
Possible regression causes
^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Regressions in this often track regressions in explicit and heap unclassified. If we see a regression in resident, but not in other reports this can indicate we are leaking untracked memory (perhaps through shared memory, graphics allocations, file handles, etc).

------------------
Other references
------------------

`Are We Slim Yet MDN web docs <https://developer.mozilla.org/en-US/docs/Mozilla/Performance/AWSY>`_
