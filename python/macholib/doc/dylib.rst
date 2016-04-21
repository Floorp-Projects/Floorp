:mod:`macholib.dylib` --- Generic dylib path manipulation
=========================================================

.. module:: macholib.dylib
   :synopsis: Generic dylib path manipulation

This module defines a function :func:`dylib_info` that can extract
useful information from the name of a dynamic library.

.. function:: dylib_info(filename)

   A dylib name can take one of the following four forms:

   * ``Location/Name.SomeVersion_Suffix.dylib``

   * ``Location/Name.SomeVersion.dylib``

   * ``Location/Name_Suffix.dylib``

   * ``Location/Name.dylib``

   Returns None if not found or a mapping equivalent to::
   
     dict(
         location='Location',
         name='Name.SomeVersion_Suffix.dylib',
         shortname='Name',
         version='SomeVersion',
         suffix='Suffix',
     )

   .. note:: *SomeVersion* and *Suffix* are optional and my be ``None``
      if not present.
