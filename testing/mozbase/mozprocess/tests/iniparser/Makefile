#
# iniparser Makefile
#

# source files
SRCS = iniparser.c \
	dictionary.c

# include rules for platform determination
include platform.mk

# flags for the the various systems
ifeq ($(UNAME), Linux)
    # Compiler settings
    CC      = gcc
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
    SHLD = libtool
    CFLAGS  = -v -arch i386 -isysroot /Developer/SDKs/MacOSX10.6.sdk -fPIC -Wall -ansi -pedantic
    LDFLAGS = -arch_only i386
endif
ifeq ($(WIN32), 1)
    CC = cl
    CFLAGS = //Od //D "_WIN32" //D "WIN32" //D "_CONSOLE" //D "_CRT_SECURE_NO_WARNINGS" //D "_UNICODE" //D "UNICODE" //Gm //EHsc //RTC1 //MDd //W3 //nologo //c //ZI //TC
    LDFLAGS = //OUT:"iniparser.lib" //NOLOGO
    LINK = lib
    RM = rm -f
endif

# windows build rules
ifeq ($(WIN32), 1)

COMPILE.c = $(CC) $(CFLAGS) -c
OBJS = $(SRCS:.c=.obj)

all: iniparser.obj dictionary.obj iniparser.lib

iniparser.obj: dictionary.obj
	@($(CC) $(CFLAGS) iniparser.c)

dictionary.obj:
	@(echo "compiling dictionary; WIN32: $(WIN32); platform: $(UNAME)")
	@($(CC) $(CFLAGS) dictionary.c)

iniparser.lib:	dictionary.obj iniparser.obj
	@(echo "linking $(OBJS)")
	@($(LINK) $(LDFLAGS) $(OBJS))
else

# *nix (and Mac) build rules
RM = rm -f
COMPILE.c = $(CC) $(CFLAGS) -c
OBJS = $(SRCS:.c=.o)

all:	libiniparser.a libiniparser.so

.c.o:
	@(echo "platform: $(UNAME), WIN32=$(WIN32); compiling $< ...")
	@($(COMPILE.c) -o $@ $<)

libiniparser.a:	$(OBJS)
	@($(AR) $(ARFLAGS) libiniparser.a $(OBJS))

ifeq ($(UNAME), Linux)
libiniparser.so:	$(OBJS)
	@$(SHLD) $(LDSHFLAGS) -o $@.0 $(OBJS) $(LDFLAGS)
else
libiniparser.so:	$(OBJS)
	@$(SHLD) -o $@.0 $(LDFLAGS) $(OBJS)
endif
endif

clean:
	$(RM) $(OBJS) libiniparser.*
