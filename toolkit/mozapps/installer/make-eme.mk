# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(MOZILLA_DIR)/toolkit/mozapps/installer/signing.mk

ifdef MOZ_SIGN_CMD
  ifeq ($(OS_ARCH),WINNT)
    # Note: CWD is DIST
    MAKE_SIGN_EME_VOUCHER := $(PYTHON) $(MOZILLA_DIR)/python/eme/gen-eme-voucher.py -input $(STAGEPATH)$(MOZ_PKG_DIR)/plugin-container.exe -output $(STAGEPATH)$(MOZ_PKG_DIR)/voucher.bin && \
      $(MOZ_SIGN_CMD) -f emevoucher "$(STAGEPATH)$(MOZ_PKG_DIR)/voucher.bin"
  endif
endif
