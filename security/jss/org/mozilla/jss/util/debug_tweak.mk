
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
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

# Since Java doesn't support preprocessing, we need to make two versions
# of Debug.java: one has debugging enabled, the other has debugging
# disabled.  Since the class is called Debug, the file must be called
# Debug.java.  So we actually have two versions of the file, and we
# copy one of them to Debug.java depending on whether we are building
# debuggable or not.  A hack, to be sure, and I'm open to better ideas.
# (nicolson)

ifdef BUILD_OPT
	JSS_DEBUG_SOURCE_FILE = Debug_ship.java
else
	JSS_DEBUG_SOURCE_FILE = Debug_debug.java
endif

# Since we're introducing new rules before the global rules.mk, we will
# wipe out the default rule.  So put this here to keep "all" the default.
jss_util_all: all

export::
	@echo "Copying $(JSS_DEBUG_SOURCE_FILE) to Debug.java"
	cp $(JSS_DEBUG_SOURCE_FILE) Debug.java
	chmod 0644 Debug.java

clean::
	rm -f Debug.java
