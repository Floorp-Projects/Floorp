:mod:`macholib.framework` --- Generic framework path manipulation
==========================================================================

.. module:: macholib.framework
   :synopsis: Generic framework path manipulation


This module defines a function :func:`framework_info` that can extract
useful information from the name of a dynamic library in a framework.

.. function:: framework_info(filename)

   A framework name can take one of the following four forms:

   * ``Location/Name.framework/Versions/SomeVersion/Name_Suffix``

   * ``Location/Name.framework/Versions/SomeVersion/Name``

   * ``Location/Name.framework/Name_Suffix``

   * ``Location/Name.framework/Name``

   Returns ``None`` if not found, or a mapping equivalent to::

      dict(
          location='Location',
          name='Name.framework/Versions/SomeVersion/Name_Suffix',
          shortname='Name',
          version='SomeVersion',
          suffix='Suffix',
      )

  .. note:: *SomeVersion* and *Suffix* are optional and may be None
     if not present.
