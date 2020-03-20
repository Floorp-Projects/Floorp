## Process this file with automake to produce Makefile.in

# Copyright (c) 2011, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# This allows #includes to be relative to src/
AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS =
AM_CXXFLAGS =

if ANDROID_HOST
# This allows using fixed NDK headers when building for Android.
AM_CXXFLAGS += -I$(top_srcdir)/src/common/android/include
# This is only necessary for building the unit tests until GTest is upgraded
# to a future version.
AM_CXXFLAGS += -I$(top_srcdir)/src/common/android/testing/include
endif

AM_CXXFLAGS += $(WARN_CXXFLAGS)

if LINUX_HOST
# Build as PIC on Linux, for linux_client_unittest_shlib
AM_CFLAGS += -fPIC
AM_CXXFLAGS += -fPIC
endif

# Specify include paths for ac macros
ACLOCAL_AMFLAGS = -I m4

# License file is called LICENSE not COPYING
AUTOMAKE_OPTIONS = foreign

## Documentation
docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)

dist_doc_DATA = \
	AUTHORS \
	ChangeLog \
	INSTALL \
	LICENSE \
	NEWS \
	README.md

## Headers
if LINUX_HOST
includeclhdir = $(includedir)/$(PACKAGE)/client/linux/handler
includeclh_HEADERS = $(top_srcdir)/src/client/linux/handler/*.h

includecldwcdir = $(includedir)/$(PACKAGE)/client/linux/dump_writer_common
includecldwc_HEADERS = $(top_srcdir)/src/client/linux/dump_writer_common/*.h

includeclmdir = $(includedir)/$(PACKAGE)/client/linux/minidump_writer
includeclm_HEADERS = $(top_srcdir)/src/client/linux/minidump_writer/*.h

includeclcdir = $(includedir)/$(PACKAGE)/client/linux/crash_generation
includeclc_HEADERS = $(top_srcdir)/src/client/linux/crash_generation/*.h

includelssdir = $(includedir)/$(PACKAGE)/third_party/lss
includelss_HEADERS = $(top_srcdir)/src/third_party/lss/*.h

includecldir = $(includedir)/$(PACKAGE)/common/linux
includecl_HEADERS = $(top_srcdir)/src/common/linux/*.h
endif

includegbcdir = $(includedir)/$(PACKAGE)/google_breakpad/common
includegbc_HEADERS = $(top_srcdir)/src/google_breakpad/common/*.h

includecdir = $(includedir)/$(PACKAGE)/common
includec_HEADERS = $(top_srcdir)/src/common/*.h

includepdir = $(includedir)/$(PACKAGE)/processor
includep_HEADERS = $(top_srcdir)/src/processor/*.h

## pkgconfig files
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

## Common test logic
if SYSTEM_TEST_LIBS
TEST_CFLAGS = $(GTEST_CFLAGS) $(GMOCK_CFLAGS)
TEST_LIBS = $(GTEST_LIBS) -lgtest_main $(GMOCK_LIBS)
TEST_DEPS =
else
TEST_CFLAGS = \
	-I$(top_srcdir)/src/testing/include \
	-I$(top_srcdir)/src/testing/googletest/include \
	-I$(top_srcdir)/src/testing/googletest \
	-I$(top_srcdir)/src/testing/googlemock/include \
	-I$(top_srcdir)/src/testing/googlemock \
	-I$(top_srcdir)/src/testing
TEST_LIBS = src/testing/libtesting.a
TEST_DEPS = $(TEST_LIBS)
endif

## Libraries
check_LIBRARIES =
noinst_LIBRARIES =
lib_LIBRARIES =
bin_PROGRAMS =
check_PROGRAMS =
EXTRA_PROGRAMS =
CLEANFILES =

check_LIBRARIES += src/testing/libtesting.a

if !SYSTEM_TEST_LIBS
src_testing_libtesting_a_SOURCES = \
	src/breakpad_googletest_includes.h \
	src/testing/googletest/src/gtest-all.cc \
	src/testing/googletest/src/gtest_main.cc \
	src/testing/googlemock/src/gmock-all.cc
src_testing_libtesting_a_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
endif

if !DISABLE_PROCESSOR
lib_LIBRARIES += src/libbreakpad.a
pkgconfig_DATA += breakpad.pc
noinst_LIBRARIES += src/third_party/libdisasm/libdisasm.a
endif

if LINUX_HOST
lib_LIBRARIES += src/client/linux/libbreakpad_client.a
pkgconfig_DATA += breakpad-client.pc

src_client_linux_libbreakpad_client_a_SOURCES = \
	src/client/linux/crash_generation/crash_generation_client.cc \
	src/client/linux/crash_generation/crash_generation_server.cc \
	src/client/linux/dump_writer_common/thread_info.cc \
	src/client/linux/dump_writer_common/ucontext_reader.cc \
	src/client/linux/handler/exception_handler.cc \
	src/client/linux/handler/exception_handler.h \
	src/client/linux/handler/minidump_descriptor.cc \
	src/client/linux/handler/minidump_descriptor.h \
	src/client/linux/log/log.cc \
	src/client/linux/log/log.h \
	src/client/linux/microdump_writer/microdump_writer.cc \
	src/client/linux/microdump_writer/microdump_writer.h \
	src/client/linux/minidump_writer/linux_core_dumper.cc \
	src/client/linux/minidump_writer/linux_dumper.cc \
	src/client/linux/minidump_writer/linux_ptrace_dumper.cc \
	src/client/linux/minidump_writer/minidump_writer.cc \
	src/client/minidump_file_writer-inl.h \
	src/client/minidump_file_writer.cc \
	src/client/minidump_file_writer.h \
	src/common/convert_UTF.cc \
	src/common/convert_UTF.h \
	src/common/md5.cc \
	src/common/md5.h \
	src/common/string_conversion.cc \
	src/common/string_conversion.h \
	src/common/linux/elf_core_dump.cc \
	src/common/linux/elfutils.cc \
	src/common/linux/elfutils.h \
	src/common/linux/file_id.cc \
	src/common/linux/file_id.h \
	src/common/linux/guid_creator.cc \
	src/common/linux/guid_creator.h \
	src/common/linux/linux_libc_support.cc \
	src/common/linux/memory_mapped_file.cc \
	src/common/linux/safe_readlink.cc
if !HAVE_GETCONTEXT
src_client_linux_libbreakpad_client_a_SOURCES += \
	src/common/linux/breakpad_getcontext.S
endif
endif LINUX_HOST

if !DISABLE_PROCESSOR
src_libbreakpad_a_SOURCES = \
	src/google_breakpad/common/breakpad_types.h \
	src/google_breakpad/common/minidump_format.h \
	src/google_breakpad/common/minidump_size.h \
	src/google_breakpad/processor/basic_source_line_resolver.h \
	src/google_breakpad/processor/call_stack.h \
	src/google_breakpad/processor/code_module.h \
	src/google_breakpad/processor/code_modules.h \
	src/google_breakpad/processor/dump_context.h \
	src/google_breakpad/processor/dump_object.h \
	src/google_breakpad/processor/exploitability.h \
	src/google_breakpad/processor/fast_source_line_resolver.h \
	src/google_breakpad/processor/memory_region.h \
	src/google_breakpad/processor/microdump.h \
	src/google_breakpad/processor/microdump_processor.h \
	src/google_breakpad/processor/minidump.h \
	src/google_breakpad/processor/minidump_processor.h \
	src/google_breakpad/processor/process_result.h \
	src/google_breakpad/processor/process_state.h \
	src/google_breakpad/processor/proc_maps_linux.h \
	src/google_breakpad/processor/source_line_resolver_base.h \
	src/google_breakpad/processor/source_line_resolver_interface.h \
	src/google_breakpad/processor/stack_frame.h \
	src/google_breakpad/processor/stack_frame_cpu.h \
	src/google_breakpad/processor/stack_frame_symbolizer.h \
	src/google_breakpad/processor/stackwalker.h \
	src/google_breakpad/processor/symbol_supplier.h \
	src/google_breakpad/processor/system_info.h \
	src/processor/address_map-inl.h \
	src/processor/address_map.h \
	src/processor/basic_code_module.h \
	src/processor/basic_code_modules.cc \
	src/processor/basic_code_modules.h \
	src/processor/basic_source_line_resolver_types.h \
	src/processor/basic_source_line_resolver.cc \
	src/processor/call_stack.cc \
	src/processor/cfi_frame_info.cc \
	src/processor/cfi_frame_info.h \
	src/processor/contained_range_map-inl.h \
	src/processor/contained_range_map.h \
	src/processor/convert_old_arm64_context.cc \
	src/processor/convert_old_arm64_context.h \
	src/processor/disassembler_x86.h \
	src/processor/disassembler_x86.cc \
	src/processor/dump_context.cc \
	src/processor/dump_object.cc \
	src/processor/exploitability.cc \
	src/processor/exploitability_linux.h \
	src/processor/exploitability_linux.cc \
	src/processor/exploitability_win.h \
	src/processor/exploitability_win.cc \
	src/processor/fast_source_line_resolver_types.h \
	src/processor/fast_source_line_resolver.cc \
	src/processor/linked_ptr.h \
	src/processor/logging.h \
	src/processor/logging.cc \
	src/processor/map_serializers-inl.h \
	src/processor/map_serializers.h \
	src/processor/microdump.cc \
	src/processor/microdump_processor.cc \
	src/processor/minidump.cc \
	src/processor/minidump_processor.cc \
	src/processor/module_comparer.cc \
	src/processor/module_comparer.h \
	src/processor/module_factory.h \
	src/processor/module_serializer.cc \
	src/processor/module_serializer.h \
	src/processor/pathname_stripper.cc \
	src/processor/pathname_stripper.h \
	src/processor/postfix_evaluator-inl.h \
	src/processor/postfix_evaluator.h \
	src/processor/process_state.cc \
	src/processor/proc_maps_linux.cc \
	src/processor/range_map-inl.h \
	src/processor/range_map.h \
	src/processor/simple_serializer-inl.h \
	src/processor/simple_serializer.h \
	src/processor/simple_symbol_supplier.cc \
	src/processor/simple_symbol_supplier.h \
	src/processor/windows_frame_info.h \
	src/processor/source_line_resolver_base_types.h \
	src/processor/source_line_resolver_base.cc \
	src/processor/stack_frame_cpu.cc \
	src/processor/stack_frame_symbolizer.cc \
	src/processor/stackwalker.cc \
	src/processor/stackwalker_amd64.cc \
	src/processor/stackwalker_amd64.h \
	src/processor/stackwalker_arm.cc \
	src/processor/stackwalker_arm.h \
	src/processor/stackwalker_arm64.cc \
	src/processor/stackwalker_arm64.h \
	src/processor/stackwalker_address_list.cc \
	src/processor/stackwalker_address_list.h \
	src/processor/stackwalker_mips.cc \
	src/processor/stackwalker_mips.h \
	src/processor/stackwalker_ppc.cc \
	src/processor/stackwalker_ppc.h \
	src/processor/stackwalker_ppc64.cc \
	src/processor/stackwalker_ppc64.h \
	src/processor/stackwalker_sparc.cc \
	src/processor/stackwalker_sparc.h \
	src/processor/stackwalker_x86.cc \
	src/processor/stackwalker_x86.h \
	src/processor/static_address_map-inl.h \
	src/processor/static_address_map.h \
	src/processor/static_contained_range_map-inl.h \
	src/processor/static_contained_range_map.h \
	src/processor/static_map_iterator-inl.h \
	src/processor/static_map_iterator.h \
	src/processor/static_map-inl.h \
	src/processor/static_map.h \
	src/processor/static_range_map-inl.h \
	src/processor/static_range_map.h \
	src/processor/symbolic_constants_win.cc \
	src/processor/symbolic_constants_win.h \
	src/processor/tokenize.cc \
	src/processor/tokenize.h

src_third_party_libdisasm_libdisasm_a_SOURCES = \
	src/third_party/libdisasm/ia32_implicit.c \
	src/third_party/libdisasm/ia32_implicit.h \
	src/third_party/libdisasm/ia32_insn.c \
	src/third_party/libdisasm/ia32_insn.h \
	src/third_party/libdisasm/ia32_invariant.c \
	src/third_party/libdisasm/ia32_invariant.h \
	src/third_party/libdisasm/ia32_modrm.c \
	src/third_party/libdisasm/ia32_modrm.h \
	src/third_party/libdisasm/ia32_opcode_tables.c \
	src/third_party/libdisasm/ia32_opcode_tables.h \
	src/third_party/libdisasm/ia32_operand.c \
	src/third_party/libdisasm/ia32_operand.h \
	src/third_party/libdisasm/ia32_reg.c \
	src/third_party/libdisasm/ia32_reg.h \
	src/third_party/libdisasm/ia32_settings.c \
	src/third_party/libdisasm/ia32_settings.h \
	src/third_party/libdisasm/libdis.h \
	src/third_party/libdisasm/qword.h \
	src/third_party/libdisasm/x86_disasm.c \
	src/third_party/libdisasm/x86_format.c \
	src/third_party/libdisasm/x86_imm.c \
	src/third_party/libdisasm/x86_imm.h \
	src/third_party/libdisasm/x86_insn.c \
	src/third_party/libdisasm/x86_misc.c \
	src/third_party/libdisasm/x86_operand_list.c \
	src/third_party/libdisasm/x86_operand_list.h

## Programs
bin_PROGRAMS += \
	src/processor/microdump_stackwalk \
	src/processor/minidump_dump \
	src/processor/minidump_stackwalk
endif !DISABLE_PROCESSOR

if LINUX_HOST
EXTRA_PROGRAMS += \
	src/client/linux/linux_dumper_unittest_helper
CLEANFILES += \
	src/client/linux/linux_dumper_unittest_helper

if !DISABLE_TOOLS
bin_PROGRAMS += \
	src/tools/linux/core2md/core2md \
	src/tools/linux/dump_syms/dump_syms \
	src/tools/linux/md2core/minidump-2-core \
	src/tools/linux/symupload/minidump_upload \
	src/tools/linux/symupload/sym_upload
if X86_HOST
bin_PROGRAMS += \
	src/tools/mac/dump_syms/dump_syms_mac
endif
endif
endif LINUX_HOST


## Tests
if !DISABLE_PROCESSOR
check_PROGRAMS += \
	src/common/test_assembler_unittest \
	src/processor/address_map_unittest \
	src/processor/basic_source_line_resolver_unittest \
	src/processor/cfi_frame_info_unittest \
	src/processor/contained_range_map_unittest \
	src/processor/disassembler_x86_unittest \
	src/processor/exploitability_unittest \
	src/processor/fast_source_line_resolver_unittest \
	src/processor/map_serializers_unittest \
	src/processor/microdump_processor_unittest \
	src/processor/minidump_processor_unittest \
	src/processor/minidump_unittest \
	src/processor/static_address_map_unittest \
	src/processor/static_contained_range_map_unittest \
	src/processor/static_map_unittest \
	src/processor/static_range_map_unittest \
	src/processor/pathname_stripper_unittest \
	src/processor/postfix_evaluator_unittest \
	src/processor/proc_maps_linux_unittest \
	src/processor/range_map_truncate_lower_unittest \
	src/processor/range_map_truncate_upper_unittest \
	src/processor/range_map_unittest \
	src/processor/stackwalker_amd64_unittest \
	src/processor/stackwalker_arm_unittest \
	src/processor/stackwalker_arm64_unittest \
	src/processor/stackwalker_address_list_unittest \
	src/processor/stackwalker_mips_unittest \
	src/processor/stackwalker_mips64_unittest \
	src/processor/stackwalker_x86_unittest \
	src/processor/synth_minidump_unittest
endif

if LINUX_HOST
EXTRA_PROGRAMS += \
	src/client/linux/linux_client_unittest_shlib
CLEANFILES += \
	src/client/linux/linux_client_unittest_shlib

check_PROGRAMS += \
	src/client/linux/linux_client_unittest \
	src/common/linux/google_crashdump_uploader_test

if !DISABLE_TOOLS
check_PROGRAMS += \
	src/common/dumper_unittest \
	src/tools/linux/md2core/minidump_2_core_unittest
if X86_HOST
check_PROGRAMS += \
	src/common/mac/macho_reader_unittest
endif
endif
endif LINUX_HOST

if !DISABLE_PROCESSOR
if SELFTEST
check_PROGRAMS += \
	src/processor/stackwalker_selftest
endif SELFTEST
endif !DISABLE_PROCESSOR

if !DISABLE_PROCESSOR
check_SCRIPTS = \
	src/processor/microdump_stackwalk_test \
	src/processor/microdump_stackwalk_machine_readable_test \
	src/processor/minidump_dump_test \
	src/processor/minidump_stackwalk_test \
	src/processor/minidump_stackwalk_machine_readable_test
endif

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)

if ANDROID_HOST
# Since Autotools 1.2, tests are run through a special "test driver" script.
# Unfortunately, it's not possible anymore to specify an alternative shell to
# run them on connected devices, so use a slightly modified version of the
# driver for Android.
LOG_DRIVER = $(top_srcdir)/android/test-driver
else
# The default Autotools test driver script.
if TESTS_AS_ROOT
LOG_DRIVER = $(top_srcdir)/autotools/root-test-driver $(top_srcdir)/autotools/test-driver
else
LOG_DRIVER = $(top_srcdir)/autotools/test-driver
endif !TESTS_AS_ROOT
endif !ANDROID_HOST

if LINUX_HOST
src_client_linux_linux_dumper_unittest_helper_SOURCES = \
	src/client/linux/minidump_writer/linux_dumper_unittest_helper.cc
src_client_linux_linux_dumper_unittest_helper_LDFLAGS=$(PTHREAD_CFLAGS)
src_client_linux_linux_dumper_unittest_helper_CC=$(PTHREAD_CC)
if ANDROID_HOST
# On Android PTHREAD_CFLAGS is empty, and adding src/common/android/include
# to the include path is necessary to build this program.
src_client_linux_linux_dumper_unittest_helper_CXXFLAGS=$(AM_CXXFLAGS)
else
src_client_linux_linux_dumper_unittest_helper_CXXFLAGS=$(PTHREAD_CFLAGS)
endif

src_client_linux_linux_client_unittest_shlib_SOURCES = \
	$(src_testing_libtesting_a_SOURCES) \
	src/client/linux/handler/exception_handler_unittest.cc \
	src/client/linux/microdump_writer/microdump_writer_unittest.cc \
	src/client/linux/minidump_writer/directory_reader_unittest.cc \
	src/client/linux/minidump_writer/cpu_set_unittest.cc \
	src/client/linux/minidump_writer/line_reader_unittest.cc \
	src/client/linux/minidump_writer/linux_core_dumper.cc \
	src/client/linux/minidump_writer/linux_core_dumper_unittest.cc \
	src/client/linux/minidump_writer/linux_ptrace_dumper_unittest.cc \
	src/client/linux/minidump_writer/minidump_writer_unittest.cc \
	src/client/linux/minidump_writer/minidump_writer_unittest_utils.cc \
	src/client/linux/minidump_writer/proc_cpuinfo_reader_unittest.cc \
	src/common/linux/elf_core_dump.cc \
	src/common/linux/linux_libc_support_unittest.cc \
	src/common/linux/tests/auto_testfile.h \
	src/common/linux/tests/crash_generator.cc \
	src/common/memory_allocator_unittest.cc \
	src/common/tests/auto_tempdir.h \
	src/common/tests/file_utils.cc \
	src/common/tests/file_utils.h \
	src/processor/basic_code_modules.cc \
	src/processor/convert_old_arm64_context.cc \
	src/processor/dump_context.cc \
	src/processor/dump_object.cc \
	src/processor/logging.cc \
	src/processor/minidump.cc \
	src/processor/pathname_stripper.cc \
	src/processor/proc_maps_linux.cc
if !HAVE_GETCONTEXT
src_client_linux_linux_client_unittest_shlib_SOURCES += \
	src/common/linux/breakpad_getcontext.S
endif

src_client_linux_linux_client_unittest_shlib_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_client_linux_linux_client_unittest_shlib_LDFLAGS = \
	-shared \
	-Wl,-h,linux_client_unittest_shlib
src_client_linux_linux_client_unittest_shlib_LDADD = \
	src/client/linux/crash_generation/crash_generation_client.o \
	src/client/linux/dump_writer_common/thread_info.o \
	src/client/linux/dump_writer_common/ucontext_reader.o \
	src/client/linux/handler/exception_handler.o \
	src/client/linux/handler/minidump_descriptor.o \
	src/client/linux/log/log.o \
	src/client/linux/microdump_writer/microdump_writer.o \
	src/client/linux/minidump_writer/linux_dumper.o \
	src/client/linux/minidump_writer/linux_ptrace_dumper.o \
	src/client/linux/minidump_writer/minidump_writer.o \
	src/client/minidump_file_writer.o \
	src/common/convert_UTF.o \
	src/common/md5.o \
	src/common/linux/elfutils.o \
	src/common/linux/file_id.o \
	src/common/linux/guid_creator.o \
	src/common/linux/linux_libc_support.o \
	src/common/linux/memory_mapped_file.o \
	src/common/linux/safe_readlink.o \
	src/common/string_conversion.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
if !HAVE_GETCONTEXT
src_client_linux_linux_client_unittest_shlib_SOURCES += \
	src/common/linux/breakpad_getcontext_unittest.cc
endif
if ANDROID_HOST
src_client_linux_linux_client_unittest_shlib_LDFLAGS += \
	-llog -lm
endif

src_client_linux_linux_client_unittest_shlib_DEPENDENCIES = \
	src/client/linux/linux_dumper_unittest_helper \
	src/client/linux/libbreakpad_client.a \
	$(TEST_DEPS) \
	src/libbreakpad.a

src_client_linux_linux_client_unittest_SOURCES =
# The extra-long build id is for a test in minidump_writer_unittest.cc.
src_client_linux_linux_client_unittest_LDFLAGS = \
	-Wl,-rpath,'$$ORIGIN' \
	-Wl,--build-id=0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f
if ANDROID_HOST
src_client_linux_linux_client_unittest_LDFLAGS += \
        -llog
endif

src_client_linux_linux_client_unittest_LDADD = \
	src/client/linux/linux_client_unittest_shlib \
	$(TEST_LIBS)

src_client_linux_linux_client_unittest_DEPENDENCIES = \
	src/client/linux/linux_client_unittest_shlib

if !DISABLE_TOOLS
src_tools_linux_core2md_core2md_SOURCES = \
	src/tools/linux/core2md/core2md.cc

src_tools_linux_core2md_core2md_LDADD = \
	src/client/linux/libbreakpad_client.a

src_tools_linux_dump_syms_dump_syms_SOURCES = \
	src/common/dwarf_cfi_to_module.cc \
	src/common/dwarf_cu_to_module.cc \
	src/common/dwarf_line_to_module.cc \
	src/common/dwarf_range_list_handler.cc \
	src/common/language.cc \
	src/common/module.cc \
	src/common/path_helper.cc \
	src/common/stabs_reader.cc \
	src/common/stabs_to_module.cc \
	src/common/dwarf/bytereader.cc \
	src/common/dwarf/dwarf2diehandler.cc \
	src/common/dwarf/dwarf2reader.cc \
	src/common/dwarf/elf_reader.cc \
	src/common/linux/crc32.cc \
	src/common/linux/dump_symbols.cc \
	src/common/linux/dump_symbols.h \
	src/common/linux/elf_symbols_to_module.cc \
	src/common/linux/elf_symbols_to_module.h \
	src/common/linux/elfutils.cc \
	src/common/linux/file_id.cc \
	src/common/linux/linux_libc_support.cc \
	src/common/linux/memory_mapped_file.cc \
	src/common/linux/safe_readlink.cc \
	src/tools/linux/dump_syms/dump_syms.cc
src_tools_linux_dump_syms_dump_syms_CXXFLAGS = \
	$(RUST_DEMANGLE_CFLAGS)
src_tools_linux_dump_syms_dump_syms_LDADD = \
	$(RUST_DEMANGLE_LIBS)

src_tools_linux_md2core_minidump_2_core_SOURCES = \
	src/common/linux/memory_mapped_file.cc \
	src/common/path_helper.cc \
	src/tools/linux/md2core/minidump-2-core.cc \
	src/tools/linux/md2core/minidump_memory_range.h

src_tools_linux_symupload_minidump_upload_SOURCES = \
	src/common/linux/http_upload.cc \
	src/tools/linux/symupload/minidump_upload.cc
src_tools_linux_symupload_minidump_upload_LDADD = -ldl

src_tools_linux_symupload_sym_upload_SOURCES = \
	src/common/linux/http_upload.cc \
	src/common/linux/http_upload.h \
	src/common/linux/libcurl_wrapper.cc \
	src/common/linux/libcurl_wrapper.h \
	src/common/linux/symbol_collector_client.cc \
	src/common/linux/symbol_collector_client.h \
	src/common/linux/symbol_upload.cc \
	src/common/linux/symbol_upload.h \
	src/tools/linux/symupload/sym_upload.cc
src_tools_linux_symupload_sym_upload_LDADD = -ldl

src_tools_mac_dump_syms_dump_syms_mac_SOURCES = \
	src/common/dwarf_cfi_to_module.cc \
	src/common/dwarf_cu_to_module.cc \
	src/common/dwarf_line_to_module.cc \
	src/common/dwarf_range_list_handler.cc \
	src/common/language.cc \
	src/common/md5.cc \
	src/common/module.cc \
	src/common/path_helper.cc \
	src/common/stabs_reader.cc \
	src/common/stabs_to_module.cc \
	src/common/dwarf/bytereader.cc \
	src/common/dwarf/dwarf2diehandler.cc \
	src/common/dwarf/dwarf2reader.cc \
	src/common/dwarf/elf_reader.cc \
	src/common/mac/arch_utilities.cc \
	src/common/mac/dump_syms.cc \
	src/common/mac/dump_syms.h \
	src/common/mac/file_id.cc \
	src/common/mac/file_id.h \
	src/common/mac/macho_id.cc \
	src/common/mac/macho_id.h \
	src/common/mac/macho_reader.cc \
	src/common/mac/macho_reader.h \
	src/common/mac/macho_utilities.cc \
	src/common/mac/macho_utilities.h \
	src/common/mac/macho_walker.cc \
	src/common/mac/macho_walker.h \
	src/tools/mac/dump_syms/dump_syms_tool.cc
src_tools_mac_dump_syms_dump_syms_mac_CXXFLAGS= \
	-I$(top_srcdir)/src/third_party/mac_headers \
	$(RUST_DEMANGLE_CFLAGS) \
	-DHAVE_MACH_O_NLIST_H
src_tools_mac_dump_syms_dump_syms_mac_LDADD= \
	$(RUST_DEMANGLE_LIBS)

src_common_dumper_unittest_SOURCES = \
	src/common/byte_cursor_unittest.cc \
	src/common/convert_UTF.cc \
	src/common/dwarf_cfi_to_module.cc \
	src/common/dwarf_cfi_to_module_unittest.cc \
	src/common/dwarf_cu_to_module.cc \
	src/common/dwarf_cu_to_module_unittest.cc \
	src/common/dwarf_line_to_module.cc \
	src/common/dwarf_line_to_module_unittest.cc \
	src/common/dwarf_range_list_handler.cc \
	src/common/language.cc \
	src/common/memory_range_unittest.cc \
	src/common/module.cc \
	src/common/module_unittest.cc \
	src/common/path_helper.cc \
	src/common/stabs_reader.cc \
	src/common/stabs_reader_unittest.cc \
	src/common/stabs_to_module.cc \
	src/common/stabs_to_module_unittest.cc \
	src/common/string_conversion.cc \
	src/common/string_conversion_unittest.cc \
	src/common/test_assembler.cc \
	src/common/dwarf/bytereader.cc \
	src/common/dwarf/bytereader.h \
	src/common/dwarf/bytereader-inl.h \
	src/common/dwarf/bytereader_unittest.cc \
	src/common/dwarf/cfi_assembler.cc \
	src/common/dwarf/cfi_assembler.h \
	src/common/dwarf/dwarf2diehandler.cc \
	src/common/dwarf/dwarf2diehandler_unittest.cc \
	src/common/dwarf/dwarf2reader.cc \
	src/common/dwarf/dwarf2reader.h \
	src/common/dwarf/elf_reader.cc \
	src/common/dwarf/elf_reader.h \
	src/common/dwarf/dwarf2reader_cfi_unittest.cc \
	src/common/dwarf/dwarf2reader_die_unittest.cc \
	src/common/dwarf/dwarf2reader_test_common.h \
	src/common/linux/crc32.cc \
	src/common/linux/dump_symbols.cc \
	src/common/linux/dump_symbols_unittest.cc \
	src/common/linux/elf_core_dump.cc \
	src/common/linux/elf_core_dump_unittest.cc \
	src/common/linux/elf_symbols_to_module.cc \
	src/common/linux/elf_symbols_to_module_unittest.cc \
	src/common/linux/elfutils.cc \
	src/common/linux/file_id.cc \
	src/common/linux/file_id_unittest.cc \
	src/common/linux/linux_libc_support.cc \
	src/common/linux/memory_mapped_file.cc \
	src/common/linux/memory_mapped_file_unittest.cc \
	src/common/linux/safe_readlink.cc \
	src/common/linux/safe_readlink_unittest.cc \
	src/common/linux/synth_elf.cc \
	src/common/linux/synth_elf_unittest.cc \
	src/common/linux/tests/crash_generator.cc \
	src/common/linux/tests/crash_generator.h \
	src/common/testdata/func-line-pairing.h \
	src/common/tests/file_utils.cc
src_common_dumper_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS) \
	$(RUST_DEMANGLE_CFLAGS) \
	$(PTHREAD_CFLAGS)
src_common_dumper_unittest_LDADD = \
	$(TEST_LIBS) \
	$(RUST_DEMANGLE_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_common_mac_macho_reader_unittest_SOURCES = \
	src/common/dwarf_cfi_to_module.cc \
	src/common/dwarf_cu_to_module.cc \
	src/common/dwarf_line_to_module.cc \
	src/common/language.cc \
	src/common/md5.cc \
	src/common/module.cc \
	src/common/path_helper.cc \
	src/common/stabs_reader.cc \
	src/common/stabs_to_module.cc \
	src/common/test_assembler.cc \
	src/common/dwarf/bytereader.cc \
	src/common/dwarf/cfi_assembler.cc \
	src/common/dwarf/dwarf2diehandler.cc \
	src/common/dwarf/dwarf2reader.cc \
	src/common/dwarf/elf_reader.cc \
	src/common/mac/arch_utilities.cc \
	src/common/mac/file_id.cc \
	src/common/mac/macho_id.cc \
	src/common/mac/macho_reader.cc \
	src/common/mac/macho_reader_unittest.cc \
	src/common/mac/macho_utilities.cc \
	src/common/mac/macho_walker.cc \
	src/common/tests/file_utils.cc
src_common_mac_macho_reader_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS) \
	-I$(top_srcdir)/src/third_party/mac_headers \
	-DHAVE_MACH_O_NLIST_H \
	$(PTHREAD_CFLAGS)
src_common_mac_macho_reader_unittest_LDADD = \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
endif

src_common_linux_google_crashdump_uploader_test_SOURCES = \
	src/common/linux/google_crashdump_uploader.cc \
	src/common/linux/google_crashdump_uploader_test.cc \
	src/common/linux/libcurl_wrapper.cc
src_common_linux_google_crashdump_uploader_test_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_common_linux_google_crashdump_uploader_test_LDADD = \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS) \
	-ldl

src_tools_linux_md2core_minidump_2_core_unittest_SOURCES = \
	src/tools/linux/md2core/minidump_memory_range_unittest.cc
src_tools_linux_md2core_minidump_2_core_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_tools_linux_md2core_minidump_2_core_unittest_LDADD = \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

endif LINUX_HOST

if !DISABLE_PROCESSOR
src_processor_address_map_unittest_SOURCES = \
	src/processor/address_map_unittest.cc
src_processor_address_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o

src_processor_basic_source_line_resolver_unittest_SOURCES = \
	src/processor/basic_source_line_resolver_unittest.cc
src_processor_basic_source_line_resolver_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_basic_source_line_resolver_unittest_LDADD = \
	src/processor/basic_source_line_resolver.o \
	src/processor/cfi_frame_info.o \
	src/processor/pathname_stripper.o \
	src/processor/logging.o \
	src/processor/source_line_resolver_base.o \
	src/processor/tokenize.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_cfi_frame_info_unittest_SOURCES = \
	src/processor/cfi_frame_info_unittest.cc
src_processor_cfi_frame_info_unittest_LDADD = \
	src/processor/cfi_frame_info.o \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_cfi_frame_info_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_contained_range_map_unittest_SOURCES = \
	src/processor/contained_range_map_unittest.cc
src_processor_contained_range_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o

src_processor_exploitability_unittest_SOURCES = \
	src/processor/exploitability_unittest.cc
src_processor_exploitability_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_exploitability_unittest_LDADD = \
	src/processor/convert_old_arm64_context.o \
	src/processor/minidump_processor.o \
	src/processor/process_state.o \
	src/processor/disassembler_x86.o \
	src/processor/exploitability.o \
	src/processor/exploitability_linux.o \
	src/processor/exploitability_win.o \
	src/processor/basic_code_modules.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/call_stack.o \
	src/processor/cfi_frame_info.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/logging.o \
	src/processor/minidump.o \
	src/processor/pathname_stripper.o \
	src/processor/proc_maps_linux.o \
	src/processor/simple_symbol_supplier.o \
	src/processor/source_line_resolver_base.o \
	src/processor/stack_frame_cpu.o \
	src/processor/stack_frame_symbolizer.o \
	src/processor/stackwalker.o \
	src/processor/stackwalker_address_list.o \
	src/processor/stackwalker_amd64.o \
	src/processor/stackwalker_arm.o \
	src/processor/stackwalker_arm64.o \
	src/processor/stackwalker_mips.o \
	src/processor/stackwalker_ppc.o \
	src/processor/stackwalker_ppc64.o \
	src/processor/stackwalker_sparc.o \
	src/processor/stackwalker_x86.o \
	src/processor/symbolic_constants_win.o \
	src/processor/tokenize.o \
	src/third_party/libdisasm/libdisasm.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_disassembler_x86_unittest_SOURCES = \
	src/processor/disassembler_x86_unittest.cc
src_processor_disassembler_x86_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_disassembler_x86_unittest_LDADD = \
	src/processor/disassembler_x86.o \
	src/third_party/libdisasm/libdisasm.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_fast_source_line_resolver_unittest_SOURCES = \
	src/processor/fast_source_line_resolver_unittest.cc
src_processor_fast_source_line_resolver_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_fast_source_line_resolver_unittest_LDADD = \
	src/processor/fast_source_line_resolver.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/cfi_frame_info.o \
	src/processor/module_comparer.o \
	src/processor/module_serializer.o \
	src/processor/pathname_stripper.o \
	src/processor/logging.o \
	src/processor/source_line_resolver_base.o \
	src/processor/tokenize.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_map_serializers_unittest_SOURCES = \
	src/processor/map_serializers_unittest.cc
src_processor_map_serializers_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_map_serializers_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_microdump_processor_unittest_SOURCES = \
	src/processor/microdump_processor_unittest.cc
src_processor_microdump_processor_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_microdump_processor_unittest_LDADD = \
	src/processor/basic_code_modules.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/call_stack.o \
        src/processor/convert_old_arm64_context.o \
	src/processor/cfi_frame_info.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/logging.o \
	src/processor/microdump.o \
	src/processor/microdump_processor.o \
	src/processor/pathname_stripper.o \
	src/processor/process_state.o \
	src/processor/simple_symbol_supplier.o \
	src/processor/source_line_resolver_base.o \
	src/processor/stack_frame_symbolizer.o \
	src/processor/stackwalker.o \
	src/processor/stackwalker_address_list.o \
	src/processor/stackwalker_amd64.o \
	src/processor/stackwalker_arm.o \
	src/processor/stackwalker_arm64.o \
	src/processor/stackwalker_mips.o \
	src/processor/stackwalker_ppc.o \
	src/processor/stackwalker_ppc64.o \
	src/processor/stackwalker_sparc.o \
	src/processor/stackwalker_x86.o \
	src/processor/tokenize.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_minidump_processor_unittest_SOURCES = \
	src/processor/minidump_processor_unittest.cc
src_processor_minidump_processor_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_minidump_processor_unittest_LDADD = \
	src/processor/basic_code_modules.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/call_stack.o \
	src/processor/cfi_frame_info.o \
	src/processor/convert_old_arm64_context.o \
	src/processor/disassembler_x86.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/exploitability.o \
	src/processor/exploitability_linux.o \
	src/processor/exploitability_win.o \
	src/processor/logging.o \
	src/processor/minidump_processor.o \
	src/processor/minidump.o \
	src/processor/pathname_stripper.o \
	src/processor/process_state.o \
	src/processor/proc_maps_linux.o \
	src/processor/source_line_resolver_base.o \
	src/processor/stack_frame_cpu.o \
	src/processor/stack_frame_symbolizer.o \
	src/processor/stackwalker.o \
	src/processor/stackwalker_address_list.o \
	src/processor/stackwalker_amd64.o \
	src/processor/stackwalker_arm.o \
	src/processor/stackwalker_arm64.o \
	src/processor/stackwalker_mips.o \
	src/processor/stackwalker_ppc.o \
	src/processor/stackwalker_ppc64.o \
	src/processor/stackwalker_sparc.o \
	src/processor/stackwalker_x86.o \
	src/processor/symbolic_constants_win.o \
	src/processor/tokenize.o \
	src/third_party/libdisasm/libdisasm.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_minidump_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/minidump_unittest.cc \
	src/processor/synth_minidump.cc
src_processor_minidump_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_minidump_unittest_LDADD = \
	src/processor/basic_code_modules.o \
	src/processor/convert_old_arm64_context.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/logging.o \
	src/processor/minidump.o \
	src/processor/pathname_stripper.o \
	src/processor/proc_maps_linux.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_proc_maps_linux_unittest_SOURCES = \
	src/processor/proc_maps_linux.cc \
	src/processor/proc_maps_linux_unittest.cc
src_processor_proc_maps_linux_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_proc_maps_linux_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	src/third_party/libdisasm/libdisasm.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_static_address_map_unittest_SOURCES = \
	src/processor/static_address_map_unittest.cc
src_processor_static_address_map_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_static_address_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_static_contained_range_map_unittest_SOURCES = \
	src/processor/static_contained_range_map_unittest.cc
src_processor_static_contained_range_map_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_static_contained_range_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_static_map_unittest_SOURCES = \
	src/processor/static_map_unittest.cc
src_processor_static_map_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_static_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_static_range_map_unittest_SOURCES = \
	src/processor/static_range_map_unittest.cc
src_processor_static_range_map_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_static_range_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_pathname_stripper_unittest_SOURCES = \
	src/processor/pathname_stripper_unittest.cc
src_processor_pathname_stripper_unittest_LDADD = \
	src/processor/pathname_stripper.o \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_postfix_evaluator_unittest_SOURCES = \
	src/processor/postfix_evaluator_unittest.cc
src_processor_postfix_evaluator_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_range_map_truncate_lower_unittest_SOURCES = \
	src/processor/range_map_truncate_lower_unittest.cc
src_processor_range_map_truncate_lower_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_range_map_truncate_lower_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_range_map_truncate_upper_unittest_SOURCES = \
	src/processor/range_map_truncate_upper_unittest.cc
src_processor_range_map_truncate_upper_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_range_map_truncate_upper_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_range_map_unittest_SOURCES = \
	src/processor/range_map_unittest.cc
src_processor_range_map_unittest_LDADD = \
	src/processor/logging.o \
	src/processor/pathname_stripper.o \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_stackwalker_selftest_SOURCES = \
	src/processor/stackwalker_selftest.cc
src_processor_stackwalker_selftest_LDADD = \
	src/processor/basic_code_modules.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/call_stack.o \
	src/processor/disassembler_x86.o \
	src/processor/exploitability.o \
	src/processor/exploitability_linux.o \
	src/processor/exploitability_win.o \
	src/processor/logging.o \
	src/processor/minidump.o \
	src/processor/pathname_stripper.o \
	src/processor/proc_maps_linux.o \
	src/processor/source_line_resolver_base.o \
	src/processor/stack_frame_cpu.o \
	src/processor/stack_frame_symbolizer.o \
	src/processor/stackwalker.o \
	src/processor/stackwalker_address_list.o \
	src/processor/stackwalker_amd64.o \
	src/processor/stackwalker_arm.o \
	src/processor/stackwalker_arm64.o \
	src/processor/stackwalker_mips.o \
	src/processor/stackwalker_ppc.o \
	src/processor/stackwalker_ppc64.o \
	src/processor/stackwalker_sparc.o \
	src/processor/stackwalker_x86.o \
	src/processor/tokenize.o \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_processor_stackwalker_amd64_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_amd64_unittest.cc
src_processor_stackwalker_amd64_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_amd64_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_stackwalker_arm_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_arm_unittest.cc
src_processor_stackwalker_arm_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_arm_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_stackwalker_arm64_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_arm64_unittest.cc
src_processor_stackwalker_arm64_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_arm64_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_stackwalker_address_list_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_address_list_unittest.cc
src_processor_stackwalker_address_list_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_address_list_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_stackwalker_mips_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_mips_unittest.cc
src_processor_stackwalker_mips_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_mips_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_stackwalker_mips64_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_mips64_unittest.cc
src_processor_stackwalker_mips64_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_mips64_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_stackwalker_x86_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_x86_unittest.cc
src_processor_stackwalker_x86_unittest_LDADD = \
	src/libbreakpad.a \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
src_processor_stackwalker_x86_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)

src_processor_synth_minidump_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/common/test_assembler.h \
	src/processor/synth_minidump_unittest.cc \
	src/processor/synth_minidump.cc \
	src/processor/synth_minidump.h
src_processor_synth_minidump_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_processor_synth_minidump_unittest_LDADD = \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

src_common_test_assembler_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/common/test_assembler.h \
	src/common/test_assembler_unittest.cc
src_common_test_assembler_unittest_CPPFLAGS = \
	$(AM_CPPFLAGS) $(TEST_CFLAGS)
src_common_test_assembler_unittest_LDADD = \
	$(TEST_LIBS) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

## Non-installables
noinst_PROGRAMS =
noinst_SCRIPTS = $(check_SCRIPTS)

src_processor_minidump_dump_SOURCES = \
	src/processor/minidump_dump.cc
src_processor_minidump_dump_LDADD = \
	src/processor/basic_code_modules.o \
	src/processor/convert_old_arm64_context.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/logging.o \
	src/processor/minidump.o \
	src/processor/pathname_stripper.o \
	src/processor/proc_maps_linux.o

src_processor_microdump_stackwalk_SOURCES = \
	src/processor/microdump_stackwalk.cc
src_processor_microdump_stackwalk_LDADD = \
	src/common/path_helper.o \
	src/processor/basic_code_modules.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/call_stack.o \
	src/processor/convert_old_arm64_context.o \
	src/processor/cfi_frame_info.o \
	src/processor/disassembler_x86.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/logging.o \
	src/processor/microdump.o \
	src/processor/microdump_processor.o \
	src/processor/pathname_stripper.o \
	src/processor/process_state.o \
	src/processor/simple_symbol_supplier.o \
	src/processor/source_line_resolver_base.o \
	src/processor/stack_frame_cpu.o \
	src/processor/stack_frame_symbolizer.o \
	src/processor/stackwalk_common.o \
	src/processor/stackwalker.o \
	src/processor/stackwalker_address_list.o \
	src/processor/stackwalker_amd64.o \
	src/processor/stackwalker_arm.o \
	src/processor/stackwalker_arm64.o \
	src/processor/stackwalker_mips.o \
	src/processor/stackwalker_ppc.o \
	src/processor/stackwalker_ppc64.o \
	src/processor/stackwalker_sparc.o \
	src/processor/stackwalker_x86.o \
	src/processor/tokenize.o \
	src/third_party/libdisasm/libdisasm.a

src_processor_minidump_stackwalk_SOURCES = \
	src/processor/minidump_stackwalk.cc
src_processor_minidump_stackwalk_LDADD = \
	src/common/path_helper.o \
	src/processor/basic_code_modules.o \
	src/processor/basic_source_line_resolver.o \
	src/processor/call_stack.o \
	src/processor/cfi_frame_info.o \
	src/processor/convert_old_arm64_context.o \
	src/processor/disassembler_x86.o \
	src/processor/dump_context.o \
	src/processor/dump_object.o \
	src/processor/exploitability.o \
	src/processor/exploitability_linux.o \
	src/processor/exploitability_win.o \
	src/processor/logging.o \
	src/processor/minidump.o \
	src/processor/minidump_processor.o \
	src/processor/pathname_stripper.o \
	src/processor/process_state.o \
	src/processor/proc_maps_linux.o \
	src/processor/simple_symbol_supplier.o \
	src/processor/source_line_resolver_base.o \
	src/processor/stack_frame_cpu.o \
	src/processor/stack_frame_symbolizer.o \
	src/processor/stackwalk_common.o \
	src/processor/stackwalker.o \
	src/processor/stackwalker_address_list.o \
	src/processor/stackwalker_amd64.o \
	src/processor/stackwalker_arm.o \
	src/processor/stackwalker_arm64.o \
	src/processor/stackwalker_mips.o \
	src/processor/stackwalker_ppc.o \
	src/processor/stackwalker_ppc64.o \
	src/processor/stackwalker_sparc.o \
	src/processor/stackwalker_x86.o \
	src/processor/symbolic_constants_win.o \
	src/processor/tokenize.o \
	src/third_party/libdisasm/libdisasm.a

endif !DISABLE_PROCESSOR

## Additional files to be included in a source distribution
##
## find src/client src/common src/processor/testdata src/tools \
##     -type f \! -path '*/.svn/*' -print | sort | \
##     sed -e s/'^\(.*\)$'/'\t\1 \\'/
EXTRA_DIST = \
	$(SCRIPTS) \
	src/client/linux/data/linux-gate-amd.sym \
	src/client/linux/data/linux-gate-intel.sym \
	src/client/mac/handler/breakpad_nlist_64.cc \
	src/client/mac/handler/breakpad_nlist_64.h \
	src/client/mac/handler/dynamic_images.cc \
	src/client/mac/handler/dynamic_images.h \
	src/client/mac/handler/exception_handler.cc \
	src/client/mac/handler/exception_handler.h \
	src/client/mac/handler/mach_vm_compat.h \
	src/client/mac/handler/minidump_generator.cc \
	src/client/mac/handler/minidump_generator.h \
	src/client/mac/handler/minidump_test.xcodeproj/project.pbxproj \
	src/client/mac/handler/minidump_tests32-Info.plist \
	src/client/mac/handler/minidump_tests64-Info.plist \
	src/client/mac/handler/obj-cTestCases-Info.plist \
	src/client/mac/handler/protected_memory_allocator.cc \
	src/client/mac/handler/protected_memory_allocator.h \
	src/client/mac/handler/ucontext_compat.h \
	src/client/mac/handler/testcases/testdata/dump_syms_i386_breakpad.sym \
	src/client/mac/tests/BreakpadFramework_Test.mm \
	src/client/mac/tests/crash_generation_server_test.cc \
	src/client/mac/tests/exception_handler_test.cc \
	src/client/mac/tests/minidump_generator_test.cc \
	src/client/mac/tests/minidump_generator_test_helper.cc \
	src/client/mac/tests/spawn_child_process.h \
	src/client/mac/tests/testlogging.h \
	src/client/minidump_file_writer_unittest.cc \
	src/client/solaris/handler/Makefile \
	src/client/solaris/handler/exception_handler.cc \
	src/client/solaris/handler/exception_handler.h \
	src/client/solaris/handler/exception_handler_test.cc \
	src/client/solaris/handler/minidump_generator.cc \
	src/client/solaris/handler/minidump_generator.h \
	src/client/solaris/handler/minidump_test.cc \
	src/client/solaris/handler/solaris_lwp.cc \
	src/client/solaris/handler/solaris_lwp.h \
	src/client/windows/breakpad_client.gyp \
	src/client/windows/handler/exception_handler.cc \
	src/client/windows/handler/exception_handler.h \
	src/client/windows/handler/exception_handler.gyp \
	src/client/windows/sender/crash_report_sender.cc \
	src/client/windows/sender/crash_report_sender.h \
	src/client/windows/sender/crash_report_sender.gyp \
	src/common/dwarf/dwarf2diehandler.h \
	src/common/dwarf/dwarf2enums.h \
	src/common/dwarf/line_state_machine.h \
	src/common/dwarf/types.h \
	src/common/mac/arch_utilities.h \
	src/common/mac/byteswap.h \
	src/common/mac/HTTPMultipartUpload.h \
	src/common/mac/HTTPMultipartUpload.m \
	src/common/mac/string_utilities.cc \
	src/common/mac/string_utilities.h \
	src/common/mac/super_fat_arch.h \
	src/common/scoped_ptr.h \
	src/common/solaris/dump_symbols.cc \
	src/common/solaris/dump_symbols.h \
	src/common/solaris/file_id.cc \
	src/common/solaris/file_id.h \
	src/common/solaris/guid_creator.cc \
	src/common/solaris/guid_creator.h \
	src/common/solaris/message_output.h \
	src/common/windows/guid_string.cc \
	src/common/windows/guid_string.h \
	src/common/windows/http_upload.cc \
	src/common/windows/http_upload.h \
	src/common/windows/pdb_source_line_writer.cc \
	src/common/windows/pdb_source_line_writer.h \
	src/common/windows/string_utils-inl.h \
	src/common/windows/string_utils.cc \
	src/processor/microdump_stackwalk_test_vars \
	src/processor/stackwalk_common.cc \
	src/processor/stackwalk_common.h \
	src/processor/stackwalker_selftest_sol.s \
	src/processor/testdata/ascii_read_av_block_write.dmp \
	src/processor/testdata/ascii_read_av_clobber_write.dmp \
	src/processor/testdata/ascii_read_av_conditional.dmp \
	src/processor/testdata/ascii_read_av.dmp \
	src/processor/testdata/ascii_read_av_then_jmp.dmp \
	src/processor/testdata/ascii_read_av_xchg_write.dmp \
	src/processor/testdata/ascii_write_av_arg_to_call.dmp \
	src/processor/testdata/ascii_write_av.dmp \
	src/processor/testdata/exec_av_on_stack.dmp \
	src/processor/testdata/linux_divide_by_zero.dmp \
	src/processor/testdata/linux_executable_heap.dmp \
	src/processor/testdata/linux_executable_stack.dmp \
	src/processor/testdata/linux_inside_module_exe_region1.dmp \
	src/processor/testdata/linux_inside_module_exe_region2.dmp \
	src/processor/testdata/linux_jmp_to_0.dmp \
	src/processor/testdata/linux_jmp_to_module_not_exe_region.dmp \
	src/processor/testdata/linux_null_dereference.dmp \
	src/processor/testdata/linux_null_read_av.dmp \
	src/processor/testdata/linux_outside_module.dmp \
	src/processor/testdata/linux_overflow.dmp \
	src/processor/testdata/linux_raise_sigabrt.dmp \
	src/processor/testdata/linux_stack_pointer_in_module.dmp \
	src/processor/testdata/linux_stack_pointer_in_stack.dmp \
	src/processor/testdata/linux_stack_pointer_in_stack_alt_name.dmp \
	src/processor/testdata/linux_stacksmash.dmp \
	src/processor/testdata/linux_write_to_nonwritable_module.dmp \
	src/processor/testdata/linux_write_to_nonwritable_region_math.dmp \
	src/processor/testdata/linux_write_to_outside_module.dmp \
	src/processor/testdata/linux_write_to_outside_module_via_math.dmp \
	src/processor/testdata/linux_write_to_under_4k.dmp \
	src/processor/testdata/microdump-arm64.dmp \
	src/processor/testdata/microdump-arm.dmp \
	src/processor/testdata/microdump-mips32.dmp \
	src/processor/testdata/microdump-mips64.dmp \
	src/processor/testdata/microdump-multiple.dmp \
	src/processor/testdata/microdump.stackwalk-arm64.out \
	src/processor/testdata/microdump.stackwalk-arm.out \
	src/processor/testdata/microdump.stackwalk.machine_readable-arm64.out \
	src/processor/testdata/microdump.stackwalk.machine_readable-arm.out \
	src/processor/testdata/microdump-withcrashreason.dmp \
	src/processor/testdata/microdump-x86.dmp \
	src/processor/testdata/minidump_32bit_crash_addr.dmp \
	src/processor/testdata/minidump2.dmp \
	src/processor/testdata/minidump2.dump.out \
	src/processor/testdata/minidump2.stackwalk.machine_readable.out \
	src/processor/testdata/minidump2.stackwalk.out \
	src/processor/testdata/module0.out \
	src/processor/testdata/module1.out \
	src/processor/testdata/module2.out \
	src/processor/testdata/module3_bad.out \
	src/processor/testdata/module4_bad.out \
	src/processor/testdata/null_read_av.dmp \
	src/processor/testdata/null_write_av.dmp \
	src/processor/testdata/read_av_clobber_write.dmp \
	src/processor/testdata/read_av_conditional.dmp \
	src/processor/testdata/read_av_non_null.dmp \
	src/processor/testdata/stack_exhaustion.dmp \
	src/processor/testdata/write_av_non_null.dmp \
	src/processor/testdata/symbols/kernel32.pdb/BCE8785C57B44245A669896B6A19B9542/kernel32.sym \
	src/processor/testdata/symbols/ld-2.13.so/C32AD7E235EA6112E02A5B9D6219C4850/ld-2.13.so.sym \
	src/processor/testdata/symbols/libc-2.13.so/F4F8DFCD5A5FB5A7CE64717E9E6AE3890/libc-2.13.so.sym \
	src/processor/testdata/symbols/libgcc_s.so.1/18B180F90887D8F8B5C35D185444AF4C0/libgcc_s.so.1.sym \
	src/processor/testdata/symbols/microdump/breakpad_unittests/D6D1FEC9A15DE7F38A236898871A2E770/breakpad_unittests.sym \
	src/processor/testdata/symbols/microdump/breakpad_unittests/DA7778FB66018A4E9B4110ED06E730D00/breakpad_unittests.sym \
	src/processor/testdata/symbols/microdump/crash_example/6E72E2F1A5F59AB3D51356FDFE394D490/crash_example.sym \
	src/processor/testdata/symbols/microdump/crash_example/8F36148CC4647A8116CAF2A25F591F570/crash_example.sym \
	src/processor/testdata/symbols/null_read_av/7B7D1968FF0D47AE4366E9C3A7E1B6750/null_read_av.sym \
	src/processor/testdata/symbols/overflow/B0E1FC01EF48E39CAF5C881D2DF0C3840/overflow.sym \
	src/processor/testdata/symbols/test_app.pdb/5A9832E5287241C1838ED98914E9B7FF1/test_app.sym \
	src/processor/testdata/test_app.cc \
	src/testing/googletest/include/gtest/gtest.h \
	src/testing/googletest/include/gtest/gtest-death-test.h \
	src/testing/googletest/include/gtest/gtest-matchers.h \
	src/testing/googletest/include/gtest/gtest-message.h \
	src/testing/googletest/include/gtest/gtest-param-test.h \
	src/testing/googletest/include/gtest/gtest-printers.h \
	src/testing/googletest/include/gtest/gtest-spi.h \
	src/testing/googletest/include/gtest/gtest-test-part.h \
	src/testing/googletest/include/gtest/gtest-typed-test.h \
	src/testing/googletest/include/gtest/gtest_pred_impl.h \
	src/testing/googletest/include/gtest/gtest_prod.h \
	src/testing/googletest/include/gtest/internal/custom/gtest-port.h \
	src/testing/googletest/include/gtest/internal/custom/gtest-printers.h \
	src/testing/googletest/include/gtest/internal/custom/gtest.h \
	src/testing/googletest/include/gtest/internal/gtest-death-test-internal.h \
	src/testing/googletest/include/gtest/internal/gtest-filepath.h \
	src/testing/googletest/include/gtest/internal/gtest-internal.h \
	src/testing/googletest/include/gtest/internal/gtest-param-util-generated.h \
	src/testing/googletest/include/gtest/internal/gtest-param-util.h \
	src/testing/googletest/include/gtest/internal/gtest-port-arch.h \
	src/testing/googletest/include/gtest/internal/gtest-port.h \
	src/testing/googletest/include/gtest/internal/gtest-string.h \
	src/testing/googletest/include/gtest/internal/gtest-type-util.h \
	src/testing/googletest/src/gtest.cc \
	src/testing/googletest/src/gtest-death-test.cc \
	src/testing/googletest/src/gtest-filepath.cc \
	src/testing/googletest/src/gtest-internal-inl.h \
	src/testing/googletest/src/gtest-matchers.cc \
	src/testing/googletest/src/gtest-port.cc \
	src/testing/googletest/src/gtest-printers.cc \
	src/testing/googletest/src/gtest-test-part.cc \
	src/testing/googletest/src/gtest-typed-test.cc \
	src/testing/googlemock/include/gmock/gmock.h \
	src/testing/googlemock/include/gmock/gmock-actions.h \
	src/testing/googlemock/include/gmock/gmock-cardinalities.h \
	src/testing/googlemock/include/gmock/gmock-function-mocker.h \
	src/testing/googlemock/include/gmock/gmock-generated-actions.h \
	src/testing/googlemock/include/gmock/gmock-generated-function-mockers.h \
	src/testing/googlemock/include/gmock/gmock-generated-matchers.h \
	src/testing/googlemock/include/gmock/gmock-matchers.h \
	src/testing/googlemock/include/gmock/gmock-more-actions.h \
	src/testing/googlemock/include/gmock/gmock-more-matchers.h \
	src/testing/googlemock/include/gmock/gmock-nice-strict.h \
	src/testing/googlemock/include/gmock/gmock-spec-builders.h \
	src/testing/googlemock/include/gmock/internal/custom/gmock-generated-actions.h \
	src/testing/googlemock/include/gmock/internal/custom/gmock-matchers.h \
	src/testing/googlemock/include/gmock/internal/custom/gmock-port.h \
	src/testing/googlemock/include/gmock/internal/gmock-internal-utils.h \
	src/testing/googlemock/include/gmock/internal/gmock-port.h \
	src/testing/googlemock/include/gmock/internal/gmock-pp.h \
	src/testing/googlemock/src/gmock.cc \
	src/testing/googlemock/src/gmock-cardinalities.cc \
	src/testing/googlemock/src/gmock-internal-utils.cc \
	src/testing/googlemock/src/gmock-matchers.cc \
	src/testing/googlemock/src/gmock-spec-builders.cc \
	src/testing/googlemock/src/gmock_main.cc \
	src/third_party/curl/COPYING \
	src/third_party/curl/curlbuild.h \
	src/third_party/curl/curl.h \
	src/third_party/curl/curlrules.h \
	src/third_party/curl/curlver.h \
	src/third_party/curl/easy.h \
	src/third_party/curl/mprintf.h \
	src/third_party/curl/multi.h \
	src/third_party/curl/stdcheaders.h \
	src/third_party/curl/typecheck-gcc.h \
	src/third_party/curl/types.h \
	src/third_party/mac_headers/architecture/byte_order.h \
	src/third_party/mac_headers/i386/_types.h \
	src/third_party/mac_headers/mach/boolean.h \
	src/third_party/mac_headers/mach/i386/boolean.h \
	src/third_party/mac_headers/mach/i386/vm_param.h \
	src/third_party/mac_headers/mach/i386/vm_types.h \
	src/third_party/mac_headers/mach/machine/boolean.h \
	src/third_party/mac_headers/mach/machine.h \
	src/third_party/mac_headers/mach/machine/thread_state.h \
	src/third_party/mac_headers/mach/machine/thread_status.h \
	src/third_party/mac_headers/mach/machine/vm_types.h \
	src/third_party/mac_headers/mach-o/arch.h \
	src/third_party/mac_headers/mach-o/fat.h \
	src/third_party/mac_headers/mach-o/loader.h \
	src/third_party/mac_headers/mach-o/nlist.h \
	src/third_party/mac_headers/mach/thread_status.h \
	src/third_party/mac_headers/mach/vm_prot.h \
	src/third_party/mac_headers/README \
	src/third_party/musl/README \
	src/third_party/musl/COPYRIGHT \
	src/third_party/musl/README.breakpad \
	src/third_party/musl/VERSION \
	src/third_party/musl/include/elf.h \
	src/tools/mac/crash_report/crash_report.mm \
	src/tools/mac/crash_report/crash_report.xcodeproj/project.pbxproj \
	src/tools/mac/crash_report/on_demand_symbol_supplier.h \
	src/tools/mac/crash_report/on_demand_symbol_supplier.mm \
	src/tools/mac/dump_syms/dump_syms.xcodeproj/project.pbxproj \
	src/tools/mac/dump_syms/dump_syms_tool.cc \
	src/tools/mac/symupload/minidump_upload.m \
	src/tools/mac/symupload/symupload.m \
	src/tools/mac/symupload/symupload.xcodeproj/project.pbxproj \
	src/tools/solaris/dump_syms/Makefile \
	src/tools/solaris/dump_syms/dump_syms.cc \
	src/tools/solaris/dump_syms/run_regtest.sh \
	src/tools/solaris/dump_syms/testdata/dump_syms_regtest.cc \
	src/tools/solaris/dump_syms/testdata/dump_syms_regtest.o \
	src/tools/solaris/dump_syms/testdata/dump_syms_regtest.stabs \
	src/tools/solaris/dump_syms/testdata/dump_syms_regtest.sym \
	src/tools/windows/converter/ms_symbol_server_converter.cc \
	src/tools/windows/converter/ms_symbol_server_converter.h \
	src/tools/windows/converter/ms_symbol_server_converter.gyp \
	src/tools/windows/dump_syms/dump_syms.cc \
	src/tools/windows/dump_syms/dump_syms.gyp \
	src/tools/windows/dump_syms/run_regtest.sh \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest.cc \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest.pdb \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest.sym \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest64.sym \
	src/tools/windows/dump_syms/testdata/omap_reorder_bbs.sym \
	src/tools/windows/dump_syms/testdata/omap_reorder_funcs.sym \
	src/tools/windows/dump_syms/testdata/omap_stretched.sym \
	src/tools/windows/dump_syms/testdata/omap_stretched_filled.sym \
	src/tools/windows/symupload/symupload.cc \
	src/tools/windows/symupload/symupload.gyp

mostlyclean-local:
	-find src -name '*.dwo' -exec rm -f {} +
