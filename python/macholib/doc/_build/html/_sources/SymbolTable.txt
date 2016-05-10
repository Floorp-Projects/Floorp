:mod:`macholib.SymbolTable` --- Class to read the symbol table from a Mach-O header
===================================================================================

.. module:: macholib.SymbolTable
   :synopsis: Class to read the symbol table from a Mach-O header

This module is deprecated because it is not by the author
and likely contains bugs. It also does not work for 64-bit binaries.

.. class:: SymbolTable(macho[, openfile])

   Reads the SymbolTable for the given Mach-O object.

   The option argument *openfile* specifies the 
   function to use to open the file, defaulting to
   the builtin :func:`open` function.

   .. warning:: As far as we know this class is not used
      by any user of the modulegraph package, and the code
      has not been updated after the initial implementation.

      The end result of this is that the code does not
      support 64-bit code at all and likely doesn't work
      properly for 32-bit code as well.
