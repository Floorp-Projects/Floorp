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
# The Initial Developer of the Original Code is
# Mozilla Corporation
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dan Mills <thunder@mozilla.com> (original author)
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

idl = IWeaveCrypto.idl
cpp_sources = WeaveCrypto.cpp WeaveCryptoModule.cpp
target = WeaveCrypto # will have .so / .dylib / .dll appended

sdkdir =
destdir = ..
platformdir = $(destdir)/platform/$(platform)

xpidl = $(sdkdir)/bin/xpidl

# FIXME: we don't actually require this for e.g. clean
ifndef sdkdir
  $(warning No 'sdkdir' variable given)
  $(warning It should point to the location of the Gecko SDK)
  $(warning For example: "make sdkdir=/foo/bar/baz")
  $(error )
endif

######################################################################

headers = -I$(sdkdir)/include \
          -I$(sdkdir)/include/system_wrappers \
          -I$(sdkdir)/include/nss \
          -I$(sdkdir)/include/xpcom \
          -I$(sdkdir)/include/string \
          -I$(sdkdir)/include/pipnss \
          -I$(sdkdir)/include/nspr \
          -I$(sdkdir)/sdk/include

cppflags += -c -pipe -Os \
            -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
            -fpascal-strings -fno-common -fshort-wchar -pthread \
            -Wall -Wconversion -Wpointer-arith -Woverloaded-virtual -Wsynth \
            -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align \
            -Wno-long-long \
            -include xpcom-config.h $(headers)

libdirs = -L$(sdkdir)/lib -L$(sdkdir)/bin
libs = -lxpcomglue_s -lxpcom \
       -lcrmf -lsmime3 -lssl3 -lnss3 -lnssutil3 -lsoftokn3 \
       -lplds4 -lplc4 -lnspr4

ldflags += -pthread -pipe -bundle \
           -Wl,-executable_path,$(sdkdir)/bin \
           -Wl,-dead_strip \
           -Wl,-exported_symbol \
           -Wl,_NSGetModule \
           $(libdirs) $(libs)

######################################################################
# Platform detection

sys := $(shell uname -s)

ifeq ($(sys), Linux)
  os = Linux
  compiler = gcc3
  cxx = c++
  so = so
  cppflags += -shared
else
  ifeq ($(sys), Darwin)
    os = Darwin
    compiler = gcc3
    cxx = c++
    so = dylib
    cppflags += -dynamiclib
  else
    ifeq ($(os), MINGW32_NT-5.1)
      $(error Sorry, windows is not supported yet)
      os = WINNT
      compiler = msvc
      cxx = c++ # fixme
      so = dll
      cppflags += -shared
    else
      $(error Sorry, your os is unknown/unsupported: $(os))
    endif
  endif
endif

machine := $(shell uname -m)

ifeq ($(machine), i386)
  arch = x86
else
  ifeq ($(machine), i586)
    arch = x86
  else
    ifeq ($(machine), i686)
      arch = x86
    else
      ifeq ($(machine), ppc) # FIXME: verify
        arch = ppc
      else
        # FIXME: x86_64, ia64, sparc, Alpha
        $(error Sorry, your arch is unknown/unsupported: $(machine))
      endif
    endif
  endif
endif

platform = $(os)_$(arch)-$(compiler)

idl_headers = $(idl:.idl=.h)
idl_typelib = $(idl:.idl=.xpt)
cpp_objects = $(cpp_sources:.cpp=.o)
so_target = $(target:=.$(so))

######################################################################

all: build # default target

build: $(so_target) $(idl_typelib)

install: build
	mkdir -p $(destdir)/components
	mkdir -p $(platformdir)/components
	cp $(idl_typelib) $(destdir)/components
	cp $(so_target) $(platformdir)/components

clean: 
	rm -f $(so_target) $(cpp_objects) $(idl_typelib) $(idl_headers)

# rules to build the c headers and .xpt from idl

$(idl_headers): $(idl)
	$(xpidl) -m header -I$(sdkdir)/idl $(@:.h=.idl)

$(idl_typelib): $(idl)
	$(xpidl) -m typelib -I$(sdkdir)/idl $(@:.xpt=.idl)

# "main" (internal) rules, build sources and link the component

$(cpp_objects): $(cpp_sources)
	$(cxx) -o $@ $(cppflags) $(@:.o=.cpp)

$(so_target): $(idl_headers) $(cpp_objects)
	$(cxx) -o $@ $(ldflags) $(cpp_objects)
	chmod +x $@
#	strip $@
