# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(MOZILLA_DIR)/toolkit/mozapps/installer/signing.mk

ifdef MOZ_SIGN_CMD
  ifeq ($(OS_ARCH),WINNT)
    ifneq ($(TARGET_CPU),x86_64)
      # The argument to this macro is the directory where plugin-container.exe
      # exists, and where voucher.bin will be generated.
      MAKE_SIGN_EME_VOUCHER = $(PYTHON) $(MOZILLA_DIR)/python/eme/gen-eme-voucher.py -input $(1)/plugin-container.exe -output $(1)/voucher.bin && \
        $(MOZ_SIGN_CMD) -f emevoucher "$(1)/voucher.bin"
    endif
  endif
endif
