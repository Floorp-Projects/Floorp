Converts a clock/calendar time to an absolute time.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtime.h>

   PRTime PR_ImplodeTime(const PRExplodedTime *exploded);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has these parameters:

``exploded``
   A pointer to the clock/calendar time to be converted.

.. _Returns:

Returns
~~~~~~~

An absolute time value.

.. _Description:

Description
-----------

This function converts the specified clock/calendar time to an absolute
time and returns the converted time value.
