
# GNU Makefile for MIME 
#

MSGSDK_ROOT = ../../../../C
CONFIG_ROOT = ../../../../CONFIG
BUILD_ROOT =  ../../../../C
MKDIR = mkdir -p

LIBDIR = $(OBJDIR)/lib
OBJDEST = $(LIBDIR)/libmime
LIBNAME = libmime

include $(CONFIG_ROOT)/msgsdk.mk

LOCAL_INCDIR = ../include
GLOBAL_INCDIR = ../../../include
CFLAGS += -I$(LOCAL_INCDIR) -I$(GLOBAL_INCDIR)

ifeq ($(ARCH), AIX)
LIBNAME = libmime
DLL_SUFFIX = a
endif


#
# Build full target library file and path
#
ifeq ($(ARCH), AIX)
LIBTARGET = $(addsuffix _shr.a, \
        $(addprefix $(LIBDIR)/, $(LIBNAME)))
else
LIBTARGET = $(addsuffix .$(DLL_SUFFIX), \
        $(addprefix $(LIBDIR)/, $(LIBNAME)))
endif

MIME_OBJS= mime.o mime_internal.o mimeparser.o util.o vector.o mimeDataSink.o

OBJS = $(addprefix $(OBJDEST)/, $(MIME_OBJS)) 

all:	$(OBJDEST) $(LIBDIR) $(LIBTARGET)

clean:
	rm -rf $(OBJDEST)*
	rm -rf $(OBJDEST)/*

$(LIBDIR):
	echo creating $(LIBDIR)
	$(MKDIR) $(LIBDIR)

$(OBJDEST):
	echo creating $(OBJDEST)
	$(MKDIR) $(OBJDEST)

ifeq ($(ARCH), WINNT)
$(LIBTARGET): $(OBJS)
	$(PURIFY) $(LINK_DLL) /DEF:mime.def $(OBJS)
else
ifeq ($(ARCH), AIX)
$(LIBTARGET): $(OBJS)
	$(LD) -o $(LIBTARGET) $(OBJS) $(DLL_LDFLAGS) -bE:mime.exp -lc_r
else
ifeq ($(ARCH), IRIX)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -exports_file mime.exp -soname $(LIBNAME).so -o $(LIBTARGET) $(OBJS)
else
ifeq ($(ARCH), IRIX64)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -exports_file mime.exp -soname $(LIBNAME).so -o $(LIBTARGET) $(OBJS)
else
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -o $(LIBTARGET) $(OBJS)
endif
endif
endif
endif
