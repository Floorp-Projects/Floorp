# optional component libgd
# This should be run using 'gmake'
########################################################################

topsrcdir	= .

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifneq ("$(LIBGD_DIR)", "")
LIBGD_OBJDIR	= $(OBJDIR)/gd
LIBGD		= $(LIBGD_OBJDIR)/libgd.$(LIB_SUFFIX)
GDDEMO		= $(LIBGD_OBJDIR)/gddemo$(EXE_SUFFIX)
GIFTOGD		= $(LIBGD_OBJDIR)/giftogd$(EXE_SUFFIX)
WEBGIF		= $(LIBGD_OBJDIR)/webgif$(EXE_SUFFIX)

#LIBPATH	+= -L$(LIBGD_OBJDIR)
#INCLUDES	+= -I./$(LIBGD_DIR)

LIBGD_SRCS	= gd.c gdfontt.c gdfonts.c gdfontmb.c gdfontl.c gdfontg.c
LIBGD_SRCS2	= $(addprefix $(LIBGD_DIR)/, $(LIBGD_SRCS))

LIBGD_OBJS	= $(addprefix $(LIBGD_OBJDIR)/, $(LIBGD_SRCS:.c=.$(OBJ_SUFFIX)))

LIBGD_ALL	= $(LIBGD_OBJDIR) $(LIBGD) $(GDDEMO) $(GIFTOGD) $(WEBGIF)

libgd:: $(LIBGD_ALL)

$(LIBGD_OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(LIBGD_OBJDIR)
else
	[ -d $(LIBGD_OBJDIR) ] || mkdir -p $(LIBGD_OBJDIR)
endif

$(LIBGD): $(LIBGD_OBJS)
	@$(ECHO) "\n===== [`date`] making libgd...\n"
	$(AR) rc $(LIBGD) $(LIBGD_OBJS)
	cp $(LIBGD_DIR)/gd.h $(LIBGD_OBJDIR)
	cp $(LIBGD_DIR)/demoin.gif $(LIBGD_OBJDIR)
	cp $(LIBGD_DIR)/readme.txt $(LIBGD_OBJDIR)/gd.txt
	cp $(LIBGD_DIR)/index.html $(LIBGD_OBJDIR)/gd.html

$(GDDEMO): $(LIBGD) $(LIBGD_OBJDIR)/gddemo.$(OBJ_SUFFIX)
	$(COMPILE) $(LIBGD_OBJDIR)/gddemo.$(OBJ_SUFFIX) $(LIBPATH) $(LIBGD) $(LIBS) $(OS_LINKFLAGS) -o $(GDDEMO)

$(GIFTOGD): $(LIBGD) $(LIBGD_OBJDIR)/giftogd.$(OBJ_SUFFIX)
	$(COMPILE) $(LIBGD_OBJDIR)/giftogd.$(OBJ_SUFFIX) $(LIBPATH) $(LIBGD) $(LIBS) $(OS_LINKFLAGS) -o $(GIFTOGD)

$(WEBGIF): $(LIBGD) $(LIBGD_OBJDIR)/webgif.$(OBJ_SUFFIX)
	$(COMPILE) $(LIBGD_OBJDIR)/webgif.$(OBJ_SUFFIX) $(LIBPATH) $(LIBGD) $(LIBS) $(OS_LINKFLAGS) -o $(WEBGIF)

distclean::
	$(RM) $(LIBGD) $(LIBGD_OBJS)

$(LIBGD_OBJDIR)/%.$(OBJ_SUFFIX): $(LIBGD_DIR)/%.c
ifeq ($(ARCH), WINNT)
	$(COMPILE) -c -MT $< -Fo$(LIBGD_OBJDIR)/$*.$(OBJ_SUFFIX)
else	
	$(COMPILE) -c $< -o $(LIBGD_OBJDIR)/$*.$(OBJ_SUFFIX)
endif

endif
