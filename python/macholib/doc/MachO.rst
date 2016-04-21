:mod:`macholib.MachO` --- Utilities for reading and writing Mach-O headers
==========================================================================

.. module:: macholib.MachO
   :synopsis: Utilities for reading and writing Mach-O headers

This module defines a class :class:`Macho`, which enables reading
and writing the Mach-O header of an executable file or dynamic
library on MacOS X.

.. class:: MachO(filename)

   Creates a MachO object by reading the Mach-O headers from
   *filename*.

   The *filename* should refer to an existing file in Mach-O
   format, and can refer to fat (universal) binaries.

.. note:: more information will be added later
