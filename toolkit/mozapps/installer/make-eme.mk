# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(MOZILLA_DIR)/toolkit/mozapps/installer/signing.mk

ifdef MOZ_SIGN_CMD
  ifeq (,$(filter-out WINNT Darwin,$(OS_ARCH)))
    # The first argument to this macro is the directory where the
    # plugin-container binary exists, and the second is where voucher.bin will
    # be generated. If the second argument is not specified, it defaults to the
    # same as the first.
    MAKE_SIGN_EME_VOUCHER = $(PYTHON) $(MOZILLA_DIR)/python/eme/gen-eme-voucher.py -input $(1)/$(MOZ_CHILD_PROCESS_NAME) -output $(or $(2),$(1))/voucher.bin && \
      $(MOZ_SIGN_CMD) -f emevoucher "$(or $(2),$(1))/voucher.bin"
  endif
endif
