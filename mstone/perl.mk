# optional component perl
# This should be run using 'gmake'
########################################################################

topsrcdir	= .

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

PERL_OBJDIR	= $(OBJDIR)/perl
PERL_PKGDIR	= $(PKGDIR)/perl

###package:: $(PERL_PKGDIR) package-perl

$(PERL_PKGDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(PERL_PKGDIR)
	mkdir $(PERL_PKGDIR)/bin
	mkdir $(PERL_PKGDIR)/lib
	mkdir $(PERL_PKGDIR)/lib/$(PERL_REV)
	mkdir $(PERL_PKGDIR)/lib/$(PERL_REV)/$(PERL_OS)
else
	[ -d $(PERL_PKGDIR)/bin ] || mkdir -p $(PERL_PKGDIR)/bin
	[ -d $(PERL_PKGDIR)/lib ] || mkdir -p $(PERL_PKGDIR)/lib
# subdirs created by perl install
endif

ifeq ("$(PERL_FILES)", "")

# building our own perl
###all:: perl
all:: 

PERL_TARGET	= $(PERL_OBJDIR)/perl
PERL_SRCSTAMP	= $(PERL_OBJDIR)/perl_src.stamp
PERL_TESTSTAMP	= $(PERL_OBJDIR)/perl_test.stamp
PERL_CONFIG_H	= $(PERL_OBJDIR)/config.h
PERL_FINAL_PKGDIR = /opt/mstone/perl
PERL_ADMIN	= mailstone+perl@netscape.com
PERL_PAGER	= /bin/more

perl:: $(PERL_OBJDIR) $(PERL_TARGET) $(PERL_TESTSTAMP)

$(PERL_OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(PERL_OBJDIR)
else
	[ -d $(PERL_OBJDIR) ] || mkdir -p $(PERL_OBJDIR)
endif

$(PERL_TARGET): $(PERL_CONFIG_H)
	@$(ECHO) "\n===== [`date`] making perl...\n"
	( cd $(PERL_OBJDIR); $(MAKE) MAKE=$(MAKE) )
	@$(ECHO) "\n===== [`date`] making perl done.\n"

$(PERL_CONFIG_H): $(PERL_OBJDIR)/config.over
	@$(ECHO) "\n===== [`date`] making perl config.h...\n"
	(\
	cd $(PERL_OBJDIR); \
	rm -f config.sh makedir makedepend makeaperl config.h cflags \
	      Policy.sh Makefile writemain perl.exp perlmain.c makefile; \
	$(ECHO) MAKE=$(MAKE) MAKEFLAGS=$(MAKEFLAGS); \
	MAKEFLAGS= ; export MAKEFLAGS; \
	sh Configure -Dprefix=$(PERL_FINAL_PKGDIR) \
	       -Uinstallusrbinperl -Uusethreads -Uusedl \
	       -Dcc="$(CC)" -Dmake=$(MAKE) \
	       -Dcf_email=$(PERL_ADMIN) \
	       -Dperladmin=$(PERL_ADMIN) -Dpager=$(PERL_PAGER) $(PERL_OS_CONFIGURE) -de ; \
	)

$(PERL_OBJDIR)/config.over: $(PERL_SRCSTAMP)
	@$(ECHO) "\n===== [`date`] making perl config.over...\n"
	(\
	perl_pkgdir=`pwd`/$(PERL_PKGDIR); \
	cd $(PERL_OBJDIR); \
	$(ECHO) "\
installprefix=$${perl_pkgdir}\n\
$(ECHO) \"overriding tmp install dir from \$$prefix to \$$installprefix\"\n\
test -d \$$installprefix || mkdir \$$installprefix\n\
test -d \$$installprefix/bin || mkdir \$$installprefix/bin\n\
installarchlib=\`$(ECHO) \$$installarchlib | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installbin=\`$(ECHO) \$$installbin | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installman1dir=\`$(ECHO) \$$installman1dir | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installman3dir=\`$(ECHO) \$$installman3dir | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installprivlib=\`$(ECHO) \$$installprivlib | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installscript=\`$(ECHO) \$$installscript | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installsitelib=\`$(ECHO) \$$installsitelib | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installsitearch=\`$(ECHO) \$$installsitearch | sed \"s!\$$prefix!\$$installprefix!\"\`" \
> config.over;\
	)

$(PERL_SRCSTAMP): $(PERL_DIR)
	@$(ECHO) "\n===== [`date`] making perl src links from $(PERL_DIR) to $(PERL_OBJDIR)...\n"
	-rm -f $(PERL_SRCSTAMP)
	[ -d $(PERL_OBJDIR) ] || mkdir -p $(PERL_OBJDIR)
	-(\
	perl_dir=`pwd`/$(PERL_DIR); \
	cd $(PERL_OBJDIR); \
	for i in `(cd $${perl_dir} && find . -type d -print)` ; do \
	  $(ECHO) "linking dir $$i ..."; \
	  [ -d $$i ] || mkdir $$i; \
	  for j in `(cd $${perl_dir}/$$i; echo *)` ; do \
	    [ -f $${perl_dir}/$$i/$$j -o -h $${perl_dir}/$$i/$$j ] && ( \
	    /bin/true || $(ECHO) " $$i/$$j"; \
	    ln -s $${perl_dir}/$$i/$$j $$i/$$j); \
	  done; \
	done; \
	)
	touch $(PERL_SRCSTAMP)
	@$(ECHO) "\n===== [`date`] making perl src links done...\n"

$(PERL_TESTSTAMP): $(PERL_TARGET)
	@$(ECHO) "\n===== [`date`] making perl test-notty...\n"
	-rm -f $(PERL_TESTSTAMP)
	-( cd $(PERL_OBJDIR); $(MAKE) MAKE=$(MAKE) test-notty )
	touch $(PERL_TESTSTAMP)
	@$(ECHO) "\n===== [`date`] making perl test-notty done.\n"

package-perl:: $(PERL_PKGDIR)/Artistic
	@$(ECHO) "\n===== [`date`] making perl package done.\n"

$(PERL_PKGDIR)/Artistic:
	@$(ECHO) "\n===== [`date`] making perl package ...\n"
	( cd $(PERL_OBJDIR); $(MAKE) MAKE=$(MAKE) install )
	-rm -rf $(PERL_PKGDIR)/lib/$(PERL_REV)/*/CORE/
	cp $(PERL_OBJDIR)/Artistic $(PERL_PKGDIR)

else # PERL_FILES

# importing a perl
package-perl:: $(PERL_PKGDIR) $(PERL_TARGET)

$(PERL_TARGET):	$(PERL_FILES) $(PERL_BIN_FILES) $(PERL_LIB_FILES)
	cp $(PERL_FILES) $(PERL_PKGDIR)/
	cp $(PERL_BIN_FILES) $(PERL_PKGDIR)/bin/
	cp $(PERL_LIB_FILES) $(PERL_PKGDIR)/lib/$(PERL_REV)/
	cp $(PERL_LIB_OS_FILES) $(PERL_PKGDIR)/lib/$(PERL_REV)/$(PERL_OS)/

endif # PERL_FILES
