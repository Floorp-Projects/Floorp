.. _no-single-arg-cu-import:

=======================
no-single-arg-cu-import
=======================

Rule Details
------------

Rejects calls to "Cu.import" that do not supply a second argument (meaning they
add the exported properties into global scope).
