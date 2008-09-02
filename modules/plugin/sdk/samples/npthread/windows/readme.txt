03-05-2002

This sample is an attempt to write a wrapper plugin which would run the
real plugin in a separate thread. The current code is just a first prototype
version aimed to determine the very possibility of such thing. It is not
designed to handle more than one instance of one plugin. Another limitations
are: it only relays browser-to-plugin calls in thread event based matter
(calls from the plugin to the browser are just made directly by function
pointer; it does not implement notifications back from the plugin thread
to the calling thread, so it simply waits before each NPP_* call until
the plugin thread is done with the previous NPP_* call.

The wrapper tested with Basic plugin sample from the plugin
SDK, so some common plugin crashes can be modelled. Work is still
required to make it functional with more complicated plugins
like Flash.

Steps to see it in action:

  -- place the wrapper plugin (npthread.dll) in the plugins folder
  -- remove npnul32.dll from the plugins folder
  -- rename the plugin you want to run in a separate thread adding
     two zeroes at the beginnig (ren npbasic.dll 00npbasic.dll)
  -- run test case for the plugin in question
