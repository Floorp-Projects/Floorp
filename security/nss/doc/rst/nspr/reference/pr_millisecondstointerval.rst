PR_MillisecondsToInterval
=========================

Converts standard clock milliseconds to platform-dependent intervals.

.. _Syntax:

Syntax
------

.. code:: eval

    #include <prinrval.h>

    PRIntervalTime PR_MillisecondsToInterval(PRUint32 milli);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``milli``
   The number of milliseconds to convert to interval form.

.. _Returns:

Returns
~~~~~~~

Platform-dependent equivalent of the value passed in the ``milli``
parameter.
