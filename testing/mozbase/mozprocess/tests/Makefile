#
# proclaunch  tests Makefile
#
UNAME := $(shell uname -s)
ifeq ($(UNAME), MINGW32_NT-6.1)
WIN32 = 1
endif
ifeq ($(UNAME), MINGW32_NT-5.1)
WIN32 = 1
endif

ifeq ($(WIN32), 1)
CC      = cl
LINK    = link
CFLAGS  = //Od //I "iniparser" //D "WIN32" //D "_WIN32" //D "_DEBUG" //D "_CONSOLE" //D "_UNICODE" //D "UNICODE" //Gm //EHsc //RTC1 //MDd //W3 //nologo //c //ZI //TC
LFLAGS  = //OUT:"proclaunch.exe" //INCREMENTAL //LIBPATH:"iniparser\\" //NOLOGO //DEBUG //SUBSYSTEM:CONSOLE //DYNAMICBASE //NXCOMPAT //MACHINE:X86 //ERRORREPORT:PROMPT iniparser.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib
RM      = rm -f

default: all
all: iniparser proclaunch

iniparser:
	$(MAKE) -C iniparser

proclaunch.obj: proclaunch.c
	$(CC) $(CFLAGS) proclaunch.c

proclaunch: proclaunch.obj
	$(LINK) $(LFLAGS) proclaunch.obj

else
CC      = gcc
ifeq ($(UNAME), Linux)
CFLAGS  = -g -v -Iiniparser
else
CFLAGS  = -g -v -arch i386 -Iiniparser
endif

LFLAGS  = -L.. -liniparser
AR	    = ar
ARFLAGS = rcv
RM      = rm -f


default: all

all: libiniparser.a proclaunch

libiniparser.a:
	$(MAKE) -C iniparser

proclaunch: proclaunch.c
	$(CC) $(CFLAGS) -o proclaunch proclaunch.c -Iiniparser -Liniparser -liniparser

clean veryclean:
	$(RM) proclaunch 
endif
