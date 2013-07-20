# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_CONFIG_MK
$(error Must include config.mk before this file.)
endif

ifdef WEBRTC_CONFIG_INCLUDED
$(error Must not include webrtc-config.mk twice.)
endif

WEBRTC_CONFIG_INCLUDED = 1

EXTRA_DEPS += $(topsrcdir)/media/webrtc/webrtc-config.mk

LOCAL_INCLUDES += \
  -I$(topsrcdir)/media/webrtc/trunk/webrtc \
  $(NULL)
