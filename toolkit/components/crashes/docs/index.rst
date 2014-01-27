=============
Crash Manager
=============

The **Crash Manager** is a service and interface for managing crash
data within the Gecko application.

From JavaScript, the service can be accessed via::

   Cu.import("resource://gre/modules/Services.jsm");
   let crashManager = Services.crashmanager;

That will give you an instance of ``CrashManager`` from ``CrashManager.jsm``.
From there, you can access and manipulate crash data.

Other Documents
===============

.. toctree::
   :maxdepth: 1

   crash-events
