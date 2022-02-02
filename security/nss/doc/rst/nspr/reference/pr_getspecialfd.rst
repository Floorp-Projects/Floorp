Gets the file descriptor that represents the standard input, output, or
error stream.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRFileDesc* PR_GetSpecialFD(PRSpecialFD id);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``id``
   A pointer to an enumerator of type ``PRSpecialFD``, indicating the
   type of I/O stream desired: ``PR_StandardInput``,
   ``PR_StandardOutput``, or ``PR_StandardError``.

.. _Returns:

Returns
~~~~~~~

If the ``id`` parameter is valid, ``PR_GetSpecialFD`` returns a file
descriptor that represents the corresponding standard I/O stream.
Otherwise, ``PR_GetSpecialFD`` returns ``NULL`` and sets the error to
``PR_INVALID_ARGUMENT_ERROR``.

.. _Description:

Description
-----------

Type ``PRSpecialFD`` is defined as follows:

.. code:: eval

   typedef enum PRSpecialFD{
      PR_StandardInput,
      PR_StandardOutput,
      PR_StandardError
   } PRSpecialFD;

``#define PR_STDIN PR_GetSpecialFD(PR_StandardInput)``
``#define PR_STDOUT PR_GetSpecialFD(PR_StandardOutput)``
``#define PR_STDERR PR_GetSpecialFD(PR_StandardError)``

File descriptors returned by ``PR_GetSpecialFD`` are owned by the
runtime and should not be closed by the caller.
