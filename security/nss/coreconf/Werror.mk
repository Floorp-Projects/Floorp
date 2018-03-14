#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This sets WARNING_CFLAGS for gcc-like compilers.

ifndef CC_IS_CLANG
  CC_IS_CLANG := $(and $(findstring clang, $(shell $(CC) --version 2>&1)), 1)
  # Export CC_IS_CLANG to save a shell invocation when recursing.
  export CC_IS_CLANG
endif

ifdef CC_IS_CLANG
  # Clang claims GCC 4.2.1 compatibility, see GCC_VERSION
  CC_IS_GCC = 1
  # Export CC_IS_GCC to save a shell invocation when recursing.
  export CC_IS_GCC
endif

ifndef CC_IS_GCC
  CC_IS_GCC := $(shell $(CC) -x c -E -Wall -Werror /dev/null >/dev/null 2>&1 && echo 1)
  # Export CC_IS_GCC to save a shell invocation when recursing.
  export CC_IS_GCC
endif

ifndef CC_NAME
  ifeq (1,$(CC_IS_GCC))
    CC_NAME := $(shell $(CC) -? 2>&1 >/dev/null | sed -e 's/:.*//;1q')
  else
    CC_NAME := $(notdir $(CC))
  endif
  # Export CC_NAME to save a shell invocation when recursing.
  export CC_NAME
endif

ifndef GCC_VERSION
  ifeq (1,$(CC_IS_GCC))
    GCC_VERSION := $(subst ., ,$(shell $(CC) -dumpversion || echo x.x.x))
    # Export GCC_VERSION to save a shell invocation when recursing.
    export GCC_VERSION
  endif
endif

ifndef WARNING_CFLAGS
  ifneq (1,$(CC_IS_GCC))
    WARNING_CFLAGS = $(NULL)
  else
    # This tests to see if enabling the warning is possible before
    # setting an option to disable it.
    disable_warning = $(shell $(CC) -x c -E -Werror -W$(1) /dev/null >/dev/null 2>&1 && echo -Wno-$(1))

    WARNING_CFLAGS = -Wall
    ifdef CC_IS_CLANG
      # -Qunused-arguments : clang objects to arguments that it doesn't understand
      #    and fixing this would require rearchitecture
      WARNING_CFLAGS += -Qunused-arguments
      # -Wno-parentheses-equality : because clang warns about macro expansions
      WARNING_CFLAGS += $(call disable_warning,parentheses-equality)
      ifdef BUILD_OPT
        # clang is unable to handle glib's expansion of strcmp and similar for optimized
        # builds, so ignore the resulting errors.
        # See https://llvm.org/bugs/show_bug.cgi?id=20144
        WARNING_CFLAGS += $(call disable_warning,array-bounds)
        WARNING_CFLAGS += $(call disable_warning,unevaluated-expression)
      endif
    endif # if clang

    ifndef NSS_ENABLE_WERROR
      ifeq ($(OS_TARGET),Android)
        # Android lollipop generates the following warning:
        # error: call to 'sprintf' declared with attribute warning:
        #   sprintf is often misused; please use snprintf [-Werror]
        # So, just suppress -Werror entirely on Android
        NSS_ENABLE_WERROR = 0
        $(warning OS_TARGET is Android, disabling -Werror)
      else
        ifdef CC_IS_CLANG
          # Clang reports its version as an older gcc, but it's OK
          NSS_ENABLE_WERROR = 1
        else
          ifneq (,$(filter 4.8 4.9,$(word 1,$(GCC_VERSION)).$(word 2,$(GCC_VERSION))))
            NSS_ENABLE_WERROR = 1
          endif
          ifeq (,$(filter 0 1 2 3 4,$(word 1,$(GCC_VERSION))))
            NSS_ENABLE_WERROR = 1
          endif
        endif
        ifndef NSS_ENABLE_WERROR
          $(warning Unable to find gcc 4.8 or greater, disabling -Werror)
          NSS_ENABLE_WERROR = 0
        endif
      endif
    endif #ndef NSS_ENABLE_WERROR

    ifeq ($(NSS_ENABLE_WERROR),1)
      WARNING_CFLAGS += -Werror
    else
      # Old versions of gcc (< 4.8) don't support #pragma diagnostic in functions.
      # Use this to disable use of that #pragma and the warnings it suppresses.
      WARNING_CFLAGS += -DNSS_NO_GCC48
    endif
  endif
  export WARNING_CFLAGS
endif # ndef WARNING_CFLAGS
