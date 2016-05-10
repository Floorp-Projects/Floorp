:mod:`macholib.MachOStandalone` --- Create standalone application bundles
==========================================================================

.. module:: macholib.MachOStandalone
   :synopsis: Create standalone application bundles

This module defines class :class:`MachOStandalone` which locates
all Mach-O files in a directory (assumed to be the root of an
application or plugin bundle) and then copies all non-system 
dependencies for the located files into the bundle.

.. class:: MachOStandalone(base[, dest[, graph[, env[, executable_path]]]])

