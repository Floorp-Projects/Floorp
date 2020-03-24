
.. _univ.Choice:

.. |ASN.1| replace:: Choice

|ASN.1| type
------------

.. autoclass:: pyasn1.type.univ.Choice(componentType=None, tagSet=tagSet(), subtypeSpec=ConstraintsIntersection())
   :members: isValue, isSameTypeWith, isSuperTypeOf, tagSet, effectiveTagSet, tagMap, componentType, subtypeSpec,
             getComponentByPosition, setComponentByPosition, getComponentByName, setComponentByName, setDefaultComponents,
             getComponentByType, setComponentByType, getName, getComponent, isInconsistent

   .. note::

        The |ASN.1| type can only hold a single component at a time belonging to the list of allowed types.

   .. automethod:: pyasn1.type.univ.Choice.clone(componentType=None, tagSet=tagSet(), subtypeSpec=ConstraintsIntersection())
   .. automethod:: pyasn1.type.univ.Choice.subtype(componentType=None, implicitTag=Tag(), explicitTag=Tag(),subtypeSpec=ConstraintsIntersection())
