# Makefile for mstone
# use gmake
# Builds each of the components and then packages everything

topsrcdir	= .

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

CP := cp -p
ECHO := /bin/echo

# dynamically find the optional directories names
LIBGD_DIR	= $(wildcard gd?.*)
GNUPLOT_DIR	= $(wildcard gnuplot-?.*)
PERL_DIR	= $(wildcard perl5.*)

# HACK, should figure out dynamically from PERL_DIR
PERL_REV	= 5.00503

# file that we package
GDFILES		= gd.txt gd.html gd.h libgd.* demoin.gif gddemo giftogd webgif
GNUPLOTFILES	= gnuplot gnuplot_x11 gnuplot.1 gnuplot.gih Copyright

TARBALL		= mstone-4.2-$(NSARCH)$(OBJDIR_TAG).tar.gz

all:: usage

usage::
	@$(ECHO) "gmake [ release | rpackage | debug | dpackage ]"
	@$(ECHO) "      [ DIST=/m/dist/mailstone/1999xxxx dist ]"
	@$(ECHO) "      [ all_DBG | all_OPT ]"
	@$(ECHO) "perl is" $(PERL_DIR) ". gd is" $(LIBGD_DIR) ". gnuplot is" $(GNUPLOT_DIR) "."

targets::	$(OBJDIR) mailclient

ifneq (,$(LIBGD_DIR))
targets::	libgd
endif

ifneq (,$(GNUPLOT_DIR))
targets::	gnuplot
endif

ifneq (,$(PERL_DIR))
targets::	perl
endif

$(OBJDIR):
	-mkdir -p $(OBJDIR)

mailclient::
	@$(ECHO) "\n===== [`date`] making OS_CONFIG=$(NSARCH) BUILD_VARIANT=$(BUILD_VARIANT)\n"
	cd src; $(MAKE) BUILD_VARIANT=$(BUILD_VARIANT) OBJDIR=../$(OBJDIR) PKGDIR=../$(PKGDIR) all

# Use our top level makefiles to drive the component builds
libgd::
	$(MAKE) -f gd.mk LIBGD_DIR=$(LIBGD_DIR) libgd

gnuplot::
	$(MAKE) -f gnuplot.mk LIBGD_DIR=$(LIBGD_DIR) GNUPLOT_DIR=$(GNUPLOT_DIR) gnuplot

perl::
	$(MAKE) -f perl.mk PERL_DIR=$(PERL_DIR) PERL_REV=$(PERL_REV) perl

# Create packaging binary directories
# Note: dont make gd or gnuplot here.  For multi-OS, they are links
mkpkgdirs::	$(PKGDIR)/bin $(PKGDIR)/conf $(PKGDIR)/data $(PKGDIR)/doc

$(PKGDIR)/bin:
	mkdir -p $(PKGDIR)/bin

$(PKGDIR)/conf:
	mkdir -p $(PKGDIR)/conf

$(PKGDIR)/data:
	mkdir -p $(PKGDIR)/data

$(PKGDIR)/doc:
	mkdir -p $(PKGDIR)/doc

$(PKGDIR)/gd:
	mkdir -p $(PKGDIR)/gd

$(PKGDIR)/gnuplot:
	mkdir -p $(PKGDIR)/gnuplot


# operating system independent share-files (at least for Unix)
pkg-share-files:: mkpkgdirs
	@$(ECHO) "\n===== [`date`] making package share-files...\n"
	$(CP) mstone setup process $(PKGDIR)
	(cd $(PKGDIR); [ ! -f cleanup ] || rm -f cleanup; ln -s setup cleanup)
	(cd $(PKGDIR); [ ! -f checktime ] || rm -f checktime; ln -s setup checktime)
	(cd $(PKGDIR); [ ! -f timesync ] || rm -f timesync; ln -s setup timesync)
	$(CP) nsarch bin/*.pl $(PKGDIR)/bin
	$(CP) conf/*.wld conf/*.html $(PKGDIR)/conf
	$(CP) data/*.msg $(PKGDIR)/data
	$(CP) doc/*.html doc/*.gif $(PKGDIR)/doc
	$(CP) INSTALL $(PKGDIR)
	$(CP) README $(PKGDIR)
	$(CP) ChangeLog $(PKGDIR)
	$(CP) LICENSE $(PKGDIR)
	@$(ECHO) "\n===== [`date`] making package share-files done.\n"

# split out OS specific file so that combined packaging possible (set PKGDIR)
pkg-arch-files-gd::	 $(PKGDIR)/gd $(OBJDIR)/gd/libgd.a
	$(CP) $(addprefix $(OBJDIR)/gd/, $(GDFILES)) $(PKGDIR)/gd
	-$(STRIP) $(PKGDIR)/gd/webgif $(PKGDIR)/gd/giftogd $(PKGDIR)/gd/gddemo

pkg-arch-files-gnuplot::	$(PKGDIR)/gnuplot $(OBJDIR)/gnuplot/gnuplot
	$(CP) $(addprefix $(OBJDIR)/gnuplot/, $(GNUPLOTFILES)) $(PKGDIR)/gnuplot
	-$(STRIP) $(PKGDIR)/gnuplot/gnuplot $(PKGDIR)/gnuplot/gnuplot_x11

ifneq (,$(LIBGD_DIR))
pkg-arch-files::	pkg-arch-files-gd
endif

ifneq (,$(GNUPLOT_DIR))
pkg-arch-files::	pkg-arch-files-gnuplot
endif

pkg-arch-files::	$(PKGDIR)/bin $(OBJDIR)/mailclient
	$(CP) $(OBJDIR)/mailclient $(PKGDIR)/bin
	-$(STRIP) $(PKGDIR)/bin/mailclient
	@$(ECHO) "\n===== [`date`] making package arch-files done.\n"

pkg-perl-files:: $(OBJDIR)/perl/perl
	@$(ECHO) "\n===== [`date`] making package perl-files...\n"
	@[ -d $(PKGDIR)/perl ] || mkdir -p $(PKGDIR)/perl
	$(MAKE) -f perl.mk PERL_DIR=$(PERL_DIR) PERL_REV=$(PERL_REV) BUILD_VARIANT=$(BUILD_VARIANT) OBJDIR=$(OBJDIR) PKGDIR=$(PKGDIR) package-perl
	find $(PKGDIR)/perl/lib -name .packlist -exec rm {} \; -print
	-$(STRIP) $(PKGDIR)/perl/bin/perl
	-$(STRIP) $(PKGDIR)/perl/bin/a2p
	@$(ECHO) "\n===== [`date`] making package perl-files done.\n"

# for combined packaging, this shouldnt be part of pkg-arch-files (special case)
ifneq (,$(PERL_DIR))
pkg::	pkg-perl-files
endif

pkg::	targets build/$(TARBALL)

build/$(TARBALL):	 pkg-share-files pkg-arch-files
	@$(ECHO) "\n===== [`date`] making os package file...\n"
	-rm -f build/$(TARBALL)
	cd $(dir $(PKGDIR)) && \
	  tar cf - . | gzip > ../../$(TARBALL)
	@$(ECHO) "\n===== [`date`] making os package file done.\n"

########################################################################
# Generate a combined build for every Unix OS that is already packaged
# NT has to be done seperately because it has different file names
# We have to nuke some old parts, because permissions wont allow overwrites
# Finally, dont ship perl development headers and libraries
all_DBG all_OPT::
	@$(ECHO) "\n===== [`date`] unified packaging for $@...\n"
	$(MAKE) NSARCH=$@ OBJDIR_TAG='' PKGDIR=$(topsrcdir)/build/package/$@.OBJ/mstone pkg-share-files
	./ospkg.sh $@.OBJ $(notdir $(shell ls -d build/package/[A-Z]*$(subst all,,$@.OBJ)))
	@$(ECHO) "\n===== [`date`] making unified compressed tar file...\n"
	-rm -f ./build/mstone-4.2-$@.tar.gz
	(cd ./build/package/$@.OBJ && tar cf - . | gzip > ../../mstone-4.1-$@.tar.gz)
	@$(ECHO) "\n===== [`date`] making unified packaging for $@ done.\n"


########################################################################
# Install and re-map short Linux name to standard Netscape convention
dist::
	@[ "$(DIST)" != "" ] || ($(MAKE) usage && /bin/false)
	@[ ! -d "$(DIST)" ] || ($(ECHO) "Error: $(DIST) already exists" && \
		$(MAKE) usage && /bin/false)
	mkdir -p $(DIST)
	cd build/package; $(CP) -R * $(DIST)
	cd $(DIST); for l in Linux*_???.OBJ ; do \
		nn=`echo $$l | sed -e 's/_OPT/_glibc_PTH_OPT/' | sed -e 's/_DBG/_glibc_PTH_DBG/'`; \
		ln -s $$l $$nn; done

release::
	@$(ECHO) "\n===== [`date`] making release build...\n"
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT targets
	@$(ECHO) "\n===== [`date`] making release build done.\n"

rpackage:: release
	@$(ECHO) "\n===== [`date`] making release package...\n"
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT pkg
	@$(ECHO) "\n===== [`date`] making release package done.\n"

debug::	targets
#	@$(ECHO) "\n===== [`date`] making debug build...\n"
#	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG targets
#	@$(ECHO) "\n===== [`date`] making debug build done.\n"

dpackage:: pkg
#dpackage:: debug
#	@$(ECHO) "\n===== [`date`] making debug package...\n"
#	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG pkg
#	@$(ECHO) "\n===== [`date`] making debug package done.\n"

cleanvariant::
	(cd src; $(MAKE) OBJDIR=../$(OBJDIR) clean)
	rm -rf $(PKGDIR)

clean::
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT cleanvariant
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG cleanvariant

distcleanvariant::
	(cd src; $(MAKE) OBJDIR=../$(OBJDIR) distclean)
	rm -rf $(OBJDIR) $(PKGDIR)

distclean:: clean
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT distcleanvariant
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG distcleanvariant
