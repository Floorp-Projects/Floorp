# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq (Darwin_1,$(OS_TARGET)_$(MOZ_REPLACE_MALLOC))
OS_LDFLAGS += \
  -Wl,-U,_replace_init \
  -Wl,-U,_replace_get_bridge \
  -Wl,-U,_replace_malloc \
  -Wl,-U,_replace_posix_memalign \
  -Wl,-U,_replace_aligned_alloc \
  -Wl,-U,_replace_calloc \
  -Wl,-U,_replace_realloc \
  -Wl,-U,_replace_free \
  -Wl,-U,_replace_memalign \
  -Wl,-U,_replace_valloc \
  -Wl,-U,_replace_malloc_usable_size \
  -Wl,-U,_replace_malloc_good_size \
  -Wl,-U,_replace_jemalloc_stats \
  -Wl,-U,_replace_jemalloc_purge_freed_pages \
  -Wl,-U,_replace_jemalloc_free_dirty_pages \
  $(NULL)

ifneq ($(MOZ_REPLACE_MALLOC_LINKAGE),compiler support)
OS_LDFLAGS += -flat_namespace
endif
ifeq ($(MOZ_REPLACE_MALLOC_LINKAGE),dummy library)
OS_LDFLAGS += -Wl,-weak_library,$(DEPTH)/memory/replace/dummy/$(DLL_PREFIX)dummy_replace_malloc$(DLL_SUFFIX)
endif

EXTRA_DEPS += $(topsrcdir)/mozglue/build/replace_malloc.mk
endif
