Sanitizer
=========

.. toctree::
  :maxdepth: 1
  :hidden:
  :glob:

  *

**Address Sanitizer**

Address Sanitizer (ASan) is a fast memory error detector that detects use-after-free and out-of-bound bugs in C/C++ programs. It uses a compile-time instrumentation to check all reads and writes during the execution. In addition, the runtime part replaces the malloc and free functions to check dynamically allocated memory. More information on how ASan works can be found on the Address Sanitizer wiki.

:ref:`More information <Address Sanitizer>`

**ASan Nightly Project**

The ASan Nightly Project involves building a Firefox Nightly browser with the popular AddressSanitizer tool and enhancing it with remote crash reporting capabilities for any errors detected.

:ref:`More information <ASan Nightly>`
