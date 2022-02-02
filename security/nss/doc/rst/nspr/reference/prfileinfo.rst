File information structure used with ``PR_GetFileInfo`` and
``PR_GetOpenFileInfo``.

.. _Syntax:

Syntax
~~~~~~

.. code:: eval

   #include <prio.h>

   struct PRFileInfo {
      PRFileType type;
      PRUint32 size;
      PRTime creationTime;
      PRTime modifyTime;
   };

   typedef struct PRFileInfo PRFileInfo;

 

.. _Fields:

Fields
~~~~~~

The structure has the following fields:

``type``
   Type of file. See ``PRFileType``.
``size``
   Size, in bytes, of file's contents.
``creationTime``
   Creation time per definition of ``PRTime``. See
   `prtime.h <https://dxr.mozilla.org/mozilla-central/source/nsprpub/pr/include/prtime.h>`__.
``modifyTime``
   Last modification time per definition of ``PRTime``. See
   `prtime.h <https://dxr.mozilla.org/mozilla-central/source/nsprpub/pr/include/prtime.h>`__.

.. _Description:

Description
-----------

The ``PRFileInfo`` structure provides information about a file, a
directory, or some other kind of file system object, as specified by the
``type`` field.
