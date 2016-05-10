Command-line tools
==================

python -m macholib find
-----------------------

Usage::

        $ python -mmacholib find dir...

Print the paths of all MachO binaries
in the specified directories.

python -m macholib standalone
-----------------------------

Usage::

        $ python -m macholib standalone appbundle...

Convert one or more application bundles into 
standalone bundles. That is, copy all non-system
shared libraries and frameworks used by the bundle
into the bundle and rewrite load commands.

python -mmacholib dump
----------------------

Usage::
        
        $ python -mmacholib dump dir...

Prints information about all architectures in a 
Mach-O file as well as all libraries it links 
to.
