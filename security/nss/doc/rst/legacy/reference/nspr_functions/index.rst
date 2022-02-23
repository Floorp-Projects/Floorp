.. _mozilla_projects_nss_reference_nspr_functions:

NSPR functions
==============

.. container::

   `NSPR <https://www.mozilla.org/projects/nspr/>`__ is a platform abstraction library that provides
   a cross-platform API to common OS services.  NSS uses NSPR internally as the porting layer. 
   However, a small number of NSPR functions are required for using the certificate verification and
   SSL functions in NSS.  These NSPR functions are listed in this section.

.. _nspr_initialization_and_shutdown:

`NSPR initialization and shutdown <#nspr_initialization_and_shutdown>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSPR is automatically initialized by the first NSPR function called by the application.  Call
   ```PR_Cleanup`` </en-US/PR_Cleanup>`__ to shut down NSPR and clean up its resources.\ `
    </en-US/PR_Init>`__

   -  `PR_Cleanup </en-US/PR_Cleanup>`__

.. _error_reporting:

`Error reporting <#error_reporting>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS uses NSPR's thread-specific error code to report errors.  Call
   ```PR_GetError`` </en-US/PR_GetError>`__ to get the error code of the last failed NSS or NSPR
   function.  Call ```PR_SetError`` </en-US/PR_SetError>`__ to set the error code, which can be
   retrieved with ``PR_GetError`` later.

   The NSS functions ``PORT_GetError`` and ``PORT_SetError`` are simply wrappers of ``PR_GetError``
   and ``PR_SetError``.

   -  `PR_GetError </en-US/PR_GetError>`__
   -  `PR_SetError </en-US/PR_SetError>`__

.. _calendar_time:

`Calendar time <#calendar_time>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS certificate verification functions take a ``PRTime`` parameter that specifies the time
   instant at which the validity of the certificate should verified.  The NSPR function
   ```PR_Now`` </en-US/PR_Now>`__ returns the current time in ``PRTime``.

   -  `PR_Now </en-US/PR_Now>`__

.. _interval_time:

`Interval time <#interval_time>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The NSPR socket I/O functions ```PR_Recv`` </en-US/PR_Recv>`__ and
   ```PR_Send`` </en-US/PR_Send>`__ (used by the NSS SSL functions) take a ``PRIntervalTime``
   timeout parameter.  ``PRIntervalTime`` has an abstract, platform-dependent time unit.  Call
   ```PR_SecondsToInterval`` </en-US/PR_SecondsToInterval>`__ or ``PR_MillisecondsToInterval`` to
   convert a time interval in seconds or milliseconds to ``PRIntervalTime``.

   -  `PR_SecondsToInterval </en-US/PR_SecondsToInterval>`__
   -  `PR_MillisecondsToInterval </en-US/PR_MillisecondsToInterval>`__

.. _nspr_io_layering:

`NSPR I/O layering <#nspr_io_layering>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSPR file descriptors can be layered, corresponding to the layers in the network stack.  The SSL
   library in NSS implements the SSL protocol as an NSPR I/O layer, which sits on top of another
   NSPR I/O layer that represents TCP.

   You can implement an NSPR I/O layer that wraps your own TCP socket code.  The following NSPR
   functions allow you to create your own NSPR I/O layer and manipulate it.

   -  `PR_GetUniqueIdentity </en-US/PR_GetUniqueIdentity>`__
   -  `PR_CreateIOLayerStub </en-US/PR_CreateIOLayerStub>`__
   -  `PR_GetDefaultIOMethods </en-US/PR_GetDefaultIOMethods>`__
   -  `PR_GetIdentitiesLayer </en-US/PR_GetIdentitiesLayer>`__
   -  `PR_GetLayersIdentity </en-US/PR_GetLayersIdentity>`__
   -  `PR_PushIOLayer </en-US/PR_PushIOLayer>`__
   -  `PR_PopIOLayer </en-US/PR_PopIOLayer>`__

.. _wrapping_a_native_file_descriptor:

`Wrapping a native file descriptor <#wrapping_a_native_file_descriptor>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   If your current TCP socket code uses the standard BSD socket API, a lighter-weight method than
   creating your own NSPR I/O layer is to simply import a native file descriptor into NSPR.  This
   method is convenient and works for most applications.

   -  `PR_ImportTCPSocket </en-US/PR_ImportTCPSocket>`__

.. _socket_io_functions:

`Socket I/O functions <#socket_io_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   As mentioned above, the SSL library in NSS implements the SSL protocol as an NSPR I/O layer. 
   Users call NSPR socket I/O functions to read from, write to, and shut down an SSL connection, and
   to close an NSPR file descriptor.

   -  `PR_Read </en-US/PR_Read>`__
   -  `PR_Write </en-US/PR_Write>`__
   -  `PR_Recv </en-US/PR_Recv>`__
   -  `PR_Send </en-US/PR_Send>`__
   -  `PR_GetSocketOption </en-US/PR_GetSocketOption>`__
   -  `PR_SetSocketOption </en-US/PR_SetSocketOption>`__
   -  `PR_Shutdown </en-US/PR_Shutdown>`__
   -  `PR_Close </en-US/PR_Close>`__