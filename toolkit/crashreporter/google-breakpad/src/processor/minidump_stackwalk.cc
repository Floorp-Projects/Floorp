// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// minidump_stackwalk.cc: Process a minidump with MinidumpProcessor, printing
// the results, including stack traces.
//
// Author: Mark Mentovai

#include <cstdio>
#include <cstdlib>
#include <string>

#include "google_airbag/processor/call_stack.h"
#include "google_airbag/processor/minidump.h"
#include "google_airbag/processor/minidump_processor.h"
#include "google_airbag/processor/process_state.h"
#include "google_airbag/processor/stack_frame_cpu.h"
#include "processor/pathname_stripper.h"
#include "processor/scoped_ptr.h"
#include "processor/simple_symbol_supplier.h"

namespace {

using std::string;
using google_airbag::CallStack;
using google_airbag::MinidumpModule;
using google_airbag::MinidumpProcessor;
using google_airbag::PathnameStripper;
using google_airbag::ProcessState;
using google_airbag::scoped_ptr;
using google_airbag::SimpleSymbolSupplier;
using google_airbag::StackFrame;
using google_airbag::StackFramePPC;
using google_airbag::StackFrameX86;

// PrintRegister prints a register's name and value to stdout.  It will
// print four registers on a line.  For the first register in a set,
// pass 0 for |sequence|.  For registers in a set, pass the most recent
// return value of PrintRegister.  Note that PrintRegister will print a
// newline before the first register (with |sequence| set to 0) is printed.
// The caller is responsible for printing the final newline after a set
// of registers is completely printed, regardless of the number of calls
// to PrintRegister.
static int PrintRegister(const char *name, u_int32_t value, int sequence) {
  if (sequence % 4 == 0) {
    printf("\n ");
  }
  printf(" %5s = 0x%08x", name, value);
  return ++sequence;
}

// PrintStack prints the call stack in |stack| to stdout, in a reasonably
// useful form.  Module, function, and source file names are displayed if
// they are available.  The code offset to the base code address of the
// source line, function, or module is printed, preferring them in that
// order.  If no source line, function, or module information is available,
// an absolute code offset is printed.
//
// If |cpu| is a recognized CPU name, relevant register state for each stack
// frame printed is also output, if available.
static void PrintStack(const CallStack *stack, const string &cpu) {
  int frame_count = stack->frames()->size();
  for (int frame_index = 0; frame_index < frame_count; ++frame_index) {
    const StackFrame *frame = stack->frames()->at(frame_index);
    printf("%2d  ", frame_index);

    if (!frame->module_name.empty()) {
      printf("%s", PathnameStripper::File(frame->module_name).c_str());
      if (!frame->function_name.empty()) {
        printf("!%s", frame->function_name.c_str());
        if (!frame->source_file_name.empty()) {
          string source_file = PathnameStripper::File(frame->source_file_name);
          printf(" [%s : %d + 0x%llx]", source_file.c_str(),
                                        frame->source_line,
                                        frame->instruction -
                                          frame->source_line_base);
        } else {
          printf(" + 0x%llx", frame->instruction - frame->function_base);
        }
      } else {
        printf(" + 0x%llx", frame->instruction - frame->module_base);
      }
    } else {
      printf("0x%llx", frame->instruction);
    }

    int sequence = 0;
    if (cpu == "x86") {
      const StackFrameX86 *frame_x86 =
          reinterpret_cast<const StackFrameX86*>(frame);

      if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EIP)
        sequence = PrintRegister("eip", frame_x86->context.eip, sequence);
      if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_ESP)
        sequence = PrintRegister("esp", frame_x86->context.esp, sequence);
      if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EBP)
        sequence = PrintRegister("ebp", frame_x86->context.ebp, sequence);
      if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EBX)
        sequence = PrintRegister("ebx", frame_x86->context.ebx, sequence);
      if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_ESI)
        sequence = PrintRegister("esi", frame_x86->context.esi, sequence);
      if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EDI)
        sequence = PrintRegister("edi", frame_x86->context.edi, sequence);
      if (frame_x86->context_validity == StackFrameX86::CONTEXT_VALID_ALL) {
        sequence = PrintRegister("eax", frame_x86->context.eax, sequence);
        sequence = PrintRegister("ecx", frame_x86->context.ecx, sequence);
        sequence = PrintRegister("edx", frame_x86->context.edx, sequence);
        sequence = PrintRegister("efl", frame_x86->context.eflags, sequence);
      }
    } else if (cpu == "ppc") {
      const StackFramePPC *frame_ppc =
          reinterpret_cast<const StackFramePPC*>(frame);

      if (frame_ppc->context_validity & StackFramePPC::CONTEXT_VALID_SRR0)
        sequence = PrintRegister("srr0", frame_ppc->context.srr0, sequence);
      if (frame_ppc->context_validity & StackFramePPC::CONTEXT_VALID_GPR1)
        sequence = PrintRegister("r1", frame_ppc->context.gpr[1], sequence);
    }

    printf("\n");
  }
}

// Processes |minidump_file| using MinidumpProcessor.  |symbol_path|, if
// non-empty, is the base directory of a symbol storage area, laid out in
// the format required by SimpleSymbolSupplier.  If such a storage area
// is specified, it is made available for use by the MinidumpProcessor.
//
// Returns the value of MinidumpProcessor::Process.  If processing succeeds,
// prints identifying OS and CPU information from the minidump, crash
// information if the minidump was produced as a result of a crash, and
// call stacks for each thread contained in the minidump.  All information
// is printed to stdout.
static bool PrintMinidumpProcess(const string &minidump_file,
                                 const string &symbol_path) {
  scoped_ptr<SimpleSymbolSupplier> symbol_supplier;
  if (!symbol_path.empty()) {
    // TODO(mmentovai): check existence of symbol_path if specified?
    symbol_supplier.reset(new SimpleSymbolSupplier(symbol_path));
  }

  MinidumpProcessor minidump_processor(symbol_supplier.get());

  // Process the minidump.
  scoped_ptr<ProcessState> process_state(
      minidump_processor.Process(minidump_file));
  if (!process_state.get()) {
    fprintf(stderr, "MinidumpProcessor::Process failed\n");
    return false;
  }

  // Print OS and CPU information.
  string cpu = process_state->cpu();
  string cpu_info = process_state->cpu_info();
  printf("Operating system: %s\n", process_state->os().c_str());
  printf("                  %s\n", process_state->os_version().c_str());
  printf("CPU: %s\n", cpu.c_str());
  if (!cpu_info.empty()) {
    // This field is optional.
    printf("     %s\n", cpu_info.c_str());
  }
  printf("\n");

  // Print crash information.
  if (process_state->crashed()) {
    printf("Crash reason:  %s\n", process_state->crash_reason().c_str());
    printf("Crash address: 0x%llx\n", process_state->crash_address());
  } else {
    printf("No crash\n");
  }

  // If the thread that requested the dump is known, print it first.
  int requesting_thread = process_state->requesting_thread(); 
  if (requesting_thread != -1) {
    printf("\n");
    printf("Thread %d (%s)\n",
          requesting_thread,
          process_state->crashed() ? "crashed" :
                                     "requested dump, did not crash");
    PrintStack(process_state->threads()->at(requesting_thread), cpu);
  }

  // Print all of the threads in the dump.
  int thread_count = process_state->threads()->size();
  for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
    if (thread_index != requesting_thread) {
      // Don't print the crash thread again, it was already printed.
      printf("\n");
      printf("Thread %d\n", thread_index);
      PrintStack(process_state->threads()->at(thread_index), cpu);
    }
  }

  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s <minidump-file> [symbol-path]\n", argv[0]);
    return 1;
  }

  const char *minidump_file = argv[1];
  const char *symbol_path = "";
  if (argc == 3) {
    symbol_path = argv[2];
  }

  return PrintMinidumpProcess(minidump_file, symbol_path) ? 0 : 1;
}
