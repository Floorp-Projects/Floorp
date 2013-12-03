MOZ_ANDROID_SHARED_ID = $(ANDROID_PACKAGE_NAME).sharedID
MOZ_ANDROID_SHARED_ACCOUNT_TYPE = $(ANDROID_PACKAGE_NAME)_sync

# We released these builds to the public with shared IDs and need to
# keep them consistent.
ifeq (org.mozilla.firefox,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.firefox.sharedID
MOZ_ANDROID_SHARED_ACCOUNT_TYPE = org.mozilla.firefox_sync
else ifeq (org.mozilla.firefox_beta,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.firefox.sharedID
MOZ_ANDROID_SHARED_ACCOUNT_TYPE = org.mozilla.firefox_sync
else ifeq (org.mozilla.fennec_aurora,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.fennec.sharedID
MOZ_ANDROID_SHARED_ACCOUNT_TYPE = org.mozilla.fennec_sync
else ifeq (org.mozilla.fennec,$(ANDROID_PACKAGE_NAME))
MOZ_ANDROID_SHARED_ID = org.mozilla.fennec.sharedID
MOZ_ANDROID_SHARED_ACCOUNT_TYPE = org.mozilla.fennec_sync
endif
