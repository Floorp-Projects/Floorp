PR_IntervalToMilliseconds
=========================

Converts platform-dependent intervals to standard clock milliseconds.

.. _Syntax:

Syntax
------

.. code:: eval

    #include <prinrval.h>

    PRUint32 PR_IntervalToMilliseconds(PRIntervalTime ticks);

.. _Parameter:

Parameter
~~~~~~~~~

``ticks``
   The number of platform-dependent intervals to convert.

.. _Returns:

Returns
~~~~~~~

Equivalent in milliseconds of the value passed in the ``ticks``
parameter.

.. _Description:

Description
-----------

Conversion may cause overflow, which is not reported.
