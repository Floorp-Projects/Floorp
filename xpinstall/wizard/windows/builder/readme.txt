
The contents of this file are subject to the Netscape Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/NPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Mozilla Communicator client code, 
released March 31, 1998. 

The Initial Developer of the Original Code is Netscape Communications 
Corporation.  Portions created by Netscape are
Copyright (C) 1999 Netscape Communications Corporation. All
Rights Reserved.

Contributor(s): 
 Sean Su <ssu@netscape.com>


Purpose:
  To build the mozilla self-extracting installer and its corresponding .xpi files
  given a mozilla build on a local system.

Requirements:
1. Perl needs to be installed correctly on the build system because cwd.pm is used.
   Preferably, perl version 5.004 or newer should be used.
   
2. Mozilla\xpinstall\wizard\windows needs to be built.
  a. run nmake -f makefile.win from the above directory


Build.pl requires no parameters.  When it finishes, it will have created a
temporary staging area in mozilla\dist\stage to build the .xpi archives from.
The self-extracting installer (mozilla-win32-installer.exe) will be delivered to:
  mozilla\dist\win32_o.obj\install

The .xpi archives will be delivered to:
  mozilla\dist\win32_o.obj\install\xpi

Mozilla-win32-installer.exe does not require the .xpi archives once its been created
because they have been packaged up in the .exe file.

The .xpi archives will also be copied to:
  mozilla\dist\win32_o.obj\install

This is so setup.exe can install them.  Setup.exe is usually run when debugging the
installer code.  This makes it easier to debug.

