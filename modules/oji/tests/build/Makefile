# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 
DIRS= \
	TestLoader \
	JM \
	TM \
	LCM \
	JNI 

TOP_DIR = ..

all: createdir check_env $(DIRS)

$(DIRS)::
	@echo "############## Building $@ ####################"
	@cd $(TOP_DIR)/src/$@; \
		if test ! -f depend.mak; then >depend.mak; fi; \
		$(MAKE) make_depends; \
		$(MAKE) all;

check_env:
	@if test ! -d $(MOZILLA_HOME)/dist/include; then echo "WARNING: MOZILLA_HOME variable is incorrect !"; exit 1; fi

createdir:
	@mkdir -p bin
	@mkdir -p classes

clean:
	@for d in $(DIRS); do \
		rm -rf $(TOP_DIR)/src/$$d/obj; \
		find $(TOP_DIR)/src/$$d -name depend.mak -type f -exec rm -f {} \;; \
		find $(TOP_DIR)/src/$$d -name cmd.mak -type f -exec rm -f {} \;; \
	done
	@rm -rf bin/* classes/*





