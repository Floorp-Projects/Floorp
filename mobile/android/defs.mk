MOZ_ANDROID_SHARED_ID = $(ANDROID_PACKAGE_NAME).sharedID
# Firefox Accounts Account types are per-package.
MOZ_ANDROID_SHARED_FXACCOUNT_TYPE = $(ANDROID_PACKAGE_NAME)_fxaccount

# We released these builds to the public with shared IDs and need to
# keep them consistent.
ifeq (org.mozilla.firefox,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.firefox.sharedID
else ifeq (org.mozilla.firefox_beta,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.firefox.sharedID
else ifeq (org.mozilla.fennec_aurora,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.fennec.sharedID
else ifeq (org.mozilla.fennec,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.fennec.sharedID
endif
