.. _mozilla_projects_nss_nss_third-party_code:

NSS Third-Party Code
====================

.. container::

   This is a list of third-party code included in the NSS repository, broken into two lists: Code
   that can be compiled into the NSS libraries, and code that is only used for testing.

   Note that not all code that can be compiled into the NSS libraries necessarily *is*. Often this
   is configurable at build time, with various trade-offs.

.. _compiled_in:

`Compiled In <#compiled_in>`__
------------------------------

.. container::

   -  sqlite [/lib/sqlite]
   -  BerkleyDB [/lib/dbm]
   -  zlib [/lib/zlib]
   -  libjar [/lib/jar]
   -  Fiat-Crypto, Ring [lib/freebl/ecl]

.. _used_for_tests:

`Used for Tests <#used_for_tests>`__
------------------------------------

.. container::

   -  GTest [/gtests]

.. _downloaded_by_certain_test_tooling:

`Downloaded by certain test tooling <#downloaded_by_certain_test_tooling>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  TLSFuzzer [/tests/tlsfuzzer]
   -  BoGo tests [/tests/bogo]
   -  BoringSSL, OpenSSL [/tests/interop]