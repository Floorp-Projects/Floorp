# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is SniffURI.
#
# The Initial Developer of the Original Code is
# Erik van der Poel <erik@vanderpoel.org>.
# Portions created by the Initial Developer are Copyright (C) 1998-2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Bruce Robson <bns_robson@hotmail.com>
#
# ***** END LICENSE BLOCK *****

# Linux, Unix or Windows with Cygwin (type "./configure; make")
CFLAGS = -Wall -pedantic -D_REENTRANT $(O_or_g)
EXE =
LIBS = @LIBS@
OBJ = o
RM = rm -f

# Windows without Cygwin (type "nmake")
CFLAGS = /nologo /DWINDOWS=1 /D_WIN32_WINNT=0x0400
EXE = .exe
LIBS = wsock32.lib
OBJ = obj
RM = del

#O_or_g = -g
O_or_g = -O

PURIFY =
#PURIFY = purify
#PURIFY = purify -windows=no

OBJS =			\
	addurl.$(OBJ)	\
	app.$(OBJ)	\
	file.$(OBJ)	\
	hash.$(OBJ)	\
	html.$(OBJ)	\
	http.$(OBJ)	\
	io.$(OBJ)	\
	mime.$(OBJ)	\
	net.$(OBJ)	\
	thread.$(OBJ)	\
	url.$(OBJ)	\
	utils.$(OBJ)	\
	view.$(OBJ)

EXES =			\
	dnstest$(EXE)	\
	ftp$(EXE)	\
	grab$(EXE)	\
	httpd$(EXE)	\
	link$(EXE)	\
	pop$(EXE)	\
	proxy$(EXE)	\
	robot$(EXE)	\
	urltest$(EXE)	\
	view.cgi$(EXE)

#all: dnstest$(EXE)
#all: ftp$(EXE)
#all: grab$(EXE)
#all: httpd$(EXE)
#all: link$(EXE)
#all: pop$(EXE)
#all: proxy$(EXE)
#all: robot$(EXE)
#all: urltest$(EXE)
#all: view.cgi$(EXE)
all: $(EXES)

dnstest$(EXE): dns.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) dns.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) dns.c $(OBJS) $(LIBS) -o $@

ftp$(EXE): ftp.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) ftp.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) ftp.c $(OBJS) $(LIBS) -o $@

grab$(EXE): grab.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) grab.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) grab.c $(OBJS) $(LIBS) -o $@

httpd$(EXE): httpd.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) httpd.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) httpd.c $(OBJS) $(LIBS) -o $@

link$(EXE): link.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) link.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) link.c $(OBJS) $(LIBS) -o $@

pop$(EXE): pop.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) pop.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) pop.c $(OBJS) $(LIBS) -o $@

proxy$(EXE): proxy.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) proxy.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) proxy.c $(OBJS) $(LIBS) -o $@

robot$(EXE): robot.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) robot.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) robot.c $(OBJS) $(LIBS) -o $@

urltest$(EXE): urltest.c url.$(OBJ) utils.$(OBJ)
	@echo $(PURIFY) $(CC) $(CFLAGS) urltest.c '$$(OBJS)' -o $@
	@$(PURIFY) $(CC) $(CFLAGS) urltest.c url.$(OBJ) utils.$(OBJ) -o $@

view.cgi$(EXE): cgiview.c $(OBJS)
	@echo $(PURIFY) $(CC) $(CFLAGS) cgiview.c '$$(OBJS)' $(LIBS) -o $@
	@$(PURIFY) $(CC) $(CFLAGS) cgiview.c $(OBJS) $(LIBS) -o $@

clean:
	$(RM) *.$(OBJ) $(EXES)

win:
	make -f Makefile.in clean
	sed -e 's/^#Windows#//' Makefile.in > Makefile
	tar cf - * | gzip > .ws.tgz
	smbclient //10.1.1.3/ws -N -c 'put .ws.tgz'
	rm .ws.tgz
