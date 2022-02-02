PR_MicrosecondsToInterval
=========================

Converts standard clock microseconds to platform-dependent intervals.

.. _Syntax:

Syntax
------

.. code:: eval

    #include <prinrval.h>

    PRIntervalTime PR_MicrosecondsToInterval(PRUint32 milli);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``micro``
   The number of microseconds to convert to interval form.

.. _Returns:

Returns
~~~~~~~

Platform-dependent equivalent of the value passed in the ``micro``
parameter.
