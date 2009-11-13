FennecInstaller implementation to replace CAB installer.

This version extracts a 7z file appended to the EXE, i.e. works like a
self-extracted archive.

For example if you want to install fennec-1.0a3pre.en-US.wince-arm.7z, build
the program, run the following command:

  cat fennec_installer.exe fennec-1.0a3pre.en-US.wince-arm.7z >fennec-1.0a3pre.en-US.wince-arm.exe

Then copy fennec-1.0a3pre.en-US.wince-arm.exe to a device and run it.

Note: Requires lib7z.
