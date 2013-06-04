#
# mozprocess proclaunch tests Makefile
#

# include rules for platform determination
include iniparser/platform.mk

ifeq ($(WIN32), 1)
# Win 32
CC      = cl
LINK    = link
CFLAGS  = //Od //I "iniparser" //D "WIN32" //D "_WIN32" //D "_DEBUG" //D "_CONSOLE" //D "_UNICODE" //D "UNICODE" //Gm //EHsc //RTC1 //MDd //W3 //nologo //c //ZI //TC
LFLAGS  = //OUT:"proclaunch.exe" //INCREMENTAL //LIBPATH:"iniparser\\" //NOLOGO //DEBUG //SUBSYSTEM:CONSOLE //DYNAMICBASE //NXCOMPAT //ERRORREPORT:PROMPT iniparser.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib
RM      = rm -f

all: iniparser proclaunch

iniparser:
	$(MAKE) -C iniparser

proclaunch.obj: proclaunch.c
	@(echo "compiling proclaunch; platform: $(UNAME), WIN32: $(WIN32)")
	$(CC) $(CFLAGS) proclaunch.c

proclaunch: proclaunch.obj
	$(LINK) $(LFLAGS) proclaunch.obj

clean:
	$(RM) proclaunch.exe projloaunch.obj
else
# *nix/Mac
LFLAGS  = -L.. -liniparser
AR	    = ar
ARFLAGS = rcv
RM      = rm -f
CC      = gcc
ifeq ($(UNAME), Linux)
CFLAGS  = -g -v -Iiniparser
else
CFLAGS  = -g -v -arch i386 -Iiniparser
endif

all: libiniparser.a proclaunch

libiniparser.a:
	$(MAKE) -C iniparser

proclaunch: proclaunch.c
	@(echo "compiling proclaunch; platform: $(UNAME), WIN32: $(WIN32)")
	$(CC) $(CFLAGS) -o proclaunch proclaunch.c -Iiniparser -Liniparser -liniparser

clean:
	$(RM) proclaunch
	$(MAKE) -C iniparser clean
endif
