# Makefile for mailstone
# use gmake

OS_CONFIG := $(shell ./nsarch)
OBJDIR=$(OS_CONFIG)$(OBJDIR_TAG).OBJ
BUILDDIR=build/$(OBJDIR)
PKGDIR=build/package/$(OBJDIR)/mailstone
CP := cp -p
STRIP := strip
ECHO := /bin/echo

all:: usage

usage::
	@$(ECHO) "gmake [ release | rpackage | debug | dpackage ]"
	@$(ECHO) "      [ DIST=/m/dist/mailstone/1999xxxx dist ]"
	@$(ECHO) "      [ unix_DBG | unix_OPT ]"

targets:: mailclient

mailclient::
	@$(ECHO) "\n===== [`date`] making OS_CONFIG=$(OS_CONFIG) BUILD_VARIANT=$(BUILD_VARIANT)\n"
	(cd src; $(MAKE) BUILD_VARIANT=$(BUILD_VARIANT) OBJDIR=../$(BUILDDIR) PKGDIR=../$(PKGDIR) all)

GDFILES		= gd.txt gd.html gd.h libgd.* demoin.gif gddemo giftogd webgif
GNUPLOTFILES	= gnuplot gnuplot_x11 gnuplot.1 gnuplot.gih Copyright

# Create packaging binary directories
# Note: dont make gd or gnuplot here.  For multi-OS, they are links
mkpkgdirs::
	[ -d $(PKGDIR)/bin ] || mkdir -p $(PKGDIR)/bin
	[ -d $(PKGDIR)/conf ] || mkdir -p $(PKGDIR)/conf
	[ -d $(PKGDIR)/data ] || mkdir -p $(PKGDIR)/data

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
	#$(CP) doc/MailStone.html $(PKGDIR)/doc
	$(CP) INSTALL $(PKGDIR)
	$(CP) README $(PKGDIR)
	$(CP) ChangeLog $(PKGDIR)
	$(CP) LICENSE $(PKGDIR)
	@$(ECHO) "\n===== [`date`] making package share-files done.\n"

# split out OS specific file so that combined packaging possible (set PKGDIR)
###pkg-arch-files:: mkpkgdirs $(BUILDDIR)/mailclient $(BUILDDIR)/gd/libgd.a \
###	  $(BUILDDIR)/gnuplot/gnuplot
pkg-arch-files:: mkpkgdirs $(BUILDDIR)/mailclient
	@$(ECHO) "\n===== [`date`] making package arch-files...\n"
	[ -d $(PKGDIR)/gd ] || mkdir -p $(PKGDIR)/gd
###	[ -d $(PKGDIR)/gnuplot ] || mkdir -p $(PKGDIR)/gnuplot
	$(CP) $(BUILDDIR)/mailclient $(PKGDIR)/bin
###	$(CP) $(addprefix $(BUILDDIR)/gd/, $(GDFILES)) $(PKGDIR)/gd
###	$(CP) $(addprefix $(BUILDDIR)/gnuplot/, $(GNUPLOTFILES)) $(PKGDIR)/gnuplot
ifeq ($(BUILD_VARIANT),release)
	-$(STRIP) $(PKGDIR)/bin/mailclient
###	-$(STRIP) $(PKGDIR)/gd/webgif $(PKGDIR)/gd/giftogd $(PKGDIR)/gd/gddemo
###	-$(STRIP) $(PKGDIR)/gnuplot/gnuplot $(PKGDIR)/gnuplot/gnuplot_x11
endif
	@$(ECHO) "\n===== [`date`] making package arch-files done.\n"

pkg-perl-files:: $(BUILDDIR)/perl/perl
	@$(ECHO) "\n===== [`date`] making package perl-files...\n"
	@[ -d $(PKGDIR)/perl ] || mkdir -p $(PKGDIR)/perl
	(cd src; $(MAKE) BUILD_VARIANT=$(BUILD_VARIANT) OBJDIR=../$(BUILDDIR) PKGDIR=../$(PKGDIR) package-perl)
	find $(PKGDIR)/perl/lib -name .packlist -exec rm {} \; -print
ifeq ($(BUILD_VARIANT),release)
	-$(STRIP) $(PKGDIR)/perl/bin/perl
	-$(STRIP) $(PKGDIR)/perl/bin/a2p
endif
	@$(ECHO) "\n===== [`date`] making package perl-files done.\n"

###pkg:: targets pkg-share-files pkg-arch-files pkg-perl-files
pkg:: targets pkg-share-files pkg-arch-files
	@$(ECHO) "\n===== [`date`] making os package file...\n"
	-rm -f ./build/mailstone-4.1-$(OS_CONFIG)$(OBJDIR_TAG).tar.gz
	(cd $(dir $(PKGDIR)) && \
	  tar cf - . | gzip > ../../mailstone-4.1-$(OS_CONFIG)$(OBJDIR_TAG).tar.gz)
	@$(ECHO) "\n===== [`date`] making os package file done.\n"

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

# Generate a combined build for every Unix OS that is already built and packaged
# NT has to be done seperately because it has different file names
# We have to nuke some old parts, because permissions wont allow overwrites
# Finally, dont ship perl development headers and libraries
unix_DBG unix_OPT::
	@$(ECHO) "\n===== [`date`] making unified packaging for $@...\n"
	$(MAKE) OBJDIR=$@.OBJ pkg-share-files
	(\
	pkgdir=./build/package/$@.OBJ/mailstone; export pkgdir; \
	oslist="$(notdir $(shell ls -d build/[A-V]*$(subst unix,,$@.OBJ)))"; \
	goodperl=`ls -d ./build/package/*/mailstone/perl/man/man1 | head -1 | sed -e 's:perl/man/man1::'`; \
	rm -rf $${pkgdir}/perl/lib/*; \
	$(ECHO) "\n===== [`date`] making shared perl packaging from $$goodperl...\n"; \
	(cd $${goodperl} && tar cf - perl/man) | (cd $${pkgdir} && tar xf -); \
	cp $${goodperl}/perl/Artistic $${pkgdir}/perl; \
	for obj in $$oslist ; do \
	    arch=`echo $${obj} | sed -e 's/_OPT.OBJ//' | sed -e 's/_DBG.OBJ//'` ;\
	    perlarchdir=$${pkgdir}/perl/arch/$${arch}; export perlarchdir; \
	    $(MAKE) PKGDIR=$${pkgdir}/bin/$${arch} BUILDDIR=./build/$${obj} pkg-arch-files ; \
	    $(ECHO) "\n===== [`date`] making arch perl packaging $${perlarchdir}...\n"; \
	    (cd ./build/package/$${obj}/mailstone && tar cf - perl/lib) | (cd $${pkgdir} && tar xf -); \
	    [ -d $${perlarchdir} ] || mkdir -p $${perlarchdir}; \
	    (cd ./build/package/$${obj}/mailstone/perl/bin && tar cf - .) | (cd $${perlarchdir} && tar xf -); \
	done; \
	rm -rf $${pkgdir}/perl/lib/5.00503/*/CORE/; \
	: )
	@$(ECHO) "\n===== [`date`] making unified compressed tar file...\n"
	-rm -f ./build/mailstone-4.1-$@.tar.gz
	(cd ./build/package/$@.OBJ && tar cf - . | gzip > ../../mailstone-4.1-$@.tar.gz)
	@$(ECHO) "\n===== [`date`] making unified packaging for $@ done.\n"

release::
	@$(ECHO) "\n===== [`date`] making release build...\n"
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT targets
	@$(ECHO) "\n===== [`date`] making release build done.\n"

rpackage:: release
	@$(ECHO) "\n===== [`date`] making release package...\n"
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT pkg
	@$(ECHO) "\n===== [`date`] making release package done.\n"

debug::
	@$(ECHO) "\n===== [`date`] making debug build...\n"
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG targets
	@$(ECHO) "\n===== [`date`] making debug build done.\n"

dpackage:: debug
	@$(ECHO) "\n===== [`date`] making debug package...\n"
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG pkg
	@$(ECHO) "\n===== [`date`] making debug package done.\n"

cleanvariant::
	(cd src; $(MAKE) OBJDIR=../build/$(OBJDIR) clean)
	rm -rf $(PKGDIR)

clean::
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT cleanvariant
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG cleanvariant

distcleanvariant::
	(cd src; $(MAKE) OBJDIR=../build/$(OBJDIR) distclean)
	rm -rf $(BUILDDIR) $(PKGDIR)

distclean:: clean
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT distcleanvariant
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG distcleanvariant
