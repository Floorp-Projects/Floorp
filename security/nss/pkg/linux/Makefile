#
# Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.2 2003/02/07 07:16:37 kirk.erickson%sun.com Exp $"
#

CORE_DEPTH = ../../..

NAME        = sun-nss
RELEASE     = 1
TOPDIR      = /usr/src/redhat
VERSION     = `grep NSS_VERSION $(CORE_DEPTH)/../dist/public/nss/nss.h \
				| sed -e 's/"$$//' -e 's/.*"//' -e 's/ .*//'`

include $(CORE_DEPTH)/coreconf/config.mk

publish:
	$(MAKE) clean
	mkdir -p usr/lib/mps
	find $(CORE_DEPTH)/../dist/$(OBJDIR)/lib -type l \
		\( -name "*.so" -o -name "*.chk" \) \
		-exec cp -L {} usr/lib/mps \;
	mkdir -p usr/include/mps
	cp -Lr $(CORE_DEPTH)/../dist/public/* usr/include/mps
	tar czvf $(NAME)-$(VERSION).tar.gz usr
	sed -e "s/NAME_REPLACE/$(NAME)/" \
		-e "s/VERSION_REPLACE/$(VERSION)/" \
		-e "s/RELEASE_REPLACE/$(RELEASE)/" \
	<$(NAME).spec >temp.spec
	echo "" >>temp.spec
	echo "%files" >>temp.spec
	echo "%defattr(-,root,root)" >>temp.spec
	echo "%dir /usr" >>temp.spec
	echo "%dir /usr/lib" >>temp.spec
	echo "%dir /usr/lib/mps" >>temp.spec
	find usr \( -name "*.so" -o -name "*.chk" \) \
		| sed -e "s-^-/-" >>temp.spec
	echo "" >>temp.spec
	echo "%files devel" >>temp.spec
	echo "%defattr(-,root,root)" >>temp.spec
	find usr -type d | sed -e "s-^-%dir /-" >>temp.spec
	find usr -type f ! \( -name "*.so" -o -name "*.chk" \) \
		| sed -e "s-^-/-" >>temp.spec
	cp $(NAME)-$(VERSION).tar.gz $(TOPDIR)/SOURCES
	rpm -ba temp.spec
	if [ ! -d RPMS ] ; then mkdir -p RPMS ; fi
	if [ ! -d SRPMS ] ; then mkdir -p SRPMS ; fi
	cp -v $(TOPDIR)/RPMS/i386/$(NAME)-$(VERSION)-* RPMS
	cp -v $(TOPDIR)/RPMS/i386/$(NAME)-devel-$(VERSION)-* RPMS
	cp -v $(TOPDIR)/SRPMS/$(NAME)-$(VERSION)-* SRPMS

clean::
	rm -rf $(TOPDIR)/BUILD/$(NAME)
	rm -rf RPMS SRPMS usr
	rm -f temp.spec
	rm -f $(NAME)-$(VERSION).tar.gz

include $(CORE_DEPTH)/coreconf/rules.mk
