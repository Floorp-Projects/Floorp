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
# The Original Code is Web Sniffer.
# 
# The Initial Developer of the Original Code is Erik van der Poel.
# Portions created by Erik van der Poel are
# Copyright (C) 1998,1999,2000 Erik van der Poel.
# All Rights Reserved.
# 
# Contributor(s): Bruce Robson
# 

# Solaris
LIBS = -lsocket -lnsl

# Windows NT
#LIBS =

CC = gcc

#O_or_g = -g
O_or_g = -O

CFLAGS = -Wall -pedantic -D_REENTRANT $(O_or_g)

PURIFY =
#PURIFY = purify
#PURIFY = purify -windows=no

OBJS = \
	addurl.o \
	file.o \
	hash.o \
	html.o \
	http.o \
	io.o \
	mime.o \
	net.o \
	url.o \
	utils.o \
	view.o

EXES = \
	dnstest \
	ftp \
	grab \
	link \
	pop \
	proxy \
	robot \
	urltest \
	view.cgi

#all: dnstest
#all: ftp
#all: grab
#all: link
#all: pop
#all: proxy
#all: robot
all: view.cgi
#all: $(EXES)

dnstest: dns.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) dns.c $(OBJS) $(LIBS) -o $@

ftp: ftp.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) ftp.c $(OBJS) $(LIBS) -o $@

grab: grab.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) grab.c $(OBJS) $(LIBS) -o $@

link: link.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) link.c $(OBJS) $(LIBS) -o $@

pop: pop.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) pop.c $(OBJS) $(LIBS) -o $@

proxy: proxy.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) proxy.c $(OBJS) $(LIBS) -o $@

robot: robot.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) robot.c $(OBJS) -lthread $(LIBS) -o $@

urltest: url.c utils.c
	$(PURIFY) $(CC) $(CFLAGS) -DURL_TEST url.c utils.c -o $@

view.cgi: cgiview.c $(OBJS)
	$(PURIFY) $(CC) $(CFLAGS) cgiview.c $(OBJS) $(LIBS) -o $@

clean:
	rm -f *.o $(EXES)
