.. _reject-some-requires:

====================
reject-some-requires
====================

Rule Details
------------

This takes an option, a regular expression.  Invocations of
``require`` with a string literal argument are matched against this
regexp; and if it matches, the ``require`` use is flagged.
