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

ARCH := $(shell uname)

ifeq ($(ARCH), SunOS)
	DEFINES  = -KPIC -DSVR4 -DSOLARIS -DSYSV -D__svr4 -D__svr4__ \
 -D_REENTRANT -DSOLARIS2_5 -D_SVID_GETTOD -DXP_UNIX -UDEBUG -DNDEBUG \
 -DXP_UNIX
	INCPATH  = -I. -I../include/dbm -I../include/nspr -I../include/security
	LIBPATH  = -L../lib
	LIBS     = -lnss -lssl -lpkcs7 -lpkcs12 -lsecmod -lcert -lkey \
 -lcrypto -lsecutil -lhash -ldbm -lplc4 -lplds4 -lnspr4 -lsocket -lnsl
	CFLAGS   = -g
	CC       = cc
endif # SunOS

# The rules to build the sample apps appear below.

server:
	$(CC) $(CFLAGS) $@.c -o $@ $(DEFINES) $(INCPATH) $(LIBPATH) $(LIBS)

client:
	$(CC) $(CFLAGS) $@.c -o $@ $(DEFINES) $(INCPATH) $(LIBPATH) $(LIBS)

clean:
	rm -fr server client server.o client.o

