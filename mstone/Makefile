# Makefile for mstone
# use gmake
# Builds each of the components and then packages everything

topsrcdir	= .

CP := cp -p

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif


# dynamically find the optional directories names
LIBGD_DIR	= $(wildcard gd?.*)
GNUPLOT_DIR	= $(wildcard gnuplot-?.*)
PERL_DIR	= $(wildcard perl5.*)

# file that we package
GDFILES		= gd.txt gd.html libgd.* gd.h demoin.gif gddemo giftogd webgif
GNUPLOTFILES	= gnuplot Copyright gnuplot_x11 gnuplot.1 gnuplot.gih 

VERSION		= 4.3
TARBALL		= mstone-$(VERSION)-$(NSARCH)$(OBJDIR_TAG).tar.gz
ZIPFILE		= mstone-$(VERSION)-$(NSARCH)$(OBJDIR_TAG).zip
TARBALL_ALL	= mstone-$(VERSION)-all$(OBJDIR_TAG).tar.gz
ZIPFILE_ALL	= mstone-$(VERSION)-all$(OBJDIR_TAG).zip

NTFILES		= mstone.bat process.bat setup.bat

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
libgd $(OBJDIR)/gd/libgd.a::
	$(MAKE) -f gd.mk LIBGD_DIR=$(LIBGD_DIR) libgd

gnuplot $(OBJDIR)/gnuplot/gnuplot::
	$(MAKE) -f gnuplot.mk LIBGD_DIR=$(LIBGD_DIR) GNUPLOT_DIR=$(GNUPLOT_DIR) gnuplot

perl $(OBJDIR)/perl/perl::
	$(MAKE) -f perl.mk PERL_DIR=$(PERL_DIR) perl

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

$(PKGDIR)/perl:
	mkdir -p $(PKGDIR)/perl


# operating system independent share-files (at least for Unix)
pkg-share-files:: mkpkgdirs
	@$(ECHO) "\n===== [`date`] making package share-files...\n"
	$(CP) mstone process $(PKGDIR)
	(cd $(PKGDIR); [ ! -f setup ] || rm -f setup; ln -s mstone setup)
	(cd $(PKGDIR); [ ! -f cleanup ] || rm -f cleanup; ln -s mstone cleanup)
	(cd $(PKGDIR); [ ! -f checktime ] || rm -f checktime; ln -s mstone checktime)
	(cd $(PKGDIR); [ ! -f timesync ] || rm -f timesync; ln -s mstone timesync)
	$(CP) nsarch bin/*.pl $(PKGDIR)/bin
	$(CP) conf/*.wld conf/*.wld.in conf/*.html $(PKGDIR)/conf
	$(CP) data/*.msg $(PKGDIR)/data
	$(CP) doc/*.html doc/*.gif $(PKGDIR)/doc
	$(CP) INSTALL $(PKGDIR)
	$(CP) README $(PKGDIR)
	$(CP) ChangeLog $(PKGDIR)
	$(CP) LICENSE $(PKGDIR)

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
	@$(ECHO) "\n===== [`date`] making package arch-files...\n"
	$(CP) $(OBJDIR)/mailclient $(PKGDIR)/bin
	-$(STRIP) $(PKGDIR)/bin/mailclient

pkg-perl-files:: $(PKGDIR)/bin $(OBJDIR)/perl/perl
	@$(ECHO) "\n===== [`date`] making package perl-files...\n"
	$(MAKE) -f perl.mk PERL_DIR=$(PERL_DIR) \
		BUILD_VARIANT=$(BUILD_VARIANT) \
		OBJDIR=$(OBJDIR) PKGDIR=$(PKGDIR) package-perl
	find $(PKGDIR)/perl/lib -name .packlist -exec rm {} \; -print
	-$(STRIP) $(PKGDIR)/perl/bin/perl
	-$(STRIP) $(PKGDIR)/perl/bin/a2p

# for combined packaging, this should not be part of pkg-arch-files
# perl is handled seperately do to its size
ifneq (,$(PERL_DIR))
pkg::	pkg-perl-files
endif

pkg::	targets pkg-share-files pkg-arch-files

tarball:	build/$(TARBALL)

build/$(TARBALL):
	@$(ECHO) "\n===== [`date`] making os tar file...\n"
	-rm -f build/$(TARBALL)
	cd $(dir $(PKGDIR)) && tar cf - . | gzip > ../../$(TARBALL)

zipfile:	build/$(ZIPFILE)

build/$(ZIPFILE):
	@$(ECHO) "\n===== [`date`] making os zip file...\n"
	-rm -f build/$(ZIPFILE)
	cd $(dir $(PKGDIR)) && zip -r -q ../../$(ZIPFILE) .


########################################################################
# Generate a combined build for every Unix OS that is already packaged
# NT has to be done seperately because it has different file names
# We have to nuke some old parts, because permissions wont allow overwrites
# Finally, dont ship perl development headers and libraries
all_DBG all_OPT::
	@$(ECHO) "===== [`date`] unified packaging for $@..."
	$(MAKE) NSARCH=$@ OBJDIR_TAG='' \
		PKGDIR=$(topsrcdir)/build/package/$@.OBJ/mstone pkg-share-files
	./ospkg.sh $@.OBJ \
	    $(notdir $(shell ls -d build/package/[A-Z]*$(subst all,,$@.OBJ)))
	[ -d $(topsrcdir)/build/package/$@.OBJ/mstone/bin/WINNT4.0 ] \
		&& cp -p $(NTFILES) $(topsrcdir)/build/package/$@.OBJ/mstone; :
	-rm -f ./build/mstone-$(VERSION)-$@.tar.gz
	cd ./build/package/$@.OBJ \
		&& tar cf - . | gzip > ../../mstone-$(VERSION)-$@.tar.gz
	-rm -f ./build/mstone-$(VERSION)-$@.zip
	cd ./build/package/$@.OBJ \
		&& zip -r -q ../../mstone-$(VERSION)-$@.zip .


########################################################################
# Copy all the packaged trees to the distribution site
# Copy in the unified tarball and zip file
# Link mstone to mailstone for Netscape back compatibility
# Re-map short Linux name to standard Netscape convention
dist::
	@[ "$(DIST)" != "" ] || ($(MAKE) usage && /bin/false)
	@[ ! -d "$(DIST)" ] || ($(ECHO) "Error: $(DIST) already exists" && \
		$(MAKE) usage && /bin/false)
	mkdir -p $(DIST)
	cp -p build/mstone-$(VERSION)-all_*.*[a-z] $(DIST)/
	(cd build/package; tar cf - *.OBJ) | (cd $(DIST); tar xf - )
	for l in $(DIST)/*_???.OBJ ; do \
		(cd $$l; ln -s mstone mailstone); done
	cd $(DIST); for l in Linux*_???.OBJ ; do \
		nn=`echo $$l | sed -e 's/_OPT/_glibc_PTH_OPT/' | sed -e 's/_DBG/_glibc_PTH_DBG/'`; \
		ln -s $$l $$nn; done

# since the default is release mode, this can just work off a dependency
release::	targets
#release::
#	@$(ECHO) "\n===== [`date`] making release build..."
#	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT targets
	@$(ECHO) "\n===== [`date`] making release build done."

rpackage:: release pkg #tarball zipfile
#rpackage:: release
#	@$(ECHO) "\n===== [`date`] making release package..."
#	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT pkg tarball zipfile
	@$(ECHO) "\n===== [`date`] making release package done."

# since the default is release mode, start a make with the right mode
debug::	
	@$(ECHO) "\n===== [`date`] making debug build..."
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG targets
	@$(ECHO) "\n===== [`date`] making debug build done."

# We dont usually bother to tar up a debug build
dpackage:: debug
	@$(ECHO) "\n===== [`date`] making debug package..."
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG pkg
	@$(ECHO) "\n===== [`date`] making debug package done."

# These are old and may be broken
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
