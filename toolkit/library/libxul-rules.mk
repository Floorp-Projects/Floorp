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
# The Original Code is Mozilla libxul
#
# The Initial Developer of the Original Code is
# Benjamin Smedberg <benjamin@smedbergs.us>
#
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

EXTRA_DSO_LDOPTS += \
	$(MOZ_FIX_LINK_PATHS) \
	$(LIBS_DIR) \
	$(JPEG_LIBS) \
	$(PNG_LIBS) \
	$(MOZ_JS_LIBS) \
	$(NSS_LIBS) \
	$(NULL)

ifdef MOZ_NATIVE_ZLIB
EXTRA_DSO_LDOPTS += $(ZLIB_LIBS)
else
EXTRA_DSO_LDOPTS += $(MOZ_ZLIB_LIBS)
endif

# need widget/src/windows for resource.h (included from widget.rc)
LOCAL_INCLUDES += \
	-I$(topsrcdir)/config \
	-I$(topsrcdir)/widget/src/windows \
	-I$(topsrcdir)/widget/src/build \
	$(NULL)

OS_LIBS += $(LIBICONV)

DEFINES += \
	-D_IMPL_NS_COM \
	-D_IMPL_NS_STRINGAPI \
	-DEXPORT_XPT_API \
	-DEXPORT_XPTC_API \
	-D_IMPL_NS_GFX \
	-D_IMPL_NS_WIDGET \
	$(NULL)

ifdef MOZ_ENABLE_CAIRO_GFX
ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
OS_LIBS += $(call EXPAND_LIBNAME,usp10)
endif
ifneq (,$(filter $(MOZ_WIDGET_TOOLKIT),mac cocoa))
EXTRA_DSO_LDOPTS += -lcups
ifdef MOZ_ENABLE_GLITZ
EXTRA_DSO_LDOPTS += -lmozglitzagl -framework OpenGL -framework AGL
endif
endif
endif # MOZ_ENABLE_CAIRO_GFX

ifdef MOZ_ENABLE_PANGO
EXTRA_DSO_LDOPTS += $(MOZ_PANGO_LIBS)
endif

ifneq (,$(MOZ_ENABLE_CANVAS)$(MOZ_SVG))
EXTRA_DSO_LDOPTS += $(MOZ_CAIRO_LIBS)
endif

export:: dlldeps.cpp

dlldeps.cpp: $(topsrcdir)/xpcom/build/dlldeps.cpp
	$(INSTALL) $^ .

GARBAGE += dlldeps.cpp
