# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef MOZ_PKG_FORMAT
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
MOZ_PKG_FORMAT  = DMG
else
ifeq (,$(filter-out WINNT, $(OS_ARCH)))
MOZ_PKG_FORMAT  = ZIP
else
ifeq (,$(filter-out SunOS, $(OS_ARCH)))
   MOZ_PKG_FORMAT  = BZ2
else
   ifeq (,$(filter-out gtk2 gtk3 qt, $(MOZ_WIDGET_TOOLKIT)))
      MOZ_PKG_FORMAT  = BZ2
   else
      ifeq (android,$(MOZ_WIDGET_TOOLKIT))
          MOZ_PKG_FORMAT = APK
      else
          MOZ_PKG_FORMAT = TGZ
      endif
   endif
endif
endif
endif
endif # MOZ_PKG_FORMAT

ifeq ($(OS_ARCH),WINNT)
INSTALLER_DIR   = windows
endif

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifndef _APPNAME
_APPNAME = $(MOZ_MACBUNDLE_NAME)
endif
ifndef _BINPATH
_BINPATH = /$(_APPNAME)/Contents/MacOS
endif # _BINPATH
ifndef _RESPATH
# Resource path for the precomplete file
_RESPATH = /$(_APPNAME)/Contents/Resources
endif
ifdef UNIVERSAL_BINARY
STAGEPATH = universal/
endif
endif

PACKAGE_BASE_DIR = $(ABS_DIST)
PACKAGE       = $(PKG_PATH)$(PKG_BASENAME)$(PKG_SUFFIX)

# By default, the SDK uses the same packaging type as the main bundle,
# but on mac it is a .tar.bz2
SDK_SUFFIX    = $(PKG_SUFFIX)
SDK           = $(SDK_PATH)$(PKG_BASENAME).sdk$(SDK_SUFFIX)
ifdef UNIVERSAL_BINARY
SDK           = $(SDK_PATH)$(PKG_BASENAME)-$(TARGET_CPU).sdk$(SDK_SUFFIX)
endif

# JavaScript Shell packaging
JSSHELL_BINS  = \
  js$(BIN_SUFFIX) \
  $(DLL_PREFIX)mozglue$(DLL_SUFFIX) \
  $(NULL)

ifndef MOZ_SYSTEM_NSPR
  ifdef MOZ_FOLD_LIBS
    JSSHELL_BINS += $(DLL_PREFIX)nss3$(DLL_SUFFIX)
  else
    JSSHELL_BINS += \
      $(DLL_PREFIX)nspr4$(DLL_SUFFIX) \
      $(DLL_PREFIX)plds4$(DLL_SUFFIX) \
      $(DLL_PREFIX)plc4$(DLL_SUFFIX) \
      $(NULL)
  endif # MOZ_FOLD_LIBS
endif # MOZ_SYSTEM_NSPR

ifdef MSVC_C_RUNTIME_DLL
  JSSHELL_BINS += $(MSVC_C_RUNTIME_DLL)
endif
ifdef MSVC_CXX_RUNTIME_DLL
  JSSHELL_BINS += $(MSVC_CXX_RUNTIME_DLL)
endif

ifdef WIN_UCRT_REDIST_DIR
  JSSHELL_BINS += $(notdir $(wildcard $(DIST)/bin/api-ms-win-*.dll))
  JSSHELL_BINS += ucrtbase.dll
endif

MAKE_JSSHELL  = $(call py_action,zip,-C $(DIST)/bin $(abspath $(PKG_JSSHELL)) $(JSSHELL_BINS))

JARLOG_DIR = $(topobjdir)/jarlog/
JARLOG_FILE_AB_CD = $(JARLOG_DIR)/$(AB_CD).log

TAR_CREATE_FLAGS := --exclude=.mkdir.done $(TAR_CREATE_FLAGS)
CREATE_FINAL_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode=go-w --exclude=.mkdir.done -f
UNPACK_TAR       = tar -xf-

ifeq ($(MOZ_PKG_FORMAT),TAR)
  PKG_SUFFIX	= .tar
  INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) > $(PACKAGE)
  INNER_UNMAKE_PACKAGE	= $(UNPACK_TAR) < $(UNPACKAGE)
  MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk > '$(SDK)'
endif

ifeq ($(MOZ_PKG_FORMAT),TGZ)
  PKG_SUFFIX	= .tar.gz
  INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | gzip -vf9 > $(PACKAGE)
  INNER_UNMAKE_PACKAGE	= gunzip -c $(UNPACKAGE) | $(UNPACK_TAR)
  MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | gzip -vf9 > '$(SDK)'
endif

ifeq ($(MOZ_PKG_FORMAT),BZ2)
  PKG_SUFFIX	= .tar.bz2
  ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
    INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - -C $(STAGEPATH)$(MOZ_PKG_DIR) $(_APPNAME) | bzip2 -vf > $(PACKAGE)
  else
    INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | bzip2 -vf > $(PACKAGE)
  endif
  INNER_UNMAKE_PACKAGE	= bunzip2 -c $(UNPACKAGE) | $(UNPACK_TAR)
  MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | bzip2 -vf > '$(SDK)'
endif

ifeq ($(MOZ_PKG_FORMAT),ZIP)
  ifdef MOZ_EXTERNAL_SIGNING_FORMAT
    # We can't use sha2signcode on zip files
    MOZ_EXTERNAL_SIGNING_FORMAT := $(filter-out sha2signcode,$(MOZ_EXTERNAL_SIGNING_FORMAT))
  endif
  PKG_SUFFIX	= .zip
  INNER_MAKE_PACKAGE	= $(ZIP) -r9D $(PACKAGE) $(MOZ_PKG_DIR) \
    -x \*/.mkdir.done
  INNER_UNMAKE_PACKAGE	= $(UNZIP) $(UNPACKAGE)
  MAKE_SDK = $(call py_action,zip,'$(SDK)' $(MOZ_APP_NAME)-sdk)
endif

ifeq ($(MOZ_PKG_FORMAT),SFX7Z)
  PKG_SUFFIX	= .exe
  INNER_MAKE_PACKAGE	= rm -f app.7z && \
    mv $(MOZ_PKG_DIR) core && \
    $(CYGWIN_WRAPPER) 7z a -r -t7z app.7z -mx -m0=BCJ2 -m1=LZMA:d25 \
      -m2=LZMA:d19 -m3=LZMA:d19 -mb0:1 -mb0s1:2 -mb0s2:3 && \
    mv core $(MOZ_PKG_DIR) && \
    cat $(SFX_HEADER) app.7z > $(PACKAGE) && \
    chmod 0755 $(PACKAGE)
  INNER_UNMAKE_PACKAGE	= $(CYGWIN_WRAPPER) 7z x $(UNPACKAGE) core && \
    mv core $(MOZ_PKG_DIR)
endif

#Create an RPM file
ifeq ($(MOZ_PKG_FORMAT),RPM)
  PKG_SUFFIX  = .rpm
  MOZ_NUMERIC_APP_VERSION = $(shell echo $(MOZ_PKG_VERSION) | sed 's/[^0-9.].*//' )
  MOZ_RPM_RELEASE = $(shell echo $(MOZ_PKG_VERSION) | sed 's/[0-9.]*//' )

  RPMBUILD_TOPDIR=$(ABS_DIST)/rpmbuild
  RPMBUILD_RPMDIR=$(ABS_DIST)
  RPMBUILD_SRPMDIR=$(ABS_DIST)
  RPMBUILD_SOURCEDIR=$(RPMBUILD_TOPDIR)/SOURCES
  RPMBUILD_SPECDIR=$(topsrcdir)/toolkit/mozapps/installer/linux/rpm
  RPMBUILD_BUILDDIR=$(ABS_DIST)/..

  SPEC_FILE = $(RPMBUILD_SPECDIR)/mozilla.spec
  RPM_INCIDENTALS=$(topsrcdir)/toolkit/mozapps/installer/linux/rpm

  RPM_CMD = \
    echo Creating RPM && \
    $(PYTHON) -m mozbuild.action.preprocessor \
      -DMOZ_APP_NAME=$(MOZ_APP_NAME) \
      -DMOZ_APP_DISPLAYNAME='$(MOZ_APP_DISPLAYNAME)' \
      $(RPM_INCIDENTALS)/mozilla.desktop \
      -o $(RPMBUILD_SOURCEDIR)/$(MOZ_APP_NAME).desktop && \
    rm -rf $(ABS_DIST)/$(TARGET_CPU) && \
    $(RPMBUILD) -bb \
    $(SPEC_FILE) \
    --target $(TARGET_CPU) \
    --buildroot $(RPMBUILD_TOPDIR)/BUILDROOT \
    --define 'moz_app_name $(MOZ_APP_NAME)' \
    --define 'moz_app_displayname $(MOZ_APP_DISPLAYNAME)' \
    --define 'moz_app_version $(MOZ_APP_VERSION)' \
    --define 'moz_numeric_app_version $(MOZ_NUMERIC_APP_VERSION)' \
    --define 'moz_rpm_release $(MOZ_RPM_RELEASE)' \
    --define 'buildid $(BUILDID)' \
    --define 'moz_source_repo $(shell awk '$$2 == "MOZ_SOURCE_REPO" {print $$3}' $(DEPTH)/source-repo.h)' \
    --define 'moz_source_stamp $(shell awk '$$2 == "MOZ_SOURCE_STAMP" {print $$3}' $(DEPTH)/source-repo.h)' \
    --define 'moz_branding_directory $(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)' \
    --define '_topdir $(RPMBUILD_TOPDIR)' \
    --define '_rpmdir $(RPMBUILD_RPMDIR)' \
    --define '_sourcedir $(RPMBUILD_SOURCEDIR)' \
    --define '_specdir $(RPMBUILD_SPECDIR)' \
    --define '_srcrpmdir $(RPMBUILD_SRPMDIR)' \
    --define '_builddir $(RPMBUILD_BUILDDIR)' \
    --define '_prefix $(prefix)' \
    --define '_libdir $(libdir)' \
    --define '_bindir $(bindir)' \
    --define '_datadir $(datadir)' \
    --define '_installdir $(installdir)'

  ifdef ENABLE_TESTS
    RPM_CMD += \
      --define 'createtests yes' \
      --define '_testsinstalldir $(shell basename $(installdir))'
  endif

  ifdef INSTALL_SDK
    RPM_CMD += \
      --define 'createdevel yes' \
      --define '_idldir $(idldir)' \
      --define '_sdkdir $(sdkdir)' \
      --define '_includedir $(includedir)'
  endif

  #For each of the main, tests, sdk rpms we want to make sure that
  #if they exist that they are in objdir/dist/ and that they get
  #uploaded and that they are beside the other build artifacts
  MAIN_RPM= $(MOZ_APP_NAME)-$(MOZ_NUMERIC_APP_VERSION)-$(MOZ_RPM_RELEASE).$(BUILDID).$(TARGET_CPU)$(PKG_SUFFIX)
  UPLOAD_EXTRA_FILES += $(MAIN_RPM)
  RPM_CMD += && mv $(TARGET_CPU)/$(MAIN_RPM) $(ABS_DIST)/

  ifdef ENABLE_TESTS
    TESTS_RPM=$(MOZ_APP_NAME)-tests-$(MOZ_NUMERIC_APP_VERSION)-$(MOZ_RPM_RELEASE).$(BUILDID).$(TARGET_CPU)$(PKG_SUFFIX)
    UPLOAD_EXTRA_FILES += $(TESTS_RPM)
    RPM_CMD += && mv $(TARGET_CPU)/$(TESTS_RPM) $(ABS_DIST)/
  endif

  ifdef INSTALL_SDK
    SDK_RPM=$(MOZ_APP_NAME)-devel-$(MOZ_NUMERIC_APP_VERSION)-$(MOZ_RPM_RELEASE).$(BUILDID).$(TARGET_CPU)$(PKG_SUFFIX)
    UPLOAD_EXTRA_FILES += $(SDK_RPM)
    RPM_CMD += && mv $(TARGET_CPU)/$(SDK_RPM) $(ABS_DIST)/
  endif

  INNER_MAKE_PACKAGE = $(RPM_CMD)
  #Avoiding rpm repacks, going to try creating/uploading xpi in rpm files instead
  INNER_UNMAKE_PACKAGE = $(error Try using rpm2cpio and cpio)

endif #Create an RPM file


ifeq ($(MOZ_PKG_FORMAT),APK)
include $(MOZILLA_DIR)/toolkit/mozapps/installer/upload-files-$(MOZ_PKG_FORMAT).mk
endif

ifeq ($(MOZ_PKG_FORMAT),DMG)
  PKG_SUFFIX	= .dmg

  _ABS_MOZSRCDIR = $(shell cd $(MOZILLA_DIR) && pwd)
  PKG_DMG_SOURCE = $(STAGEPATH)$(MOZ_PKG_DIR)
  INNER_MAKE_PACKAGE	= $(call py_action,make_dmg,'$(PKG_DMG_SOURCE)' '$(PACKAGE)')
  INNER_UNMAKE_PACKAGE	= \
    set -ex; \
    rm -rf $(ABS_DIST)/unpack.tmp; \
    mkdir -p $(ABS_DIST)/unpack.tmp; \
    $(_ABS_MOZSRCDIR)/build/package/mac_osx/unpack-diskimage $(UNPACKAGE) /tmp/$(MOZ_PKG_APPNAME)-unpack $(ABS_DIST)/unpack.tmp; \
    rsync -a '$(ABS_DIST)/unpack.tmp/$(_APPNAME)' $(MOZ_PKG_DIR); \
    if test -n '$(MOZ_PKG_MAC_DSSTORE)' ; then \
      mkdir -p '$(dir $(MOZ_PKG_MAC_DSSTORE))'; \
      rsync -a '$(ABS_DIST)/unpack.tmp/.DS_Store' '$(MOZ_PKG_MAC_DSSTORE)'; \
    fi; \
    if test -n '$(MOZ_PKG_MAC_BACKGROUND)' ; then \
      mkdir -p '$(dir $(MOZ_PKG_MAC_BACKGROUND))'; \
      rsync -a '$(ABS_DIST)/unpack.tmp/.background/$(notdir $(MOZ_PKG_MAC_BACKGROUND))' '$(MOZ_PKG_MAC_BACKGROUND)'; \
    fi; \
    if test -n '$(MOZ_PKG_MAC_ICON)' ; then \
      mkdir -p '$(dir $(MOZ_PKG_MAC_ICON))'; \
      rsync -a '$(ABS_DIST)/unpack.tmp/.VolumeIcon.icns' '$(MOZ_PKG_MAC_ICON)'; \
    fi; \
    rm -rf $(ABS_DIST)/unpack.tmp; \
    if test -n '$(MOZ_PKG_MAC_RSRC)' ; then \
      cp $(UNPACKAGE) $(MOZ_PKG_APPNAME).tmp.dmg && \
      hdiutil unflatten $(MOZ_PKG_APPNAME).tmp.dmg && \
      { /Developer/Tools/DeRez -skip plst -skip blkx $(MOZ_PKG_APPNAME).tmp.dmg > '$(MOZ_PKG_MAC_RSRC)' || { rm -f $(MOZ_PKG_APPNAME).tmp.dmg && false; }; } && \
      rm -f $(MOZ_PKG_APPNAME).tmp.dmg; \
    fi
  # The plst and blkx resources are skipped because they belong to each
  # individual dmg and are created by hdiutil.
  SDK_SUFFIX = .tar.bz2
  MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | bzip2 -vf > '$(SDK)'
endif

ifdef MOZ_INTERNAL_SIGNING_FORMAT
  MOZ_SIGN_PREPARED_PACKAGE_CMD=$(MOZ_SIGN_CMD) $(foreach f,$(MOZ_INTERNAL_SIGNING_FORMAT),-f $(f)) $(foreach i,$(SIGN_INCLUDES),-i $(i)) $(foreach x,$(SIGN_EXCLUDES),-x $(x))
  ifeq (WINNT,$(OS_ARCH))
    MOZ_SIGN_PREPARED_PACKAGE_CMD += --nsscmd '$(ABS_DIST)/bin/shlibsign$(BIN_SUFFIX) -v -i'
  endif
endif

# For final GPG / authenticode signing / dmg signing if required
ifdef MOZ_EXTERNAL_SIGNING_FORMAT
  MOZ_SIGN_PACKAGE_CMD=$(MOZ_SIGN_CMD) $(foreach f,$(MOZ_EXTERNAL_SIGNING_FORMAT),-f $(f))
endif

ifdef MOZ_SIGN_PREPARED_PACKAGE_CMD
  ifeq (Darwin, $(OS_ARCH))
    MAKE_PACKAGE    = $(or $(call MAKE_SIGN_EME_VOUCHER,$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(MOZ_CHILD_PROCESS_NAME).app/Contents/MacOS,$(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH)),true) \
                      && (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH) && $(CREATE_PRECOMPLETE_CMD)) \
                      && cd ./$(PKG_DMG_SOURCE) && $(MOZ_SIGN_PREPARED_PACKAGE_CMD) $(MOZ_MACBUNDLE_NAME) \
                      && cd $(PACKAGE_BASE_DIR) && $(INNER_MAKE_PACKAGE)
  else
    MAKE_PACKAGE    = $(MOZ_SIGN_PREPARED_PACKAGE_CMD) $(MOZ_PKG_DIR) \
                      && $(or $(call MAKE_SIGN_EME_VOUCHER,$(STAGEPATH)$(MOZ_PKG_DIR)),true) \
                      && (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH) && $(CREATE_PRECOMPLETE_CMD)) \
                      && $(INNER_MAKE_PACKAGE)
  endif #Darwin

else
  MAKE_PACKAGE    = (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH) && $(CREATE_PRECOMPLETE_CMD)) && $(INNER_MAKE_PACKAGE)
endif

ifdef MOZ_SIGN_PACKAGE_CMD
  MAKE_PACKAGE    += && $(MOZ_SIGN_PACKAGE_CMD) '$(PACKAGE)'
endif

ifdef MOZ_SIGN_CMD
  MAKE_SDK           += && $(MOZ_SIGN_CMD) -f gpg '$(SDK)'
endif

NO_PKG_FILES += \
	core \
	bsdecho \
	js \
	js-config \
	jscpucfg \
	nsinstall \
	viewer \
	TestGtkEmbed \
	elf-dynstr-gc \
	mangle* \
	maptsv* \
	mfc* \
	msdump* \
	msmap* \
	nm2tsv* \
	nsinstall* \
	res/samples \
	res/throbber \
	shlibsign* \
	certutil* \
	pk12util* \
	BadCertServer* \
	OCSPStaplingServer* \
	GenerateOCSPResponse* \
	chrome/chrome.rdf \
	chrome/app-chrome.manifest \
	chrome/overlayinfo \
	components/compreg.dat \
	components/xpti.dat \
	content_unit_tests \
	necko_unit_tests \
	*.dSYM \
	$(NULL)

# If a manifest has not been supplied, the following
# files should be excluded from the package too
ifndef MOZ_PKG_MANIFEST
  NO_PKG_FILES += ssltunnel*
endif

ifdef MOZ_DMD
  NO_PKG_FILES += SmokeDMD
endif

DEFINES += -DDLL_PREFIX=$(DLL_PREFIX) -DDLL_SUFFIX=$(DLL_SUFFIX) -DBIN_SUFFIX=$(BIN_SUFFIX)

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
  DEFINES += -DDIR_MACOS=Contents/MacOS/ -DDIR_RESOURCES=Contents/Resources/
else
  DEFINES += -DDIR_MACOS= -DDIR_RESOURCES=
endif

ifdef MOZ_FOLD_LIBS
  DEFINES += -DMOZ_FOLD_LIBS=1
endif

GARBAGE		+= $(DIST)/$(PACKAGE) $(PACKAGE)

# The following target stages files into two directories: one directory for
# core files, and one for optional extensions based on the information in
# the MOZ_PKG_MANIFEST file.

PKG_ARG = , '$(pkg)'

# MOZ_PKG_MANIFEST is the canonical way to define the package manifest (which
# the packager will preprocess), but for a smooth transition, we derive it
# from the now deprecated MOZ_PKG_MANIFEST_P when MOZ_PKG_MANIFEST is not
# defined.
ifndef MOZ_PKG_MANIFEST
  ifdef MOZ_PKG_MANIFEST_P
    MOZ_PKG_MANIFEST := $(MOZ_PKG_MANIFEST_P)
  endif # MOZ_PKG_MANIFEST_P
endif # MOZ_PKG_MANIFEST

ifndef MOZ_PACKAGER_FORMAT
  MOZ_PACKAGER_FORMAT = $(error MOZ_PACKAGER_FORMAT is not set)
endif

ifneq (android,$(MOZ_WIDGET_TOOLKIT))
  OPTIMIZEJARS = 1
  ifneq (gonk,$(MOZ_WIDGET_TOOLKIT))
    ifdef NIGHTLY_BUILD
      DISABLE_JAR_COMPRESSION = 1
    endif
  endif
endif

# A js binary is needed to perform verification of JavaScript minification.
# We can only use the built binary when not cross-compiling. Environments
# (such as release automation) can provide their own js binary to enable
# verification when cross-compiling.
ifndef JS_BINARY
  ifndef CROSS_COMPILE
    JS_BINARY = $(wildcard $(DIST)/bin/js)
  endif
endif

ifeq ($(OS_TARGET), WINNT)
  INSTALLER_PACKAGE = $(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe
endif

# These are necessary because some of our packages/installers contain spaces
# in their filenames and GNU Make's $(wildcard) function doesn't properly
# deal with them.
empty :=
space = $(empty) $(empty)
QUOTED_WILDCARD = $(if $(wildcard $(subst $(space),?,$(1))),'$(1)')
ESCAPE_SPACE = $(subst $(space),\$(space),$(1))
ESCAPE_WILDCARD = $(subst $(space),?,$(1))

# This variable defines which OpenSSL algorithm to use to
# generate checksums for files that we upload
CHECKSUM_ALGORITHM_PARAM = -d sha512 -d md5 -d sha1

# This variable defines where the checksum file will be located
CHECKSUM_FILE = '$(DIST)/$(PKG_PATH)/$(CHECKSUMS_FILE_BASENAME).checksums'
CHECKSUM_FILES = $(CHECKSUM_FILE)

# Upload MAR tools only if AB_CD is unset or en_US
ifeq (,$(AB_CD:en-US=))
  ifeq (WINNT,$(OS_TARGET))
    UPLOAD_EXTRA_FILES += host/bin/mar.exe
    UPLOAD_EXTRA_FILES += host/bin/mbsdiff.exe
  else
    UPLOAD_EXTRA_FILES += host/bin/mar
    UPLOAD_EXTRA_FILES += host/bin/mbsdiff
  endif
endif

UPLOAD_FILES= \
  $(call QUOTED_WILDCARD,$(DIST)/$(PACKAGE)) \
  $(call QUOTED_WILDCARD,$(INSTALLER_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(COMPLETE_MAR)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(LANGPACK)) \
  $(call QUOTED_WILDCARD,$(wildcard $(DIST)/$(PARTIAL_MAR))) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOZHARNESS_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(TEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(CPP_TEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(XPC_TEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOCHITEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(TALOS_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(REFTEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(WP_TEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(GTEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(SYMBOL_ARCHIVE_BASENAME).zip) \
  $(call QUOTED_WILDCARD,$(DIST)/$(SDK)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(SDK).asc) \
  $(call QUOTED_WILDCARD,$(MOZ_SOURCESTAMP_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_BUILDINFO_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_BUILDID_INFO_TXT_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_MOZINFO_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_TEST_PACKAGES_FILE)) \
  $(call QUOTED_WILDCARD,$(PKG_JSSHELL)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(SYMBOL_FULL_ARCHIVE_BASENAME).zip) \
  $(if $(UPLOAD_EXTRA_FILES), $(foreach f, $(UPLOAD_EXTRA_FILES), $(wildcard $(DIST)/$(f))))

ifdef MOZ_CODE_COVERAGE
  UPLOAD_FILES += \
    $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(CODE_COVERAGE_ARCHIVE_BASENAME).zip)
endif

ifdef UNIFY_DIST
  UNIFY_ARCH := $(notdir $(patsubst %/,%,$(dir $(UNIFY_DIST))))
  UPLOAD_FILES += \
    $(call QUOTED_WILDCARD,$(UNIFY_DIST)/$(SDK_PATH)$(PKG_BASENAME)-$(UNIFY_ARCH).sdk$(SDK_SUFFIX)) \
    $(call QUOTED_WILDCARD,$(UNIFY_DIST)/$(SDK_PATH)$(PKG_BASENAME)-$(UNIFY_ARCH).sdk$(SDK_SUFFIX).asc)
endif

SIGN_CHECKSUM_CMD=
ifdef MOZ_SIGN_CMD
  # If we're signing with gpg, we'll have a bunch of extra detached signatures to
  # upload. We also want to sign our checksums file
  SIGN_CHECKSUM_CMD=$(MOZ_SIGN_CMD) -f gpg $(CHECKSUM_FILE)

  CHECKSUM_FILES += $(CHECKSUM_FILE).asc
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(COMPLETE_MAR).asc)
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(wildcard $(DIST)/$(PARTIAL_MAR).asc))
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(INSTALLER_PACKAGE).asc)
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PACKAGE).asc)
endif

ifdef MOZ_STUB_INSTALLER
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_INST_PATH)$(PKG_STUB_BASENAME).exe)
endif

ifndef MOZ_PKG_SRCDIR
  MOZ_PKG_SRCDIR = $(topsrcdir)
endif

SRC_TAR_PREFIX = $(MOZ_APP_NAME)-$(MOZ_PKG_VERSION)
SRC_TAR_EXCLUDE_PATHS += \
  --exclude='.hg*' \
  --exclude='CVS' \
  --exclude='.cvs*' \
  --exclude='.mozconfig*' \
  --exclude='*.pyc' \
  --exclude='$(MOZILLA_DIR)/Makefile' \
  --exclude='$(MOZILLA_DIR)/dist'
ifdef MOZ_OBJDIR
  SRC_TAR_EXCLUDE_PATHS += --exclude='$(MOZ_OBJDIR)'
endif
CREATE_SOURCE_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode=go-w $(SRC_TAR_EXCLUDE_PATHS) --transform='s,^\./,$(SRC_TAR_PREFIX)/,' -f

SOURCE_TAR = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).tar.xz
HG_BUNDLE_FILE = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_BUNDLE_BASENAME).bundle
SOURCE_CHECKSUM_FILE = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).checksums
SOURCE_UPLOAD_FILES = $(SOURCE_TAR)

HG ?= hg
CREATE_HG_BUNDLE_CMD  = $(HG) -v -R $(topsrcdir) bundle --base null
ifdef HG_BUNDLE_REVISION
  CREATE_HG_BUNDLE_CMD += -r $(HG_BUNDLE_REVISION)
endif
CREATE_HG_BUNDLE_CMD += $(HG_BUNDLE_FILE)
ifdef UPLOAD_HG_BUNDLE
  SOURCE_UPLOAD_FILES  += $(HG_BUNDLE_FILE)
endif
