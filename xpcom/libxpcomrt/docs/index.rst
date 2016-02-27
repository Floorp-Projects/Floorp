==========================
 XPCOM Standalone Library
==========================

What it is for
--------------
The XPCOM standalone library, libxpcomrt, was created to support building the WebRTC
standalone library. The libxpcomrt library contains only the parts of XPCOM that are required
to run WebRTC; parts such as the cycle collector and the startup cache required only by Gecko
are not included. A library containing a small subset of Necko was also
created to support the WebRTC standalone library.

The libxcomrt library was created specifically to support the WebRTC standalone library.
It is not intended to be used as a general purpose library to add XPCOM functionality to
an application. It is likely that some of the code contained in the libxpcomrt library
has unresolved symbols that may be exposed if used for purposes other than being linked
into the WebRTC standalone library.

How to use it
-------------
When compiling code utilizing libxpcomrt, both ``MOZILLA_INTERNAL_API`` and ``MOZILLA_XPCOMRT_API``
must be defined in addition to whatever standard flags are used to compile Gecko.
The library is initialized with ``NS_InitXPCOMRT()`` and shutdown with ``NS_ShutdownXPCOMRT()``.
Both functions are declared in xpcom/libxpcomrt/XPCOMRTInit.h.
Only a small number of services which are required for the WebRTC
standalone library to function are included with libxpcomrt. The dynamic loading of services is not
supported. Including a service through ``NSMODULE_DEFN`` and static linking is also not supported.
The only way to add a service to libxpcomrt is to explicitly start the service during
``nsComponentManagerImpl::Init`` in xpcom/components/nsComponentManager.cpp.
The best method to determine what parts of XPCOM are included in libxpcomrt is to examine the
xpcom/libxpcomrt/moz.build file. It contains all of the XPCOM source files used to build libxpcomrt.
A few of the services that are included are:

* UUID Generator
* DNS Service
* Socket Transport Service
* IDN Service

All dependencies on ipc/chromium have been removed.
IO and preference services are not included making this library of limited utility.
