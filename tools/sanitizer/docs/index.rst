Sanitizer
=========

.. toctree::
  :maxdepth: 1
  :hidden:
  :glob:

  *

**Address Sanitizer**

Address Sanitizer (ASan) is a fast memory error detector that detects use-after-free and out-of-bound bugs in C/C++ programs. It uses a compile-time instrumentation to check all reads and writes during the execution. In addition, the runtime part replaces the malloc and free functions to check dynamically allocated memory.

:ref:`More information <Address Sanitizer>`

**Thread Sanitizer**

Thread Sanitizer (TSan) is a fast data race detector for C/C++ programs. It uses a compile-time instrumentation to check all non-race-free memory access at runtime. Unlike other tools, it understands compiler-builtin atomics and synchronization and therefore provides very accurate results with no false positives (except if unsupported synchronization primitives like inline assembly or memory fences are used).

:ref:`More information <Thread Sanitizer>`

**Memory Sanitizer**

Memory Sanitizer (MSan) is a fast detector used for uninitialized memory in C/C++ programs. It uses a compile-time instrumentation to ensure that all memory access at runtime uses only memory that has been initialized.

:ref:`More information <Memory Sanitizer>`

**ASan Nightly Project**

The ASan Nightly Project involves building a Firefox Nightly browser with the popular AddressSanitizer tool and enhancing it with remote crash reporting capabilities for any errors detected.

:ref:`More information <ASan Nightly>`
