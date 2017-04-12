##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##


# libaom reverse dependencies (targets that depend on libaom)
AOM_NONDEPS=$(addsuffix .$(VCPROJ_SFX),aom gtest)
AOM_RDEPS=$(foreach vcp,\
              $(filter-out $(AOM_NONDEPS),$^), --dep=$(vcp:.$(VCPROJ_SFX)=):aom)

aom.sln: $(wildcard *.$(VCPROJ_SFX))
	@echo "    [CREATE] $@"
	$(SRC_PATH_BARE)/build/make/gen_msvs_sln.sh \
            $(if $(filter aom.$(VCPROJ_SFX),$^),$(AOM_RDEPS)) \
            --dep=test_libaom:gtest \
            --ver=$(CONFIG_VS_VERSION)\
            --out=$@ $^
aom.sln.mk: aom.sln
	@true

PROJECTS-yes += aom.sln aom.sln.mk
-include aom.sln.mk

# Always install this file, as it is an unconditional post-build rule.
INSTALL_MAPS += src/%     $(SRC_PATH_BARE)/%
INSTALL-SRCS-yes            += $(target).mk
