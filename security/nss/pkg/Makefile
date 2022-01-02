#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

CORE_DEPTH = ../..
DEPTH      = ../..

include $(CORE_DEPTH)/coreconf/config.mk

publish:
ifeq ($(OS_TARGET),Linux)
	rm -rf $(OBJDIR)
	cp -r linux $(OBJDIR)
	$(MAKE) -C $(OBJDIR) publish
endif
ifeq ($(OS_TARGET),SunOS)
	rm -rf $(OBJDIR)
	cp -r solaris $(OBJDIR)
	$(MAKE) -C $(OBJDIR) publish
endif

clean::
	rm -rf $(OBJDIR)

include $(CORE_DEPTH)/coreconf/rules.mk
