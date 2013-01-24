ifeq ($(OS_TARGET),WINNT)
ifneq (,$(filter nightly,$(MOZ_UPDATE_CHANNEL)))
NO_PROFILE_GUIDED_OPTIMIZE := 1 # Don't PGO
endif
endif
