rem -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
rem 
rem  The contents of this file are subject to the Netscape Public License
rem  Version 1.0 (the "NPL"); you may not use this file except in
rem  compliance with the NPL.  You may obtain a copy of the NPL at
rem  http://www.mozilla.org/NPL/
rem 
rem  Software distributed under the NPL is distributed on an "AS IS" basis,
rem  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
rem  for the specific language governing rights and limitations under the
rem  NPL.
rem 
rem  The Initial Developer of this code under the NPL is Netscape
rem  Communications Corporation.  Portions created by Netscape are
rem  Copyright (C) 1998 Netscape Communications Corporation.  All Rights
rem  Reserved.
rem 

del s:\ns\raptor\ui\src\windows\win32_d.obj\%1.obj
del s:\ns\raptor\ui\tests\windows\win32_d.obj\winmain.obj
nmake -f makefile.win
