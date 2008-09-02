Before building the project the following must be done.

1. ..\..\bin\xpidl -m header -I..\..\idl nsISimplePlugin.idl
This will generate nsISimplePlugin.h which is needed to successfuly
compile the plugin

2. ..\..\bin\xpidl -m typelib -I..\..\idl nsISimplePlugin.idl
This will create nsISimplePlugin.xpt file which should go to the
Mozilla Components folder.

Mechanism of accessing the service manager is implemented in Mozilla
builds starting with 0.9.4. It will not work with Netscape 6.1.
