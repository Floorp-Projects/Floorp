.. _mozilla_projects_nss_http_delegation:

HTTP delegation
===============

`Background <#background>`__
----------------------------

.. container::

   Up to version 3.11, :ref:`mozilla_projects_nss` connects directly over
   `HTTP <https://developer.mozilla.org/en-US/docs/Web/HTTP>`__ to an OCSP responder to make the
   request and fetch the response. It does so in a blocking fashion, and also directly to the
   responder, ignoring any proxy the application may wish to use. This causes OCSP requests to fail
   if the network environment requires the use of a proxy.

   There are two possible solutions to this limitation. Instead of improving the simple HTTP client
   in NSS, the NSS team has decided to provide an NSS API to register application callback
   functions. If provided by the application, NSS will use the registered HTTP client for querying
   an OSCP responder.

   This NSS feature is currently targeted to first appear in NSS version 3.11.1. More details can be
   found in `bug 152426 <https://bugzilla.mozilla.org/show_bug.cgi?id=152426>`__.

   In order to use the HTTP Delegation feature in your NSS-based application, you need to implement
   several callback functions. Your callback functions might be a full implementation of a HTTP
   client. Or you might choose to leverage an existing HTTP client library and implement the
   callback functions as a thin layer that forwards requests from NSS to the HTTP client library.

   To learn about all the details, please read the documentation contained in the NSS C header
   files. Look for function SEC_RegisterDefaultHttpClient and all functions having names that start
   with SEC_Http.

   To find an example implementation, you may look at
   `bug 111384 <https://bugzilla.mozilla.org/show_bug.cgi?id=111384>`__, which tracks the
   implementation in Mozilla client applications.

.. _instructions_for_specifying_an_ocsp_proxy:

`Specifying an OCSP proxy <#instructions_for_specifying_an_ocsp_proxy>`__
-------------------------------------------------------------------------

.. container::

   The remainder of this document is a short HOWTO.

   One might expect the API defines a simple function that accepts the URI and data to be sent, and
   returns the result data. But there is no such simple interface.

   The API should allow NSS to use the HTTP client either asynchronously or synchronously. In
   addition, during an application session with OCSP enabled, a large number of OCSP requests might
   have to be sent. Therefore the API should allow for keep-alive (persistent) HTTP connections.

   HTTP URIs consist of host:port and a path, e.g.
   http://ocsp.provider.com:80/cgi-bin/ocsp-responder

   If NSS needs to access a HTTP server, it will request that an "http server session object" be
   created (SEC_HttpServer_CreateSessionFcn).

   The http server session object is logically associated with host and port destination
   information, in our example this is "host ocsp.provider.com port 80". The object may be used by
   the application to associate it with a physical network connection.

   (NSS might choose to be smart, and only create a single http server session object for each
   server encountered. NSS might also choose to be simple, and request multiple objects for the same
   server. The application must support both strategies.)

   The logical http server session object is expected to remain valid until explicitly destroyed
   (SEC_HttpServer_FreeSessionFcn). Should the application be unable to keep a physical connection
   alive all the time, the application is expected to create new connections automatically.

   NSS may choose to repeatedly call a "network connection keep alive" function
   (SEC_HttpServer_KeepAliveSessionFcn) on the server session object, giving application code a
   chance to do whatever is required.

   For each individual HTTP request, NSS will request the creation of a "http request object"
   (SEC_HttpRequest_CreateFcn). No full URI is provided as a parameter. Instead, the parameters are
   a server session object (that carries host and port information already) and the request path. In
   our example the path is "/cgi-bin/ocsp-responder". (When issueing GET requests, the
   "?query-string=data" portion should already be appended to the request path)

   After creation, NSS might call functions to provide additional details of the HTTP request (e.g.
   SEC_HttpRequest_SetPostDataFcn). The application is expected to collect the details for later
   use.

   Once NSS is finished providing all details, it will request to initiate the actual network
   communication (SEC_HttpRequest_TrySendAndReceiveFcn). The application should try to reuse
   existing network connections associated with the server session object.

   Once the HTTP response has been obtained from the HTTP server, the function will provide the
   results in its "out parameters".

   Please read the source code documentation to learn how to use this API synchronously or
   asynchronously.

   Now that we have explained the interaction between NSS, the callback functions and the
   application, let's look at the steps required by the application to initially register the
   callbacks.

   Make sure you have completed the NSS initialization before you attempt to register the callbacks.

   Look at SEC_HttpClientFcn, which is a (versioned) table of function pointers. Create an instance
   of this type and supply a pointer to your implementation for each entry in the function table.

   Finally register your HTTP client implementation with a call to SEC_RegisterDefaultHttpClient.