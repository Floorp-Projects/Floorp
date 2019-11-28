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
