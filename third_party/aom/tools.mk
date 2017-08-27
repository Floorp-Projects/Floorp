##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##

# List of tools to build.
ifeq ($(CONFIG_ENTROPY_STATS), yes)
TOOLS-$(CONFIG_AV1_ENCODER)      += aom_entropy_optimizer.c
aom_entropy_optimizer.GUID        = 3afa9b05-940b-4d68-b5aa-55157d8ed7b4
aom_entropy_optimizer.DESCRIPTION = Offline default probability optimizer
endif

#
# End of specified files. The rest of the build rules should happen
# automagically from here.
#

# Tools need different flags based on whether we're building
# from an installed tree or a version controlled tree. Determine
# the proper paths.
ifeq ($(HAVE_ALT_TREE_LAYOUT),yes)
    LIB_PATH-yes := $(SRC_PATH_BARE)/../lib
    INC_PATH-yes := $(SRC_PATH_BARE)/../include
else
    LIB_PATH-yes                     += $(if $(BUILD_PFX),$(BUILD_PFX),.)
    INC_PATH-$(CONFIG_AV1_DECODER)   += $(SRC_PATH_BARE)/av1
    INC_PATH-$(CONFIG_AV1_ENCODER)   += $(SRC_PATH_BARE)/av1
endif
INC_PATH-$(CONFIG_LIBYUV) += $(SRC_PATH_BARE)/third_party/libyuv/include
LIB_PATH := $(call enabled,LIB_PATH)
INC_PATH := $(call enabled,INC_PATH)
INTERNAL_CFLAGS = $(addprefix -I,$(INC_PATH))
INTERNAL_LDFLAGS += $(addprefix -L,$(LIB_PATH))

# Expand list of selected tools to build (as specified above)
TOOLS           = $(addprefix tools/,$(call enabled,TOOLS))
ALL_SRCS        = $(foreach ex,$(TOOLS),$($(notdir $(ex:.c=)).SRCS))
CFLAGS += -I../include
CODEC_EXTRA_LIBS=$(sort $(call enabled,CODEC_EXTRA_LIBS))

ifneq ($(CONFIG_CODEC_SRCS), yes)
  CFLAGS += -I../include/vpx
endif

# Expand all tools sources into a variable containing all sources
# for that tools (not just them main one specified in TOOLS)
# and add this file to the list (for MSVS workspace generation)
$(foreach ex,$(TOOLS),$(eval $(notdir $(ex:.c=)).SRCS += $(ex) tools.mk))


# Create build/install dependencies for all tools. The common case
# is handled here. The MSVS case is handled below.
NOT_MSVS = $(if $(CONFIG_MSVS),,yes)
DIST-BINS-$(NOT_MSVS)      += $(addprefix bin/,$(TOOLS:.c=$(EXE_SFX)))
DIST-SRCS-yes              += $(ALL_SRCS)
OBJS-$(NOT_MSVS)           += $(call objs,$(ALL_SRCS))
BINS-$(NOT_MSVS)           += $(addprefix $(BUILD_PFX),$(TOOLS:.c=$(EXE_SFX)))

# Instantiate linker template for all tools.
ifeq ($(CONFIG_OS_SUPPORT), yes)
CODEC_EXTRA_LIBS-$(CONFIG_AV1)            += m
else
    ifeq ($(CONFIG_GCC), yes)
    CODEC_EXTRA_LIBS-$(CONFIG_AV1)        += m
    endif
endif

CODEC_LIB=$(if $(CONFIG_DEBUG_LIBS),aom_g,aom)
ifneq ($(filter darwin%,$(TGT_OS)),)
SHARED_LIB_SUF=.dylib
else
ifneq ($(filter os2%,$(TGT_OS)),)
SHARED_LIB_SUF=_dll.a
else
SHARED_LIB_SUF=.so
endif
endif
CODEC_LIB_SUF=$(if $(CONFIG_SHARED),$(SHARED_LIB_SUF),.a)
$(foreach bin,$(BINS-yes),\
    $(eval $(bin):$(LIB_PATH)/lib$(CODEC_LIB)$(CODEC_LIB_SUF))\
    $(eval $(call linker_template,$(bin),\
        $(call objs,$($(notdir $(bin:$(EXE_SFX)=)).SRCS)) \
        -l$(CODEC_LIB) $(addprefix -l,$(CODEC_EXTRA_LIBS))\
        )))

# The following pairs define a mapping of locations in the distribution
# tree to locations in the source/build trees.
INSTALL_MAPS += src/%.c   %.c
INSTALL_MAPS += src/%     $(SRC_PATH_BARE)/%
INSTALL_MAPS += bin/%     %
INSTALL_MAPS += %         %


# Build Visual Studio Projects. We use a template here to instantiate
# explicit rules rather than using an implicit rule because we want to
# leverage make's VPATH searching rather than specifying the paths on
# each file in TOOLS. This has the unfortunate side effect that
# touching the source files trigger a rebuild of the project files
# even though there is no real dependency there (the dependency is on
# the makefiles). We may want to revisit this.
define vcproj_template
$(1): $($(1:.$(VCPROJ_SFX)=).SRCS) vpx.$(VCPROJ_SFX)
	$(if $(quiet),@echo "    [vcproj] $$@")
	$(qexec)$$(GEN_VCPROJ)\
            --exe\
            --target=$$(TOOLCHAIN)\
            --name=$$(@:.$(VCPROJ_SFX)=)\
            --ver=$$(CONFIG_VS_VERSION)\
            --proj-guid=$$($$(@:.$(VCPROJ_SFX)=).GUID)\
            --src-path-bare="$(SRC_PATH_BARE)" \
            $$(if $$(CONFIG_STATIC_MSVCRT),--static-crt) \
            --out=$$@ $$(INTERNAL_CFLAGS) $$(CFLAGS) \
            $$(INTERNAL_LDFLAGS) $$(LDFLAGS) $$^
endef
TOOLS_BASENAME := $(notdir $(TOOLS))
PROJECTS-$(CONFIG_MSVS) += $(TOOLS_BASENAME:.c=.$(VCPROJ_SFX))
INSTALL-BINS-$(CONFIG_MSVS) += $(foreach p,$(VS_PLATFORMS),\
                               $(addprefix bin/$(p)/,$(TOOLS_BASENAME:.c=.exe)))
$(foreach proj,$(call enabled,PROJECTS),\
    $(eval $(call vcproj_template,$(proj))))
