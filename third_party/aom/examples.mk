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


LIBYUV_SRCS +=  third_party/libyuv/include/libyuv/basic_types.h  \
                third_party/libyuv/include/libyuv/convert.h \
                third_party/libyuv/include/libyuv/convert_argb.h \
                third_party/libyuv/include/libyuv/convert_from.h \
                third_party/libyuv/include/libyuv/cpu_id.h  \
                third_party/libyuv/include/libyuv/planar_functions.h  \
                third_party/libyuv/include/libyuv/rotate.h  \
                third_party/libyuv/include/libyuv/row.h  \
                third_party/libyuv/include/libyuv/scale.h  \
                third_party/libyuv/include/libyuv/scale_row.h  \
                third_party/libyuv/source/cpu_id.cc \
                third_party/libyuv/source/planar_functions.cc \
                third_party/libyuv/source/row_any.cc \
                third_party/libyuv/source/row_common.cc \
                third_party/libyuv/source/row_gcc.cc \
                third_party/libyuv/source/row_mips.cc \
                third_party/libyuv/source/row_neon.cc \
                third_party/libyuv/source/row_neon64.cc \
                third_party/libyuv/source/row_win.cc \
                third_party/libyuv/source/scale.cc \
                third_party/libyuv/source/scale_any.cc \
                third_party/libyuv/source/scale_common.cc \
                third_party/libyuv/source/scale_gcc.cc \
                third_party/libyuv/source/scale_mips.cc \
                third_party/libyuv/source/scale_neon.cc \
                third_party/libyuv/source/scale_neon64.cc \
                third_party/libyuv/source/scale_win.cc \

LIBWEBM_COMMON_SRCS += third_party/libwebm/common/hdr_util.cc \
                       third_party/libwebm/common/hdr_util.h \
                       third_party/libwebm/common/webmids.h

LIBWEBM_MUXER_SRCS += third_party/libwebm/mkvmuxer/mkvmuxer.cc \
                      third_party/libwebm/mkvmuxer/mkvmuxerutil.cc \
                      third_party/libwebm/mkvmuxer/mkvwriter.cc \
                      third_party/libwebm/mkvmuxer/mkvmuxer.h \
                      third_party/libwebm/mkvmuxer/mkvmuxertypes.h \
                      third_party/libwebm/mkvmuxer/mkvmuxerutil.h \
                      third_party/libwebm/mkvparser/mkvparser.h \
                      third_party/libwebm/mkvmuxer/mkvwriter.h

LIBWEBM_PARSER_SRCS = third_party/libwebm/mkvparser/mkvparser.cc \
                      third_party/libwebm/mkvparser/mkvreader.cc \
                      third_party/libwebm/mkvparser/mkvparser.h \
                      third_party/libwebm/mkvparser/mkvreader.h

# Add compile flags and include path for libwebm sources.
ifeq ($(CONFIG_WEBM_IO),yes)
  CXXFLAGS     += -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS
  INC_PATH-yes += $(SRC_PATH_BARE)/third_party/libwebm
endif

# List of examples to build. UTILS are tools meant for distribution
# while EXAMPLES demonstrate specific portions of the API.
UTILS-$(CONFIG_AV1_DECODER) += aomdec.c
aomdec.SRCS                 += md5_utils.c md5_utils.h
aomdec.SRCS                 += aom_ports/mem_ops.h
aomdec.SRCS                 += aom_ports/mem_ops_aligned.h
aomdec.SRCS                 += aom_ports/msvc.h
aomdec.SRCS                 += aom_ports/aom_timer.h
aomdec.SRCS                 += aom/aom_integer.h
aomdec.SRCS                 += args.c args.h
aomdec.SRCS                 += ivfdec.c ivfdec.h
aomdec.SRCS                 += tools_common.c tools_common.h
aomdec.SRCS                 += y4menc.c y4menc.h
ifeq ($(CONFIG_LIBYUV),yes)
  aomdec.SRCS                 += $(LIBYUV_SRCS)
endif
ifeq ($(CONFIG_WEBM_IO),yes)
  aomdec.SRCS                 += $(LIBWEBM_COMMON_SRCS)
  aomdec.SRCS                 += $(LIBWEBM_MUXER_SRCS)
  aomdec.SRCS                 += $(LIBWEBM_PARSER_SRCS)
  aomdec.SRCS                 += webmdec.cc webmdec.h
endif
aomdec.GUID                  = BA5FE66F-38DD-E034-F542-B1578C5FB950
aomdec.DESCRIPTION           = Full featured decoder
UTILS-$(CONFIG_AV1_ENCODER) += aomenc.c
aomenc.SRCS                 += args.c args.h y4minput.c y4minput.h aomenc.h
aomenc.SRCS                 += ivfdec.c ivfdec.h
aomenc.SRCS                 += ivfenc.c ivfenc.h
aomenc.SRCS                 += rate_hist.c rate_hist.h
aomenc.SRCS                 += tools_common.c tools_common.h
aomenc.SRCS                 += examples/encoder_util.h examples/encoder_util.c
aomenc.SRCS                 += warnings.c warnings.h
aomenc.SRCS                 += aom_ports/mem_ops.h
aomenc.SRCS                 += aom_ports/mem_ops_aligned.h
aomenc.SRCS                 += aom_ports/msvc.h
aomenc.SRCS                 += aom_ports/aom_timer.h
aomenc.SRCS                 += aomstats.c aomstats.h
ifeq ($(CONFIG_LIBYUV),yes)
  aomenc.SRCS                 += $(LIBYUV_SRCS)
endif
ifeq ($(CONFIG_WEBM_IO),yes)
  aomenc.SRCS                 += $(LIBWEBM_COMMON_SRCS)
  aomenc.SRCS                 += $(LIBWEBM_MUXER_SRCS)
  aomenc.SRCS                 += $(LIBWEBM_PARSER_SRCS)
  aomenc.SRCS                 += webmenc.cc webmenc.h
endif
aomenc.GUID                  = 548DEC74-7A15-4B2B-AFC3-AA102E7C25C1
aomenc.DESCRIPTION           = Full featured encoder

ifeq ($(CONFIG_ANALYZER),yes)
  EXAMPLES-$(CONFIG_AV1_DECODER)     += analyzer.cc
  analyzer.GUID                       = 83827a8c-e3c3-4b19-8832-0cfc206c4496
  analyzer.SRCS                      += ivfdec.h ivfdec.c
  analyzer.SRCS                      += av1/decoder/inspection.h
  analyzer.SRCS                      += av1/decoder/inspection.c
  analyzer.SRCS                      += video_reader.h video_reader.c
  analyzer.SRCS                      += tools_common.h tools_common.c
  analyzer.DESCRIPTION                = Bitstream analyzer
endif

ifeq ($(CONFIG_INSPECTION),yes)
EXAMPLES-$(CONFIG_AV1_DECODER) += inspect.c
inspect.GUID                   = FA46A420-3356-441F-B0FD-60AA1345C181
inspect.SRCS                   += ivfdec.h ivfdec.c
inspect.SRCS                   += args.c args.h
inspect.SRCS                   += tools_common.h tools_common.c
inspect.SRCS                   += video_common.h
inspect.SRCS                   += video_reader.h video_reader.c
inspect.SRCS                   += aom_ports/mem_ops.h
inspect.SRCS                   += aom_ports/mem_ops_aligned.h
inspect.SRCS                   += aom_ports/msvc.h
inspect.DESCRIPTION             = Dump inspection data
endif

EXAMPLES-$(CONFIG_AV1_DECODER)     += simple_decoder.c
simple_decoder.GUID                 = D3BBF1E9-2427-450D-BBFF-B2843C1D44CC
simple_decoder.SRCS                += ivfdec.h ivfdec.c
simple_decoder.SRCS                += tools_common.h tools_common.c
simple_decoder.SRCS                += video_common.h
simple_decoder.SRCS                += video_reader.h video_reader.c
simple_decoder.SRCS                += aom_ports/mem_ops.h
simple_decoder.SRCS                += aom_ports/mem_ops_aligned.h
simple_decoder.SRCS                += aom_ports/msvc.h
simple_decoder.DESCRIPTION          = Simplified decoder loop
EXAMPLES-$(CONFIG_AV1_DECODER)     += decode_to_md5.c
decode_to_md5.SRCS                 += md5_utils.h md5_utils.c
decode_to_md5.SRCS                 += ivfdec.h ivfdec.c
decode_to_md5.SRCS                 += tools_common.h tools_common.c
decode_to_md5.SRCS                 += video_common.h
decode_to_md5.SRCS                 += video_reader.h video_reader.c
decode_to_md5.SRCS                 += aom_ports/mem_ops.h
decode_to_md5.SRCS                 += aom_ports/mem_ops_aligned.h
decode_to_md5.SRCS                 += aom_ports/msvc.h
decode_to_md5.GUID                  = 59120B9B-2735-4BFE-B022-146CA340FE42
decode_to_md5.DESCRIPTION           = Frame by frame MD5 checksum
EXAMPLES-$(CONFIG_AV1_ENCODER)  += simple_encoder.c
simple_encoder.SRCS             += ivfenc.h ivfenc.c
simple_encoder.SRCS             += tools_common.h tools_common.c
simple_encoder.SRCS             += video_common.h
simple_encoder.SRCS             += video_writer.h video_writer.c
simple_encoder.SRCS             += aom_ports/msvc.h
simple_encoder.GUID              = 4607D299-8A71-4D2C-9B1D-071899B6FBFD
simple_encoder.DESCRIPTION       = Simplified encoder loop
EXAMPLES-$(CONFIG_AV1_ENCODER)  += lossless_encoder.c
lossless_encoder.SRCS           += ivfenc.h ivfenc.c
lossless_encoder.SRCS           += tools_common.h tools_common.c
lossless_encoder.SRCS           += video_common.h
lossless_encoder.SRCS           += video_writer.h video_writer.c
lossless_encoder.SRCS           += aom_ports/msvc.h
lossless_encoder.GUID            = B63C7C88-5348-46DC-A5A6-CC151EF93366
lossless_encoder.DESCRIPTION     = Simplified lossless encoder
EXAMPLES-$(CONFIG_AV1_ENCODER)  += twopass_encoder.c
twopass_encoder.SRCS            += ivfenc.h ivfenc.c
twopass_encoder.SRCS            += tools_common.h tools_common.c
twopass_encoder.SRCS            += video_common.h
twopass_encoder.SRCS            += video_writer.h video_writer.c
twopass_encoder.SRCS            += aom_ports/msvc.h
twopass_encoder.GUID             = 73494FA6-4AF9-4763-8FBB-265C92402FD8
twopass_encoder.DESCRIPTION      = Two-pass encoder loop
EXAMPLES-$(CONFIG_AV1_DECODER)  += decode_with_drops.c
decode_with_drops.SRCS          += ivfdec.h ivfdec.c
decode_with_drops.SRCS          += tools_common.h tools_common.c
decode_with_drops.SRCS          += video_common.h
decode_with_drops.SRCS          += video_reader.h video_reader.c
decode_with_drops.SRCS          += aom_ports/mem_ops.h
decode_with_drops.SRCS          += aom_ports/mem_ops_aligned.h
decode_with_drops.SRCS          += aom_ports/msvc.h
decode_with_drops.GUID           = CE5C53C4-8DDA-438A-86ED-0DDD3CDB8D26
decode_with_drops.DESCRIPTION    = Drops frames while decoding
EXAMPLES-$(CONFIG_AV1_ENCODER)     += set_maps.c
set_maps.SRCS                      += ivfenc.h ivfenc.c
set_maps.SRCS                      += tools_common.h tools_common.c
set_maps.SRCS                      += video_common.h
set_maps.SRCS                      += video_writer.h video_writer.c
set_maps.SRCS                      += aom_ports/msvc.h
set_maps.GUID                       = ECB2D24D-98B8-4015-A465-A4AF3DCC145F
set_maps.DESCRIPTION                = Set active and ROI maps
ifeq ($(CONFIG_AV1_ENCODER),yes)
ifeq ($(CONFIG_AV1_DECODER),yes)
EXAMPLES-$(CONFIG_AV1_ENCODER)     += aom_cx_set_ref.c
aom_cx_set_ref.SRCS                += ivfenc.h ivfenc.c
aom_cx_set_ref.SRCS                += tools_common.h tools_common.c
aom_cx_set_ref.SRCS                += examples/encoder_util.h
aom_cx_set_ref.SRCS                += examples/encoder_util.c
aom_cx_set_ref.SRCS                += video_common.h
aom_cx_set_ref.SRCS                += video_writer.h video_writer.c
aom_cx_set_ref.SRCS                += aom_ports/msvc.h
aom_cx_set_ref.GUID                 = C5E31F7F-96F6-48BD-BD3E-10EBF6E8057A
aom_cx_set_ref.DESCRIPTION          = AV1 set encoder reference frame
endif
endif

# Handle extra library flags depending on codec configuration

# We should not link to math library (libm) on RVCT
# when building for bare-metal targets
ifeq ($(CONFIG_OS_SUPPORT), yes)
CODEC_EXTRA_LIBS-$(CONFIG_AV1)            += m
else
    ifeq ($(CONFIG_GCC), yes)
    CODEC_EXTRA_LIBS-$(CONFIG_AV1)        += m
    endif
endif
#
# End of specified files. The rest of the build rules should happen
# automagically from here.
#


# Examples need different flags based on whether we're building
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


# Expand list of selected examples to build (as specified above)
UTILS           = $(call enabled,UTILS)
EXAMPLES        = $(addprefix examples/,$(call enabled,EXAMPLES))
ALL_EXAMPLES    = $(UTILS) $(EXAMPLES)
UTIL_SRCS       = $(foreach ex,$(UTILS),$($(ex:.c=).SRCS) $($(ex:.cc=).SRCS))
ALL_SRCS        = $(foreach ex, $(ALL_EXAMPLES),  \
                      $($(notdir $(ex:.c=)).SRCS) \
                      $($(notdir $(ex:.cc=)).SRCS))
CODEC_EXTRA_LIBS=$(sort $(call enabled,CODEC_EXTRA_LIBS))


# Expand all example sources into a variable containing all sources
# for that example (not just them main one specified in UTILS/EXAMPLES)
# and add this file to the list (for MSVS workspace generation)
EXAMPLES_C = $(filter-out %.cc, $(ALL_EXAMPLES))
$(foreach ex,$(EXAMPLES_C), \
    $(eval $(notdir $(ex:.c=)).SRCS += $(ex) examples.mk))
EXAMPLES_CXX = $(filter-out %.c, $(ALL_EXAMPLES))
$(foreach ex,$(EXAMPLES_CXX), \
    $(eval $(notdir $(ex:.cc=)).SRCS += $(ex) examples.mk))

# Create build/install dependencies for all examples. The common case
# is handled here. The MSVS case is handled below.
NOT_MSVS = $(if $(CONFIG_MSVS),,yes)
DIST-BINS-$(NOT_MSVS)      += $(addprefix bin/,$(EXAMPLES_C:.c=$(EXE_SFX)))
DIST-BINS-$(NOT_MSVS)      += $(addprefix bin/,$(EXAMPLES_CXX:.cc=$(EXE_SFX)))
INSTALL-BINS-$(NOT_MSVS)   += $(addprefix bin/,$(UTILS:.c=$(EXE_SFX)))
DIST-SRCS-yes              += $(ALL_SRCS)
INSTALL-SRCS-yes           += $(UTIL_SRCS)
OBJS-$(NOT_MSVS)           += $(call objs,$(ALL_SRCS))
BINS-$(NOT_MSVS)           += $(addprefix $(BUILD_PFX), \
                                  $(EXAMPLES_C:.c=$(EXE_SFX)))
BINS-$(NOT_MSVS)           += $(addprefix $(BUILD_PFX), \
                                  $(EXAMPLES_CXX:.cc=$(EXE_SFX)))

# Instantiate linker template for all examples.
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


# Set up additional MSVS environment
ifeq ($(CONFIG_MSVS),yes)
CODEC_LIB=$(if $(CONFIG_SHARED),aom,$(if $(CONFIG_STATIC_MSVCRT),aommt,aommd))
# This variable uses deferred expansion intentionally, since the results of
# $(wildcard) may change during the course of the Make.
VS_PLATFORMS = $(foreach d,$(wildcard */Release/$(CODEC_LIB).lib),$(word 1,$(subst /, ,$(d))))
INSTALL_MAPS += $(foreach p,$(VS_PLATFORMS),bin/$(p)/%  $(p)/Release/%)
endif

# Build Visual Studio Projects. We use a template here to instantiate
# explicit rules rather than using an implicit rule because we want to
# leverage make's VPATH searching rather than specifying the paths on
# each file in ALL_EXAMPLES. This has the unfortunate side effect that
# touching the source files trigger a rebuild of the project files
# even though there is no real dependency there (the dependency is on
# the makefiles). We may want to revisit this.
define vcproj_template
$(1): $($(1:.$(VCPROJ_SFX)=).SRCS) aom.$(VCPROJ_SFX)
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
            $$(INTERNAL_LDFLAGS) $$(LDFLAGS) -l$$(CODEC_LIB) $$^
endef
ALL_EXAMPLES_BASENAME := $(notdir $(ALL_EXAMPLES))
PROJECTS-$(CONFIG_MSVS) += $(ALL_EXAMPLES_BASENAME:.c=.$(VCPROJ_SFX))
INSTALL-BINS-$(CONFIG_MSVS) += $(foreach p,$(VS_PLATFORMS),\
                               $(addprefix bin/$(p)/,$(ALL_EXAMPLES_BASENAME:.c=.exe)))
$(foreach proj,$(call enabled,PROJECTS),\
    $(eval $(call vcproj_template,$(proj))))

#
# Documentation Rules
#
%.dox: %.c
	@echo "    [DOXY] $@"
	@mkdir -p $(dir $@)
	@echo "/*!\page example_$(@F:.dox=) $(@F:.dox=)" > $@
	@echo "   \includelineno $(<F)" >> $@
	@echo "*/" >> $@

samples.dox: examples.mk
	@echo "    [DOXY] $@"
	@echo "/*!\page samples Sample Code" > $@
	@echo "    This SDK includes a number of sample applications."\
	      "Each sample documents a feature of the SDK in both prose"\
	      "and the associated C code."\
	      "The following samples are included: ">>$@
	@$(foreach ex,$(sort $(notdir $(EXAMPLES:.c=))),\
	   echo "     - \subpage example_$(ex) $($(ex).DESCRIPTION)" >> $@;)
	@echo >> $@
	@echo "    In addition, the SDK contains a number of utilities."\
              "Since these utilities are built upon the concepts described"\
              "in the sample code listed above, they are not documented in"\
              "pieces like the samples are. Their source is included here"\
              "for reference. The following utilities are included:" >> $@
	@$(foreach ex,$(sort $(UTILS:.c=)),\
	   echo "     - \subpage example_$(ex) $($(ex).DESCRIPTION)" >> $@;)
	@echo "*/" >> $@

CLEAN-OBJS += examples.doxy samples.dox $(ALL_EXAMPLES:.c=.dox)
DOCS-yes += examples.doxy samples.dox
examples.doxy: samples.dox $(ALL_EXAMPLES:.c=.dox)
	@echo "INPUT += $^" > $@
