
# GNU Makefile for pop3
#

MSGSDK_ROOT = ../../C
CONFIG_ROOT = ../../CONFIG
BUILD_ROOT = ../
MKDIR = mkdir -p

LIBDIR = $(OBJDIR)/lib
OBJDEST = $(LIBDIR)/libcomm
LIBNAME = libcomm


include $(CONFIG_ROOT)/msgsdk.mk

LOCAL_INCDIR = ../include
CFLAGS += -I$(LOCAL_INCDIR) -D_REENTRANT -D_THREAD_SAFE

ifeq ($(ARCH), AIX)
LIBNAME = libcomm_shr
DLL_SUFFIX = a
endif

#
# Build full target library file and path
#
LIBTARGET = $(addsuffix .$(DLL_SUFFIX), \
        $(addprefix $(LIBDIR)/, $(LIBNAME)))

COMMON_OBJS= nsio.o nssocket.o linklist.o nsStream.o

OBJS = $(addprefix $(OBJDEST)/, $(COMMON_OBJS)) 

all:	$(OBJDEST) $(LIBDIR) $(LIBTARGET)

clean:
	rm -rf $(OBJDEST)*
	rm -rf $(OBJDEST)/*

$(LIBDIR):
	echo OBJDEST = $(OBJDEST)
	echo LIBDIR = $(LIBDIR)
	echo LIBTARGET = $(LIBTARGET)
	echo creating $(LIBDIR)
	$(MKDIR) $(LIBDIR)

$(OBJDEST):
	echo creating $(OBJDEST)
	$(MKDIR) $(OBJDEST)

ifeq ($(ARCH), WINNT)
$(LIBTARGET): $(OBJS)
	$(PURIFY) $(LINK_DLL)  /DEF:comm.def /DEFAULTLIB:WSOCK32.LIB $(OBJS)
else
ifeq ($(ARCH), AIX)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -bnoentry -o $(LIBTARGET) $(OBJS) -bE:comm.exp -lc_r -lc
else
ifeq ($(ARCH), IRIX)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -soname $(LIBNAME).so -o $(LIBTARGET) $(OBJS)
else
ifeq ($(ARCH), IRIX64)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -soname $(LIBNAME).so -o $(LIBTARGET) $(OBJS)
else
$(LIBTARGET): $(OBJS)
	echo LIBDIR = $(LIBDIR)
	echo LIBTARGET = $(LIBTARGET)
	$(LD) $(DLL_LDFLAGS) -o $(LIBTARGET) $(OBJS)
endif
endif
endif
endif
