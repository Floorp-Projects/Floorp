## Process this file with automake to produce Makefile.in

# Copyright (c) 2010, Google Inc.
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

# Specify include paths for ac macros
ACLOCAL_AMFLAGS = -I m4

## Documentation
docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)

dist_doc_DATA = \
	AUTHORS \
	COPYING \
	ChangeLog \
	INSTALL \
	NEWS \
	README


## Libraries
lib_LTLIBRARIES = src/libbreakpad.la src/client/linux/libbreakpad_client.la

src_client_linux_libbreakpad_client_la_SOURCES = \
	src/client/linux/crash_generation/crash_generation_client.cc \
	src/client/linux/handler/exception_handler.cc \
	src/client/linux/minidump_writer/linux_dumper.cc \
	src/client/linux/minidump_writer/minidump_writer.cc \
	src/client/minidump_file_writer.cc \
	src/common/convert_UTF.c \
	src/common/md5.c \
	src/common/string_conversion.cc \
	src/common/linux/file_id.cc \
	src/common/linux/guid_creator.cc

src_libbreakpad_la_SOURCES = \
	src/google_breakpad/common/breakpad_types.h \
	src/google_breakpad/common/minidump_format.h \
	src/google_breakpad/common/minidump_size.h \
	src/google_breakpad/processor/basic_source_line_resolver.h \
	src/google_breakpad/processor/call_stack.h \
	src/google_breakpad/processor/code_module.h \
	src/google_breakpad/processor/code_modules.h \
	src/google_breakpad/processor/memory_region.h \
	src/google_breakpad/processor/minidump.h \
	src/google_breakpad/processor/minidump_processor.h \
	src/google-breakpad/processor/network_source_line_resolver.h \
	src/google_breakpad/processor/process_state.h \
	src/google_breakpad/processor/source_line_resolver_interface.h \
	src/google_breakpad/processor/stack_frame.h \
	src/google_breakpad/processor/stack_frame_cpu.h \
	src/google_breakpad/processor/stackwalker.h \
	src/google_breakpad/processor/symbol_supplier.h \
	src/google_breakpad/processor/system_info.h \
	src/processor/address_map-inl.h \
	src/processor/address_map.h \
	src/processor/basic_code_module.h \
	src/processor/basic_code_modules.cc \
	src/processor/basic_code_modules.h \
	src/processor/basic_source_line_resolver.cc \
	src/processor/binarystream.h \
	src/processor/binarystream.cc \
	src/processor/call_stack.cc \
	src/processor/cfi_frame_info.cc \
	src/processor/cfi_frame_info.h \
	src/processor/contained_range_map-inl.h \
	src/processor/contained_range_map.h \
	src/processor/linked_ptr.h \
	src/processor/logging.h \
	src/processor/logging.cc \
	src/processor/minidump.cc \
	src/processor/minidump_processor.cc \
	src/processor/network_interface.h \
	src/processor/network_source_line_resolver.cc \
	src/processor/network_source_line_server.cc \
	src/processor/network_source_line_server.h \
	src/processor/pathname_stripper.cc \
	src/processor/pathname_stripper.h \
	src/processor/postfix_evaluator-inl.h \
	src/processor/postfix_evaluator.h \
	src/processor/process_state.cc \
	src/processor/range_map-inl.h \
	src/processor/range_map.h \
	src/processor/scoped_ptr.h \
	src/processor/simple_symbol_supplier.cc \
	src/processor/simple_symbol_supplier.h \
	src/processor/windows_frame_info.h \
	src/processor/stackwalker.cc \
	src/processor/stackwalker_amd64.cc \
	src/processor/stackwalker_amd64.h \
	src/processor/stackwalker_arm.cc \
	src/processor/stackwalker_arm.h \
	src/processor/stackwalker_ppc.cc \
	src/processor/stackwalker_ppc.h \
	src/processor/stackwalker_sparc.cc \
	src/processor/stackwalker_sparc.h \
	src/processor/stackwalker_x86.cc \
	src/processor/stackwalker_x86.h \
	src/processor/tokenize.cc \
	src/processor/tokenize.h \
	src/processor/udp_network.cc \
	src/processor/udp_network.h


## Programs
bin_PROGRAMS = \
	src/client/linux/linux_dumper_unittest_helper \
	src/processor/minidump_dump \
	src/processor/minidump_stackwalk \
	src/processor/source_daemon 

## Tests
check_PROGRAMS = \
	src/client/linux/linux_client_unittest \
	src/common/test_assembler_unittest \
	src/processor/address_map_unittest \
	src/processor/binarystream_unittest \
	src/processor/basic_source_line_resolver_unittest \
	src/processor/cfi_frame_info_unittest \
	src/processor/contained_range_map_unittest \
	src/processor/minidump_processor_unittest \
	src/processor/minidump_unittest \
	src/processor/network_source_line_resolver_unittest \
	src/processor/network_source_line_resolver_server_unittest \
	src/processor/network_source_line_server_unittest \
	src/processor/pathname_stripper_unittest \
	src/processor/postfix_evaluator_unittest \
	src/processor/range_map_unittest \
	src/processor/stackwalker_amd64_unittest \
	src/processor/stackwalker_arm_unittest \
	src/processor/stackwalker_x86_unittest \
	src/processor/synth_minidump_unittest

if SELFTEST
check_PROGRAMS += \
	src/processor/stackwalker_selftest
endif SELFTEST

check_SCRIPTS = \
	src/processor/minidump_dump_test \
	src/processor/minidump_stackwalk_test \
	src/processor/minidump_stackwalk_machine_readable_test

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)
TESTS_ENVIRONMENT =

src_client_linux_linux_dumper_unittest_helper_SOURCES = \
	src/client/linux/minidump_writer/linux_dumper_unittest_helper.cc
src_client_linux_linux_dumper_unittest_helper_CXXFLAGS=$(PTHREAD_CFLAGS)
src_client_linux_linux_dumper_unittest_helper_LDFLAGS=$(PTHREAD_CFLAGS)
src_client_linux_linux_dumper_unittest_helper_CC=$(PTHREAD_CC)

src_client_linux_linux_client_unittest_SOURCES = \
	src/client/linux/handler/exception_handler_unittest.cc \
	src/client/linux/minidump_writer/directory_reader_unittest.cc \
	src/client/linux/minidump_writer/line_reader_unittest.cc \
	src/client/linux/minidump_writer/linux_dumper_unittest.cc \
	src/client/linux/minidump_writer/minidump_writer_unittest.cc \
	src/common/memory_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc

src_client_linux_linux_client_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_client_linux_linux_client_unittest_LDADD = \
	src/client/linux/handler/exception_handler.lo \
	src/client/linux/crash_generation/crash_generation_client.lo \
	src/client/linux/minidump_writer/linux_dumper.lo \
	src/client/linux/minidump_writer/minidump_writer.lo \
	src/client/minidump_file_writer.lo \
	src/common/convert_UTF.lo \
	src/common/md5.lo \
	src/common/linux/file_id.lo \
	src/common/linux/guid_creator.lo \
	src/common/string_conversion.lo
src_client_linux_linux_client_unittest_DEPENDENCIES = src/client/linux/linux_dumper_unittest_helper src/client/linux/libbreakpad_client.la

src_processor_address_map_unittest_SOURCES = \
	src/processor/address_map_unittest.cc
src_processor_address_map_unittest_LDADD = \
	src/processor/logging.lo \
	src/processor/pathname_stripper.lo

src_processor_binarystream_unittest_SOURCES = \
	src/processor/binarystream_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/src/gmock-all.cc
src_processor_binarystream_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_binarystream_unittest_LDADD = \
	src/processor/binarystream.lo

src_processor_basic_source_line_resolver_unittest_SOURCES = \
	src/processor/basic_source_line_resolver_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/src/gmock-all.cc
src_processor_basic_source_line_resolver_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_basic_source_line_resolver_unittest_LDADD = \
	src/processor/basic_source_line_resolver.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/pathname_stripper.lo \
	src/processor/logging.lo \
	src/processor/tokenize.lo

src_processor_cfi_frame_info_unittest_SOURCES = \
	src/processor/cfi_frame_info_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc
src_processor_cfi_frame_info_unittest_LDADD = \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/pathname_stripper.lo
src_processor_cfi_frame_info_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing

src_processor_contained_range_map_unittest_SOURCES = \
	src/processor/contained_range_map_unittest.cc
src_processor_contained_range_map_unittest_LDADD = \
	src/processor/logging.lo \
	src/processor/pathname_stripper.lo

src_processor_minidump_processor_unittest_SOURCES = \
	src/processor/minidump_processor_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/src/gmock-all.cc
src_processor_minidump_processor_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_minidump_processor_unittest_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/basic_source_line_resolver.lo \
	src/processor/call_stack.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/minidump_processor.lo \
	src/processor/minidump.lo \
	src/processor/pathname_stripper.lo \
	src/processor/process_state.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_amd64.lo \
	src/processor/stackwalker_arm.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_sparc.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/tokenize.lo

src_processor_minidump_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/minidump_unittest.cc \
	src/processor/synth_minidump.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc
src_processor_minidump_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_minidump_unittest_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/logging.lo \
	src/processor/minidump.lo \
	src/processor/pathname_stripper.lo

src_processor_network_source_line_resolver_unittest_SOURCES = \
	src/processor/network_source_line_resolver_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/src/gmock-all.cc
src_processor_network_source_line_resolver_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_network_source_line_resolver_unittest_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/binarystream.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/network_source_line_resolver.lo \
	src/processor/pathname_stripper.lo \
	src/processor/tokenize.lo \
	src/processor/udp_network.lo

src_processor_network_source_line_resolver_server_unittest_SOURCES = \
	src/processor/network_source_line_resolver_server_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/src/gmock-all.cc
src_processor_network_source_line_resolver_server_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_network_source_line_resolver_server_unittest_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/basic_source_line_resolver.lo \
	src/processor/binarystream.lo \
	src/processor/call_stack.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/minidump_processor.lo \
	src/processor/minidump.lo \
	src/processor/network_source_line_resolver.lo \
	src/processor/network_source_line_server.lo \
	src/processor/pathname_stripper.lo \
	src/processor/process_state.lo \
	src/processor/simple_symbol_supplier.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_amd64.lo \
	src/processor/stackwalker_arm.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_sparc.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/tokenize.lo \
	src/processor/udp_network.lo

src_processor_network_source_line_server_unittest_SOURCES = \
	src/processor/network_source_line_server_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/src/gmock-all.cc
src_processor_network_source_line_server_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing
src_processor_network_source_line_server_unittest_LDADD = \
	src/processor/binarystream.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/network_source_line_server.lo \
	src/processor/pathname_stripper.lo \
	src/processor/udp_network.lo

src_processor_pathname_stripper_unittest_SOURCES = \
	src/processor/pathname_stripper_unittest.cc
src_processor_pathname_stripper_unittest_LDADD = \
	src/processor/pathname_stripper.lo

src_processor_postfix_evaluator_unittest_SOURCES = \
	src/processor/postfix_evaluator_unittest.cc
src_processor_postfix_evaluator_unittest_LDADD = \
	src/processor/logging.lo \
	src/processor/pathname_stripper.lo

src_processor_range_map_unittest_SOURCES = \
	src/processor/range_map_unittest.cc
src_processor_range_map_unittest_LDADD = \
	src/processor/logging.lo \
	src/processor/pathname_stripper.lo

src_processor_stackwalker_selftest_SOURCES = \
	src/processor/stackwalker_selftest.cc
src_processor_stackwalker_selftest_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/basic_source_line_resolver.lo \
	src/processor/call_stack.lo \
	src/processor/logging.lo \
	src/processor/minidump.lo \
	src/processor/pathname_stripper.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_amd64.lo \
	src/processor/stackwalker_arm.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_sparc.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/tokenize.lo

src_processor_stackwalker_amd64_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_amd64_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc
src_processor_stackwalker_amd64_unittest_LDADD = \
	src/libbreakpad.la
src_processor_stackwalker_amd64_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing

src_processor_stackwalker_arm_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_arm_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc
src_processor_stackwalker_arm_unittest_LDADD = \
	src/libbreakpad.la
src_processor_stackwalker_arm_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing

src_processor_stackwalker_x86_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/processor/stackwalker_x86_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc
src_processor_stackwalker_x86_unittest_LDADD = \
	src/libbreakpad.la
src_processor_stackwalker_x86_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing

src_processor_synth_minidump_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/common/test_assembler.h \
	src/processor/synth_minidump_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc \
	src/processor/synth_minidump.cc \
	src/processor/synth_minidump.h
src_processor_synth_minidump_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing

src_common_test_assembler_unittest_SOURCES = \
	src/common/test_assembler.cc \
	src/common/test_assembler.h \
	src/common/test_assembler_unittest.cc \
	src/testing/gtest/src/gtest-all.cc \
	src/testing/gtest/src/gtest_main.cc \
	src/testing/src/gmock-all.cc
src_common_test_assembler_unittest_CPPFLAGS = \
	-I$(top_srcdir)/src/testing/include \
        -I$(top_srcdir)/src/testing/gtest/include \
	-I$(top_srcdir)/src/testing/gtest \
	-I$(top_srcdir)/src/testing

## Non-installables
noinst_PROGRAMS =
noinst_SCRIPTS = $(check_SCRIPTS)

src_processor_minidump_dump_SOURCES = \
	src/processor/minidump_dump.cc
src_processor_minidump_dump_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/logging.lo \
	src/processor/minidump.lo \
	src/processor/pathname_stripper.lo

src_processor_minidump_stackwalk_SOURCES = \
	src/processor/minidump_stackwalk.cc
src_processor_minidump_stackwalk_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/basic_source_line_resolver.lo \
	src/processor/binarystream.lo \
	src/processor/call_stack.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/minidump.lo \
	src/processor/minidump_processor.lo \
	src/processor/pathname_stripper.lo \
	src/processor/process_state.lo \
	src/processor/network_source_line_resolver.lo \
	src/processor/simple_symbol_supplier.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_amd64.lo \
	src/processor/stackwalker_arm.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_sparc.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/tokenize.lo \
	src/processor/udp_network.lo

src_processor_source_daemon_SOURCES = \
	src/processor/source_daemon.cc
src_processor_source_daemon_LDADD = \
	src/processor/basic_code_modules.lo \
	src/processor/basic_source_line_resolver.lo \
	src/processor/binarystream.lo \
	src/processor/cfi_frame_info.lo \
	src/processor/logging.lo \
	src/processor/network_source_line_server.lo \
	src/processor/pathname_stripper.lo \
	src/processor/simple_symbol_supplier.lo \
	src/processor/tokenize.lo \
	src/processor/udp_network.lo

## Additional files to be included in a source distribution
##
## find src/client src/common src/processor/testdata src/tools \
##     -type f \! -path '*/.svn/*' -print | sort | \
##     sed -e s/'^\(.*\)$'/'\t\1 \\'/
EXTRA_DIST = \
	$(SCRIPTS) \
	src/processor/stackwalk_selftest_sol.s \
	src/client/linux/handler/Makefile \
	src/client/linux/handler/exception_handler.cc \
	src/client/linux/handler/exception_handler.h \
	src/client/linux/handler/exception_handler_test.cc \
	src/client/linux/handler/linux_thread.cc \
	src/client/linux/handler/linux_thread.h \
	src/client/linux/handler/linux_thread_test.cc \
	src/client/linux/handler/minidump_generator.cc \
	src/client/linux/handler/minidump_generator.h \
	src/client/linux/handler/minidump_test.cc \
	src/client/mac/handler/dynamic_images.cc \
	src/client/mac/handler/dynamic_images.h \
	src/client/mac/handler/exception_handler.cc \
	src/client/mac/handler/exception_handler.h \
	src/client/mac/handler/exception_handler_test.cc \
	src/client/mac/handler/minidump_generator.cc \
	src/client/mac/handler/minidump_generator.h \
	src/client/mac/handler/minidump_generator_test.cc \
	src/client/mac/handler/minidump_test.xcodeproj/project.pbxproj \
	src/client/mac/handler/protected_memory_allocator.cc \
	src/client/mac/handler/protected_memory_allocator.h \
	src/client/minidump_file_writer-inl.h \
	src/client/minidump_file_writer.cc \
	src/client/minidump_file_writer.h \
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
	src/client/windows/breakpad_client.sln \
	src/client/windows/handler/exception_handler.cc \
	src/client/windows/handler/exception_handler.h \
	src/client/windows/handler/exception_handler.vcproj \
	src/client/windows/sender/crash_report_sender.cc \
	src/client/windows/sender/crash_report_sender.h \
	src/client/windows/sender/crash_report_sender.vcproj \
	src/common/convert_UTF.c \
	src/common/convert_UTF.h \
	src/common/linux/dump_symbols.cc \
	src/common/linux/dump_symbols.h \
	src/common/linux/file_id.cc \
	src/common/linux/file_id.h \
	src/common/linux/guid_creator.cc \
	src/common/linux/guid_creator.h \
	src/common/linux/http_upload.cc \
	src/common/linux/http_upload.h \
	src/common/mac/HTTPMultipartUpload.h \
	src/common/mac/HTTPMultipartUpload.m \
	src/common/mac/dump_syms.h \
	src/common/mac/dump_syms.mm \
	src/common/mac/file_id.cc \
	src/common/mac/file_id.h \
	src/common/mac/macho_id.cc \
	src/common/mac/macho_id.h \
	src/common/mac/macho_utilities.cc \
	src/common/mac/macho_utilities.h \
	src/common/mac/macho_walker.cc \
	src/common/mac/macho_walker.h \
	src/common/mac/string_utilities.cc \
	src/common/mac/string_utilities.h \
	src/common/md5.cc \
	src/common/md5.h \
	src/common/solaris/dump_symbols.cc \
	src/common/solaris/dump_symbols.h \
	src/common/solaris/file_id.cc \
	src/common/solaris/file_id.h \
	src/common/solaris/guid_creator.cc \
	src/common/solaris/guid_creator.h \
	src/common/solaris/message_output.h \
	src/common/string_conversion.cc \
	src/common/string_conversion.h \
	src/common/windows/guid_string.cc \
	src/common/windows/guid_string.h \
	src/common/windows/http_upload.cc \
	src/common/windows/http_upload.h \
	src/common/windows/pdb_source_line_writer.cc \
	src/common/windows/pdb_source_line_writer.h \
	src/common/windows/string_utils-inl.h \
	src/common/windows/string_utils.cc \
	src/processor/testdata/minidump2.dmp \
	src/processor/testdata/minidump2.dump.out \
	src/processor/testdata/minidump2.stackwalk.machine_readable.out \
	src/processor/testdata/minidump2.stackwalk.out \
	src/processor/testdata/module1.out \
	src/processor/testdata/module2.out \
	src/processor/testdata/module3_bad.out \
	src/processor/testdata/module4_bad.out \
	src/processor/testdata/symbols/kernel32.pdb/BCE8785C57B44245A669896B6A19B9542/kernel32.sym \
	src/processor/testdata/symbols/test_app.pdb/5A9832E5287241C1838ED98914E9B7FF1/test_app.sym \
	src/processor/testdata/test_app.cc \
	src/tools/linux/dump_syms/Makefile \
	src/tools/linux/dump_syms/dump_syms.cc \
	src/tools/linux/symupload/Makefile \
	src/tools/linux/symupload/minidump_upload.cc \
	src/tools/linux/symupload/sym_upload.cc \
	src/tools/mac/crash_report/crash_report.mm \
	src/tools/mac/crash_report/crash_report.xcodeproj/project.pbxproj \
	src/tools/mac/crash_report/on_demand_symbol_supplier.h \
	src/tools/mac/crash_report/on_demand_symbol_supplier.mm \
	src/tools/mac/dump_syms/dump_syms.xcodeproj/project.pbxproj \
	src/tools/mac/dump_syms/dump_syms_tool.m \
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
	src/tools/windows/converter/ms_symbol_server_converter.vcproj \
	src/tools/windows/dump_syms/dump_syms.cc \
	src/tools/windows/dump_syms/dump_syms.vcproj \
	src/tools/windows/dump_syms/run_regtest.sh \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest.cc \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest.pdb \
	src/tools/windows/dump_syms/testdata/dump_syms_regtest.sym \
	src/tools/windows/symupload/symupload.cc \
	src/tools/windows/symupload/symupload.vcproj


## Additional rules
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

