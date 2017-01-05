# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# We shouldn't sign the first pass of a PGO build
ifndef MOZ_PROFILE_GENERATE

# Signing support
ifdef MOZ_SIGN_CMD
ifeq (WINNT,$(OS_ARCH))
MOZ_INTERNAL_SIGNING_FORMAT := sha2signcode
MOZ_EXTERNAL_SIGNING_FORMAT := sha2signcode
MOZ_EXTERNAL_SIGNING_FORMAT_STUB := sha2signcodestub
SIGN_INCLUDES := \
  '*.dll' \
  '*.exe' \
  $(NULL)

SIGN_EXCLUDES := \
  'D3DCompiler*.dll' \
  'msvc*.dll' \
  $(NULL)
endif # Windows

ifeq (Darwin, $(OS_ARCH))
MOZ_INTERNAL_SIGNING_FORMAT := dmgv2
MOZ_EXTERNAL_SIGNING_FORMAT :=
endif # Darwin

ifeq (linux-gnu,$(TARGET_OS))
MOZ_EXTERNAL_SIGNING_FORMAT :=
endif # Linux

ifdef MOZ_ASAN
MOZ_INTERNAL_SIGNING_FORMAT :=
MOZ_EXTERNAL_SIGNING_FORMAT :=
endif

endif # MOZ_SIGN_CMD

endif # MOZ_PROFILE_GENERATE
