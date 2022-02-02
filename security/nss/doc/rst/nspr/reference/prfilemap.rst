Type returned by ``PR_CreateFileMap`` and passed to ``PR_MemMap`` and
``PR_CloseFileMap``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   typedef struct PRFileMap PRFileMap;

.. _Description:

Description
-----------

The opaque structure ``PRFileMap`` represents a memory-mapped file
object. Before actually mapping a file to memory, you must create a
memory-mapped file object by calling ``PR_CreateFileMap``, which returns
a pointer to ``PRFileMap``. Then sections of the file can be mapped into
memory by passing the ``PRFileMap`` pointer to ``PR_MemMap``. The
memory-mapped file object is closed by passing the ``PRFileMap`` pointer
to ``PR_CloseFileMap``.
