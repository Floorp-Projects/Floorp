.. _mozilla_projects_nss_ssl_functions_sslintro:

sslintro
========

.. container::

   .. note::

      -  This page is part of the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` that
         we are migrating into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/Project:MDC_style_guide>`__. If you are
         inclined to help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

   .. rubric:: Overview of an SSL Application
      :name: Overview_of_an_SSL_Application

   --------------

.. _chapter_1_overview_of_an_ssl_application:

`Chapter 1
 <#chapter_1_overview_of_an_ssl_application>`__ Overview of an SSL Application
------------------------------------------------------------------------------

.. container::

   SSL and related APIs allow compliant applications to configure sockets for authenticated,
   tamper-proof, and encrypted communications. This chapter introduces some of the basic SSL
   functions. `Chapter 2, "Getting Started With SSL" <gtstd.html#1005439>`__ illustrates their use
   in sample client and server applications.

   An SSL application typically includes five parts:

   |  `Initialization <#1027662>`__
   | `Configuration <#1027742>`__
   | `Communication <#1027816>`__
   | `Functions Used by Callbacks <#1027820>`__
   | `Cleanup <#1030535>`__

   Although the details differ somewhat for client and server applications, the concepts and many of
   the functions are the same for both.

      **WARNING:** Some of the SSL header files provided as part of NSS 2.0 include both public APIs
      documented in the NSS 2.0 documentation set and private APIs intended for internal use by the
      NSS implementation of SSL. You should use only the SSL APIs (and related certificate, key, and
      PKCS #11 APIs) that are described in this document, the SSL Reference. Other APIs that may be
      exposed in the header files are not supported for application use.

.. _initialization_2:

` <#initialization_2>`__ Initialization
---------------------------------------

.. container::

   Initialization includes setting up configuration files, setting global defaults, and setting up
   callback functions. Functions used in the initialization part of an application can include the
   following:

   -   ``PR_Init``. Initializes NSPR. Must be called before any other NSS functions.
   -   ```PK11_SetPasswordFunc`` <pkfnc.html#1023128>`__. Sets the global callback function to
      obtain passwords for PKCS #11 modules. Required.
   -   ``NSS_Init``. Sets up configuration files and performs other tasks required to run Network
      Security Services. ``NSS_Init`` is *not* idempotent, so call it only once. Required.
   -   ``SSL_OptionSetDefault``. Changes default values for all subsequently opened sockets as long
      as the application is running (compare with
      :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1087792` which only configures the socket that
      is currently open). This function must be called once for each default value that needs to be
      changed. Optional.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1228530`,
      :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1100285`,
      :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1105952`, or
      :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1104647`. These functions tell the library
      which cipher suites are permitted by policy (for example, to comply with export restrictions).
      Cipher suites disabled by policy cannot be enabled by user preference. One of these functions
      must be called before any cryptographic operations can be performed with NSS.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1084747`. Enables all ciphers chosen by user
      preference. Optional.

.. _initializing_caches:

`Initializing Caches <#initializing_caches>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   SSL peers frequently reconnect after a relatively short time has passed. To avoid the overhead of
   repeating the full SSL handshake in situations like this, the SSL protocol supports the use of a
   session cache, which retains information about each connection, such as the master secret
   generated during the SSL handshake, for a predetermined length of time. If SSL can locate the
   information about a previous connection in the local session cache, it can reestablish the
   connection much more quickly than it can without the connection information.

   By default, SSL allocates one session cache. This default cache is called the *client session ID
   cache*, (also known as the client session cache, or simply the client cache). The client cache is
   used for all sessions where the program handshakes as an SSL client. It is not configurable. You
   can initialize the client cache with the function
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1138601`.

   If an application will use SSL sockets that handshake as a server, you must specifically create
   and configure a server cache, using either
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1143851` or
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1142625`. The server cache is used for all
   sessions where the program handshakes as an SSL server.

   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1138601`. Clears all sessions from the client
      session cache. Optional.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1143851`. Sets up parameters for a server
      session cache for a single-process application. Required for single-process server
      applications.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1142625`. Sets up parameters for a server
      cache for a multi-process application. Required for multi-process server applications. You can
      use either this function or :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1143851`, not
      both.

.. _configuration_2:

` <#configuration_2>`__ Configuration
-------------------------------------

.. container::

   The configuration portion of an SSL-enabled application typically begins by opening a new socket
   and then importing the new socket into the SSL environment:

   -   ``PR_NewTCPSocket``. Opens a new socket. A legal NSPR socket is required to be passed to
      ``SSL_ImportFD``, whether it is created with this function or by another method.
   -   ``SSL_ImportFD``. Makes an NSPR socket into an SSL socket. Required. Brings an ordinary NSPR
      socket into the SSL library, returning a new NSPR socket that can be used to make SSL calls.
      You can pass this function a *model* file descriptor to create the new SSL socket with the
      same configuration state as the model.

   It is also possible for an application to import a socket into SSL after the TCP connection on
   that socket has already been established. In this case, initial configuration takes place in the
   same way: pass the existing NSPR file descriptor to ``SSL_ImportFD`` and perform any additional
   configuration that has not already been determined by the model file descriptor.

   Configuration functions control the configuration of an individual socket.

   -   ``PR_GetSocketOption``. Retrieves the socket options currently set for a specified socket.
      Optional.
   -   ``PR_SetSocketOption``. Sets the socket options for a specified socket., including making it
      blocking or nonblocking. Optional.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1086543`. Sets a single configuration
      parameter of a specified socket. This function must be called once for each parameter whose
      settings you want to change from those established with ``SSL_OptionSetDefault``. Optional.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1217647`. For servers only. Configures the
      socket with the information needed to handshake as an SSL server. Required for servers.
   -   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1087792`. For clients only. Records the
      target server URL for comparison with the URL specified by the server certificate. Required
      for clients.

   Callbacks and helper functions allow you to specify such things as how authentication is
   accomplished and what happens if it fails.

   -   ``SSL_SetPKCS11PinArg``. Sets the argument passed to the PKCS #11 password callback function.
      Required.
   -   ``SSL_AuthCertificateHook``. Specifies a callback function used to authenticate an incoming
      certificate (optional for servers, necessary for clients to avoid "man-in-the-middle"
      attacks). Optional. If not specified, SSL uses the default callback function,
      :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088888`.
   -   ``SSL_BadCertHook``. Specifies a callback function to deal with a situation where
      authentication has failed. Optional.
   -   ``SSL_GetClientAuthDataHook``. Specifies a callback function for SSL to use when the server
      asks for client authentication information. This callback is required if you want to do client
      authentication. You can set the callback function to a standard one that is provided,
      :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1106762`.
   -   ``SSL_HandshakeCallback``. Specifies a callback function that will be used by SSL to inform
      either a client application or a server application when the SSL handshake is completed.
      Optional.

.. _communication_2:

` <#communication_2>`__ Communication
-------------------------------------

.. container::

   At this point the application has set up the socket to communicate using SSL. For simple
   encrypted and authenticated communications, no further calls to SSL functions are required. A
   variety of additional SSL functions are available, however. These can be used, for example, when
   interrupting and restarting socket communications, when the application needs to change socket
   parameters, or when an application imports a socket into SSL after the TCP connection on that
   socket has already been established.

   Communication between SSL sockets always begins with the SSL handshake. The handshake occurs
   automatically the first time communication is requested with a socket read/write or send/receive
   call. It is also possible to force the handshake explicitly with
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1133431` or repeat it explicitly with
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1232052`.

   Once the SSL sockets have been configured, authentication and encryption happen automatically
   whenever you use the communication functions from the NSPR library.

   A server application typically uses these functions to establish a connection:

   ``PR_Bind   PR_Listen   PR_Accept   PR_GetSockName``

   A client application typically uses these functions to establish a connection:

   |  ``PR_GetHostByName``
   | ``PR_EnumerateHostEnt``
   | ``PR_Connect``
   | ``PR_GetConnectStatus``

   When an application imports a socket into SSL after the TCP connection on that socket has already
   been established, it must call :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1058001` to
   determine whether SSL should behave like an SSL client or an SSL server. Note that this step
   would not be necessary if the socket weren't already connected. For an SSL socket that is
   configured before it is connected, SSL figures this out when the application calls ``PR_Connect``
   or ``PR_Accept``. If the socket is already connected before SSL gets involved, you must provide
   this extra hint.

   Functions that can be used by both clients and servers during communication include the
   following:

   |  ``PR_Send`` or ``PR_Write``
   | ``PR_Read`` or ``PR_Recv``
   | ``PR_GetError``
   | ``PR_GetPeerName``
   | ``PR_Sleep``
   | ``PR_Malloc``
   | ``PR_Free``
   | ``PR_Poll``
   | ``PR_Now``
   | ``PR_IntervalToMilliseconds``
   | ``PR_MillisecondsToInterval``
   | ``PR_Shutdown``
   | ``PR_Close``
   | :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1089420`

   After establishing a connection, an application first calls ``PR_Send``, ``PR_Recv``,
   ``PR_Read``, ``PR_Write``, or ``SSL_ForceHandshake`` to initiate the handshake. The application's
   protocol (for example, HTTP) determines which end has responsibility to talk first. The end that
   has to talk first should call ``PR_Send`` or ``PR_Write``, and the other end should call
   ``PR_Read`` or ``PR_Recv``.

   Use :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1133431` when the socket has been prepared
   for a handshake but neither end has anything to say immediately. This occurs, for example, when
   an HTTPS server has received a request and determines that before it can answer the request, it
   needs to request an authentication certificate from the client. At the HTTP protocol level,
   nothing more is being said (that is, no HTTP request or response is being sent), so the server
   first uses :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1232052` to begin a new handshake and
   then call ``SSL_ForceHandshake`` to drive the handshake to completion.

.. _functions_used_by_callbacks:

`Functions Used by Callbacks <#functions_used_by_callbacks>`__
--------------------------------------------------------------

.. container::

   An SSL application typically provides one or more callback functions that are called by the SSL
   or PKCS #11 library code under certain circumstances. Numerous functions provided by the NSS
   libraries are useful for such application callback functions, including these:

   |  ```CERT_CheckCertValidTimes`` <sslcrt.html#1056662>`__
   | ```CERT_GetDefaultCertDB`` <sslcrt.html#1052308>`__
   | ```CERT_DestroyCertificate`` <sslcrt.html#1050532>`__
   | ```CERT_DupCertificate`` <sslcrt.html#1058344>`__
   | ```CERT_FindCertByName`` <sslcrt.html#1050345>`__
   | ```CERT_FreeNicknames`` <sslcrt.html#1050349>`__
   | ```CERT_GetCertNicknames`` <sslcrt.html#1050346>`__
   | ```CERT_VerifyCertName`` <sslcrt.html#1050342>`__
   | ```CERT_VerifyCertNow`` <sslcrt.html#1058011>`__
   | ```PK11_FindCertFromNickname`` <pkfnc.html#1035673>`__
   | ```PK11_FindKeyByAnyCert`` <pkfnc.html#1026891>`__
   | ```PK11_SetPasswordFunc`` <pkfnc.html#1023128>`__
   | ``PL_strcpy``
   | ``PL_strdup``
   | ``PL_strfree``
   | ``PL_strlen``
   | :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1096168`
   | :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1081175`
   | :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1123385`

.. _cleanup_2:

` <#cleanup_2>`__ Cleanup
-------------------------

.. container::

   This portion of an SSL-enabled application consists primarily of closing the socket and freeing
   memory. After these tasks have been performed, call
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1061858` to close the certificate and key
   databases opened by :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1067601`, and ``PR_Cleanup``
   to coordinate a graceful shutdown of NSPR.