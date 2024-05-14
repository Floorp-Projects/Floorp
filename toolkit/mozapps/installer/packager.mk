# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(MOZILLA_DIR)/toolkit/mozapps/installer/package-name.mk
include $(MOZILLA_DIR)/toolkit/mozapps/installer/upload-files.mk

# This is how we create the binary packages we release to the public.

# browser/locales/Makefile uses this makefile for its variable defs, but
# doesn't want the libs:: rule.
ifndef PACKAGER_NO_LIBS
libs:: make-package
endif

ifdef MOZ_AUTOMATION
# This allows `RUN_{FIND_DUPES,MOZHARNESS_ZIP}=1 ./mach package` to test locally.
RUN_FIND_DUPES ?= $(MOZ_AUTOMATION)
RUN_MOZHARNESS_ZIP ?= $(MOZ_AUTOMATION)
endif

export USE_ELF_HACK

stage-package: multilocale.txt locale-manifest.in $(MOZ_PKG_MANIFEST) $(MOZ_PKG_MANIFEST_DEPS)
	NO_PKG_FILES="$(NO_PKG_FILES)" \
	$(PYTHON3) $(MOZILLA_DIR)/toolkit/mozapps/installer/packager.py $(DEFINES) $(ACDEFINES) \
		--format $(MOZ_PACKAGER_FORMAT) \
		$(addprefix --removals ,$(MOZ_PKG_REMOVALS)) \
		$(if $(filter-out 0,$(MOZ_PKG_FATAL_WARNINGS)),,--ignore-errors) \
		$(if $(MOZ_AUTOMATION),,--ignore-broken-symlinks) \
		$(if $(MOZ_PACKAGER_MINIFY),--minify) \
		$(if $(MOZ_PACKAGER_MINIFY_JS),--minify-js \
		  $(addprefix --js-binary ,$(JS_BINARY)) \
		) \
		$(addprefix --jarlog ,$(wildcard $(JARLOG_FILE_AB_CD))) \
		$(addprefix --compress ,$(JAR_COMPRESSION)) \
		$(MOZ_PKG_MANIFEST) '$(DIST)' '$(DIST)'/$(MOZ_PKG_DIR)$(if $(MOZ_PKG_MANIFEST),,$(_BINPATH:%=/%)) \
		$(if $(filter omni,$(MOZ_PACKAGER_FORMAT)),$(if $(NON_OMNIJAR_FILES),--non-resource $(NON_OMNIJAR_FILES)))
ifdef RUN_FIND_DUPES
	$(PYTHON3) $(MOZILLA_DIR)/toolkit/mozapps/installer/find-dupes.py $(DEFINES) $(ACDEFINES) $(MOZ_PKG_DUPEFLAGS) $(DIST)/$(MOZ_PKG_DIR)
endif # RUN_FIND_DUPES
ifndef MOZ_IS_COMM_TOPDIR
ifdef RUN_MOZHARNESS_ZIP
	# Package mozharness
	$(call py_action,test_archive $(MOZHARNESS_PACKAGE), \
		mozharness \
		$(ABS_DIST)/$(PKG_PATH)$(MOZHARNESS_PACKAGE))
endif # RUN_MOZHARNESS_ZIP
endif # MOZ_IS_COMM_TOPDIR
ifdef MOZ_PACKAGE_JSSHELL
	# Package JavaScript Shell
	@echo 'Packaging JavaScript Shell...'
	$(RM) $(PKG_JSSHELL)
	$(MAKE_JSSHELL)
endif # MOZ_PACKAGE_JSSHELL
ifdef MOZ_AUTOMATION
ifdef MOZ_ARTIFACT_BUILD_SYMBOLS
	@echo 'Packaging existing crashreporter symbols from artifact build...'
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	cd $(DIST)/crashreporter-symbols && \
          zip -r5D '../$(PKG_PATH)$(SYMBOL_ARCHIVE_BASENAME).zip' . -i '*.sym' -i '*.txt'
ifeq ($(MOZ_ARTIFACT_BUILD_SYMBOLS),full)
	$(call py_action,symbols_archive $(SYMBOL_FULL_ARCHIVE_BASENAME).tar.zst,'$(DIST)/$(PKG_PATH)$(SYMBOL_FULL_ARCHIVE_BASENAME).tar.zst' \
                                     $(abspath $(DIST)/crashreporter-symbols) \
                                     --full-archive)
endif
endif # MOZ_ARTIFACT_BUILD_SYMBOLS
endif # MOZ_AUTOMATION
ifdef MOZ_CODE_COVERAGE
	@echo 'Generating chrome-map for coverage data...'
	$(PYTHON3) $(topsrcdir)/mach build-backend -b ChromeMap
	@echo 'Packaging code coverage data...'
	$(RM) $(CODE_COVERAGE_ARCHIVE_BASENAME).zip
	$(PYTHON3) -mmozbuild.codecoverage.packager \
		--output-file='$(DIST)/$(PKG_PATH)$(CODE_COVERAGE_ARCHIVE_BASENAME).zip'
endif
ifdef ENABLE_MOZSEARCH_PLUGIN
	@echo 'Generating mozsearch chrome-map...'
	$(PYTHON3) $(topsrcdir)/mach build-backend -b ChromeMap
	@echo 'Generating mozsearch index tarball...'
	$(RM) $(MOZSEARCH_ARCHIVE_BASENAME).zip
	cd $(topobjdir)/mozsearch_index && \
          zip -r5D '$(ABS_DIST)/$(PKG_PATH)$(MOZSEARCH_ARCHIVE_BASENAME).zip' .
	@echo 'Generating mozsearch distinclude map...'
	cd $(topobjdir)/ && cp _build_manifests/install/dist_include '$(ABS_DIST)/$(PKG_PATH)$(MOZSEARCH_INCLUDEMAP_BASENAME).map'
	@echo 'Generating mozsearch scip index...'
	$(RM) $(MOZSEARCH_SCIP_INDEX_BASENAME).zip
	cp $(topsrcdir)/.cargo/config.toml.in $(topsrcdir)/.cargo/config.toml
	cd $(topsrcdir)/ && \
          CARGO=$(MOZ_FETCHES_DIR)/rustc/bin/cargo \
          RUSTC=$(MOZ_FETCHES_DIR)/rustc/bin/rustc \
          $(MOZ_FETCHES_DIR)/rustc/bin/rust-analyzer scip . && \
          zip -r5D '$(ABS_DIST)/$(PKG_PATH)$(MOZSEARCH_SCIP_INDEX_BASENAME).zip' \
          index.scip
	rm $(topsrcdir)/.cargo/config.toml
ifeq ($(MOZ_BUILD_APP),mobile/android)
	@echo 'Generating mozsearch java/kotlin semanticdb tarball...'
	$(RM) $(MOZSEARCH_JAVA_INDEX_BASENAME).zip
	cd $(topsrcdir)/ && \
          $(PYTHON3) $(topsrcdir)/mach android compile-all && \
          cd $(topobjdir)/mozsearch_java_index && \
          zip -r5D '$(ABS_DIST)/$(PKG_PATH)$(MOZSEARCH_JAVA_INDEX_BASENAME).zip' .
endif # MOZ_BUILD_APP == mobile/android
endif
ifeq (Darwin, $(OS_ARCH))
ifneq (,$(MOZ_ASAN)$(LIBFUZZER)$(MOZ_UBSAN))
	@echo "Rewriting sanitizer runtime dylib paths for all binaries in $(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH) ..."
	$(PYTHON3) $(MOZILLA_DIR)/build/unix/rewrite_sanitizer_dylib.py '$(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH)'
endif # MOZ_ASAN || LIBFUZZER || MOZ_UBSAN
endif # Darwin
ifndef MOZ_ARTIFACT_BUILDS
	@echo 'Generating XPT artifacts archive ($(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip)'
	$(call py_action,zip $(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip,-C $(topobjdir)/config/makefiles/xpidl '$(ABS_DIST)/$(PKG_PATH)$(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip' '*.xpt')
ifeq (Darwin, $(OS_ARCH))
	@echo 'Generating update-related macOS framework artifacts archive ($(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip)'
	$(call py_action,zip $(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip,-C '$(ABS_DIST)/update_framework_artifacts' '$(ABS_DIST)/$(PKG_PATH)$(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip' '*.framework')
endif # Darwin
else
	@echo 'Packaging existing XPT artifacts from artifact build into archive ($(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip)'
	$(call py_action,zip $(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip,-C $(ABS_DIST)/xpt_artifacts '$(ABS_DIST)/$(PKG_PATH)$(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip' '*.xpt')
ifeq (Darwin, $(OS_ARCH))
	@echo 'Packaging existing update-related macOS framework artifacts from artifact build into archive ($(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip)'
	$(call py_action,zip $(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip,-C $(ABS_DIST)/update_framework_artifacts '$(ABS_DIST)/$(PKG_PATH)$(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip' '*.framework')
endif # Darwin
endif # MOZ_ARTIFACT_BUILDS

prepare-package: stage-package

make-package-internal: prepare-package make-sourcestamp-file
	@echo 'Compressing...'
	$(call MAKE_PACKAGE,$(DIST))
	echo $(PACKAGE) > $(ABS_DIST)/package_name.txt

make-package: FORCE
	$(MAKE) make-package-internal
ifeq (WINNT,$(OS_ARCH))
ifeq ($(MOZ_PKG_FORMAT),ZIP)
	$(MAKE) -C windows ZIP_IN='$(ABS_DIST)/$(PACKAGE)' installer
endif
endif
ifdef MOZ_AUTOMATION
	cp $(DEPTH)/mozinfo.json $(MOZ_MOZINFO_FILE)
	$(PYTHON3) $(MOZILLA_DIR)/toolkit/mozapps/installer/informulate.py \
		$(MOZ_BUILDINFO_FILE) $(MOZ_BUILDHUB_JSON) $(MOZ_BUILDID_INFO_TXT_FILE) \
		$(MOZ_PKG_PLATFORM) \
		$(if $(or $(filter-out mobile/android,$(MOZ_BUILD_APP)),$(MOZ_ANDROID_WITH_FENNEC)), \
		--package=$(DIST)/$(PACKAGE) --installer=$(INSTALLER_PACKAGE), \
		--no-download \
	  )
endif
	$(TOUCH) $@

GARBAGE += make-package

make-sourcestamp-file::
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	@awk '$$2 == "MOZ_BUILDID" {print $$3}' $(DEPTH)/buildid.h > $(MOZ_SOURCESTAMP_FILE)
ifdef MOZ_INCLUDE_SOURCE_INFO
	@awk '$$2 == "MOZ_SOURCE_URL" {print $$3}' $(DEPTH)/source-repo.h >> $(MOZ_SOURCESTAMP_FILE)
endif

# The install target will install the application to prefix/lib/appname-version
install:: prepare-package
ifneq (,$(filter WINNT Darwin,$(OS_TARGET)))
	$(error "make install" is not supported on this platform. Use "make package" instead.)
endif
	$(NSINSTALL) -D $(DESTDIR)$(installdir)
	(cd $(DIST)/$(MOZ_PKG_DIR) && $(TAR) --exclude=precomplete $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DESTDIR)$(installdir) && tar -xf -)
	$(NSINSTALL) -D $(DESTDIR)$(bindir)
	$(RM) -f $(DESTDIR)$(bindir)/$(MOZ_APP_NAME)
	ln -s $(installdir)/$(MOZ_APP_NAME) $(DESTDIR)$(bindir)

upload:
	$(PYTHON3) -u $(MOZILLA_DIR)/build/upload.py --base-path $(DIST) $(UPLOAD_FILES)
	mkdir -p `dirname $(CHECKSUM_FILE)`
	@$(PYTHON3) $(MOZILLA_DIR)/build/checksums.py \
		-o $(CHECKSUM_FILE) \
		$(CHECKSUM_ALGORITHM_PARAM) \
		$(UPLOAD_PATH)
	@echo 'CHECKSUM FILE START'
	@cat $(CHECKSUM_FILE)
	@echo 'CHECKSUM FILE END'
	$(PYTHON3) -u $(MOZILLA_DIR)/build/upload.py --base-path $(DIST) $(CHECKSUM_FILES)

# source-package creates a source tarball from the files in MOZ_PKG_SRCDIR,
# which is either set to a clean checkout or defaults to $topsrcdir
source-package:
	@echo 'Generate the sourcestamp file'
	# Make sure to have repository information available and then generate the
	# sourcestamp file.
	$(MAKE) -C $(DEPTH) 'source-repo.h' 'buildid.h'
	$(MAKE) make-sourcestamp-file
	@echo 'Packaging source tarball...'
	# We want to include the sourcestamp file in the source tarball, so copy it
	# in the root source directory. This is useful to enable telemetry submissions
	# from builds made from the source package with the correct revision information.
	# Don't bother removing it as this is only used by automation.
	@cp $(MOZ_SOURCESTAMP_FILE) '$(MOZ_PKG_SRCDIR)/sourcestamp.txt'
	$(MKDIR) -p $(DIST)/$(PKG_SRCPACK_PATH)
	(cd $(MOZ_PKG_SRCDIR) && $(CREATE_SOURCE_TAR) - ./ ) | xz -9e > $(SOURCE_TAR)

hg-bundle:
	$(MKDIR) -p $(DIST)/$(PKG_SRCPACK_PATH)
	$(CREATE_HG_BUNDLE_CMD)

source-upload:
	$(MAKE) upload UPLOAD_FILES='$(SOURCE_UPLOAD_FILES)' CHECKSUM_FILE='$(SOURCE_CHECKSUM_FILE)'


ALL_LOCALES = $(if $(filter en-US,$(LOCALES)),$(LOCALES),$(LOCALES) en-US)

# Firefox uses @RESPATH@.
# Fennec uses @BINPATH@ and doesn't have the @RESPATH@ variable defined.
ifeq ($(MOZ_BUILD_APP),mobile/android)
BASE_PATH:=@BINPATH@
MULTILOCALE_DIR = $(DIST)/$(BINPATH)/res
else
BASE_PATH:=@RESPATH@
MULTILOCALE_DIR = $(DIST)/$(RESPATH)/res
endif

# This version of the target uses MOZ_CHROME_MULTILOCALE to build multilocale.txt
# and places it in dist/bin/res - it should be used when packaging a build.
multilocale.txt: LOCALES?=$(MOZ_CHROME_MULTILOCALE)
multilocale.txt:
	$(call py_action,file_generate $@,$(MOZILLA_DIR)/toolkit/locales/gen_multilocale.py main '$(MULTILOCALE_DIR)/multilocale.txt' $(MDDEPDIR)/multilocale.txt.pp '$(MULTILOCALE_DIR)/multilocale.txt' $(ALL_LOCALES))

# This version of the target uses AB_CD to build multilocale.txt and places it
# in the $(XPI_NAME)/res dir - it should be used when repackaging a build.
multilocale.txt-%: LOCALES?=$(AB_CD)
multilocale.txt-%: MULTILOCALE_DIR=$(DIST)/xpi-stage/$(XPI_NAME)/res
multilocale.txt-%:
	$(call py_action,file_generate multilocale.txt,$(MOZILLA_DIR)/toolkit/locales/gen_multilocale.py main '$(MULTILOCALE_DIR)/multilocale.txt' $(MDDEPDIR)/multilocale.txt.pp '$(MULTILOCALE_DIR)/multilocale.txt' $(ALL_LOCALES))

locale-manifest.in: LOCALES?=$(MOZ_CHROME_MULTILOCALE)
locale-manifest.in: $(GLOBAL_DEPS) FORCE
	printf '\n[multilocale]\n' > $@
	printf '$(BASE_PATH)/res/multilocale.txt\n' >> $@
	for LOCALE in $(ALL_LOCALES) ;\
	do \
	  for ENTRY in $(MOZ_CHROME_LOCALE_ENTRIES) ;\
		do \
		  printf "$$ENTRY""$$LOCALE"'@JAREXT@\n' >> $@; \
		  printf "$$ENTRY""$$LOCALE"'.manifest\n' >> $@; \
	  done \
	done
