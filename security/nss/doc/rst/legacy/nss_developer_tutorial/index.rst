.. _mozilla_projects_nss_nss_developer_tutorial:

NSS Developer Tutorial
======================

.. _nss_coding_style:

`NSS Coding Style <#nss_coding_style>`__
----------------------------------------

.. container::

`Formatting <#formatting>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   **Line length** should not exceed 80 characters.

   **Indentation level** is 4.

   **Tabs** are used heavily in many NSS source files. Try to stay consistent when you modify
   existing code. The proper use of tabs has often been confusing for new NSS developers, so in
   ``nss/lib/ssl``, we're gradually removing the use of tabs.

   **Curly braces**: both of the following styles are allowed:

   .. code:: brush:

      if (condition) {
          action1();
      } else {
          action2();
      }

   Or:

   .. code:: brush:

      if (condition)
      {
          action1();
      }
      else
      {
          action2();
      }

   The former style is more common. When modifying existing code, try to stay consistent. In new
   code, prefer the former style, as it conserves vertical space.

   When a block of code consists of a single statement, NSS doesn’t require curly braces, so both of
   these examples are fine:

   .. code:: brush:

      if (condition) {
          action();
      }

   Or:

   .. code:: brush:

      if (condition)
          action();

   although the use of curly braces is more common.

   **Multiple-line comments** should be formatted as follows:

   .. code:: brush:

      /*
       * Line1
       * Line2
       */

   or

   .. code:: brush:

      /*
      ** Line 1
      ** Line 2
      */

   The following styles are also common, because they conserve vertical space:

   .. code:: brush:

      /* Line1
       * Line2
       */

   or

   .. code:: brush:

      /* Line1
      ** Line2
      */

   or

   .. code:: brush:

      /* Line1
       * Line2 */

`Naming <#naming>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   Public functions are named ``FOO_DoOneAction``.

   Global, but unexported functions, are usually named ``foo_DoOneAction``.

   Variable, and function parameter names, always start with a lowercase letter. The most common
   style is ``fooBarBaz``, although ``foobarbaz`` and ``foo_bar_baz`` are also used.

`Miscellaneous <#miscellaneous>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   **goto** can be used, to simplify resource deallocation, before returning from a function.

   A data buffer is usually represented as:

   .. code:: brush:

      unsigned char *data;
      unsigned int len;

   The buffer pointer is ``unsigned char *``, as opposed to ``void *``, so we can perform pointer
   arithmetic without casting. Use ``char *`` only if the data is interpreted as text characters.

   For historical reasons, the buffer length is ``unsigned int``, as opposed to ``size_t``.
   Unfortunately, this can be a source of integer overflow bugs on 64-bit systems.

.. _c_features:

`C Features <#c_features>`__
----------------------------

.. container::

   NSS requires C99.  However, not all features from C99 are equally available.

   -  Variables can be declared, at the point they are first used.
   -  The ``inline`` keyword can be used.
   -  Variadic macro arguments are permitted, but their use should be limited to using
      ``__VA_ARGS__``.
   -  The exact-width integer types in NSPR should be used, in preference to those declared in
      ``<stdint.h>`` (which will be used by NSPR in the future).
   -  Universal character names are not permitted, as are wide character types (``char16_t`` and
      ``char32_t``).  NSS source should only include ASCII text.  Escape non-printing characters
      (with ``\x`` if there is no special escape such as \\r, \\n, and \\t) and avoid defining
      string literals that use non-ASCII characters.
   -  One line comments starting with ``//`` are permitted.

   Check with nss-dev@ before using a language feature not already used, if you are uncertain.
   Please update this list if you do.

   These restrictions are different for C++ unit tests, which can use most C++11 features.  The
   `Mozilla C++ language features
   guide <https://developer.mozilla.org/en-US/docs/Using_CXX_in_Mozilla_code>`__, and the `Chromium
   C++ usage guide <https://chromium-cpp.appspot.com/>`__, list C++ features that are known to be
   widely available and compatible. You should limit features to those that appear in both guides.
   Ask on nss-dev@ if you think this is restrictive, or if you wish to prohibit a specific feature.

.. _nss_c_abi_backward_compatibility:

`NSS C ABI backward compatibility <#nss_c_abi_backward_compatibility>`__
------------------------------------------------------------------------

.. container::

`Functions <#functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Exported functions cannot be removed.

   The function prototype of an exported function, cannot be changed, with these exceptions:

   -  A ``Foo *`` parameter can be changed to ``const Foo *``. This change is always safe.

   -  Sometimes an ``int`` parameter can be changed to ``unsigned int``, or an ``int *`` parameter
      can be changed to ``unsigned int *``. Whether such a change is safe needs to be reviewed on a
      case-by-case basis.

`Types <#types>`__
------------------

.. container::

`Structs <#structs>`__
~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Members of an exported struct, cannot be reordered or removed.

   Under certain circumstances, it is safe to add new members to an exported struct at the end.

   Opaque structs give us complete freedom to change them, but require applications to call NSS
   functions, to allocate and free them.

`Enums <#enums>`__
~~~~~~~~~~~~~~~~~~

.. container::

   The numeric values of public enumerators cannot be changed. To stress this fact, we often
   explicitly assign numeric values to enumerators, rather than relying on the values assigned by
   the compiler.

.. _symbol_export_lists:

`Symbol export lists <#symbol_export_lists>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The ``manifest.mn`` file, in a directory in the NSS source tree, specifies which headers are
   public, and which headers are private.

   Public headers are in the ``EXPORTS`` variable.

   Private headers,which may be included by files in other directories, are in the
   ``PRIVATE_EXPORTS`` variable.

   Private headers, that are only included by files in the same directory, are not listed in either
   variable.

   Only functions listed in the symbol export lists (``nss.def``, ``ssl.def``, ``smime.def``, etc.)
   are truly public functions. Unfortunately, public headers may declare private functions, for
   historical reasons. The symbol export lists are the authoritative source of public functions.

.. _behavioral_changes:

`Behavioral changes <#behavioral_changes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   **Bug/quirk compatible**: Occasionally we cannot fix a bug, because applications may depend on
   the buggy behavior. We would need to add a new function to provide the desired behavior.

   Similarly, **new options** often need to be disabled by default.

.. _nss_reviewfeature_approval_process:

`NSS review/feature approval process <#nss_reviewfeature_approval_process>`__
-----------------------------------------------------------------------------

.. container::

   NSS doesn’t have 'super reviewers'. We wish to increase the number of NSS developers, who have
   broad understanding of NSS.

   One review is usually enough for the review to pass. For critical code reviews, such as a patch
   release of a stable branch, two reviews may be more reasonable.

   For new features, especially those that appear controversial, try to find a reviewer from a
   different company or organization than your own, to avoid any perceptions of bias.

.. _update_nss_in_mozilla-inbound_and_mozilla-central:

`Update NSS in mozilla-inbound and mozilla-central <#update_nss_in_mozilla-inbound_and_mozilla-central>`__
----------------------------------------------------------------------------------------------------------

.. container::

   The procedure is documented at
   `https://developer.mozilla.org//en-US/docs/Mozilla/Developer_guide/Build_Instructions/Updating_NSPR_or_NSS_in_mozilla-central <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Updating_NSPR_or_NSS_in_mozilla-central>`__.

   If it is necessary to apply private patches, please document them in
   ``<tree>/security/patches/README``.