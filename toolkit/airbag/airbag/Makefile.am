## Process this file with automake to produce Makefile.in

# Copyright (c) 2006, Google Inc.
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
lib_LTLIBRARIES = src/libairbag.la

src_libairbag_la_SOURCES = \
	src/google/airbag_types.h \
	src/google/call_stack.h \
	src/google/minidump_processor.h \
	src/google/process_state.h \
	src/google/stack_frame.h \
	src/google/stack_frame_cpu.h \
	src/google/symbol_supplier.h \
	src/processor/address_map.h \
	src/processor/address_map-inl.h \
	src/processor/call_stack.cc \
	src/processor/contained_range_map.h \
	src/processor/contained_range_map-inl.h \
	src/processor/linked_ptr.h \
	src/processor/memory_region.h \
	src/processor/minidump.cc \
	src/processor/minidump.h \
	src/processor/minidump_format.h \
	src/processor/minidump_processor.cc \
	src/processor/postfix_evaluator.h \
	src/processor/postfix_evaluator-inl.h \
	src/processor/process_state.cc \
	src/processor/range_map.h \
	src/processor/range_map-inl.h \
	src/processor/scoped_ptr.h \
	src/processor/source_line_resolver.cc \
	src/processor/source_line_resolver.h \
	src/processor/stack_frame_info.h \
	src/processor/stackwalker.cc \
	src/processor/stackwalker.h \
	src/processor/stackwalker_ppc.cc \
	src/processor/stackwalker_ppc.h \
	src/processor/stackwalker_x86.cc \
	src/processor/stackwalker_x86.h


## Programs
bin_PROGRAMS = \
	src/processor/minidump_dump \
	src/processor/minidump_stackwalk


## Tests
check_PROGRAMS = \
	src/processor/address_map_unittest \
	src/processor/contained_range_map_unittest \
	src/processor/minidump_processor_unittest \
	src/processor/postfix_evaluator_unittest \
	src/processor/range_map_unittest \
	src/processor/source_line_resolver_unittest

if SELFTEST
check_PROGRAMS += \
	src/processor/stackwalker_selftest
endif SELFTEST

check_SCRIPTS = \
	src/processor/minidump_dump_test \
	src/processor/minidump_stackwalk_test

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)
TESTS_ENVIRONMENT =

src_processor_address_map_unittest_SOURCES = \
	src/processor/address_map_unittest.cc

src_processor_contained_range_map_unittest_SOURCES = \
	src/processor/contained_range_map_unittest.cc

src_processor_minidump_processor_unittest_SOURCES = \
	src/processor/minidump_processor_unittest.cc
src_processor_minidump_processor_unittest_LDADD = \
	src/processor/call_stack.lo \
	src/processor/minidump_processor.lo \
	src/processor/minidump.lo \
	src/processor/process_state.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/source_line_resolver.lo

src_processor_postfix_evaluator_unittest_SOURCES = \
	src/processor/postfix_evaluator_unittest.cc

src_processor_range_map_unittest_SOURCES = \
	src/processor/range_map_unittest.cc

src_processor_source_line_resolver_unittest_SOURCES = \
	src/processor/source_line_resolver_unittest.cc
src_processor_source_line_resolver_unittest_LDADD = \
	src/processor/source_line_resolver.lo

src_processor_stackwalker_selftest_SOURCES = \
	src/processor/stackwalker_selftest.cc
src_processor_stackwalker_selftest_LDADD = \
	src/processor/call_stack.lo \
	src/processor/minidump.lo \
	src/processor/source_line_resolver.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_x86.lo

## Non-installables
noinst_PROGRAMS =
noinst_SCRIPTS = $(check_SCRIPTS)

src_processor_minidump_dump_SOURCES = \
	src/processor/minidump_dump.cc
src_processor_minidump_dump_LDADD = \
	src/processor/minidump.lo

src_processor_minidump_stackwalk_SOURCES = \
	src/processor/minidump_stackwalk.cc
src_processor_minidump_stackwalk_LDADD = \
	src/processor/call_stack.lo \
	src/processor/minidump.lo \
	src/processor/stackwalker.lo \
	src/processor/stackwalker_ppc.lo \
	src/processor/stackwalker_x86.lo \
	src/processor/source_line_resolver.lo


## Additional files to be included in a source distribution
EXTRA_DIST = \
	$(SCRIPTS) \
	src/processor/testdata/minidump1.dmp \
	src/processor/testdata/minidump1.out \
	src/processor/testdata/minidump1.stack.out \
	src/processor/testdata/minidump2.dmp \
	src/processor/testdata/minidump2.sym \
	src/processor/testdata/module1.out \
	src/processor/testdata/module2.out \
	src/processor/testdata/module3_bad.out


## Additional rules
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
