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


# ARM assembly files are written in RVCT-style. We use some make magic to
# filter those files to allow GCC compilation
ifeq ($(ARCH_ARM),yes)
  ASM:=$(if $(filter yes,$(CONFIG_GCC)$(CONFIG_MSVS)),.asm.s,.asm)
else
  ASM:=.asm
endif

#
# Rule to generate runtime cpu detection files
#
define rtcd_h_template
$$(BUILD_PFX)$(1).h: $$(SRC_PATH_BARE)/$(2)
	@echo "    [CREATE] $$@"
	$$(qexec)$$(SRC_PATH_BARE)/build/make/rtcd.pl --arch=$$(TGT_ISA) \
          --sym=$(1) \
          --config=$$(CONFIG_DIR)$$(target)-$$(TOOLCHAIN).mk \
          $$(RTCD_OPTIONS) $$^ > $$@
CLEAN-OBJS += $$(BUILD_PFX)$(1).h
RTCD += $$(BUILD_PFX)$(1).h
endef

CODEC_SRCS-yes += CHANGELOG
CODEC_SRCS-yes += libs.mk

include $(SRC_PATH_BARE)/aom/aom_codec.mk
CODEC_SRCS-yes += $(addprefix aom/,$(call enabled,API_SRCS))
CODEC_DOC_SRCS += $(addprefix aom/,$(call enabled,API_DOC_SRCS))

include $(SRC_PATH_BARE)/aom_mem/aom_mem.mk
CODEC_SRCS-yes += $(addprefix aom_mem/,$(call enabled,MEM_SRCS))

include $(SRC_PATH_BARE)/aom_scale/aom_scale.mk
CODEC_SRCS-yes += $(addprefix aom_scale/,$(call enabled,SCALE_SRCS))

include $(SRC_PATH_BARE)/aom_ports/aom_ports.mk
CODEC_SRCS-yes += $(addprefix aom_ports/,$(call enabled,PORTS_SRCS))

include $(SRC_PATH_BARE)/aom_dsp/aom_dsp.mk
CODEC_SRCS-yes += $(addprefix aom_dsp/,$(call enabled,DSP_SRCS))

include $(SRC_PATH_BARE)/aom_util/aom_util.mk
CODEC_SRCS-yes += $(addprefix aom_util/,$(call enabled,UTIL_SRCS))

#  AV1 make file
ifeq ($(CONFIG_AV1),yes)
  AV1_PREFIX=av1/
  include $(SRC_PATH_BARE)/$(AV1_PREFIX)av1_common.mk
endif

ifeq ($(CONFIG_AV1_ENCODER),yes)
  AV1_PREFIX=av1/
  include $(SRC_PATH_BARE)/$(AV1_PREFIX)av1_cx.mk
  CODEC_SRCS-yes += $(addprefix $(AV1_PREFIX),$(call enabled,AV1_CX_SRCS))
  CODEC_EXPORTS-yes += $(addprefix $(AV1_PREFIX),$(AV1_CX_EXPORTS))
  CODEC_SRCS-yes += $(AV1_PREFIX)av1_cx.mk aom/aom.h aom/aomcx.h
  INSTALL-LIBS-yes += include/aom/aom.h include/aom/aomcx.h
  INSTALL_MAPS += include/aom/% $(SRC_PATH_BARE)/$(AV1_PREFIX)/%
  CODEC_DOC_SRCS += aom/aom.h aom/aomcx.h
  CODEC_DOC_SECTIONS += av1 av1_encoder encoder
endif

ifeq ($(CONFIG_AV1_DECODER),yes)
  AV1_PREFIX=av1/
  include $(SRC_PATH_BARE)/$(AV1_PREFIX)av1_dx.mk
  CODEC_SRCS-yes += $(addprefix $(AV1_PREFIX),$(call enabled,AV1_DX_SRCS))
  CODEC_EXPORTS-yes += $(addprefix $(AV1_PREFIX),$(AV1_DX_EXPORTS))
  CODEC_SRCS-yes += $(AV1_PREFIX)av1_dx.mk aom/aom.h aom/aomdx.h
  INSTALL-LIBS-yes += include/aom/aom.h include/aom/aomdx.h
  INSTALL_MAPS += include/aom/% $(SRC_PATH_BARE)/$(AV1_PREFIX)/%
  CODEC_DOC_SRCS += aom/aom.h aom/aomdx.h
  CODEC_DOC_SECTIONS += av1 av1_decoder decoder
endif

AV1_PREFIX=av1/
$(BUILD_PFX)$(AV1_PREFIX)%.c.o: CFLAGS += -Wextra

ifeq ($(CONFIG_MSVS),yes)
CODEC_LIB=$(if $(CONFIG_STATIC_MSVCRT),aommt,aommd)
GTEST_LIB=$(if $(CONFIG_STATIC_MSVCRT),gtestmt,gtestmd)
# This variable uses deferred expansion intentionally, since the results of
# $(wildcard) may change during the course of the Make.
VS_PLATFORMS = $(foreach d,$(wildcard */Release/$(CODEC_LIB).lib),$(word 1,$(subst /, ,$(d))))
endif

# The following pairs define a mapping of locations in the distribution
# tree to locations in the source/build trees.
INSTALL_MAPS += include/aom/% $(SRC_PATH_BARE)/aom/%
INSTALL_MAPS += include/aom/% $(SRC_PATH_BARE)/aom_ports/%
INSTALL_MAPS += $(LIBSUBDIR)/%     %
INSTALL_MAPS += src/%     $(SRC_PATH_BARE)/%
ifeq ($(CONFIG_MSVS),yes)
INSTALL_MAPS += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/%  $(p)/Release/%)
INSTALL_MAPS += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/%  $(p)/Debug/%)
endif

CODEC_SRCS-yes += build/make/version.sh
CODEC_SRCS-yes += build/make/rtcd.pl
CODEC_SRCS-yes += aom_ports/emmintrin_compat.h
CODEC_SRCS-yes += aom_ports/mem_ops.h
CODEC_SRCS-yes += aom_ports/mem_ops_aligned.h
CODEC_SRCS-yes += aom_ports/aom_once.h
CODEC_SRCS-yes += $(BUILD_PFX)aom_config.c
INSTALL-SRCS-no += $(BUILD_PFX)aom_config.c
ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += third_party/x86inc/x86inc.asm
endif
CODEC_EXPORTS-yes += aom/exports_com
CODEC_EXPORTS-$(CONFIG_AV1_ENCODER) += aom/exports_enc
CODEC_EXPORTS-$(CONFIG_AV1_DECODER) += aom/exports_dec

INSTALL-LIBS-yes += include/aom/aom_codec.h
INSTALL-LIBS-yes += include/aom/aom_frame_buffer.h
INSTALL-LIBS-yes += include/aom/aom_image.h
INSTALL-LIBS-yes += include/aom/aom_integer.h
INSTALL-LIBS-$(CONFIG_AV1_DECODER) += include/aom/aom_decoder.h
INSTALL-LIBS-$(CONFIG_AV1_ENCODER) += include/aom/aom_encoder.h
ifeq ($(CONFIG_EXTERNAL_BUILD),yes)
ifeq ($(CONFIG_MSVS),yes)
INSTALL-LIBS-yes                  += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/$(CODEC_LIB).lib)
INSTALL-LIBS-$(CONFIG_DEBUG_LIBS) += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/$(CODEC_LIB)d.lib)
INSTALL-LIBS-$(CONFIG_SHARED) += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/aom.dll)
INSTALL-LIBS-$(CONFIG_SHARED) += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/aom.exp)
endif
else
INSTALL-LIBS-$(CONFIG_STATIC) += $(LIBSUBDIR)/libaom.a
INSTALL-LIBS-$(CONFIG_DEBUG_LIBS) += $(LIBSUBDIR)/libaom_g.a
endif

CODEC_SRCS=$(call enabled,CODEC_SRCS)
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(CODEC_SRCS)
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(call enabled,CODEC_EXPORTS)


# Generate a list of all enabled sources, in particular for exporting to gyp
# based build systems.
libaom_srcs.txt:
	@echo "    [CREATE] $@"
	@echo $(CODEC_SRCS) | xargs -n1 echo | LC_ALL=C sort -u > $@
CLEAN-OBJS += libaom_srcs.txt


ifeq ($(CONFIG_EXTERNAL_BUILD),yes)
ifeq ($(CONFIG_MSVS),yes)

aom.def: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)$(SRC_PATH_BARE)/build/make/gen_msvs_def.sh\
            --name=aom\
            --out=$@ $^
CLEAN-OBJS += aom.def

# Assembly files that are included, but don't define symbols themselves.
# Filtered out to avoid Visual Studio build warnings.
ASM_INCLUDES := \
    third_party/x86inc/x86inc.asm \
    aom_config.asm \
    aom_ports/x86_abi_support.asm \

aom.$(VCPROJ_SFX): $(CODEC_SRCS) aom.def
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            $(if $(CONFIG_SHARED),--dll,--lib) \
            --target=$(TOOLCHAIN) \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --name=aom \
            --proj-guid=DCE19DAF-69AC-46DB-B14A-39F0FAA5DB74 \
            --module-def=aom.def \
            --ver=$(CONFIG_VS_VERSION) \
            --src-path-bare="$(SRC_PATH_BARE)" \
            --out=$@ $(CFLAGS) \
            $(filter-out $(addprefix %, $(ASM_INCLUDES)), $^) \
            --src-path-bare="$(SRC_PATH_BARE)" \

PROJECTS-yes += aom.$(VCPROJ_SFX)

aom.$(VCPROJ_SFX): aom_config.asm
aom.$(VCPROJ_SFX): $(RTCD)

endif
else
LIBAOM_OBJS=$(call objs,$(CODEC_SRCS))
OBJS-yes += $(LIBAOM_OBJS)
LIBS-$(if yes,$(CONFIG_STATIC)) += $(BUILD_PFX)libaom.a $(BUILD_PFX)libaom_g.a
$(BUILD_PFX)libaom_g.a: $(LIBAOM_OBJS)

SO_VERSION_MAJOR := 0
SO_VERSION_MINOR := 0
SO_VERSION_PATCH := 0
ifeq ($(filter darwin%,$(TGT_OS)),$(TGT_OS))
LIBAOM_SO               := libaom.$(SO_VERSION_MAJOR).dylib
SHARED_LIB_SUF          := .dylib
EXPORT_FILE             := libaom.syms
LIBAOM_SO_SYMLINKS      := $(addprefix $(LIBSUBDIR)/, \
                             libaom.dylib  )
else
ifeq ($(filter iphonesimulator%,$(TGT_OS)),$(TGT_OS))
LIBAOM_SO               := libaom.$(SO_VERSION_MAJOR).dylib
SHARED_LIB_SUF          := .dylib
EXPORT_FILE             := libaom.syms
LIBAOM_SO_SYMLINKS      := $(addprefix $(LIBSUBDIR)/, libaom.dylib)
else
ifeq ($(filter os2%,$(TGT_OS)),$(TGT_OS))
LIBAOM_SO               := libaom$(SO_VERSION_MAJOR).dll
SHARED_LIB_SUF          := _dll.a
EXPORT_FILE             := libaom.def
LIBAOM_SO_SYMLINKS      :=
LIBAOM_SO_IMPLIB        := libaom_dll.a
else
LIBAOM_SO               := libaom.so.$(SO_VERSION_MAJOR).$(SO_VERSION_MINOR).$(SO_VERSION_PATCH)
SHARED_LIB_SUF          := .so
EXPORT_FILE             := libaom.ver
LIBAOM_SO_SYMLINKS      := $(addprefix $(LIBSUBDIR)/, \
                             libaom.so libaom.so.$(SO_VERSION_MAJOR) \
                             libaom.so.$(SO_VERSION_MAJOR).$(SO_VERSION_MINOR))
endif
endif
endif

LIBS-$(CONFIG_SHARED) += $(BUILD_PFX)$(LIBAOM_SO)\
                           $(notdir $(LIBAOM_SO_SYMLINKS)) \
                           $(if $(LIBAOM_SO_IMPLIB), $(BUILD_PFX)$(LIBAOM_SO_IMPLIB))
$(BUILD_PFX)$(LIBAOM_SO): $(LIBAOM_OBJS) $(EXPORT_FILE)
$(BUILD_PFX)$(LIBAOM_SO): extralibs += -lm
$(BUILD_PFX)$(LIBAOM_SO): SONAME = libaom.so.$(SO_VERSION_MAJOR)
$(BUILD_PFX)$(LIBAOM_SO): EXPORTS_FILE = $(EXPORT_FILE)

libaom.ver: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)echo "{ global:" > $@
	$(qexec)for f in $?; do awk '{print $$2";"}' < $$f >>$@; done
	$(qexec)echo "local: *; };" >> $@
CLEAN-OBJS += libaom.ver

libaom.syms: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)awk '{print "_"$$2}' $^ >$@
CLEAN-OBJS += libaom.syms

libaom.def: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)echo LIBRARY $(LIBAOM_SO:.dll=) INITINSTANCE TERMINSTANCE > $@
	$(qexec)echo "DATA MULTIPLE NONSHARED" >> $@
	$(qexec)echo "EXPORTS" >> $@
	$(qexec)awk '{print "_"$$2}' $^ >>$@
CLEAN-OBJS += libaom.def

libaom_dll.a: $(LIBAOM_SO)
	@echo "    [IMPLIB] $@"
	$(qexec)emximp -o $@ $<
CLEAN-OBJS += libaom_dll.a

define libaom_symlink_template
$(1): $(2)
	@echo "    [LN]     $(2) $$@"
	$(qexec)mkdir -p $$(dir $$@)
	$(qexec)ln -sf $(2) $$@
endef

$(eval $(call libaom_symlink_template,\
    $(addprefix $(BUILD_PFX),$(notdir $(LIBAOM_SO_SYMLINKS))),\
    $(BUILD_PFX)$(LIBAOM_SO)))
$(eval $(call libaom_symlink_template,\
    $(addprefix $(DIST_DIR)/,$(LIBAOM_SO_SYMLINKS)),\
    $(LIBAOM_SO)))


INSTALL-LIBS-$(CONFIG_SHARED) += $(LIBAOM_SO_SYMLINKS)
INSTALL-LIBS-$(CONFIG_SHARED) += $(LIBSUBDIR)/$(LIBAOM_SO)
INSTALL-LIBS-$(CONFIG_SHARED) += $(if $(LIBAOM_SO_IMPLIB),$(LIBSUBDIR)/$(LIBAOM_SO_IMPLIB))


LIBS-yes += aom.pc
aom.pc: config.mk libs.mk
	@echo "    [CREATE] $@"
	$(qexec)echo '# pkg-config file from libaom $(VERSION_STRING)' > $@
	$(qexec)echo 'prefix=$(PREFIX)' >> $@
	$(qexec)echo 'exec_prefix=$${prefix}' >> $@
	$(qexec)echo 'libdir=$${prefix}/$(LIBSUBDIR)' >> $@
	$(qexec)echo 'includedir=$${prefix}/include' >> $@
	$(qexec)echo '' >> $@
	$(qexec)echo 'Name: aom' >> $@
	$(qexec)echo 'Description: WebM Project AVx codec implementation' >> $@
	$(qexec)echo 'Version: $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)' >> $@
	$(qexec)echo 'Requires:' >> $@
	$(qexec)echo 'Conflicts:' >> $@
	$(qexec)echo 'Libs: -L$${libdir} -laom -lm' >> $@
ifeq ($(HAVE_PTHREAD_H),yes)
	$(qexec)echo 'Libs.private: -lm -lpthread' >> $@
else
	$(qexec)echo 'Libs.private: -lm' >> $@
endif
	$(qexec)echo 'Cflags: -I$${includedir}' >> $@
INSTALL-LIBS-yes += $(LIBSUBDIR)/pkgconfig/aom.pc
INSTALL_MAPS += $(LIBSUBDIR)/pkgconfig/%.pc %.pc
CLEAN-OBJS += aom.pc
endif

#
# Rule to make assembler configuration file from C configuration file
#
ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
# YASM
$(BUILD_PFX)aom_config.asm: $(BUILD_PFX)aom_config.h
	@echo "    [CREATE] $@"
	@egrep "#define [A-Z0-9_]+ [01]" $< \
	    | awk '{print $$2 " equ " $$3}' > $@
else
ADS2GAS=$(if $(filter yes,$(CONFIG_GCC)),| $(ASM_CONVERSION))
$(BUILD_PFX)aom_config.asm: $(BUILD_PFX)aom_config.h
	@echo "    [CREATE] $@"
	@egrep "#define [A-Z0-9_]+ [01]" $< \
	    | awk '{print $$2 " EQU " $$3}' $(ADS2GAS) > $@
	@echo "        END" $(ADS2GAS) >> $@
CLEAN-OBJS += $(BUILD_PFX)aom_config.asm
endif

#
# Add assembler dependencies for configuration.
#
$(filter %.s.o,$(OBJS-yes)):     $(BUILD_PFX)aom_config.asm
$(filter %$(ASM).o,$(OBJS-yes)): $(BUILD_PFX)aom_config.asm


$(shell $(SRC_PATH_BARE)/build/make/version.sh "$(SRC_PATH_BARE)" $(BUILD_PFX)aom_version.h)
CLEAN-OBJS += $(BUILD_PFX)aom_version.h

#
# Add include path for libwebm sources.
#
ifeq ($(CONFIG_WEBM_IO),yes)
  CXXFLAGS += -I$(SRC_PATH_BARE)/third_party/libwebm
endif

##
## libaom test directives
##
ifeq ($(CONFIG_UNIT_TESTS),yes)
LIBAOM_TEST_DATA_PATH ?= .

include $(SRC_PATH_BARE)/test/test.mk
LIBAOM_TEST_SRCS=$(addprefix test/,$(call enabled,LIBAOM_TEST_SRCS))
LIBAOM_TEST_BIN=./test_libaom$(EXE_SFX)
LIBAOM_TEST_DATA=$(addprefix $(LIBAOM_TEST_DATA_PATH)/,\
                     $(call enabled,LIBAOM_TEST_DATA))
libaom_test_data_url=https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx/$(1)

TEST_INTRA_PRED_SPEED_BIN=./test_intra_pred_speed$(EXE_SFX)
TEST_INTRA_PRED_SPEED_SRCS=$(addprefix test/,$(call enabled,TEST_INTRA_PRED_SPEED_SRCS))
TEST_INTRA_PRED_SPEED_OBJS := $(sort $(call objs,$(TEST_INTRA_PRED_SPEED_SRCS)))

libaom_test_srcs.txt:
	@echo "    [CREATE] $@"
	@echo $(LIBAOM_TEST_SRCS) | xargs -n1 echo | LC_ALL=C sort -u > $@
CLEAN-OBJS += libaom_test_srcs.txt

$(LIBAOM_TEST_DATA): $(SRC_PATH_BARE)/test/test-data.sha1
	@echo "    [DOWNLOAD] $@"
	$(qexec)trap 'rm -f $@' INT TERM &&\
            curl -L -o $@ $(call libaom_test_data_url,$(@F))

testdata:: $(LIBAOM_TEST_DATA)
	$(qexec)[ -x "$$(which sha1sum)" ] && sha1sum=sha1sum;\
          [ -x "$$(which shasum)" ] && sha1sum=shasum;\
          [ -x "$$(which sha1)" ] && sha1sum=sha1;\
          if [ -n "$${sha1sum}" ]; then\
            set -e;\
            echo "Checking test data:";\
            for f in $(call enabled,LIBAOM_TEST_DATA); do\
                grep $$f $(SRC_PATH_BARE)/test/test-data.sha1 |\
                    (cd $(LIBAOM_TEST_DATA_PATH); $${sha1sum} -c);\
            done; \
        else\
            echo "Skipping test data integrity check, sha1sum not found.";\
        fi

ifeq ($(CONFIG_EXTERNAL_BUILD),yes)
ifeq ($(CONFIG_MSVS),yes)
gtest.$(VCPROJ_SFX): $(SRC_PATH_BARE)/third_party/googletest/src/googletest/src/gtest-all.cc
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            --lib \
            --target=$(TOOLCHAIN) \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --name=gtest \
            --proj-guid=EC00E1EC-AF68-4D92-A255-181690D1C9B1 \
            --ver=$(CONFIG_VS_VERSION) \
            --src-path-bare="$(SRC_PATH_BARE)" \
            -D_VARIADIC_MAX=10 \
            --out=gtest.$(VCPROJ_SFX) \
	    $(SRC_PATH_BARE)/third_party/googletest/src/googletest/src/gtest-all.cc \
            -I. -I"$(SRC_PATH_BARE)/third_party/googletest/src/googletest/include" \
	    -I"$(SRC_PATH_BARE)/third_party/googletest/src/googletest"

PROJECTS-$(CONFIG_MSVS) += gtest.$(VCPROJ_SFX)

test_libaom.$(VCPROJ_SFX): $(LIBAOM_TEST_SRCS) aom.$(VCPROJ_SFX) gtest.$(VCPROJ_SFX)
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            --exe \
            --target=$(TOOLCHAIN) \
            --name=test_libaom \
            -D_VARIADIC_MAX=10 \
            --proj-guid=CD837F5F-52D8-4314-A370-895D614166A7 \
            --ver=$(CONFIG_VS_VERSION) \
            --src-path-bare="$(SRC_PATH_BARE)" \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --out=$@ $(INTERNAL_CFLAGS) $(CFLAGS) \
            -I. -I"$(SRC_PATH_BARE)/third_party/googletest/src/googletest/include" \
            $(if $(CONFIG_WEBM_IO),-I"$(SRC_PATH_BARE)/third_party/libwebm") \
            -L. -l$(CODEC_LIB) -l$(GTEST_LIB) $^

PROJECTS-$(CONFIG_MSVS) += test_libaom.$(VCPROJ_SFX)

LIBAOM_TEST_BIN := $(addprefix $(TGT_OS:win64=x64)/Release/,$(notdir $(LIBAOM_TEST_BIN)))

ifneq ($(strip $(TEST_INTRA_PRED_SPEED_OBJS)),)
PROJECTS-$(CONFIG_MSVS) += test_intra_pred_speed.$(VCPROJ_SFX)
test_intra_pred_speed.$(VCPROJ_SFX): $(TEST_INTRA_PRED_SPEED_SRCS) aom.$(VCPROJ_SFX) gtest.$(VCPROJ_SFX)
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            --exe \
            --target=$(TOOLCHAIN) \
            --name=test_intra_pred_speed \
            -D_VARIADIC_MAX=10 \
            --proj-guid=CD837F5F-52D8-4314-A370-895D614166A7 \
            --ver=$(CONFIG_VS_VERSION) \
            --src-path-bare="$(SRC_PATH_BARE)" \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --out=$@ $(INTERNAL_CFLAGS) $(CFLAGS) \
            -I. -I"$(SRC_PATH_BARE)/third_party/googletest/src/googletest/include" \
            -L. -l$(CODEC_LIB) -l$(GTEST_LIB) $^
endif  # TEST_INTRA_PRED_SPEED
endif
else

include $(SRC_PATH_BARE)/third_party/googletest/gtest.mk
GTEST_SRCS := $(addprefix third_party/,$(call enabled,GTEST_SRCS))
GTEST_OBJS=$(call objs,$(GTEST_SRCS))
ifeq ($(filter win%,$(TGT_OS)),$(TGT_OS))
# Disabling pthreads globally will cause issues on darwin and possibly elsewhere
$(GTEST_OBJS) $(GTEST_OBJS:.o=.d): CXXFLAGS += -DGTEST_HAS_PTHREAD=0
endif
GTEST_INCLUDES := -I$(SRC_PATH_BARE)/third_party/googletest/src/googletest
GTEST_INCLUDES += -I$(SRC_PATH_BARE)/third_party/googletest/src/googletest/include
$(GTEST_OBJS) $(GTEST_OBJS:.o=.d): CXXFLAGS += $(GTEST_INCLUDES)
OBJS-yes += $(GTEST_OBJS)
LIBS-yes += $(BUILD_PFX)libgtest.a $(BUILD_PFX)libgtest_g.a
$(BUILD_PFX)libgtest_g.a: $(GTEST_OBJS)

LIBAOM_TEST_OBJS=$(sort $(call objs,$(LIBAOM_TEST_SRCS)))
$(LIBAOM_TEST_OBJS) $(LIBAOM_TEST_OBJS:.o=.d): CXXFLAGS += $(GTEST_INCLUDES)
OBJS-yes += $(LIBAOM_TEST_OBJS)
BINS-yes += $(LIBAOM_TEST_BIN)

CODEC_LIB=$(if $(CONFIG_DEBUG_LIBS),aom_g,aom)
CODEC_LIB_SUF=$(if $(CONFIG_SHARED),$(SHARED_LIB_SUF),.a)
TEST_LIBS := lib$(CODEC_LIB)$(CODEC_LIB_SUF) libgtest.a
$(LIBAOM_TEST_BIN): $(TEST_LIBS)
$(eval $(call linkerxx_template,$(LIBAOM_TEST_BIN), \
              $(LIBAOM_TEST_OBJS) \
              -L. -laom -lgtest $(extralibs) -lm))

ifneq ($(strip $(TEST_INTRA_PRED_SPEED_OBJS)),)
$(TEST_INTRA_PRED_SPEED_OBJS) $(TEST_INTRA_PRED_SPEED_OBJS:.o=.d): CXXFLAGS += $(GTEST_INCLUDES)
OBJS-yes += $(TEST_INTRA_PRED_SPEED_OBJS)
BINS-yes += $(TEST_INTRA_PRED_SPEED_BIN)

$(TEST_INTRA_PRED_SPEED_BIN): $(TEST_LIBS)
$(eval $(call linkerxx_template,$(TEST_INTRA_PRED_SPEED_BIN), \
              $(TEST_INTRA_PRED_SPEED_OBJS) \
              -L. -laom -lgtest $(extralibs) -lm))
endif  # TEST_INTRA_PRED_SPEED

endif  # CONFIG_UNIT_TESTS

# Install test sources only if codec source is included
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(patsubst $(SRC_PATH_BARE)/%,%,\
    $(shell find $(SRC_PATH_BARE)/third_party/googletest -type f))
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(LIBAOM_TEST_SRCS)
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(TEST_INTRA_PRED_SPEED_SRCS)

define test_shard_template
test:: test_shard.$(1)
test-no-data-check:: test_shard_ndc.$(1)
test_shard.$(1) test_shard_ndc.$(1): $(LIBAOM_TEST_BIN)
	@set -e; \
	 export GTEST_SHARD_INDEX=$(1); \
	 export GTEST_TOTAL_SHARDS=$(2); \
	 $(LIBAOM_TEST_BIN)
test_shard.$(1): testdata
.PHONY: test_shard.$(1)
endef

NUM_SHARDS := 10
SHARDS := 0 1 2 3 4 5 6 7 8 9
$(foreach s,$(SHARDS),$(eval $(call test_shard_template,$(s),$(NUM_SHARDS))))

endif

##
## documentation directives
##
CLEAN-OBJS += libs.doxy
DOCS-yes += libs.doxy
libs.doxy: $(CODEC_DOC_SRCS)
	@echo "    [CREATE] $@"
	@rm -f $@
	@echo "INPUT += $^" >> $@
	@echo "INCLUDE_PATH += ." >> $@;
	@echo "ENABLED_SECTIONS += $(sort $(CODEC_DOC_SECTIONS))" >> $@

## Generate rtcd.h for all objects
ifeq ($(CONFIG_DEPENDENCY_TRACKING),yes)
$(OBJS-yes:.o=.d): $(RTCD)
else
$(OBJS-yes): $(RTCD)
endif

## Update the global src list
SRCS += $(CODEC_SRCS) $(LIBAOM_TEST_SRCS) $(GTEST_SRCS)

##
## aomdec/aomenc tests.
##
ifeq ($(CONFIG_UNIT_TESTS),yes)
TEST_BIN_PATH = .
ifeq ($(CONFIG_MSVS),yes)
# MSVC will build both Debug and Release configurations of tools in a
# sub directory named for the current target. Assume the user wants to
# run the Release tools, and assign TEST_BIN_PATH accordingly.
# TODO(tomfinegan): Is this adequate for ARM?
# TODO(tomfinegan): Support running the debug versions of tools?
TEST_BIN_PATH := $(addsuffix /$(TGT_OS:win64=x64)/Release, $(TEST_BIN_PATH))
endif
utiltest utiltest-no-data-check:
	$(qexec)$(SRC_PATH_BARE)/test/aomdec.sh \
		--test-data-path $(LIBAOM_TEST_DATA_PATH) \
		--bin-path $(TEST_BIN_PATH)
	$(qexec)$(SRC_PATH_BARE)/test/aomenc.sh \
		--test-data-path $(LIBAOM_TEST_DATA_PATH) \
		--bin-path $(TEST_BIN_PATH)
utiltest: testdata
else
utiltest utiltest-no-data-check:
	@echo Unit tests must be enabled to make the utiltest target.
endif

##
## Example tests.
##
ifeq ($(CONFIG_UNIT_TESTS),yes)
# All non-MSVC targets output example targets in a sub dir named examples.
EXAMPLES_BIN_PATH = examples
ifeq ($(CONFIG_MSVS),yes)
# MSVC will build both Debug and Release configurations of the examples in a
# sub directory named for the current target. Assume the user wants to
# run the Release tools, and assign EXAMPLES_BIN_PATH accordingly.
# TODO(tomfinegan): Is this adequate for ARM?
# TODO(tomfinegan): Support running the debug versions of tools?
EXAMPLES_BIN_PATH := $(TGT_OS:win64=x64)/Release
endif
exampletest exampletest-no-data-check: examples
	$(qexec)$(SRC_PATH_BARE)/test/examples.sh \
		--test-data-path $(LIBAOM_TEST_DATA_PATH) \
		--bin-path $(EXAMPLES_BIN_PATH)
exampletest: testdata
else
exampletest exampletest-no-data-check:
	@echo Unit tests must be enabled to make the exampletest target.
endif
