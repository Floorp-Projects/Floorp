#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Weave code.
#
# The Initial Developer of the Original Code is Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dan Mills <thunder@mozilla.com> (original author)
#   Justin Dolske <dolske@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

stage_dir=../dist/stage

sdkdir ?= ${MOZSDKDIR}

idl = IWeaveCrypto.idl
idl_typelib = $(idl:.idl=.xpt)
idl_header = $(idl:.idl=.h)

#
# The only thing to actually build here is the IDL's .xpt/.h form, which
# requires an SDK. So don't do that unless explicitly requested, and use
# the files checked into Mercurial instead.
#
all: stage

build: $(idl_typelib) $(idl_header) stage

# No SDK is needed unless you're modifying the IDL interface, in which
# case we'll need to rebuild the .h and .xpt files.
xpidl = $(sdkdir)/bin/xpidl
ifdef CROSS_COMPILE
xpidl = $(sdkdir)/host/bin/host_xpidl
endif
$(idl_typelib): $(idl)
ifeq ($(sdkdir),)
	$(warning No 'sdkdir' variable given)
	$(warning It should point to the location of the Gecko SDK)
	$(warning For example: "make sdkdir=/foo/bar/baz")
	$(warning Or set the MOZSDKDIR environment variable to point to it)
	$(error)
else
	$(xpidl) -m typelib -I$(sdkdir)/idl $(@:.xpt=.idl)
endif

$(idl_header): $(idl)
ifeq ($(sdkdir),)
	$(warning No 'sdkdir' variable given)
	$(warning It should point to the location of the Gecko SDK)
	$(warning For example: "make sdkdir=/foo/bar/baz")
	$(warning Or set the MOZSDKDIR environment variable to point to it)
	$(error)
else
	$(xpidl) -m header -I$(sdkdir)/idl $(@:.h=.idl)
endif

stage:
	mkdir -p $(stage_dir)/components
ifdef NO_SYMLINK
	cp -v $(idl_typelib) $(stage_dir)/components
	cp -v $(idl_header)  $(TOPSRCDIR)/crypto-obsolete/src
	cp -v WeaveCrypto.js $(stage_dir)/components
else
	ln -vsf `pwd`/$(idl_typelib) $(stage_dir)/components
	ln -vsf `pwd`/$(idl_header)  $(TOPSRCDIR)/crypto-obsolete/src
	ln -vsf `pwd`/WeaveCrypto.js $(stage_dir)/components
endif

clean:
	rm -f $(TOPSRCDIR)/crypto-obsolete/src/$(idl_header)
	# maybe hg revert the .xpt/.h?

.PHONY: all build stage clean
