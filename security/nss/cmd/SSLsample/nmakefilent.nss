# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

# NSS 2.6.2 Sample NT Makefile
#
#
# This nmake file will build server.c and client.c on Windows NT 4 SP3.
#

DEFINES=-D_X86_ -GT -DWINNT -DXP_PC -UDEBUG -U_DEBUG -DNDEBUG -DWIN32 -D_WINDOWS
INCPATH=-I. -I..\include\dbm -I..\include\nspr -I..\include\security

LIBS=nss.lib ssl.lib pkcs7.lib pkcs12.lib secmod.lib cert.lib key.lib crypto.lib secutil.lib hash.lib dbm.lib libplc3.lib libplds3.lib libnspr3.lib wsock32.lib

CFLAGS=-O2 -MD -W3 -nologo

CC=cl

LDOPTIONS=/link /LIBPATH:..\lib /nodefaultlib:libcd.lib /subsystem:console

server:
	$(CC) $(CFLAGS) /Feserver server.c getopt.c $(LIBS) $(DEFINES) $(INCPATH) $(LDOPTIONS)

client:
	$(CC) $(CFLAGS) /Feclient client.c getopt.c $(LIBS) $(DEFINES) $(INCPATH) $(LDOPTIONS)

clean:
	del /S server.exe client.exe server.lib server.exp client.lib client.exp server.obj client.obj getopt.obj

