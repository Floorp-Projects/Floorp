Returns the time zone offset information that maps the specified
``PRExplodedTime`` to GMT.

.. note::

   **Note:** Since this function requires GMT as input, its primary use
   is as "filler" for cases in which you need a do-nothing callback.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtime.h>

   PRTimeParameters PR_GMTParameters (
      const PRExplodedTime *gmt);

.. _Parameter:

Parameter
~~~~~~~~~

``gmt``
   A pointer to the clock/calendar time whose offsets are to be
   determined. This time should be specified in GMT.

.. _Returns:

Returns
~~~~~~~

A time parameters structure that expresses the time zone offsets at the
specified time.

.. _Description:

Description
-----------

This is a frequently-used time parameter callback function. You don't
normally call it directly; instead, you pass it as a parameter to
``PR_ExplodeTime()`` or ``PR_NormalizeTime()``.

This is a trivial function; for any input, it returns a
``PRTimeParameters`` structure with both fields set to zero.
