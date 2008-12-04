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
#   Godwin Chan <godwinchan@hotmail.com> (Darwin Universal Binary)
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

# OS detection

sys := $(shell uname -s)

ifeq ($(sys), Darwin)
  os = Darwin
  compiler = gcc3
  cxx = c++
  so = dylib
  cppflags += -dynamiclib -DDEBUG
else
ifeq ($(sys), Linux)
  os = Linux
  compiler = gcc3
  cxx = c++
  so = so
  cppflags += -shared
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

# Arch detection

machine := $(shell uname -m)

ifeq ($(machine), arm)
  arch = arm
else
ifeq ($(machine), i386)
  arch = x86
else
ifeq ($(machine), i586)
  arch = x86
else
ifeq ($(machine), i686)
  arch = x86
else
ifeq ($(machine), Power Macintosh)
  arch = ppc
else
ifeq ($(machine), x86_64)
  arch = x86_64
else
  $(error: Sorry, your architecture is unknown/unsupported: $(machine))
endif
endif
endif
endif
endif
endif

# Universal binary so no need for $(arch) for Darwin

ifeq ($(sys), Darwin)
  platform = $(os)
else
  platform = $(os)_$(arch)-$(compiler)
endif

###################################################################
# Target and objects

ifeq ($(sys), Darwin)
  target = WeaveCrypto
  target_i386 = WeaveCrypto.i386
  target_ppc = WeaveCrypto.ppc
  so_target = $(target:=.$(so))
  so_target_i386 = $(target_i386:=.$(so))
  so_target_ppc  = $(target_ppc:=.$(so))
  cpp_objects_i386 = $(cpp_sources:.cpp=.oi386)
  cpp_objects_ppc  = $(cpp_sources:.cpp=.oppc)
else
  target = WeaveCrypto
  so_target = $(target:=.$(so))
  cpp_objects = $(cpp_sources:.cpp=.o)
endif

# source and path configurations
idl = IWeaveCrypto.idl
cpp_sources = WeaveCrypto.cpp WeaveCryptoModule.cpp

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

# libraries
libdirs := $(sdkdir)/lib $(sdkdir)/bin
libs := xpcomglue_s xpcom nspr4 \
        crmf smime3 ssl3 nss3 nssutil3 \
        plds4 plc4

ifeq ($(os), linux)
  libs := xpcom_core $(libs)
endif

# compiler and Linker Flags

ifeq ($(os), Darwin)
  libdirs := $(patsubst %,-L%,$(libdirs))
  libs := $(patsubst %,-l%,$(libs))
  cppflags_i386 += -c -pipe -Os -arch i386 \
                   -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
                   -fno-common -fshort-wchar -fpascal-strings -pthread \
                   -Wall -Wconversion -Wpointer-arith -Woverloaded-virtual -Wsynth \
                   -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align \
                   -Wno-long-long \
                   -include xpcom-config.h $(headers) \
                   -isysroot /Developer/SDKs/MacOSX10.4u.sdk
  ldflags_i386 += -pthread -pipe -bundle -arch i386 \
                  -Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk \
                  -Wl,-executable_path,$(sdkdir)/bin \
                  -Wl,-dead_strip \
                  -Wl,-exported_symbol \
                  -Wl,_NSGetModule \
                  $(libdirs) $(libs)
  cppflags_ppc += -c -pipe -Os -arch ppc \
                  -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
                  -fno-common -fshort-wchar -fpascal-strings -pthread \
                  -Wall -Wconversion -Wpointer-arith -Woverloaded-virtual -Wsynth \
                  -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align \
                  -Wno-long-long \
                  -include xpcom-config.h $(headers) \
                  -isysroot /Developer/SDKs/MacOSX10.4u.sdk \
                  -force_cpusubtype_ALL 
  ldflags_ppc += -pthread -pipe -bundle -arch ppc \
                 -Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk \
                 -Wl,-executable_path,$(sdkdir)/bin \
                 -Wl,-dead_strip \
                 -Wl,-exported_symbol \
                 -Wl,_NSGetModule \
                 -force_cpusubtype_ALL \
                 $(libdirs) $(libs)  
else
ifeq ($(os), Linux)
  libdirs := $(patsubst %,-L%,$(libdirs))
  libs := $(patsubst %,-l%,$(libs))
  cppflags += -pipe -Os \
              -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
              -fno-common -pthread \
              -Wall -Wconversion -Wpointer-arith -Woverloaded-virtual -Wsynth \
              -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align \
              -Wno-long-long \
              -include xpcom-config.h $(headers)
ifeq ($(arch), x86)
  cppflags += -fshort-wchar
else
endif
  ldflags += -pthread -pipe -DMOZILLA_STRICT_API \
             -Wl,-dead_strip \
             -Wl,-exported_symbol \
             -Wl,-z,defs -Wl,-h,WeaveCrypto.so \
             -Wl,-rpath-link,$(sdkdir)/bin \
             $(sdkdir)/lib/libxpcomglue_s.a \
             $(libdirs) $(libs)
else
ifeq ($(os), WINNT)
  libdirs := $(patsubst %,-LIBPATH:%,$(libdirs))
  libs := $(patsubst %,$(sdkdir)/lib/%.lib,$(libs))
  cppflags += -c -nologo -O1 -GR- -TP -MT -Zc:wchar_t- -W3 -Gy $(headers) \
    -DNDEBUG -DTRIMMED -D_CRT_SECURE_NO_DEPRECATE=1 \
    -D_CRT_NONSTDC_NO_DEPRECATE=1 -DWINVER=0x500 -D_WIN32_WINNT=0x500 \
    -D_WIN32_IE=0x0500 -DX_DISPLAY_MISSING=1 -DMOZILLA_VERSION=\"1.9pre\" \
    -DMOZILLA_VERSION_U=1.9pre -DHAVE_SNPRINTF=1 -D_WINDOWS=1 -D_WIN32=1 \
    -DWIN32=1 -DXP_WIN=1 -DXP_WIN32=1 -DHW_THREADS=1 -DSTDC_HEADERS=1 \
    -DWIN32_LEAN_AND_MEAN=1 -DNO_X11=1 -DHAVE_MMINTRIN_H=1 \
    -DHAVE_OLEACC_IDL=1 -DHAVE_ATLBASE_H=1 -DHAVE_WPCAPI_H=1 -D_X86_=1 \
    -DD_INO=d_ino
  ldflags += -DLL -NOLOGO -SUBSYSTEM:WINDOWS -NXCOMPAT -SAFESEH -IMPLIB:fake.lib \
    $(libdirs) $(libs) \
    kernel32.lib user32.lib gdi32.lib winmm.lib wsock32.lib advapi32.lib
  rcflags := -r $(headers)
endif
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
	rm -f $(so_target) $(so_target_i386) $(so_target_ppc) \
          $(cpp_objects) $(cpp_objects_i386) $(cpp_objects_ppc) \
          $(idl_typelib) $(idl_headers) $(target:=.res) fake.lib fake.exp

# rules to build the c headers and .xpt from idl
$(idl_headers): $(idl)
	$(xpidl) -m header -I$(sdkdir)/idl $(@:.h=.idl)

$(idl_typelib): $(idl)
	$(xpidl) -m typelib -I$(sdkdir)/idl $(@:.xpt=.idl)

# build and link rules
ifeq ($(os), Darwin)
  $(so_target): $(so_target_i386) $(so_target_ppc)
	lipo -create -output $(so_target) -arch ppc $(so_target_ppc) \
             -arch i386 $(so_target_i386)
	chmod +x $(so_target)

  #i386
  $(cpp_objects_i386): $(cpp_sources)
	$(cxx) -o $@ $(cppflags_i386) $(@:.oi386=.cpp)

  $(so_target_i386): $(idl_headers) $(cpp_objects_i386)
	$(cxx) -o $@ $(ldflags_i386) $(cpp_objects_i386)
	chmod +x $(so_target_i386)

  #ppc
  $(cpp_objects_ppc): $(cpp_sources)
	$(cxx) -o $@ $(cppflags_ppc) $(@:.oppc=.cpp)

  $(so_target_ppc): $(idl_headers) $(cpp_objects_ppc)
	$(cxx) -o $@ $(ldflags_ppc) $(cpp_objects_ppc)
	chmod +x $(so_target_ppc)
else
ifeq ($(os), Linux)
  $(so_target): $(idl_headers)
	$(cxx) $(cppflags) -o $@ $(cpp_sources) $(ldflags)
	chmod +x $@
else
ifeq ($(os), WINNT)
  $(target:=.res): $(target:=.rc)
	rc -Fo$@ $(rcflags) $(target:=.rc)

  $(cpp_objects): $(cpp_sources)
	$(cxx) -Fo$@ -Fd$(@:.o=.pdb) $(cppflags) $(@:.o=.cpp)

  $(so_target): $(idl_headers) $(cpp_objects) $(target:=.res)
	link -OUT:$@ -PDB:$(@:.dll=.pdb) $(cpp_objects) $(target:=.res) $(ldflags)
	chmod +x $@
endif
endif
endif
