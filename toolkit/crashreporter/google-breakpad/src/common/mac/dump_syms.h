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

// dump_syms.h: Interface for DumpSymbols.  This class will take a mach-o file
// and extract the symbol information and write it to a file using the
// breakpad symbol file format.  
// NOTE: Only Stabs format is currently supported -- not DWARF.

#import <Foundation/Foundation.h>

@interface DumpSymbols : NSObject {
 @protected
  NSString *sourcePath_;              // Source of symbols (STRONG)
  NSString *architecture_;            // Architecture to extract (STRONG)
  NSMutableDictionary *addresses_;    // Addresses and symbols (STRONG)
  NSMutableDictionary *sources_;      // Address and Source file paths (STRONG)
  NSMutableArray *cppAddresses_;      // Addresses of C++ symbols (STRONG)
  NSMutableDictionary *headers_;      // Mach-o header information (STRONG)
  NSMutableDictionary *lastFunctionStartDict_; // Keyed by section# (STRONG)
  NSMutableDictionary *sectionNumbers_; // Keyed by seg/sect name (STRONG)
}

- (id)initWithContentsOfFile:(NSString *)machoFile;

- (NSArray *)availableArchitectures;

// One of ppc, x86, i386, ppc64, x86_64
// If the architecture is not available, it will return NO
// If not set, the native architecture will be used
- (BOOL)setArchitecture:(NSString *)architecture;
- (NSString *)architecture;

// Write the symbols to |symbolFilePath|.  Return YES if successful.
- (BOOL)writeSymbolFile:(NSString *)symbolFilePath;

@end
