
.. _type.opentype:

Dynamic or open type
--------------------

ASN.1 allows data structure designer to leave "holes" in field type
specification of :ref:`Sequence <univ.Sequence>` or
:ref:`Set <univ.Set>` types.

The idea behind that feature is that there can be times, when the
exact field type is not known at the design time, or it is anticipated
that new field types may come up in the future.

This "hole" type is manifested in the data structure by :ref:`Any <univ.Any>`
type. Technically, the actual type is serialized into an octet stream
and then put into :ref:`Any <univ.Any>` "container", which is in fact an
(untagged, by default) specialization of ASN.1
:ref:`OctetString <univ.OctetString>` type.

.. code-block:: bash

       Algorithm ::= SEQUENCE {
               algorithm OBJECT IDENTIFIER,
               parameters ANY DEFINED BY algorithm OPTIONAL
       }

On the receiving end, to know how to interpret the open type
serialization, the receiver can rely on the supplied value in
the other field of the data structure. That other field is
semantically linked with the open type field. This link
is expressed in ASN.1 by the *DEFINE BY* clause.

From ASN.1 perspective, it is not an error if the decoder does
not know a type selector value it receives. In that case pyasn1 decoder
just leaves serialized blob in the open type field.

.. note::

   By default, ASN.1 ANY type has no tag. That makes it an
   "invisible" in serialization. However, like any other ASN.1 type,
   ANY type can be subtyped by :ref:`tagging <type.tag>`.

Besides scalar open type fields, ASN.1 allows using *SET OF*
or *SEQUENCE OF* containers holding zero or more of *ANY*
scalars.

.. code-block:: bash

   AttributeTypeAndValues ::= SEQUENCE {
      type OBJECT IDENTIFIER,
      values SET OF ANY DEFINED BY type
   }

.. note::

   A single type selector field is used to guide the decoder
   of potentially many elements of a *SET OF* or *SEQUENCE OF* container
   all at once. That implies that all *ANY* elements must be of the same
   type in any given instance of a data structure.

When expressing ASN.1 type "holes" in pyasn1, the
:ref:`OpenType <opentype.OpenType>` object should be used to establish
a semantic link between type selector field and open type field.

.. code-block:: python

   algo_map = {
       ObjectIdentifier('1.2.840.113549.1.1.1'): rsaEncryption(),
       ObjectIdentifier('1.2.840.113549.1.1.2'): md2WithRSAEncryption()
   }


   class Algorithm(Sequence):
       """
       Algorithm ::= SEQUENCE {
               algorithm OBJECT IDENTIFIER,
               parameters ANY DEFINED BY algorithm
       }
       """
       componentType = NamedTypes(
           NamedType('algorithm', ObjectIdentifier()),
           NamedType('parameters', Any(),
                     openType=OpenType('algorithm', algo_map))
       )

Similarly for `SET OF ANY DEFINED BY` or `SEQUENCE OF ANY DEFINED BY`
constructs:

.. code-block:: python

   algo_map = {
       ObjectIdentifier('1.2.840.113549.1.1.1'): rsaEncryption(),
       ObjectIdentifier('1.2.840.113549.1.1.2'): md2WithRSAEncryption()
   }


   class Algorithm(Sequence):
       """
       Algorithm ::= SEQUENCE {
               algorithm OBJECT IDENTIFIER,
               parameters SET OF ANY DEFINED BY algorithm
       }
       """
       componentType = NamedTypes(
           NamedType('algorithm', ObjectIdentifier()),
           NamedType('parameters', SetOf(componentType=Any()),
                     openType=OpenType('algorithm', algo_map))
       )

.. toctree::
   :maxdepth: 2

   /pyasn1/type/opentype/opentype
