
.. _type.base:

ASN.1 type system
-----------------

The ASN.1 language defines a collection of data types such as *INTEGER*
or *SET*. With pyasn1, ASN.1 types are represented by Python classes.
The base classes are described in this part of the documentation.

User code might not need to use them directly, except for figuring out
if given object belongs to ASN.1 type or not.

.. toctree::
   :maxdepth: 2

   /pyasn1/type/base/asn1type
   /pyasn1/type/base/simpleasn1type
   /pyasn1/type/base/constructedasn1type
   /pyasn1/type/base/novalue
