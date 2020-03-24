
.. _univ.Set:

.. |ASN.1| replace:: Set

|ASN.1| type
------------

.. autoclass:: pyasn1.type.univ.Set(componentType=NamedTypes(), tagSet=TagSet(), subtypeSpec=ConstraintsIntersection())
   :members: isValue, isSameTypeWith, isSuperTypeOf, tagSet, effectiveTagSet, tagMap, componentType, subtypeSpec,
             getComponentByPosition, setComponentByPosition, getComponentByName, setComponentByName, setDefaultComponents,
             getComponentByType, setComponentByType, clear, reset, isInconsistent

   .. note::

        The |ASN.1| type models a collection of named ASN.1 components.
        Ordering of the components **is not** preserved upon de/serialisation.

   .. automethod:: pyasn1.type.univ.Set.clone(componentType=NamedTypes(), tagSet=TagSet(), subtypeSpec=ConstraintsIntersection())
   .. automethod:: pyasn1.type.univ.Set.subtype(componentType=NamedTypes(), implicitTag=Tag(), explicitTag=Tag(),subtypeSpec=ConstraintsIntersection())
