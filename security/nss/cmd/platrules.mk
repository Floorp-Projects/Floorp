#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

show:
	@echo "DEFINES 		= ${DEFINES}"
	@echo "EXTRA_LIBS	= >$(EXTRA_LIBS)<"
	@echo "IMPORT_LIBRARY 	= $(IMPORT_LIBRARY)"
	@echo "LIBRARY 		= $(LIBRARY)"
	@echo "OBJS 		= $(OBJS)"
	@echo "OS_ARCH 		= $(OS_ARCH)"
	@echo "PROGRAM 		= >$(PROGRAM)<"
	@echo "PROGRAMS 	= $(PROGRAMS)"
	@echo "SHARED_LIBRARY 	= $(SHARED_LIBRARY)"
	@echo "TARGETS 		= >$(TARGETS)<"

showplatform:
	@echo $(PLATFORM)
