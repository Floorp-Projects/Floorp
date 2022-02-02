Guaranteed to be an unsigned 64-bit integer on all platforms.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtypes.h>

   typedef definition PRUint64;

.. _Description:

Description
-----------

May be defined in several different ways, depending on the platform. For
syntax details for each platform, see
`prtypes.h <https://dxr.mozilla.org/mozilla-central/source/nsprpub/pr/include/prtypes.h>`__.

.. _Notes:

Notes
-----

.. note::

   **Note:** Prior to Gecko 12.0, ``PRUint64`` was actually treated as a
   signed 64-bit integer by
   `XPConnect </en-US/docs/Mozilla/Tech/XPCOM/Language_bindings/XPConnect>`__.
   This has been fixed.

 
