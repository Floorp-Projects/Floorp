// Copyright (c) 2010, Google Inc.
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

// This file contains constants used in the network source line server
// protocol.

// Brief protocol description:
//
// Requests are sent via UDP. All requests begin with a sequence number
// that is prepended to the response. The sequence number is opaque
// to the server, it is provided for client tracking of requests.
//
// In this file, request and response fields will be described as:
// <foo:N>, which should be read as "foo, an N byte unsigned integer
// in network byte order". Strings will be described as <foo:S>.
//
// A client request looks like:
// <seq:2><command:1><command data>
// Where <seq> is a sequence number as described above, <command>
// is one of the commands listed below, and <command data> is arbitrary
// data, defined per-command.
//
// A server response looks like:
// <seq:2><status:1><data>
// Where <seq> is the same sequence number from the request,
// <status> is one of the OK or ERROR values defined below,
// with OK signifying that the request was formatted properly and
// the response contains data, and ERROR signifying that the request
// was malformed in some way. <data> is arbitrary data, defined
// per-command below.
//
// Strings are sent as a 2-byte integer encoding the length, followed
// by <length> bytes.
//
// Valid Commands:
//==================================================
// <HAS:1><module name:S><debug file:S><debug identifier:S>
// example: <HAS><0x8>test.dll<0x8>test.pdb<0xA>0123456789
//
// Query whether the module with this filename and debug information
// has been previously loaded.
//
// Server Response:
// <loaded:1>
// Where <loaded> is one of MODULE_LOADED or MODULE_NOT_LOADED
//
//==================================================
// <LOAD:1><module name:S><debug file:S><debug identifier:S>
// example: <LOAD><0x8>test.dll<0x8>test.pdb<0xA>0123456789
//
// Request that the server find and load symbols for this module.
//
// Server Response:
// <result:1>
// Where <result> is one of:
// LOAD_NOT_FOUND
//  - Symbols not found
// LOAD_INTERRUPT
//  - Processing should be interrupted, symbols may be available later
// LOAD_FAIL
//  - Found symbols, but failed to load them
// LOAD_OK
//  - Found and loaded symbols successfully
//
//==================================================
// <GET:1><module name:S><debug file:S><debug identifier:S><module base address:8><instruction address:8>
// example: <GET><0x8>test.dll<0x8>test.pdb<0x9>0123456789<0x0000000000010000><0x0000000000011A2B>
//
// Look up source line info for this module, loaded at this base address,
// for the code at this instruction address.
//
// Server Response:
// <function:S><func_base:8><source_file:S><source_line:4><source_line_base:8>
//  - As many fields as available are filled in. Fields that are not available
//    will contain an empty string, or a zero for numeric values.

//==================================================
// <GETSTACKWIN:1><module name:S><debug file:S><debug identifier:S><module base address:8><instruction address:8>
// example: <GETSTACKWIN><0x8>test.dll<0x8>test.pdb<0x9>0123456789<0x0000000000010000><0x0000000000011A2B>
//
// Look up Windows stack frame info for this module, loaded at this base
// address, for the code at this instruction address.
//
// Server Response:
// <stack:S>
//    The stack info is formatted as in the symbol file format, with
//    "STACK " omitted, as documented at:
//    http://code.google.com/p/google-breakpad/wiki/SymbolFiles
//    If no Windows stack frame info is available, an empty string is returned.

//==================================================
// <GETSTACKCFI:1><module name:S><debug file:S><debug identifier:S><module base address:8><instruction address:8>
// example: <GETSTACKCFI><0x8>test.dll<0x8>test.pdb<0x9>0123456789<0x0000000000010000><0x0000000000011A2B>
//
// Look up CFI stack frame info for this module, loaded at this base
// address, for the code at this instruction address.
//
// Server Response:
// <stack:S>
//    The stack info is formatted as in the symbol file format, with
//    "STACK " omitted, as documented at:
//    http://code.google.com/p/google-breakpad/wiki/SymbolFiles
//    If no CFI stack frame info is available, an empty string is returned.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_PROTOCOL_H_
#define GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_PROTOCOL_H_

#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {
namespace source_line_protocol {

// Response status codes
const u_int8_t OK    = 1;
const u_int8_t ERROR = 0;

// Commands
const u_int8_t HAS          = 1;
const u_int8_t LOAD         = 2;
const u_int8_t GET          = 3;
const u_int8_t GETSTACKWIN  = 4;
const u_int8_t GETSTACKCFI  = 5;

// HAS responses
const u_int8_t MODULE_NOT_LOADED  = 0;
const u_int8_t MODULE_LOADED      = 1;

// LOAD responses
const u_int8_t LOAD_NOT_FOUND = 0;
const u_int8_t LOAD_INTERRUPT = 1;
const u_int8_t LOAD_FAIL      = 2;
const u_int8_t LOAD_OK        = 3;

}  // namespace source_line_protocol
}  // namespace google_breakpad
#endif  // GOOGLE_BREAKPAD_PROCESSOR_NETWORK_SOURCE_LINE_PROTOCOL_H_
