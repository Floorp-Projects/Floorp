This is a plugin sample which demonstrates how with minimal modifications
in the 4.x legacy plugin code to achieve scripting functionality despite
of Mozilla not supporting LiveConnect in the way it was supported in 
Netscape Communicator.

To build the sample:

1. create .xpt and nsI4xScrPlugin.h out of nsI4xScrPlugin.idl file
using Netscape idl compiler xpidl.exe. The command options are:
   xpidl -m header nsI4xScrPlugin.idl
   xpidl -m typelib nsI4xScrPlugin.idl
nsISupports.idl and nsrootidl.idl are needed for this.

2. create a project and build np4xscr.dll -- the plugin itself

3. place .xpt file in the components directory and the dll in the
plugins directory

4. load test.html and see it in work

The current sample code was written for Windows but can be easily 
modified for other platforms.

Important notice: although developers who work on xpcom plugins
are strongly encouraged to use Netscape macros for common interface
method declarations and implementations, in the present sample we 
decided to use their manual implementation. This is because the technique
shown in the sample may be useful for plugins which are supposed to
work under both Mozilla based browsers and Netscape Communicator
(4.x browsers). Using the macros requires linking against some
libraries which are not present in 4.x browsers (xpcom.lib, nspr.lib).

Files which under other circumstances would benefit from using
the macros are nsScriptablePeer.h and nsScriptablePeer.cpp. The versions
which use macros are also included (nsScriptablePeer1.h and 
nsScriptablePeer1.cpp) for reference purposes.

Some header files from mozilla/dist/include and some .idl files from
mozilla/dist/idl are still needed to successfully build the sample.
