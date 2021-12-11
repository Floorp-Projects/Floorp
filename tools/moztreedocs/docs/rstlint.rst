ReStructuredText Linter
-----------------------

RST isn't the easiest of markup languages, but it's powerful and  what `Sphinx` (the library used to build our docs) uses, so we're stuck with it. But at least we now have a linter which will catch basic problems in `.rst` files early. Be sure to run:

.. code-block:: shell

   mach lint -l rst

to test your outgoing changes before submitting to review.

`More information <RST Linter>`__.
