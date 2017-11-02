# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq (Darwin_1,$(OS_TARGET)_$(MOZ_REPLACE_MALLOC))
# Technically, ) is not a necessary delimiter in the awk call, but make
# doesn't like the opening ( there being alone...
MK_LDFLAGS = \
  $(shell awk -F'[(),]' '/^MALLOC_DECL/{print "-Wl,-U,_replace_" $$2}' $(topsrcdir)/memory/build/malloc_decls.h) \
  $(NULL)

EXTRA_DEPS += $(topsrcdir)/memory/build/malloc_decls.h
endif
