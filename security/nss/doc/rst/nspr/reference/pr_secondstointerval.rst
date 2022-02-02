Converts standard clock seconds to platform-dependent intervals.

.. _Syntax:

Syntax
------

.. code:: eval

    #include <prinrval.h>

    PRIntervalTime PR_SecondsToInterval(PRUint32 seconds);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``seconds``
   The number of seconds to convert to interval form.

.. _Returns:

Returns
~~~~~~~

Platform-dependent equivalent of the value passed in the ``seconds``
parameter.
