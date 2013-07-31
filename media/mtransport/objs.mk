# -*- Mode: makefile; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- #
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
DEFINES += -DHAVE_STRDUP -DNR_SOCKET_IS_VOID_PTR

LOCAL_INCLUDES += \
 -I. \
 -I$(topsrcdir)/media/webrtc/trunk/third_party/libjingle/source/ \
 -I$(topsrcdir)/media/mtransport/ \
 -I$(topsrcdir)/media/mtransport/third_party/ \
 -I$(topsrcdir)/media/mtransport/third_party/nICEr/src/crypto \
 -I$(topsrcdir)/media/mtransport/third_party/nICEr/src/ice \
 -I$(topsrcdir)/media/mtransport/third_party/nICEr/src/net \
 -I$(topsrcdir)/media/mtransport/third_party/nICEr/src/stun \
 -I$(topsrcdir)/media/mtransport/third_party/nICEr/src/util \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/share \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/util/libekr \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/log \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/registry \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/stats \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/plugin \
 -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/event \
 $(NULL)

ifeq ($(OS_TARGET), Darwin)
LOCAL_INCLUDES += \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/darwin/include \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/generic/include \
  $(NULL)
DEFINES += -DDARWIN
endif

ifeq ($(OS_TARGET), Linux)
LOCAL_INCLUDES += \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/linux/include \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/generic/include \
  $(NULL)
DEFINES += -DLINUX
endif

ifeq ($(OS_TARGET), Android)
LOCAL_INCLUDES += \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/android/include \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/generic/include \
  $(NULL)
DEFINES += -DLINUX -DANDROID
endif

ifeq ($(OS_TARGET), WINNT)
LOCAL_INCLUDES += \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/win32/include \
  -I$(topsrcdir)/media/mtransport/third_party/nrappkit/src/port/generic/include \
  $(NULL)
DEFINES += -DWIN
endif

DEFINES += \
   -DR_DEFINED_INT2=int16_t -DR_DEFINED_UINT2=uint16_t \
   -DR_DEFINED_INT4=int32_t -DR_DEFINED_UINT4=uint32_t \
   -DR_DEFINED_INT8=int64_t -DR_DEFINED_UINT8=uint64_t \
   $(NULL)

MTRANSPORT_LCPPSRCS = \
  dtlsidentity.cpp \
  nricectx.cpp \
  nricemediastream.cpp \
  nriceresolverfake.cpp \
  nriceresolver.cpp \
  nr_socket_prsock.cpp \
  nr_timer.cpp \
  transportflow.cpp \
  transportlayer.cpp \
  transportlayerice.cpp \
  transportlayerdtls.cpp \
  transportlayerlog.cpp \
  transportlayerloopback.cpp \
  transportlayerprsock.cpp \
  $(NULL)

ifeq (gonk,$(MOZ_WIDGET_TOOLKIT))
MTRANSPORT_LCPPSRCS += \
  gonk_addrs.cpp \
  $(NULL)
endif

MTRANSPORT_CPPSRCS = $(addprefix $(topsrcdir)/media/mtransport/, $(MTRANSPORT_LCPPSRCS))

