# We borrow heavily from the kernel build setup, though we are simpler since
# we don't have Kconfig tweaking settings on us.

# The implicit make rules have it looking for RCS files, among other things.
# We instead explicitly write all the rules we care about.
# It's even quicker (saves ~200ms) to pass -r on the command line.
MAKEFLAGS=-r

# The source directory tree.
srcdir := .
abs_srcdir := $(abspath $(srcdir))

# The name of the builddir.
builddir_name ?= out

# The V=1 flag on command line makes us verbosely print command lines.
ifdef V
  quiet=
else
  quiet=quiet_
endif

# Specify BUILDTYPE=Release on the command line for a release build.
BUILDTYPE ?= Debug

# Directory all our build output goes into.
# Note that this must be two directories beneath src/ for unit tests to pass,
# as they reach into the src/ directory for data with relative paths.
builddir ?= $(builddir_name)/$(BUILDTYPE)
abs_builddir := $(abspath $(builddir))
depsdir := $(builddir)/.deps

# Object output directory.
obj := $(builddir)/obj
abs_obj := $(abspath $(obj))

# We build up a list of every single one of the targets so we can slurp in the
# generated dependency rule Makefiles in one pass.
all_deps :=



# C++ apps need to be linked with g++.
#
# Note: flock is used to seralize linking. Linking is a memory-intensive
# process so running parallel links can often lead to thrashing.  To disable
# the serialization, override LINK via an envrionment variable as follows:
#
#   export LINK=g++
#
# This will allow make to invoke N linker processes as specified in -jN.
LINK ?= flock $(builddir)/linker.lock $(CXX)

CC.target ?= $(CC)
CFLAGS.target ?= $(CFLAGS)
CXX.target ?= $(CXX)
CXXFLAGS.target ?= $(CXXFLAGS)
LINK.target ?= $(LINK)
LDFLAGS.target ?= $(LDFLAGS) 
AR.target ?= $(AR)
ARFLAGS.target ?= crsT

# N.B.: the logic of which commands to run should match the computation done
# in gyp's make.py where ARFLAGS.host etc. is computed.
# TODO(evan): move all cross-compilation logic to gyp-time so we don't need
# to replicate this environment fallback in make as well.
CC.host ?= gcc
CFLAGS.host ?=
CXX.host ?= g++
CXXFLAGS.host ?=
LINK.host ?= g++
LDFLAGS.host ?=
AR.host ?= ar
ARFLAGS.host := crsT

# Define a dir function that can handle spaces.
# http://www.gnu.org/software/make/manual/make.html#Syntax-of-Functions
# "leading spaces cannot appear in the text of the first argument as written.
# These characters can be put into the argument value by variable substitution."
empty :=
space := $(empty) $(empty)

# http://stackoverflow.com/questions/1189781/using-make-dir-or-notdir-on-a-path-with-spaces
replace_spaces = $(subst $(space),?,$1)
unreplace_spaces = $(subst ?,$(space),$1)
dirx = $(call unreplace_spaces,$(dir $(call replace_spaces,$1)))

# Flags to make gcc output dependency info.  Note that you need to be
# careful here to use the flags that ccache and distcc can understand.
# We write to a dep file on the side first and then rename at the end
# so we can't end up with a broken dep file.
depfile = $(depsdir)/$(call replace_spaces,$@).d
DEPFLAGS = -MMD -MF $(depfile).raw

# We have to fixup the deps output in a few ways.
# (1) the file output should mention the proper .o file.
# ccache or distcc lose the path to the target, so we convert a rule of
# the form:
#   foobar.o: DEP1 DEP2
# into
#   path/to/foobar.o: DEP1 DEP2
# (2) we want missing files not to cause us to fail to build.
# We want to rewrite
#   foobar.o: DEP1 DEP2 \
#               DEP3
# to
#   DEP1:
#   DEP2:
#   DEP3:
# so if the files are missing, they're just considered phony rules.
# We have to do some pretty insane escaping to get those backslashes
# and dollar signs past make, the shell, and sed at the same time.
# Doesn't work with spaces, but that's fine: .d files have spaces in
# their names replaced with other characters.
define fixup_dep
# The depfile may not exist if the input file didn't have any #includes.
touch $(depfile).raw
# Fixup path as in (1).
sed -e "s|^$(notdir $@)|$@|" $(depfile).raw >> $(depfile)
# Add extra rules as in (2).
# We remove slashes and replace spaces with new lines;
# remove blank lines;
# delete the first line and append a colon to the remaining lines.
sed -e 's|\\||' -e 'y| |\n|' $(depfile).raw |\
  grep -v '^$$'                             |\
  sed -e 1d -e 's|$$|:|'                     \
    >> $(depfile)
rm $(depfile).raw
endef

# Command definitions:
# - cmd_foo is the actual command to run;
# - quiet_cmd_foo is the brief-output summary of the command.

quiet_cmd_cc = CC($(TOOLSET)) $@
cmd_cc = $(CC.$(TOOLSET)) $(GYP_CFLAGS) $(DEPFLAGS) $(CFLAGS.$(TOOLSET)) -c -o $@ $<

quiet_cmd_cxx = CXX($(TOOLSET)) $@
cmd_cxx = $(CXX.$(TOOLSET)) $(GYP_CXXFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<

quiet_cmd_touch = TOUCH $@
cmd_touch = touch $@

quiet_cmd_copy = COPY $@
# send stderr to /dev/null to ignore messages when linking directories.
cmd_copy = ln -f "$<" "$@" 2>/dev/null || (rm -rf "$@" && cp -af "$<" "$@")

quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) $(ARFLAGS.$(TOOLSET)) $@ $(filter %.o,$^)

# Due to circular dependencies between libraries :(, we wrap the
# special "figure out circular dependencies" flags around the entire
# input list during linking.
quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ -Wl,--start-group $(LD_INPUTS) -Wl,--end-group $(LIBS)

# We support two kinds of shared objects (.so):
# 1) shared_library, which is just bundling together many dependent libraries
# into a link line.
# 2) loadable_module, which is generating a module intended for dlopen().
#
# They differ only slightly:
# In the former case, we want to package all dependent code into the .so.
# In the latter case, we want to package just the API exposed by the
# outermost module.
# This means shared_library uses --whole-archive, while loadable_module doesn't.
# (Note that --whole-archive is incompatible with the --start-group used in
# normal linking.)

# Other shared-object link notes:
# - Set SONAME to the library filename so our binaries don't reference
# the local, absolute paths used on the link command-line.
quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--whole-archive $(LD_INPUTS) -Wl,--no-whole-archive $(LIBS)

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--start-group $(filter-out FORCE_DO_CMD, $^) -Wl,--end-group $(LIBS)


# Define an escape_quotes function to escape single quotes.
# This allows us to handle quotes properly as long as we always use
# use single quotes and escape_quotes.
escape_quotes = $(subst ','\'',$(1))
# This comment is here just to include a ' to unconfuse syntax highlighting.
# Define an escape_vars function to escape '$' variable syntax.
# This allows us to read/write command lines with shell variables (e.g.
# $LD_LIBRARY_PATH), without triggering make substitution.
escape_vars = $(subst $$,$$$$,$(1))
# Helper that expands to a shell command to echo a string exactly as it is in
# make. This uses printf instead of echo because printf's behaviour with respect
# to escape sequences is more portable than echo's across different shells
# (e.g., dash, bash).
exact_echo = printf '%s\n' '$(call escape_quotes,$(1))'

# Helper to compare the command we're about to run against the command
# we logged the last time we ran the command.  Produces an empty
# string (false) when the commands match.
# Tricky point: Make has no string-equality test function.
# The kernel uses the following, but it seems like it would have false
# positives, where one string reordered its arguments.
#   arg_check = $(strip $(filter-out $(cmd_$(1)), $(cmd_$@)) \
#                       $(filter-out $(cmd_$@), $(cmd_$(1))))
# We instead substitute each for the empty string into the other, and
# say they're equal if both substitutions produce the empty string.
# .d files contain ? instead of spaces, take that into account.
command_changed = $(or $(subst $(cmd_$(1)),,$(cmd_$(call replace_spaces,$@))),\
                       $(subst $(cmd_$(call replace_spaces,$@)),,$(cmd_$(1))))

# Helper that is non-empty when a prerequisite changes.
# Normally make does this implicitly, but we force rules to always run
# so we can check their command lines.
#   $? -- new prerequisites
#   $| -- order-only dependencies
prereq_changed = $(filter-out FORCE_DO_CMD,$(filter-out $|,$?))

# Helper that executes all postbuilds, and deletes the output file when done
# if any of the postbuilds failed.
define do_postbuilds
  @E=0;\
  for p in $(POSTBUILDS); do\
    eval $$p;\
    F=$$?;\
    if [ $$F -ne 0 ]; then\
      E=$$F;\
    fi;\
  done;\
  if [ $$E -ne 0 ]; then\
    rm -rf "$@";\
    exit $$E;\
  fi
endef

# do_cmd: run a command via the above cmd_foo names, if necessary.
# Should always run for a given target to handle command-line changes.
# Second argument, if non-zero, makes it do asm/C/C++ dependency munging.
# Third argument, if non-zero, makes it do POSTBUILDS processing.
# Note: We intentionally do NOT call dirx for depfile, since it contains ? for
# spaces already and dirx strips the ? characters.
define do_cmd
$(if $(or $(command_changed),$(prereq_changed)),
  @$(call exact_echo,  $($(quiet)cmd_$(1)))
  @mkdir -p "$(call dirx,$@)" "$(dir $(depfile))"
  $(if $(findstring flock,$(word 1,$(cmd_$1))),
    @$(cmd_$(1))
    @echo "  $(quiet_cmd_$(1)): Finished",
    @$(cmd_$(1))
  )
  @$(call exact_echo,$(call escape_vars,cmd_$(call replace_spaces,$@) := $(cmd_$(1)))) > $(depfile)
  @$(if $(2),$(fixup_dep))
  $(if $(and $(3), $(POSTBUILDS)),
    $(call do_postbuilds)
  )
)
endef

# Declare the "all" target first so it is the default,
# even though we don't have the deps yet.
.PHONY: all
all:

# Use FORCE_DO_CMD to force a target to run.  Should be coupled with
# do_cmd.
.PHONY: FORCE_DO_CMD
FORCE_DO_CMD:

TOOLSET := host
# Suffix rules, putting all outputs into $(obj).
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

# Try building from generated source, too.
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

$(obj).$(TOOLSET)/%.o: $(obj)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

TOOLSET := target
# Suffix rules, putting all outputs into $(obj).
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

# Try building from generated source, too.
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

$(obj).$(TOOLSET)/%.o: $(obj)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)


ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,peerconnection_client.target.mk)))),)
  include peerconnection_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,peerconnection_server.target.mk)))),)
  include peerconnection_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_audio/resampler.target.mk)))),)
  include src/common_audio/resampler.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_audio/resampler_unittests.target.mk)))),)
  include src/common_audio/resampler_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_audio/signal_processing.target.mk)))),)
  include src/common_audio/signal_processing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_audio/signal_processing_unittests.target.mk)))),)
  include src/common_audio/signal_processing_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_audio/vad.target.mk)))),)
  include src/common_audio/vad.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_audio/vad_unittests.target.mk)))),)
  include src/common_audio/vad_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_video/jpeg_test.target.mk)))),)
  include src/common_video/jpeg_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_video/libyuv_unittests.target.mk)))),)
  include src/common_video/libyuv_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_video/webrtc_jpeg.target.mk)))),)
  include src/common_video/webrtc_jpeg.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/common_video/webrtc_libyuv.target.mk)))),)
  include src/common_video/webrtc_libyuv.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/CNG.target.mk)))),)
  include src/modules/CNG.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/G711.target.mk)))),)
  include src/modules/G711.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/G722.target.mk)))),)
  include src/modules/G722.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/G722Test.target.mk)))),)
  include src/modules/G722Test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/NetEq.target.mk)))),)
  include src/modules/NetEq.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/NetEqRTPplay.target.mk)))),)
  include src/modules/NetEqRTPplay.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/NetEqTestTools.target.mk)))),)
  include src/modules/NetEqTestTools.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/PCM16B.target.mk)))),)
  include src/modules/PCM16B.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/RTPanalyze.target.mk)))),)
  include src/modules/RTPanalyze.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/RTPcat.target.mk)))),)
  include src/modules/RTPcat.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/RTPchange.target.mk)))),)
  include src/modules/RTPchange.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/RTPencode.target.mk)))),)
  include src/modules/RTPencode.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/RTPjitter.target.mk)))),)
  include src/modules/RTPjitter.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/RTPtimeshift.target.mk)))),)
  include src/modules/RTPtimeshift.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/aec.target.mk)))),)
  include src/modules/aec.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/aec_sse2.target.mk)))),)
  include src/modules/aec_sse2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/aecm.target.mk)))),)
  include src/modules/aecm.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/agc.target.mk)))),)
  include src/modules/agc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/apm_util.target.mk)))),)
  include src/modules/apm_util.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_coding_module.target.mk)))),)
  include src/modules/audio_coding_module.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_coding_module_test.target.mk)))),)
  include src/modules/audio_coding_module_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_coding_unittests.target.mk)))),)
  include src/modules/audio_coding_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_conference_mixer.target.mk)))),)
  include src/modules/audio_conference_mixer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_conference_mixer_unittests.target.mk)))),)
  include src/modules/audio_conference_mixer_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_device.target.mk)))),)
  include src/modules/audio_device.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_device_test_api.target.mk)))),)
  include src/modules/audio_device_test_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_device_test_func.target.mk)))),)
  include src/modules/audio_device_test_func.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audio_processing.target.mk)))),)
  include src/modules/audio_processing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audioproc.target.mk)))),)
  include src/modules/audioproc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audioproc_debug_proto.target.mk)))),)
  include src/modules/audioproc_debug_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audioproc_unittest.target.mk)))),)
  include src/modules/audioproc_unittest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/audioproc_unittest_proto.target.mk)))),)
  include src/modules/audioproc_unittest_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/cng_unittests.target.mk)))),)
  include src/modules/cng_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/g711_test.target.mk)))),)
  include src/modules/g711_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/g711_unittests.target.mk)))),)
  include src/modules/g711_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/g722_unittests.target.mk)))),)
  include src/modules/g722_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iLBC.target.mk)))),)
  include src/modules/iLBC.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iLBCtest.target.mk)))),)
  include src/modules/iLBCtest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iSAC.target.mk)))),)
  include src/modules/iSAC.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iSACAPITest.target.mk)))),)
  include src/modules/iSACAPITest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iSACFix.target.mk)))),)
  include src/modules/iSACFix.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iSACFixtest.target.mk)))),)
  include src/modules/iSACFixtest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iSACSwitchSampRateTest.target.mk)))),)
  include src/modules/iSACSwitchSampRateTest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/iSACtest.target.mk)))),)
  include src/modules/iSACtest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/media_file.target.mk)))),)
  include src/modules/media_file.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/media_file_unittests.target.mk)))),)
  include src/modules/media_file_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/neteq_unittests.target.mk)))),)
  include src/modules/neteq_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/ns.target.mk)))),)
  include src/modules/ns.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/ns_fix.target.mk)))),)
  include src/modules/ns_fix.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/pcm16b_unittests.target.mk)))),)
  include src/modules/pcm16b_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/rtp_rtcp.target.mk)))),)
  include src/modules/rtp_rtcp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/rtp_rtcp_unittests.target.mk)))),)
  include src/modules/rtp_rtcp_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/test_bwe.target.mk)))),)
  include src/modules/test_bwe.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/test_fec.target.mk)))),)
  include src/modules/test_fec.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/test_framework.target.mk)))),)
  include src/modules/test_framework.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/test_rtp_rtcp_api.target.mk)))),)
  include src/modules/test_rtp_rtcp_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/udp_transport.target.mk)))),)
  include src/modules/udp_transport.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/udp_transport_unittests.target.mk)))),)
  include src/modules/udp_transport_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/unpack_aecdump.target.mk)))),)
  include src/modules/unpack_aecdump.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_capture_module.target.mk)))),)
  include src/modules/video_capture_module.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_capture_module_test.target.mk)))),)
  include src/modules/video_capture_module_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_codecs_test_framework.target.mk)))),)
  include src/modules/video_codecs_test_framework.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_codecs_test_framework_integrationtests.target.mk)))),)
  include src/modules/video_codecs_test_framework_integrationtests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_codecs_test_framework_unittests.target.mk)))),)
  include src/modules/video_codecs_test_framework_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_coding_test.target.mk)))),)
  include src/modules/video_coding_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_coding_unittests.target.mk)))),)
  include src/modules/video_coding_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_processing.target.mk)))),)
  include src/modules/video_processing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_processing_sse2.target.mk)))),)
  include src/modules/video_processing_sse2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_processing_unittests.target.mk)))),)
  include src/modules/video_processing_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_quality_measurement.target.mk)))),)
  include src/modules/video_quality_measurement.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_render_module.target.mk)))),)
  include src/modules/video_render_module.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/video_render_module_test.target.mk)))),)
  include src/modules/video_render_module_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/vp8_test.target.mk)))),)
  include src/modules/vp8_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/vp8_unittests.target.mk)))),)
  include src/modules/vp8_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/webrtc_i420.target.mk)))),)
  include src/modules/webrtc_i420.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/webrtc_utility.target.mk)))),)
  include src/modules/webrtc_utility.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/webrtc_utility_unittests.target.mk)))),)
  include src/modules/webrtc_utility_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/webrtc_video_coding.target.mk)))),)
  include src/modules/webrtc_video_coding.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/modules/webrtc_vp8.target.mk)))),)
  include src/modules/webrtc_vp8.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/system_wrappers/source/system_wrappers.target.mk)))),)
  include src/system_wrappers/source/system_wrappers.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/system_wrappers/source/system_wrappers_unittests.target.mk)))),)
  include src/system_wrappers/source/system_wrappers_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/video_engine/video_engine_core.target.mk)))),)
  include src/video_engine/video_engine_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/video_engine/video_engine_core_unittests.target.mk)))),)
  include src/video_engine/video_engine_core_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/video_engine/vie_auto_test.target.mk)))),)
  include src/video_engine/vie_auto_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/voice_engine/voe_auto_test.target.mk)))),)
  include src/voice_engine/voe_auto_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/voice_engine/voe_cmd_test.target.mk)))),)
  include src/voice_engine/voe_cmd_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/voice_engine/voice_engine_core.target.mk)))),)
  include src/voice_engine/voice_engine_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,src/voice_engine/voice_engine_unittests.target.mk)))),)
  include src/voice_engine/voice_engine_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,test/metrics.target.mk)))),)
  include test/metrics.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,test/metrics_unittests.target.mk)))),)
  include test/metrics_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,test/test_support.target.mk)))),)
  include test/test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,test/test_support_main.target.mk)))),)
  include test/test_support_main.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,test/test_support_unittests.target.mk)))),)
  include test/test_support_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gmock.target.mk)))),)
  include testing/gmock.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gmock_main.target.mk)))),)
  include testing/gmock_main.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest.target.mk)))),)
  include testing/gtest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest_main.target.mk)))),)
  include testing/gtest_main.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest_prod.host.mk)))),)
  include testing/gtest_prod.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest_prod.target.mk)))),)
  include testing/gtest_prod.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/expat/expat.target.mk)))),)
  include third_party/expat/expat.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/google-gflags/google-gflags.target.mk)))),)
  include third_party/google-gflags/google-gflags.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/jsoncpp/jsoncpp.target.mk)))),)
  include third_party/jsoncpp/jsoncpp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libjingle.target.mk)))),)
  include third_party/libjingle/libjingle.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libjingle_app.target.mk)))),)
  include third_party/libjingle/libjingle_app.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libjingle_p2p.target.mk)))),)
  include third_party/libjingle/libjingle_p2p.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjpeg_turbo/libjpeg.target.mk)))),)
  include third_party/libjpeg_turbo/libjpeg.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/libsrtp.target.mk)))),)
  include third_party/libsrtp/libsrtp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx.target.mk)))),)
  include third_party/libvpx/libvpx.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libyuv/libyuv.target.mk)))),)
  include third_party/libyuv/libyuv.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_full_do_not_use.host.mk)))),)
  include third_party/protobuf/protobuf_full_do_not_use.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_full_do_not_use.target.mk)))),)
  include third_party/protobuf/protobuf_full_do_not_use.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_lite.host.mk)))),)
  include third_party/protobuf/protobuf_lite.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_lite.target.mk)))),)
  include third_party/protobuf/protobuf_lite.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protoc.host.mk)))),)
  include third_party/protobuf/protoc.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/py_proto.target.mk)))),)
  include third_party/protobuf/py_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/config_sources.host.mk)))),)
  include third_party/yasm/config_sources.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/generate_files.host.mk)))),)
  include third_party/yasm/generate_files.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genmacro.host.mk)))),)
  include third_party/yasm/genmacro.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genmodule.host.mk)))),)
  include third_party/yasm/genmodule.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genperf.host.mk)))),)
  include third_party/yasm/genperf.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genperf_libs.host.mk)))),)
  include third_party/yasm/genperf_libs.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genstring.host.mk)))),)
  include third_party/yasm/genstring.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genversion.host.mk)))),)
  include third_party/yasm/genversion.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/re2c.host.mk)))),)
  include third_party/yasm/re2c.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/yasm.host.mk)))),)
  include third_party/yasm/yasm.host.mk
endif

quiet_cmd_regen_makefile = ACTION Regenerating $@
cmd_regen_makefile = ./build/gyp_chromium -fmake --ignore-environment "--toplevel-dir=." -Ibuild/common.gypi -I/home/jesup/src/mozilla/webrtc_import5/webrtc_update/trunk/src/supplement.gypi "--depth=." peerconnection.gyp
Makefile: src/modules/video_processing/main/test/vpm_tests.gypi third_party/expat/expat.gyp src/modules/audio_processing/ns/ns.gypi src/modules/video_coding/codecs/i420/main/source/i420.gypi third_party/yasm/yasm_compile.gypi src/common_video/jpeg/main/source/jpeg.gypi src/common_video/common_video.gyp src/modules/audio_coding/codecs/pcm16b/pcm16b.gypi third_party/google-gflags/google-gflags.gyp src/build/common.gypi src/common_audio/common_audio.gyp peerconnection.gyp src/modules/audio_coding/codecs/iSAC/isacfix_test.gypi src/voice_engine/main/source/voice_engine_core.gypi src/modules/video_coding/codecs/tools/video_codecs_tools.gypi test/metrics.gyp testing/gtest.gyp third_party/libyuv/libyuv.gyp src/modules/video_coding/main/source/video_coding.gypi third_party/libvpx/libvpx_srcs_x86.gypi src/modules/audio_coding/codecs/iSAC/fix/source/isacfix.gypi src/modules/audio_device/main/source/audio_device.gypi src/modules/audio_processing/audio_processing.gypi src/common_video/libyuv/libyuv.gypi src/modules/video_coding/codecs/test/video_codecs_test_framework.gypi src/modules/audio_processing/apm_tests.gypi src/voice_engine/voice_engine.gyp src/modules/video_processing/main/source/video_processing.gypi src/modules/media_file/source/media_file.gypi src/common_audio/vad/vad.gypi src/modules/audio_conference_mixer/source/audio_conference_mixer.gypi src/build/protoc.gypi third_party/yasm/yasm.gyp src/modules/audio_coding/codecs/ilbc/ilbc.gypi src/common_audio/signal_processing/signal_processing.gypi build/release.gypi src/modules/rtp_rtcp/source/rtp_rtcp_tests.gypi src/modules/audio_coding/codecs/iSAC/isac_test.gypi src/modules/video_coding/codecs/vp8/main/source/vp8.gypi src/modules/rtp_rtcp/test/test_bwe/test_bwe.gypi src/modules/audio_coding/neteq/neteq.gypi src/modules/audio_coding/main/source/audio_coding_module.gypi src/modules/udp_transport/source/udp_transport.gypi third_party/libjingle/libjingle.gyp src/common_audio/resampler/resampler.gypi build/internal/release_impl_official.gypi third_party/protobuf/protobuf.gyp src/video_engine/video_engine_core.gypi src/modules/video_capture/main/source/video_capture.gypi src/modules/rtp_rtcp/test/testFec/test_fec.gypi src/modules/audio_processing/utility/util.gypi src/modules/rtp_rtcp/source/rtp_rtcp.gypi src/modules/modules.gyp build/internal/release_defaults.gypi src/modules/audio_coding/codecs/g722/g722.gypi third_party/libvpx/libvpx_srcs_arm_neon.gypi third_party/libsrtp/libsrtp.gyp testing/gmock.gyp src/voice_engine/main/test/voice_engine_tests.gypi src/modules/video_coding/codecs/test_framework/test_framework.gypi src/modules/audio_processing/aec/aec.gypi third_party/libvpx/libvpx.gyp src/modules/audio_coding/codecs/g711/g711.gypi third_party/libjpeg_turbo/libjpeg.gyp src/modules/utility/source/utility.gypi src/video_engine/video_engine.gyp src/modules/audio_coding/codecs/cng/cng.gypi third_party/libvpx/libvpx_srcs_arm.gypi build/filename_rules.gypi third_party/jsoncpp/jsoncpp.gyp src/system_wrappers/source/system_wrappers.gyp build/internal/release_impl.gypi src/supplement.gypi build/use_skia_on_mac.gypi src/common_settings.gypi third_party/libvpx/libvpx_srcs_x86_64.gypi src/video_engine/test/auto_test/vie_auto_test.gypi src/modules/audio_processing/agc/agc.gypi test/test.gyp src/modules/audio_processing/aecm/aecm.gypi src/modules/video_coding/main/source/video_coding_test.gypi build/common.gypi src/modules/audio_coding/codecs/iSAC/main/source/isac.gypi src/video_engine/main/test/WindowsTest/windowstest.gypi src/modules/video_render/main/source/video_render.gypi src/modules/rtp_rtcp/test/testAPI/test_api.gypi
	$(call do_cmd,regen_makefile)

# "all" is a concatenation of the "all" targets from all the included
# sub-makefiles. This is just here to clarify.
all:

# Add in dependency-tracking rules.  $(all_deps) is the list of every single
# target in our tree. Only consider the ones with .d (dependency) info:
d_files := $(wildcard $(foreach f,$(all_deps),$(depsdir)/$(f).d))
ifneq ($(d_files),)
  # Rather than include each individual .d file, concatenate them into a
  # single file which make is able to load faster.  We split this into
  # commands that take 1000 files at a time to avoid overflowing the
  # command line.
  $(shell cat $(wordlist 1,1000,$(d_files)) > $(depsdir)/all.deps)

  ifneq ($(word 1001,$(d_files)),)
    $(shell cat $(wordlist 1001,2000,$(d_files)) >> $(depsdir)/all.deps)
  endif
  ifneq ($(word 2001,$(d_files)),)
    $(error Found unprocessed dependency files (gyp didn't generate enough rules!))
  endif

  # make looks for ways to re-generate included makefiles, but in our case, we
  # don't have a direct way. Explicitly telling make that it has nothing to do
  # for them makes it go faster.
  $(depsdir)/all.deps: ;

  include $(depsdir)/all.deps
endif
