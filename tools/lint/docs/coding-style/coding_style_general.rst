================================
Coding style - General practices
================================

-  Don't put an ``else`` right after a ``return`` (or a ``break``).
   Delete the ``else``, it's unnecessary and increases indentation
   level.
-  Don't leave debug ``printf``\ s or ``dump``\ s lying around.
-  Use `JavaDoc-style
   comments <https://www.oracle.com/technetwork/java/javase/documentation/index-137868.html>`__.
-  When fixing a problem, check to see if the problem occurs elsewhere
   in the same file, and fix it everywhere if possible.
-  End the file with a newline. Make sure your patches don't contain
   files with the text "no newline at end of file" in them.
-  Declare local variables as near to their use as possible.
-  For new files, be sure to use the right `license
   boilerplate <https://www.mozilla.org/MPL/headers/>`__, per our
   `license policy <https://www.mozilla.org/MPL/license-policy.html>`__.

Mode line
~~~~~~~~~

Files should have Emacs and vim mode line comments as the first two
lines of the file, which should set ``indent-tabs-mode`` to ``nil``. For new
files, use the following, specifying two-space indentation:

.. code-block:: cpp

   /* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
   /* This Source Code Form is subject to the terms of the Mozilla Public
    * License, v. 2.0. If a copy of the MPL was not distributed with this
    * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

Be sure to use the correct ``Mode`` in the first line, don't use ``C++`` in
JavaScript files.
