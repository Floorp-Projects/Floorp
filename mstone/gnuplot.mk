# optional component gnuplot (can use libgd)
# This should be run using 'gmake'
########################################################################

topsrcdir	= .

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifneq ("$(GNUPLOT_DIR)", "")
GNUPLOT		= gnuplot$(EXESUFFIX)
GNUPLOT_OBJDIR	= $(OBJDIR)/gnuplot
GNUPLOT_TARGET	= $(GNUPLOT_OBJDIR)/$(GNUPLOT)
GNUPLOT_HELP	= $(GNUPLOT_OBJDIR)/gnuplot.gih
GNUPLOT_CONFIG_H = $(GNUPLOT_OBJDIR)/config.h
ifneq ("$(LIBGD_DIR)", "")
  GNUPLOT_CONFIG_OPTS = --prefix=/opt/mailstone --with-gd=../$(OBJDIR)/gd --with-png=no --without-linux-vga
endif

GNUPLOT_ALL	= $(GNUPLOT_OBJDIR) $(GNUPLOT_TARGET) $(GNUPLOT_HELP)

gnuplot:: $(GNUPLOT_ALL)

$(GNUPLOT_OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(GNUPLOT_OBJDIR)
else
	[ -d $(GNUPLOT_OBJDIR) ] || mkdir -p $(GNUPLOT_OBJDIR)
endif

$(GNUPLOT_TARGET): $(GNUPLOT_CONFIG_H)
	@$(ECHO) "\n===== [`date`] making gnuplot...\n"
	(cd $(GNUPLOT_OBJDIR); $(MAKE) MAKE=$(MAKE) all)

$(GNUPLOT_HELP):
	cp $(GNUPLOT_DIR)/docs/gnuplot.1 $(GNUPLOT_OBJDIR)
	cp $(GNUPLOT_DIR)/Copyright $(GNUPLOT_OBJDIR)
	cp $(GNUPLOT_OBJDIR)/docs/gnuplot.gih $(GNUPLOT_HELP)

$(GNUPLOT_CONFIG_H):
	@$(ECHO) "\n===== [`date`] making gnuplot config.h...\n"
	(cd $(GNUPLOT_OBJDIR); CC="$(CC) $(CFLAGS)" ../../../$(GNUPLOT_DIR)/configure $(GNUPLOT_CONFIG_OPTS))

distclean::
	[ ! -f $(GNUPLOT_OBJDIR)/Makefile ] || \
	  (cd $(GNUPLOT_OBJDIR); $(MAKE) MAKE=$(MAKE) distclean)
	$(RM) $(GNUPLOT_TARGET) $(GNUPLOT_CONFIG_H)

$(GNUPLOT_OBJDIR)/%.$(OBJ_SUFFIX): $(GNUPLOT_DIR)/%.c
ifeq ($(ARCH), WINNT)
	$(COMPILE) -c -MT $< -Fo$(GNUPLOT_OBJDIR)/$*.$(OBJ_SUFFIX)
else	
	$(COMPILE) -c $< -o $(GNUPLOT_OBJDIR)/$*.$(OBJ_SUFFIX)
endif

endif
