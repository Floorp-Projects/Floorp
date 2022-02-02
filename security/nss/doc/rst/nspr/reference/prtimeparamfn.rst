This type defines a callback function to calculate and return the time
parameter offsets from a calendar time object in GMT.

.. _Syntax:

Syntax
------

.. code:: eval

    #include <prtime.h>

    typedef PRTimeParameters (PR_CALLBACK_DECL *PRTimeParamFn)
       (const PRExplodedTime *gmt);

.. _Description:

Description
-----------

The type ``PRTimeParamFn`` represents a callback function that, when
given a time instant in GMT, returns the time zone information (offset
from GMT and DST offset) at that time instant.
