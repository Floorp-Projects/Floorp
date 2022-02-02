Layer-dependent implementation data.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   typedef struct PRFilePrivate PRFilePrivate;

.. _Description:

Description
-----------

A layer implementor should collect all the private data of the layer in
the ``PRFilePrivate`` structure. Each layer has its own definition of
``PRFilePrivate``, which is hidden from other layers as well as from the
users of the layer.
