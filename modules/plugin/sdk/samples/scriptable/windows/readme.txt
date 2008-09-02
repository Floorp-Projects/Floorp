Before building the project the following must be done.

1. ..\..\..\bin\xpidl -m header -I..\..\..\idl nsIScriptablePlugin.idl
This will generate nsIScriptablePlugin.h which is needed to successfuly
compile the plugin

2. ..\..\..\bin\xpidl -m typelib -I..\..\..\idl nsIScriptablePlugin.idl
This will create nsIScriptablePlugin.xpt file which should go to the
Mozilla Components folder.
