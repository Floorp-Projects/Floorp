
.. _univ.SequenceOf:

.. |ASN.1| replace:: SequenceOf

|ASN.1| type
------------

.. autoclass:: pyasn1.type.univ.SequenceOf(componentType=NoValue(), tagSet=TagSet(), subtypeSpec=ConstraintsIntersection())
   :members: isValue, isSameTypeWith, isSuperTypeOf, tagSet, effectiveTagSet, tagMap, componentType, subtypeSpec,
             getComponentByPosition, setComponentByPosition, clear, reset, isInconsistent

   .. note::

       The |ASN.1| type models a collection of elements of a single ASN.1 type.
       Ordering of the components **is** preserved upon de/serialisation.
        
   .. automethod:: pyasn1.type.univ.SequenceOf.clone(componentType=NoValue(), tagSet=TagSet(), subtypeSpec=ConstraintsIntersection())
   .. automethod:: pyasn1.type.univ.SequenceOf.subtype(componentType=NoValue(), implicitTag=Tag(), explicitTag=Tag(),subtypeSpec=ConstraintsIntersection())
