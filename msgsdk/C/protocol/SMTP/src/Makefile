
# GNU Makefile for smtp
#

MSGSDK_ROOT = ../../../../C
CONFIG_ROOT = ../../../../CONFIG
BUILD_ROOT =  ../../../../C
MKDIR = mkdir -p

LIBDIR = $(OBJDIR)/lib
OBJDEST = $(LIBDIR)/libsmtp
LIBNAME = libsmtp

include $(CONFIG_ROOT)/msgsdk.mk

LOCAL_INCDIR = ../include
GLOBAL_INCDIR = ../../../include
CFLAGS += -I$(LOCAL_INCDIR) -I$(GLOBAL_INCDIR)

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

SMTP_OBJS= smtp.o
        
#COMMON_OBJS= $(LIBDIR)/libcomm/nsio.o \
#		$(LIBDIR)/libcomm/nssocket.o \
#		$(LIBDIR)/libcomm/linklist.o

OBJS = $(addprefix $(OBJDEST)/, $(SMTP_OBJS)) 

all:	$(OBJDEST) $(LIBDIR) $(LIBTARGET)

test:
	echo $(OBJDIR)
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
	$(PURIFY) $(LINK_DLL) /DEFAULTLIB:WSOCK32.LIB /DEFAULTLIB:$(LIBDIR)/LIBCOMM.LIB /DEF:smtp.def $(OBJS)
else
ifeq ($(ARCH), AIX)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -o $(LIBTARGET) $(OBJS) -bE:smtp.exp -L$(LIBDIR) -lcomm_shr -lc_r
else
ifeq ($(ARCH), IRIX)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -exports_file smtp.exp -soname $(LIBNAME).so -o $(LIBTARGET) $(OBJS)
else
ifeq ($(ARCH), IRIX64)
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -exports_file smtp.exp -soname $(LIBNAME).so -o $(LIBTARGET) $(OBJS)
else
$(LIBTARGET): $(OBJS)
	$(LD) $(DLL_LDFLAGS) -o $(LIBTARGET) $(OBJS)
endif
endif
endif
endif
