.. _crashes_crashmanager:

=============
Crash Manager
=============

The **Crash Manager** is a service and interface for managing crash
data within the Gecko application.

From JavaScript, the service can be accessed via::

   let crashManager = Services.crashmanager;

That will give you an instance of ``CrashManager`` from ``CrashManager.jsm``.
From there, you can access and manipulate crash data.

The crash manager stores statistical information about crashes as well as
detailed information for both browser and content crashes. The crash manager
automatically detects new browser crashes at startup by scanning for
:ref:`Crash Events`. Content process crash information on the other hand is
provided externally.

Other Documents
===============

.. toctree::
   :maxdepth: 1

   crash-events
