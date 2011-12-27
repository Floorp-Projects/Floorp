#
# iniparser Makefile
#
UNAME := $(shell uname -s)
ifeq ($(UNAME), MINGW32_NT-6.1)
WIN32 = 1
endif
ifeq ($(UNAME), MINGW32_NT-5.1)
WIN32 = 1
endif

ifeq ($(UNAME), Linux)
    # Compiler settings
    CC      = gcc
    # Ar settings to build the library
    AR	    = ar
    ARFLAGS = rcv
    SHLD = ${CC} ${CFLAGS}
    CFLAGS  = -O2 -fPIC -Wall -ansi -pedantic
    LDSHFLAGS = -shared -Wl,-Bsymbolic  -Wl,-rpath -Wl,/usr/lib -Wl,-rpath,/usr/lib
    LDFLAGS = -Wl,-rpath -Wl,/usr/lib -Wl,-rpath,/usr/lib
endif

ifeq ($(UNAME), Darwin)
    # Compiler settings
    CC      = gcc
    # Ar settings to build the library
    AR	    = ar
    ARFLAGS = rcv
    #SHLD = ${CC} ${CFLAGS}
    SHLD = libtool
    CFLAGS  = -v -arch i386 -isysroot /Developer/SDKs/MacOSX10.6.sdk -fPIC -Wall -ansi -pedantic
    LDFLAGS = -arch_only i386
endif

ifeq ($(WIN32), 1)
    CC = cl
    CFLAGS = //Od //D "_WIN32" //D "WIN32" //D "_CONSOLE" //D "_CRT_SECURE_NO_WARNINGS" //D "_UNICODE" //D "UNICODE" //Gm //EHsc //RTC1 //MDd //W3 //nologo //c //ZI //TC
    LDFLAGS = //OUT:"iniparser.lib" //NOLOGO
    LINK = lib
endif
    
ifeq ($(WIN32), 1)
SUFFIXES = .obj .c .h .lib

COMPILE.c=$(CC) $(CFLAGS) -c

#.c.obj:
#	@(echo "compiling $< ...")
#	@($(COMPILE.c) $@ $<)

all: iniparser.obj dictionary.obj iniparser.lib

SRCS = iniparser.c \
	dictionary.c
OBJS = $(SRCS:.c=.obj)

iniparser.obj: dictionary.obj
	@($(CC) $(CFLAGS) iniparser.c)

dictionary.obj:
	@($(CC) $(CFLAGS) dictionary.c)
 
iniparser.lib:	dictionary.obj iniparser.obj
	@(echo "linking $(OBJS)")
	@($(LINK) $(LDFLAGS) $(OBJS))

else
# Set RANLIB to ranlib on systems that require it (Sun OS < 4, Mac OSX)
# RANLIB  = ranlib
RANLIB = true

RM      = rm -f

# Implicit rules

SUFFIXES = .o .c .h .a .so .sl

COMPILE.c=$(CC) $(CFLAGS) -c
.c.o:
	@(echo "compiling $< ...")
	@($(COMPILE.c) -o $@ $<)


SRCS = iniparser.c \
	   dictionary.c

OBJS = $(SRCS:.c=.o)


default:	libiniparser.a libiniparser.so

libiniparser.a:	$(OBJS)
	@($(AR) $(ARFLAGS) libiniparser.a $(OBJS))
	@($(RANLIB) libiniparser.a)

ifeq ($(UNAME), Linux)
libiniparser.so:	$(OBJS)
	@$(SHLD) $(LDSHFLAGS) -o $@.0 $(OBJS) $(LDFLAGS)
else
libiniparser.so:	$(OBJS)
	@$(SHLD) -o $@.0 $(LDFLAGS) $(OBJS)
endif
endif

clean:
	$(RM) $(OBJS)

veryclean:
	$(RM) $(OBJS) libiniparser.a libiniparser.so*
	rm -rf ./html ; mkdir html
	cd test ; $(MAKE) veryclean

docs:
	@(cd doc ; $(MAKE))

check:
	@(cd test ; $(MAKE))
