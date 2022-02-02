Converts an absolute time to a clock/calendar time.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtime.h>

   void PR_ExplodeTime(
      PRTime usecs,
      PRTimeParamFn params,
      PRExplodedTime *exploded);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has these parameters:

``usecs``
   An absolute time in the ``PRTime`` format.
``params``
   A time parameter callback function.
``exploded``
   A pointer to a location where the converted time can be stored. This
   location must be preallocated by the caller.

.. _Returns:

Returns
~~~~~~~

Nothing; the buffer pointed to by ``exploded`` is filled with the
exploded time.

.. _Description:

Description
-----------

This function converts the specified absolute time to a clock/calendar
time in the specified time zone. Upon return, the location pointed to by
the exploded parameter contains the converted time value.
