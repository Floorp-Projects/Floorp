// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/error.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "lib/jpegli/common.h"

namespace jpegli {

const char* const kErrorMessageTable[] = {
    "Something went wrong.",
};

void ExitWithAbort(j_common_ptr cinfo) {
  (*cinfo->err->output_message)(cinfo);
  exit(EXIT_FAILURE);
}

void ExitWithLongJump(j_common_ptr cinfo) {
  (*cinfo->err->output_message)(cinfo);
  jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
  longjmp(*env, 1);
}

void OutputMessage(j_common_ptr cinfo) {
  fprintf(stderr, "%s", cinfo->err->msg_parm.s);
}

void FormatMessage(j_common_ptr cinfo, char* buffer) {
  memcpy(buffer, cinfo->err->msg_parm.s, JMSG_LENGTH_MAX);
}

// These are not (yet) used by this library.
void EmitMessage(j_common_ptr cinfo, int msg_level) {}
void ResetErrorManager(j_common_ptr cinfo) {}

}  // namespace jpegli

struct jpeg_error_mgr* jpegli_std_error(struct jpeg_error_mgr* err) {
  err->error_exit = jpegli::ExitWithAbort;
  err->output_message = jpegli::OutputMessage;
  err->emit_message = jpegli::EmitMessage;
  err->format_message = jpegli::FormatMessage;
  err->reset_error_mgr = jpegli::ResetErrorManager;
  err->msg_code = 0;
  err->trace_level = 0;
  err->num_warnings = 0;
  err->jpeg_message_table = jpegli::kErrorMessageTable;
  err->last_jpeg_message = 0;
  err->addon_message_table = nullptr;
  err->first_addon_message = 0;
  err->last_addon_message = 0;
  return err;
}
