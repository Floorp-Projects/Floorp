:mod:`macholib.dyld` --- Dyld emulation
=======================================

.. module:: macholib.dyld
   :synopsis: Emulation of functonality of the dynamic linker

This module defines a number of functions that can be used
to emulate the functionality of the dynamic linker (``dyld``)
w.r.t. looking for library files and framworks.

.. function:: dyld_image_suffix([env])

   Looks up the suffix to append to shared library and
   framework names and returns this value when found.
   Returns ``None`` when no suffix should be appended.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

   See the description of ``DYLD_IMAGE_SUFFIX`` in the
   manual page for dyld(1) for more information.

.. function:: dydl_framework_path([env])

   Returns a user-specified framework search path,
   or an empty list when only the default search path
   should be used.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

   See the description of ``DYLD_FRAMEWORK_PATH`` in the
   manual page for dyld(1) for more information.

.. function:: dyld_library_path([env])

   Returns a user-specified library search path,
   or an empty list when only the default search path
   should be used.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

   See the description of ``DYLD_LIBRARY_PATH`` in the
   manual page for dyld(1) for more information.

.. function:: dyld_fallback_framework_path([env])

   Return a user specified list of of directories where
   to look for frameworks that aren't in their install path,
   or an empty list when the default fallback path should
   be  used.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

   See the description of ``DYLD_FALLBACK_FRAMEWORK_PATH`` in the
   manual page for dyld(1) for more information.

.. function:: dyld_fallback_library_path([env])

   Return a user specified list of of directories where
   to look for libraries that aren't in their install path,
   or an empty list when the default fallback path should
   be  used.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

   See the description of ``DYLD_FALLBACK_LIBRARY_PATH`` in the
   manual page for dyld(1) for more information.

.. function:: dyld_image_suffix_search(iterator[, env])

   Yields all items in *iterator*, and prepents names
   with the image suffix to those items when the suffix
   is specified.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

.. function:: dyld_override_search(name[, env])

   If *name* is a framework name yield filesystem
   paths relative to the entries in the framework
   search path.

   Always yield the filesystem paths relative to the
   entries in the library search path.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

.. function:: dyld_executable_path_search(name, executable_path)

   If *name* is a path starting with ``@executable_path/`` yield
   the path relative to the specified *executable_path*.

   If *executable_path* is None nothing is yielded.

.. function:: dyld_loader_search(name, loader_path)

   If *name* is a path starting with ``@loader_path/`` yield
   the path relative to the specified *loader_path*.

   If *loader_path* is None nothing is yielded.

   .. versionadded: 1.6

.. function:: dyld_default_search(name[, env])

   Yield the filesystem locations to look for a dynamic
   library or framework using the default locations
   used by the system dynamic linker.

   This function will look in ``~/Library/Frameworks``
   for frameworks, even though the system dynamic linker
   doesn't.

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

.. function:: dyld_find(name[, executable_path[, env [, loader]]])

   Returns the path of the requested dynamic library,
   raises :exc:`ValueError` when the library cannot be found.

   This function searches for the library in the same
   locations and de system dynamic linker.

   The *executable_path* should be the filesystem path
   of the executable to which the library is linked (either
   directly or indirectly).

   The *env* argument is a dictionary, which defaults
   to :data:`os.environ`.

   The *loader_path* argument is an optional filesystem path for
   the object file (binary of shared library) that references
   *name*.

   .. versionchanged:: 1.6

      Added the *loader_path* argument.

.. function:: framework_find(fn[, executable_path[, env]])

   Find a framework using the same semantics as the
   system dynamic linker, but will accept looser names
   than the system linker.

   This function will return a correct result for input
   values like:

   * Python

   * Python.framework

   * Python.framework/Versions/Current
