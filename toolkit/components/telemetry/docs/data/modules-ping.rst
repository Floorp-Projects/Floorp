
"modules" ping
==============

This ping is sent once a week and includes the modules loaded in the Firefox process.

The client ID and environment are submitted with this ping.

Structure:

.. code-block:: js

    {
      type: "modules",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        version: 1,
        modules: [
          {
            name: <string>, // Name of the module file (e.g. xul.dll)
            version: <string>, // Version of the module
            debugID: <string>, // ID of the debug information file
            debugName: <string>, // Name of the debug information file
            certSubject: <string>, // Name of the organization that signed the binary (Optional, only defined when present)
          },
          ...
        ],
      }
    }

Notes
~~~~~

The version information is only available on Windows, it is null on other platforms.

The debug name is the name of the PDB on Windows (which isn't always the same as the module name modulo the extension, e.g. the PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb) and is the same as the module name on other platforms.

The debug ID is platform-dependent. It is compatible with the Breakpad ID used on Socorro.

Sometimes the debug name and debug ID are missing for Windows modules (often with malware). In this case, they will be "null".

The length of the modules array is limited to 512 entries.

The name and debug name are length limited, with a maximum of 64 characters.
