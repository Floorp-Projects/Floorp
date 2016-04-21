Macholib - Analyze and edit Mach-O headers
==========================================

macholib can be used to analyze and edit Mach-O headers, the executable
format used by Mac OS X.

It's typically used as a dependency analysis tool, and also to rewrite dylib
references in Mach-O headers to be ``@executable_path`` relative.

Though this tool targets a platform specific file format, it is pure python
code that is platform and endian independent.

General documentation
---------------------

.. toctree::
   :maxdepth: 1

   changelog
   license
   scripts

Reference Guide
---------------

.. toctree::
   :maxdepth: 1

   MachO
   MachoOGraph
   MachoOStandalone
   SymbolTable
   dyld
   dylib
   framework
   macho_o
   ptypes

Online Resources
----------------

* `Sourcecode repository on bitbucket <http://bitbucket.org/ronaldoussoren/macholib/>`_

* `The issue tracker <http://bitbucket.org/ronaldoussoren/macholib/issues>`_

* `Mac OS X ABI Mach-O File Format Reference at Apple <http://developer.apple.com/library/mac/#documentation/DeveloperTools/Conceptual/MachORuntime/Reference/reference.html>`_

Contributors
------------

Macholib was written by Bob Ippolito and is currently maintained by Ronald Oussoren <ronaldoussoren@mac.com>.

Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

