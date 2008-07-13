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
    cppflags += -dynamiclib -DDEBUG
  else
    ifeq ($(sys), MINGW32_NT-6.0)
      os = WINNT
      compiler = msvc
      cxx = cl
      so = dll
    else
      ifeq ($(sys), MINGW32_NT-5.1)
        os = WINNT
        compiler = msvc
        cxx = cl
        so = dll
      else
        $(error Sorry, your os is unknown/unsupported: $(sys))
      endif
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
        ifeq ($(machine), arm)
	  arch = arm
	else
          # FIXME: x86_64, ia64, sparc, Alpha
          $(error Sorry, your arch is unknown/unsupported: $(machine))
        endif
      endif
    endif
  endif
endif

platform = $(os)_$(arch)-$(compiler)

######################################################################

idl = IWeaveCrypto.idl
cpp_sources = WeaveCrypto.cpp WeaveCryptoModule.cpp
# will have .so / .dylib / .dll appended
target = WeaveCrypto

sdkdir ?= ${MOZSDKDIR}
destdir = ..
platformdir = $(destdir)/platform/$(platform)

xpidl = $(sdkdir)/bin/xpidl

# FIXME: we don't actually require this for e.g. clean
ifeq ($(sdkdir),)
  $(warning No 'sdkdir' variable given)
  $(warning It should point to the location of the Gecko SDK)
  $(warning For example: "make sdkdir=/foo/bar/baz")
  $(warning Or set the MOZSDKDIR environment variable to point to it)
  $(error )
endif

idl_headers = $(idl:.idl=.h)
idl_typelib = $(idl:.idl=.xpt)
cpp_objects = $(cpp_sources:.cpp=.o)
so_target = $(target:=.$(so))

headers = -I$(sdkdir)/include \
          -I$(sdkdir)/include/system_wrappers \
          -I$(sdkdir)/include/nss \
          -I$(sdkdir)/include/xpcom \
          -I$(sdkdir)/include/string \
          -I$(sdkdir)/include/pipnss \
          -I$(sdkdir)/include/nspr \
          -I$(sdkdir)/sdk/include

libdirs := $(sdkdir)/lib $(sdkdir)/bin
libs := xpcomglue_s xpcom nspr4 \
        crmf smime3 ssl3 nss3 nssutil3 \
        plds4 plc4

ifeq ($(os), linux)
  libs := xpcom_core $(libs)
endif

ifeq ($(compiler),msvc)
libdirs := $(patsubst %,-LIBPATH:%,$(libdirs))
libs := $(patsubst %,$(sdkdir)/lib/%.lib,$(libs))
cppflags += -c -nologo -O1 -GR- -TP -MT -Zc:wchar_t- -W3 -Gy $(headers) \
-DNDEBUG -DTRIMMED -D_CRT_SECURE_NO_DEPRECATE=1				\
-D_CRT_NONSTDC_NO_DEPRECATE=1 -DWINVER=0x500 -D_WIN32_WINNT=0x500	\
-D_WIN32_IE=0x0500 -DX_DISPLAY_MISSING=1 -DMOZILLA_VERSION=\"1.9pre\"	\
-DMOZILLA_VERSION_U=1.9pre -DHAVE_SNPRINTF=1 -D_WINDOWS=1 -D_WIN32=1	\
-DWIN32=1 -DXP_WIN=1 -DXP_WIN32=1 -DHW_THREADS=1 -DSTDC_HEADERS=1	\
-DWIN32_LEAN_AND_MEAN=1 -DNO_X11=1 -DHAVE_MMINTRIN_H=1			\
-DHAVE_OLEACC_IDL=1 -DHAVE_ATLBASE_H=1 -DHAVE_WPCAPI_H=1 -D_X86_=1	\
-DD_INO=d_ino
ldflags += -DLL -NOLOGO -SUBSYSTEM:WINDOWS -NXCOMPAT -SAFESEH -IMPLIB:fake.lib \
           $(libdirs) $(libs) \
           kernel32.lib user32.lib gdi32.lib winmm.lib wsock32.lib advapi32.lib
rcflags := -r $(headers)
else
  libdirs := $(patsubst %,-L%,$(libdirs))
  libs := $(patsubst %,-l%,$(libs))
  cppflags += -pipe -Os \
            -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
            -fno-common -fshort-wchar -pthread \
            -Wall -Wconversion -Wpointer-arith -Woverloaded-virtual -Wsynth \
            -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align \
            -Wno-long-long \
            -include xpcom-config.h $(headers)
  ifeq ($(os), Linux)
    ldflags += -pthread -pipe -DMOZILLA_STRICT_API \
            -Wl,-dead_strip \
            -Wl,-exported_symbol \
            -Wl,-z,defs -Wl,-h,WeaveCrypto.so \
            -Wl,-rpath-link,$(sdkdir)/bin \
            $(sdkdir)/lib/libxpcomglue_s.a \
            $(libdirs) $(libs)
  else
    cppflags += -fpascal-strings -c
    ldflags += -pthread -pipe -bundle \
            -Wl,-executable_path,$(sdkdir)/bin \
            -Wl,-dead_strip \
            -Wl,-exported_symbol \
            -Wl,_NSGetModule \
            $(libdirs) $(libs)
  endif
endif

######################################################################

.PHONY: all build install test-install clean

all: build # default target

build: $(so_target) $(idl_typelib)

install: build
	mkdir -p $(destdir)/components
	mkdir -p $(platformdir)/components
	cp $(idl_typelib) $(destdir)/components
	cp $(so_target) $(platformdir)/components

# gross hack to get around component registration for xpcshell tests
test-install: install
	ln -sf `pwd`/$(destdir)/defaults/preferences/sync.js $(sdkdir)/bin/defaults/pref/sync.js # fixme!!
	ln -sf `pwd`/$(destdir)/components/$(idl_typelib) $(sdkdir)/bin/components/$(idl_typelib)
	ln -sf `pwd`/$(platformdir)/components/$(so_target) $(sdkdir)/bin/components/$(so_target)
	rm -f $(sdkdir)/bin/components/compreg.dat
	rm -f $(sdkdir)/bin/components/xpti.dat

clean: 
	rm -f $(so_target) $(cpp_objects) $(idl_typelib) $(idl_headers) $(target:=.res) fake.lib fake.exp

# rules to build the c headers and .xpt from idl

$(idl_headers): $(idl)
	$(xpidl) -m header -I$(sdkdir)/idl $(@:.h=.idl)

$(idl_typelib): $(idl)
	$(xpidl) -m typelib -I$(sdkdir)/idl $(@:.xpt=.idl)

# "main" (internal) rules, build sources and link the component

$(cpp_objects): $(cpp_sources)
ifeq ($(cxx),cl)
	$(cxx) -Fo$@ -Fd$(@:.o=.pdb) $(cppflags) $(@:.o=.cpp)
endif
ifeq ($(os),Linux)
	# don't need object files, but we need to satisfy make
	touch $(cpp_objects)
else
	$(cxx) -o $@ $(cppflags) $(@:.o=.cpp)
endif

ifeq ($(compiler),msvc)
$(target:=.res): $(target:=.rc)
	rc -Fo$@ $(rcflags) $(target:=.rc)

$(so_target): $(idl_headers) $(cpp_objects) $(target:=.res)
	link -OUT:$@ -PDB:$(@:.dll=.pdb) $(cpp_objects) $(target:=.res) $(ldflags)
else
ifeq ($(os),Linux)
$(so_target): $(idl_headers) $(cpp_objects)
	$(cxx) $(cppflags) -o $@ $(cpp_sources) $(ldflags)
else
$(so_target): $(idl_headers) $(cpp_objects)
	$(cxx) -o $@ $(ldflags) $(cpp_objects)
endif
endif
	chmod +x $@
#	strip $@
