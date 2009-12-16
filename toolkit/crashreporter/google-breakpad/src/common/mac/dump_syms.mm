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

// dump_syms.mm: Create a symbol file for use with minidumps

#include <unistd.h>
#include <signal.h>
#include <cxxabi.h>
#include <stdlib.h>

#include <mach/machine.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <fcntl.h>

#import <Foundation/Foundation.h>

#import "dump_syms.h"
#import "common/mac/file_id.h"
#import "common/mac/macho_utilities.h"
#import "common/mac/dwarf/dwarf2reader.h"
#import "common/mac/dwarf/functioninfo.h"
#import "common/mac/dwarf/bytereader.h"

using google_breakpad::FileID;

static NSString *kAddressSymbolKey = @"symbol";
static NSString *kAddressConvertedSymbolKey = @"converted_symbol";
static NSString *kAddressSourceLineKey = @"line";
static NSString *kFunctionSizeKey = @"size";
static NSString *kFunctionFileKey = @"source_file";
static NSString *kHeaderBaseAddressKey = @"baseAddr";
static NSString *kHeaderSizeKey = @"size";
static NSString *kHeaderOffsetKey = @"offset";  // Offset to the header
static NSString *kHeaderIs64BitKey = @"is64";
static NSString *kHeaderCPUTypeKey = @"cpuType";

// The section for __TEXT, __text seems to be always 1.  This is useful
// for pruning out extraneous non-function symbols.
static const int kTextSection = 1;

namespace __gnu_cxx {
template<> 
  struct hash<std::string> {
    size_t operator()(const std::string& k) const {
      return hash< const char* >()( k.c_str() );
  }
};
}

// Dump FunctionMap to stdout.  Print address, function name, file
// name, line number, lowpc, and highpc if available.
void DumpFunctionMap(const dwarf2reader::FunctionMap function_map) {
  for (dwarf2reader::FunctionMap::const_iterator iter = function_map.begin();
       iter != function_map.end(); ++iter) {
    if (iter->second->name.empty()) {
      continue;
    }
    printf("%08llx: %s", iter->first,
	   iter->second->name.data());
    if (!iter->second->file.empty()) {
      printf(" - %s", iter->second->file.data());
      if (iter->second->line != 0) {
	printf(":%u", iter->second->line);
      }
    }
    if (iter->second->lowpc != 0 && iter->second->highpc != 0) {
      printf(" (%08llx - %08llx)\n",
	     iter->second->lowpc,
	     iter->second->highpc);
    }
  }
}


@interface DumpSymbols(PrivateMethods)
- (NSString *)convertCPlusPlusSymbol:(NSString *)symbol;
- (void)addFunction:(NSString *)name line:(int)line address:(uint64_t)address section:(int)section;
- (BOOL)processSymbolItem:(struct nlist_64 *)list stringTable:(char *)table;
- (BOOL)loadSymbolInfo:(void *)base offset:(uint32_t)offset;
- (BOOL)loadSymbolInfo64:(void *)base offset:(uint32_t)offset;
- (BOOL)loadSymbolInfoForArchitecture;
- (BOOL)loadDWARFSymbolInfo:(void *)base offset:(uint32_t)offset;
- (BOOL)loadSTABSSymbolInfo:(void *)base offset:(uint32_t)offset;
- (void)generateSectionDictionary:(struct mach_header*)header;
- (BOOL)loadHeader:(void *)base offset:(uint32_t)offset;
- (BOOL)loadHeader64:(void *)base offset:(uint32_t)offset;
- (BOOL)loadModuleInfo;
- (void)processDWARFLineNumberInfo:(dwarf2reader::LineMap*)line_map;
- (void)processDWARFFunctionInfo:(dwarf2reader::FunctionMap*)address_to_funcinfo;
- (void)processDWARFSourceFileInfo:(vector<dwarf2reader::SourceFileInfo>*) files;
- (BOOL)loadSymbolInfo:(void *)base offset:(uint32_t)offset;
- (dwarf2reader::SectionMap*)getSectionMapForArchitecture:(NSString*)architecture;
@end

@implementation DumpSymbols
//=============================================================================
- (NSString *)convertCPlusPlusSymbol:(NSString *)symbol {
  // __cxa_demangle will realloc this if needed
  char *buffer = (char *)malloc(1024);
  size_t buffer_size = 1024;
  int result;

  const char *sym = [symbol UTF8String];
  NSString *demangled = nil;
  buffer = abi::__cxa_demangle(sym, buffer, &buffer_size, &result);
  if (result == 0) {
    demangled = [NSString stringWithUTF8String:buffer];
  }
  free(buffer);
  return demangled;
}

//=============================================================================
- (void)addFunction:(NSString *)name line:(int)line address:(uint64_t)address section:(int)section {
  NSNumber *addressNum = [NSNumber numberWithUnsignedLongLong:address];

  if (!address)
    return;

  // If the function starts with "_Z" or "__Z" then demangle it.
  BOOL isCPP = NO;

  if ([name hasPrefix:@"__Z"]) {
    // Remove the leading underscore
    name = [name substringFromIndex:1];
    isCPP = YES;
  } else if ([name hasPrefix:@"_Z"]) {
    isCPP = YES;
  }

  // Filter out non-functions
  if ([name hasSuffix:@".eh"])
    return;

  if ([name hasSuffix:@"__func__"])
    return;

  if ([name hasSuffix:@"GCC_except_table"])
    return;

  if (isCPP) {
    // OBJCPP_MANGLING_HACK
    // There are cases where ObjC++ mangles up an ObjC name using quasi-C++ 
    // mangling:
    // @implementation Foozles + (void)barzles {
    //    static int Baz = 0;
    // } @end
    // gives you _ZZ18+[Foozles barzles]E3Baz
    // c++filt won't parse this properly, and will crash in certain cases. 
    // Logged as radar:
    // 5129938: c++filt does not deal with ObjC++ symbols
    // If 5129938 ever gets fixed, we can remove this, but for now this prevents
    // c++filt from attempting to demangle names it doesn't know how to handle.
    // This is with c++filt 2.16
    NSCharacterSet *objcppCharSet = [NSCharacterSet characterSetWithCharactersInString:@"-+[]: "];
    NSRange emptyRange = { NSNotFound, 0 };
    NSRange objcppRange = [name rangeOfCharacterFromSet:objcppCharSet];
    isCPP = NSEqualRanges(objcppRange, emptyRange);
  } else if ([name characterAtIndex:0] == '_') {
    // Remove the leading underscore
    name = [name substringFromIndex:1];
  }

  // If there's already an entry for this address, check and see if we can add
  // either the symbol, or a missing line #
  NSMutableDictionary *dict = [addresses_ objectForKey:addressNum];

  if (!dict) {
    dict = [[NSMutableDictionary alloc] init];
    [addresses_ setObject:dict forKey:addressNum];
    [dict release];
  }

  if (name && ![dict objectForKey:kAddressSymbolKey]) {
    [dict setObject:name forKey:kAddressSymbolKey];

    // only functions, not line number addresses
    [functionAddresses_ addObject:addressNum];
  }

  if (isCPP) {
    // try demangling
    NSString *demangled = [self convertCPlusPlusSymbol:name];
    if (demangled != nil)
      [dict setObject:demangled forKey:kAddressConvertedSymbolKey];
  }
  
  if (line && ![dict objectForKey:kAddressSourceLineKey])
    [dict setObject:[NSNumber numberWithUnsignedInt:line]
             forKey:kAddressSourceLineKey];

}

//=============================================================================
- (BOOL)processSymbolItem:(struct nlist_64 *)list stringTable:(char *)table {
  uint32_t n_strx = list->n_un.n_strx;
  BOOL result = NO;

  // We don't care about non-section specific information except function length
  if (list->n_sect == 0 && list->n_type != N_FUN )
    return NO;

  if (list->n_type == N_FUN) {
    if (list->n_sect != 0) {
      // we get the function address from the first N_FUN
      lastStartAddress_ = list->n_value;
    }
    else {
      // an N_FUN from section 0 may follow the initial N_FUN
      // giving us function length information
      NSMutableDictionary *dict = [addresses_ objectForKey:
        [NSNumber numberWithUnsignedLong:lastStartAddress_]];
      
      assert(dict);

      // only set the function size the first time
      // (sometimes multiple section 0 N_FUN entries appear!)
      if (![dict objectForKey:kFunctionSizeKey]) {
        [dict setObject:[NSNumber numberWithUnsignedLongLong:list->n_value]
                 forKey:kFunctionSizeKey];
      }
    }
  }
  
  int line = list->n_desc;
  
  // __TEXT __text section
  NSMutableDictionary *archSections = [sectionData_ objectForKey:architecture_];

  uint32_t mainSection = [[archSections objectForKey:@"__TEXT__text" ] sectionNumber];

  // Extract debugging information:
  // Doc: http://developer.apple.com/documentation/DeveloperTools/gdb/stabs/stabs_toc.html
  // Header: /usr/include/mach-o/stab.h:
  if (list->n_type == N_SO)  {
    NSString *src = [NSString stringWithUTF8String:&table[n_strx]];
    NSString *ext = [src pathExtension];
    NSNumber *address = [NSNumber numberWithUnsignedLongLong:list->n_value];

    // Leopard puts .c files with no code as an offset of 0, but a
    // crash can't happen here and it throws off our code that matches
    // symbols to line numbers so we ignore them..
    // Return YES because this isn't an error, just something we don't
    // care to handle.
    if ([address unsignedLongValue] == 0) {
      return YES;
    }
    // TODO(waylonis):Ensure that we get the full path for the source file
    // from the first N_SO record
    // If there is an extension, we'll consider it source code
    if ([ext length]) {
      if (!sources_)
        sources_ = [[NSMutableDictionary alloc] init];
      // Save the source associated with an address
      [sources_ setObject:src forKey:address];
      result = YES;
    }
  } else if (list->n_type == N_FUN) {
    NSString *fn = [NSString stringWithUTF8String:&table[n_strx]];
    NSRange range = [fn rangeOfString:@":" options:NSBackwardsSearch];

    if (![fn length])
      return NO;

    if (range.length > 0) {
      // The function has a ":" followed by some stuff, so strip it off
      fn = [fn substringToIndex:range.location];
    }
    
    [self addFunction:fn line:line address:list->n_value section:list->n_sect ];

    result = YES;
  } else if (list->n_type == N_SLINE && list->n_sect == mainSection) {
    [self addFunction:nil line:line address:list->n_value section:list->n_sect ];
    result = YES;
  } else if (((list->n_type & N_TYPE) == N_SECT) && !(list->n_type & N_STAB)) {
    // Regular symbols or ones that are external
    NSString *fn = [NSString stringWithUTF8String:&table[n_strx]];

    [self addFunction:fn line:0 address:list->n_value section:list->n_sect ];
    result = YES;
  }

  return result;
}

#define SwapLongLongIfNeeded(a) (swap ? NXSwapLongLong(a) : (a))
#define SwapLongIfNeeded(a) (swap ? NXSwapLong(a) : (a))
#define SwapIntIfNeeded(a) (swap ? NXSwapInt(a) : (a))
#define SwapShortIfNeeded(a) (swap ? NXSwapShort(a) : (a))

//=============================================================================
- (BOOL)loadSymbolInfo:(void *)base offset:(uint32_t)offset {
  NSMutableDictionary *archSections = [sectionData_ objectForKey:architecture_];
  if ([archSections objectForKey:@"__DWARF__debug_info"]) {
    // Treat this this as debug information
    return [self loadDWARFSymbolInfo:base offset:offset];
  }

  return [self loadSTABSSymbolInfo:base offset:offset];
}

//=============================================================================
- (BOOL)loadDWARFSymbolInfo:(void *)base offset:(uint32_t)offset {

  struct mach_header *header = (struct mach_header *) 
    ((uint32_t)base + offset);
  BOOL swap = (header->magic == MH_CIGAM);

  NSMutableDictionary *archSections = [sectionData_ objectForKey:architecture_];
  assert (archSections != nil);
  section *dbgInfoSection = [[archSections objectForKey:@"__DWARF__debug_info"] sectionPointer];
  uint32_t debugInfoSize = SwapLongIfNeeded(dbgInfoSection->size);

  // i think this will break if run on a big-endian machine
  dwarf2reader::ByteReader byte_reader(swap ?
                                       dwarf2reader::ENDIANNESS_BIG :
                                       dwarf2reader::ENDIANNESS_LITTLE);

  uint64_t dbgOffset = 0;

  dwarf2reader::SectionMap* oneArchitectureSectionMap = [self getSectionMapForArchitecture:architecture_];

  while (dbgOffset < debugInfoSize) {
    // Prepare necessary objects.
    dwarf2reader::FunctionMap off_to_funcinfo;
    dwarf2reader::FunctionMap address_to_funcinfo;
    dwarf2reader::LineMap line_map;
    vector<dwarf2reader::SourceFileInfo> files;
    vector<string> dirs;

    dwarf2reader::CULineInfoHandler line_info_handler(&files, &dirs,
						      &line_map);

    dwarf2reader::CUFunctionInfoHandler function_info_handler(&files, &dirs,
                                                              &line_map,
                                                              &off_to_funcinfo,
                                                              &address_to_funcinfo,
							      &line_info_handler,
                                                              *oneArchitectureSectionMap,
                                                              &byte_reader);

    dwarf2reader::CompilationUnit compilation_unit(*oneArchitectureSectionMap,
                                                   dbgOffset,
                                                   &byte_reader,
                                                   &function_info_handler);

    dbgOffset += compilation_unit.Start();

    // The next 3 functions take the info that the dwarf reader
    // gives and massages them into the data structures that
    // dump_syms uses 
    [self processDWARFSourceFileInfo:&files];
    [self processDWARFFunctionInfo:&address_to_funcinfo];
    [self processDWARFLineNumberInfo:&line_map];
  }

  return YES;
}

- (void)processDWARFSourceFileInfo:(vector<dwarf2reader::SourceFileInfo>*) files {
  if (!sources_)
    sources_ = [[NSMutableDictionary alloc] init];
  // Save the source associated with an address
  vector<dwarf2reader::SourceFileInfo>::const_iterator iter = files->begin();
  for (; iter != files->end(); iter++) {
    NSString *sourceFile = [NSString stringWithUTF8String:(*iter).name.c_str()];
    if ((*iter).lowpc != ULLONG_MAX) {
      NSNumber *address = [NSNumber numberWithUnsignedLongLong:(*iter).lowpc];
      if ([address unsignedLongLongValue] == 0) {
        continue;
      }
      [sources_ setObject:sourceFile forKey:address];
    }
  }
}
  
- (void)processDWARFFunctionInfo:(dwarf2reader::FunctionMap*)address_to_funcinfo {
  for (dwarf2reader::FunctionMap::const_iterator iter = address_to_funcinfo->begin();
       iter != address_to_funcinfo->end(); ++iter) {
    if (iter->second->name.empty()) {
      continue;
    }

    if (!addresses_)
      addresses_ = [[NSMutableDictionary alloc] init];

    NSNumber *addressNum = [NSNumber numberWithUnsignedLongLong:(*iter).second->lowpc];
	
    [functionAddresses_ addObject:addressNum];

    NSMutableDictionary *dict = [addresses_ objectForKey:addressNum];

    if (!dict) {
      dict = [[NSMutableDictionary alloc] init];
      [addresses_ setObject:dict forKey:addressNum];
      [dict release];
    }

    // set name of function if it isn't already set
    if (![dict objectForKey:kAddressSymbolKey]) {
      NSString *symbolName = [NSString stringWithUTF8String:iter->second->name.c_str()];
      [dict setObject:symbolName forKey:kAddressSymbolKey];
    }

    // try demangling function name if we have a mangled name
    if (![dict objectForKey:kAddressConvertedSymbolKey] &&
        !iter->second->mangled_name.empty()) {
      NSString *mangled = [NSString stringWithUTF8String:iter->second->mangled_name.c_str()];
      NSString *demangled = [self convertCPlusPlusSymbol:mangled];
      if (demangled != nil)
        [dict setObject:demangled forKey:kAddressConvertedSymbolKey];
    }
  
    // set line number for beginning of function
    if (iter->second->line && ![dict objectForKey:kAddressSourceLineKey])
      [dict setObject:[NSNumber numberWithUnsignedInt:iter->second->line]
	    forKey:kAddressSourceLineKey];

    // set function size by subtracting low PC from high PC
    if (![dict objectForKey:kFunctionSizeKey]) {
      [dict setObject:[NSNumber numberWithUnsignedLongLong:iter->second->highpc - iter->second->lowpc]
	    forKey:kFunctionSizeKey];
    }

    // Set the file that the function is in
    if (![dict objectForKey:kFunctionFileKey]) {
      [dict setObject:[NSString stringWithUTF8String:iter->second->file.c_str()]
               forKey:kFunctionFileKey];
    }
  }
}

- (void)processDWARFLineNumberInfo:(dwarf2reader::LineMap*)line_map {
  for (dwarf2reader::LineMap::const_iterator iter = line_map->begin();
       iter != line_map->end(); 
       ++iter) {

    NSNumber *addressNum = [NSNumber numberWithUnsignedLongLong:iter->first];
    NSMutableDictionary *dict = [addresses_ objectForKey:addressNum];

    if (!dict) {
      dict = [[NSMutableDictionary alloc] init];
      [addresses_ setObject:dict forKey:addressNum];
      [dict release];
    } 
	
    if (iter->second.second && ![dict objectForKey:kAddressSourceLineKey]) {
      [dict setObject:[NSNumber numberWithUnsignedInt:iter->second.second]
	    forKey:kAddressSourceLineKey];
    }

    // Set the file that the function's address is in
    if (![dict objectForKey:kFunctionFileKey]) {
      [dict setObject:[NSString stringWithUTF8String:iter->second.first.c_str()]
               forKey:kFunctionFileKey];
    }
  }
}

//=============================================================================
- (BOOL)loadSTABSSymbolInfo:(void *)base offset:(uint32_t)offset {
  struct mach_header *header = (struct mach_header *)((uint32_t)base + offset);
  BOOL swap = (header->magic == MH_CIGAM);
  uint32_t count = SwapLongIfNeeded(header->ncmds);
  struct load_command *cmd =
    (struct load_command *)((uint32_t)header + sizeof(struct mach_header));
  uint32_t symbolTableCommand = SwapLongIfNeeded(LC_SYMTAB);
  BOOL result = NO;

  if (!addresses_)
    addresses_ = [[NSMutableDictionary alloc] init];

  for (uint32_t i = 0; cmd && (i < count); ++i) {
    if (cmd->cmd == symbolTableCommand) {
      struct symtab_command *symtab = (struct symtab_command *)cmd;
      uint32_t ncmds = SwapLongIfNeeded(symtab->nsyms);
      uint32_t symoff = SwapLongIfNeeded(symtab->symoff);
      uint32_t stroff = SwapLongIfNeeded(symtab->stroff);
      struct nlist *list = (struct nlist *)((uint32_t)base + symoff + offset);
      char *strtab = ((char *)header + stroff);

      // Process each command, looking for debugging stuff
      for (uint32_t j = 0; j < ncmds; ++j, ++list) {
        // Fill in an nlist_64 structure and process with that
        struct nlist_64 nlist64;
        nlist64.n_un.n_strx = SwapLongIfNeeded(list->n_un.n_strx);
        nlist64.n_type = list->n_type;
        nlist64.n_sect = list->n_sect;
        nlist64.n_desc = SwapShortIfNeeded(list->n_desc);
        nlist64.n_value = (uint64_t)SwapLongIfNeeded(list->n_value);

        // TODO(nealsid): is this broken? we get NO if one symbol fails
        // but then we lose that information if another suceeeds
        if ([self processSymbolItem:&nlist64 stringTable:strtab])
          result = YES;
      }
    }

    uint32_t cmdSize = SwapLongIfNeeded(cmd->cmdsize);
    cmd = (struct load_command *)((uint32_t)cmd + cmdSize);
  }

  return result;
}

//=============================================================================
- (BOOL)loadSymbolInfo64:(void *)base offset:(uint32_t)offset {
  struct mach_header_64 *header = (struct mach_header_64 *)
    ((uint32_t)base + offset);
  BOOL swap = (header->magic == MH_CIGAM_64);
  uint32_t count = SwapLongIfNeeded(header->ncmds);
  struct load_command *cmd =
    (struct load_command *)((uint32_t)header + sizeof(struct mach_header));
  uint32_t symbolTableCommand = SwapLongIfNeeded(LC_SYMTAB);
  BOOL result = NO;

  for (uint32_t i = 0; cmd && (i < count); i++) {
    if (cmd->cmd == symbolTableCommand) {
      struct symtab_command *symtab = (struct symtab_command *)cmd;
      uint32_t ncmds = SwapLongIfNeeded(symtab->nsyms);
      uint32_t symoff = SwapLongIfNeeded(symtab->symoff);
      uint32_t stroff = SwapLongIfNeeded(symtab->stroff);
      struct nlist_64 *list = (struct nlist_64 *)((uint32_t)base + symoff);
      char *strtab = ((char *)header + stroff);

      // Process each command, looking for debugging stuff
      for (uint32_t j = 0; j < ncmds; ++j, ++list) {
        if (!(list->n_type & (N_STAB | N_TYPE)))
          continue;

        // Fill in an nlist_64 structure and process with that
        struct nlist_64 nlist64;
        nlist64.n_un.n_strx = SwapLongIfNeeded(list->n_un.n_strx);
        nlist64.n_type = list->n_type;
        nlist64.n_sect = list->n_sect;
        nlist64.n_desc = SwapShortIfNeeded(list->n_desc);
        nlist64.n_value = SwapLongLongIfNeeded(list->n_value);

        if ([self processSymbolItem:&nlist64 stringTable:strtab])
          result = YES;
      }
    }

    uint32_t cmdSize = SwapLongIfNeeded(cmd->cmdsize);
    cmd = (struct load_command *)((uint32_t)cmd + cmdSize);
  }

  return result;
}

//=============================================================================
- (BOOL)loadSymbolInfoForArchitecture {
  NSMutableData *data = [[NSMutableData alloc]
    initWithContentsOfMappedFile:sourcePath_];

  NSDictionary *headerInfo = [headers_ objectForKey:architecture_];
  void *base = [data mutableBytes];
  uint32_t offset =
    [[headerInfo objectForKey:kHeaderOffsetKey] unsignedLongValue];
  BOOL is64 = [[headerInfo objectForKey:kHeaderIs64BitKey] boolValue];
  BOOL result = is64 ? [self loadSymbolInfo64:base offset:offset] :
    [self loadSymbolInfo:base offset:offset];

  [data release];
  return result;
}

- (dwarf2reader::SectionMap*)getSectionMapForArchitecture:(NSString*)architecture {

  string currentArch([architecture UTF8String]);
  dwarf2reader::SectionMap *oneArchitectureSectionMap;

  ArchSectionMap::const_iterator iter = sectionsForArch_->find(currentArch);
  
  if (iter == sectionsForArch_->end()) {
    oneArchitectureSectionMap = new dwarf2reader::SectionMap();
    sectionsForArch_->insert(make_pair(currentArch, oneArchitectureSectionMap));
  } else {
    oneArchitectureSectionMap = iter->second;
  }
    
  return oneArchitectureSectionMap;
}

//=============================================================================
// build a dictionary of section numbers keyed off a string
// which is the concatenation of the segment name and the section name
- (void)generateSectionDictionary:(struct mach_header*)header {

  BOOL swap = (header->magic == MH_CIGAM);
  uint32_t count = SwapLongIfNeeded(header->ncmds);
  struct load_command *cmd =
    (struct load_command *)((uint32_t)header + sizeof(struct mach_header));
  uint32_t segmentCommand = SwapLongIfNeeded(LC_SEGMENT);
  uint32_t sectionNumber = 1;   // section numbers are counted from 1
  
  cpu_type_t cpu = SwapIntIfNeeded(header->cputype);

  NSString *arch;

  if (cpu & CPU_ARCH_ABI64)
    arch = ((cpu & ~CPU_ARCH_ABI64) == CPU_TYPE_X86) ?
      @"x86_64" : @"ppc64";
  else
    arch = (cpu == CPU_TYPE_X86) ? @"x86" : @"ppc";

  NSMutableDictionary *archSections;

  if (!sectionData_) {
    sectionData_ = [[NSMutableDictionary alloc] init];
  }
  
  if (![sectionData_ objectForKey:architecture_]) {
    [sectionData_ setObject:[[NSMutableDictionary alloc] init] forKey:arch];
  }

  archSections = [sectionData_ objectForKey:arch];

  dwarf2reader::SectionMap* oneArchitectureSectionMap = [self getSectionMapForArchitecture:arch];
  
  // loop through every segment command, then through every section
  // contained inside each of them
  for (uint32_t i = 0; cmd && (i < count); ++i) {
    if (cmd->cmd == segmentCommand) {            
      struct segment_command *seg = (struct segment_command *)cmd;
      section *sect = (section *)((uint32_t)cmd + sizeof(segment_command));
      uint32_t nsects = SwapLongIfNeeded(seg->nsects);
      
      for (uint32_t j = 0; j < nsects; ++j) {
        NSString *segSectName = [NSString stringWithFormat:@"%s%s",
          seg->segname, sect->sectname];
        
        [archSections setObject:[[MachSection alloc] initWithMachSection:sect andNumber:sectionNumber]
		      forKey:segSectName];

	// filter out sections with size 0, offset 0
	if (sect->offset != 0 && sect->size != 0) {
	  // fill sectionmap for dwarf reader
	  oneArchitectureSectionMap->insert(make_pair(sect->sectname,make_pair(((const char*)header) + SwapLongIfNeeded(sect->offset), (size_t)SwapLongIfNeeded(sect->size))));
	}

        ++sect;
        ++sectionNumber;
      }
    }

    uint32_t cmdSize = SwapLongIfNeeded(cmd->cmdsize);
    cmd = (struct load_command *)((uint32_t)cmd + cmdSize);
  }
}

//=============================================================================
- (BOOL)loadHeader:(void *)base offset:(uint32_t)offset {
  struct mach_header *header = (struct mach_header *)((uint32_t)base + offset);
  BOOL swap = (header->magic == MH_CIGAM);
  uint32_t count = SwapLongIfNeeded(header->ncmds);
  struct load_command *cmd =
    (struct load_command *)((uint32_t)header + sizeof(struct mach_header));
  uint32_t segmentCommand = SwapLongIfNeeded(LC_SEGMENT);

  [self generateSectionDictionary:header];

  for (uint32_t i = 0; cmd && (i < count); ++i) {
    if (cmd->cmd == segmentCommand) {
      struct segment_command *seg = (struct segment_command *)cmd;
      
      if (!strcmp(seg->segname, "__TEXT")) {
        uint32_t addr = SwapLongIfNeeded(seg->vmaddr);
        uint32_t size = SwapLongIfNeeded(seg->vmsize);
        cpu_type_t cpu = SwapIntIfNeeded(header->cputype);
        NSString *cpuStr = (cpu == CPU_TYPE_I386) ? @"x86" : @"ppc";

        [headers_ setObject:[NSDictionary dictionaryWithObjectsAndKeys:
          [NSNumber numberWithUnsignedLongLong:(uint64_t)addr],
          kHeaderBaseAddressKey,
          [NSNumber numberWithUnsignedLongLong:(uint64_t)size], kHeaderSizeKey,
          [NSNumber numberWithUnsignedLong:offset], kHeaderOffsetKey,
          [NSNumber numberWithBool:NO], kHeaderIs64BitKey,
          [NSNumber numberWithUnsignedLong:cpu], kHeaderCPUTypeKey,
          nil] forKey:cpuStr];

        return YES;
      }
    }

    uint32_t cmdSize = SwapLongIfNeeded(cmd->cmdsize);
    cmd = (struct load_command *)((uint32_t)cmd + cmdSize);
  }

  return NO;
}

//=============================================================================
- (BOOL)loadHeader64:(void *)base offset:(uint32_t)offset {
  struct mach_header_64 *header =
    (struct mach_header_64 *)((uint32_t)base + offset);
  BOOL swap = (header->magic == MH_CIGAM_64);
  uint32_t count = SwapLongIfNeeded(header->ncmds);
  struct load_command *cmd =
    (struct load_command *)((uint32_t)header + sizeof(struct mach_header_64));

  for (uint32_t i = 0; cmd && (i < count); ++i) {
    uint32_t segmentCommand = SwapLongIfNeeded(LC_SEGMENT_64);
    if (cmd->cmd == segmentCommand) {
      struct segment_command_64 *seg = (struct segment_command_64 *)cmd;
      if (!strcmp(seg->segname, "__TEXT")) {
        uint64_t addr = SwapLongLongIfNeeded(seg->vmaddr);
        uint64_t size = SwapLongLongIfNeeded(seg->vmsize);
        cpu_type_t cpu = SwapIntIfNeeded(header->cputype);
        cpu &= (~CPU_ARCH_ABI64);
        NSString *cpuStr = (cpu == CPU_TYPE_I386) ? @"x86_64" : @"ppc64";

        [headers_ setObject:[NSDictionary dictionaryWithObjectsAndKeys:
          [NSNumber numberWithUnsignedLongLong:addr], kHeaderBaseAddressKey,
          [NSNumber numberWithUnsignedLongLong:size], kHeaderSizeKey,
          [NSNumber numberWithUnsignedLong:offset], kHeaderOffsetKey,
          [NSNumber numberWithBool:YES], kHeaderIs64BitKey,
          [NSNumber numberWithUnsignedLong:cpu], kHeaderCPUTypeKey,
          nil] forKey:cpuStr];
        return YES;
      }
    }

    uint32_t cmdSize = SwapLongIfNeeded(cmd->cmdsize);
    cmd = (struct load_command *)((uint32_t)cmd + cmdSize);
  }

  return NO;
}

//=============================================================================
- (BOOL)loadModuleInfo {
  uint64_t result = 0;
  NSMutableData *data = [[NSMutableData alloc]
    initWithContentsOfMappedFile:sourcePath_];
  void *bytes = [data mutableBytes];
  struct fat_header *fat = (struct fat_header *)bytes;

  if (!fat) {
    [data release];
    return 0;
  }

  // Gather some information based on the header
  BOOL isFat = fat->magic == FAT_MAGIC || fat->magic == FAT_CIGAM;
  BOOL is64 = fat->magic == MH_MAGIC_64 || fat->magic == MH_CIGAM_64;
  BOOL is32 = fat->magic == MH_MAGIC || fat->magic == MH_CIGAM;
  BOOL swap = fat->magic == FAT_CIGAM || fat->magic == MH_CIGAM_64 ||
    fat->magic == MH_CIGAM;

  if (!is64 && !is32 && !isFat) {
    [data release];
    return 0;
  }

  // Load any available architectures and save the information
  headers_ = [[NSMutableDictionary alloc] init];

  if (isFat) {
    struct fat_arch *archs =
      (struct fat_arch *)((uint32_t)fat + sizeof(struct fat_header));
    uint32_t count = SwapLongIfNeeded(fat->nfat_arch);

    for (uint32_t i = 0; i < count; ++i) {
      archs[i].cputype = SwapIntIfNeeded(archs[i].cputype);
      archs[i].cpusubtype = SwapIntIfNeeded(archs[i].cpusubtype);
      archs[i].offset = SwapLongIfNeeded(archs[i].offset);
      archs[i].size = SwapLongIfNeeded(archs[i].size);
      archs[i].align = SwapLongIfNeeded(archs[i].align);

      if (archs[i].cputype & CPU_ARCH_ABI64)
        result = [self loadHeader64:bytes offset:archs[i].offset];
      else
        result = [self loadHeader:bytes offset:archs[i].offset];
    }
  } else if (is32) {
    result = [self loadHeader:bytes offset:0];
  } else {
    result = [self loadHeader64:bytes offset:0];
  }

  [data release];
  return result;
}

//=============================================================================
static BOOL WriteFormat(int fd, const char *fmt, ...) {
  va_list list;
  char buffer[4096];
  ssize_t expected, written;

  va_start(list, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, list);
  expected = strlen(buffer);
  written = write(fd, buffer, strlen(buffer));
  va_end(list);

  return expected == written;
}

//=============================================================================
- (BOOL)outputSymbolFile:(int)fd {
  // Get the baseAddress for this architecture
  NSDictionary *archDict = [headers_ objectForKey:architecture_];
  NSNumber *baseAddressNum = [archDict objectForKey:kHeaderBaseAddressKey];
  uint64_t baseAddress =
    baseAddressNum ? [baseAddressNum unsignedLongLongValue] : 0;
  NSNumber *moduleSizeNum = [archDict objectForKey:kHeaderSizeKey];
  uint64_t moduleSize =
    moduleSizeNum ? [moduleSizeNum unsignedLongLongValue] : 0;

  // UUID
  FileID file_id([sourcePath_ fileSystemRepresentation]);
  unsigned char identifier[16];
  char identifierStr[40];
  const char *moduleName = [[sourcePath_ lastPathComponent] UTF8String];
  int cpu_type = [[archDict objectForKey:kHeaderCPUTypeKey] unsignedLongValue];
  if (file_id.MachoIdentifier(cpu_type, identifier)) {
    FileID::ConvertIdentifierToString(identifier, identifierStr,
                                      sizeof(identifierStr));
  }
  else {
    fprintf(stderr, "Unable to calculate UUID of mach-o binary!\n");
    return NO;
  }

  // keep track exclusively of function addresses
  // for sanity checking function lengths
  functionAddresses_ = [[NSMutableSet alloc] init];

  // Gather the information
  [self loadSymbolInfoForArchitecture];

  NSArray *sortedAddresses = [[addresses_ allKeys]
    sortedArrayUsingSelector:@selector(compare:)];

  NSArray *sortedFunctionAddresses = [[functionAddresses_ allObjects]
    sortedArrayUsingSelector:@selector(compare:)];

  // position ourselves at the 2nd function
  unsigned int funcIndex = 1;

  // Remove the dashes from the string
  NSMutableString *compactedStr =
    [NSMutableString stringWithCString:identifierStr encoding:NSASCIIStringEncoding];
  [compactedStr replaceOccurrencesOfString:@"-" withString:@"" options:0
                                     range:NSMakeRange(0, [compactedStr length])];

  if (!WriteFormat(fd, "MODULE mac %s %s0 %s\n", [architecture_ UTF8String],
                   [compactedStr UTF8String], moduleName)) {
    return NO;
  }

  // Sources ordered by address
  NSArray *sources = [[sources_ allKeys]
    sortedArrayUsingSelector:@selector(compare:)];
  NSMutableDictionary *fileNameToFileIndex = [[NSMutableDictionary alloc] init];
  unsigned int sourceCount = [sources count];
  for (unsigned int i = 0; i < sourceCount; ++i) {
    NSString *file = [sources_ objectForKey:[sources objectAtIndex:i]];
    if (!WriteFormat(fd, "FILE %d %s\n", i + 1, [file UTF8String]))
      return NO;

    [fileNameToFileIndex setObject:[NSNumber numberWithUnsignedInt:i+1]
                            forKey:file];
  }

  // Symbols
  char terminatingChar = '\n';
  uint32_t fileIdx = 0, nextFileIdx = 0;
  uint64_t nextSourceFileAddress = 0;
  NSNumber *nextAddress;
  uint64_t nextAddressVal;
  unsigned int addressCount = [sortedAddresses count];
  
  bool insideFunction = false;
  
  for (unsigned int i = 0; i < addressCount; ++i) {
    NSNumber *address = [sortedAddresses objectAtIndex:i];
    // skip sources that have a starting address of 0
    if ([address unsignedLongValue] == 0) {
      continue;
    }

    uint64_t addressVal = [address unsignedLongLongValue] - baseAddress;

    // Get the next address to calculate the length
    if (i + 1 < addressCount) {
      nextAddress = [sortedAddresses objectAtIndex:i + 1];
      nextAddressVal = [nextAddress unsignedLongLongValue] - baseAddress;
    } else {
      nextAddressVal = baseAddress + moduleSize;
      // The symbol reader doesn't want a trailing newline
      terminatingChar = '\0';
    }
    
    NSDictionary *dict = [addresses_ objectForKey:address];
    NSNumber *line = [dict objectForKey:kAddressSourceLineKey];
    NSString *symbol = [dict objectForKey:kAddressConvertedSymbolKey];

    if (!symbol)
      symbol = [dict objectForKey:kAddressSymbolKey];

    // sanity check the function length by making sure it doesn't
    // run beyond the next function entry
    uint64_t nextFunctionAddress = 0;
    if (symbol && funcIndex < [sortedFunctionAddresses count]) {
      nextFunctionAddress = [[sortedFunctionAddresses objectAtIndex:funcIndex]
        unsignedLongLongValue] - baseAddress;
      ++funcIndex;
    }

    // Skip some symbols
    if ([symbol hasPrefix:@"vtable for"])
      continue;

    if ([symbol hasPrefix:@"__static_initialization_and_destruction_0"])
      continue;

    if ([symbol hasPrefix:@"_GLOBAL__I_"])
      continue;

    if ([symbol hasPrefix:@"__func__."])
      continue;

    if ([symbol hasPrefix:@"__gnu"])
      continue;

    if ([symbol hasPrefix:@"typeinfo "])
      continue;

    if ([symbol hasPrefix:@"EH_frame"])
      continue;

    if ([symbol hasPrefix:@"GCC_except_table"])
      continue;

    if ([symbol hasPrefix:@"__tcf"])
      continue;

    if ([symbol hasPrefix:@"non-virtual thunk"])
      continue;

    // Find the source file (if any) that contains this address
    while (sourceCount && (addressVal >= nextSourceFileAddress)) {
      fileIdx = nextFileIdx;

      if (nextFileIdx < sourceCount) {
        NSNumber *addr = [sources objectAtIndex:nextFileIdx];
        ++nextFileIdx;
        nextSourceFileAddress = [addr unsignedLongLongValue] - baseAddress;
      } else {
        nextSourceFileAddress = baseAddress + moduleSize;
        break;
      }
    }

    NSNumber *functionLength = [dict objectForKey:kFunctionSizeKey];

    if (line) {
      if (symbol && functionLength) {

        uint64_t functionLengthVal = [functionLength unsignedLongLongValue];

        insideFunction = true;
        // sanity check to make sure the length we were told does not exceed
        // the space between this function and the next
        if (nextFunctionAddress != 0) {
          uint64_t functionLengthVal2 = nextFunctionAddress - addressVal;

          if(functionLengthVal > functionLengthVal2 ) {
            functionLengthVal = functionLengthVal2;
          }
        }

        // Function
        if (!WriteFormat(fd, "FUNC %llx %llx 0 %s\n", addressVal,
                         functionLengthVal, [symbol UTF8String]))
          return NO;
      }

      // Throw out line number information that doesn't correspond to
      // any function
      if (insideFunction) {
        // Source line
        uint64_t length = nextAddressVal - addressVal;

        // if fileNameToFileIndex/dict has an entry for the
        // file/kFunctionFileKey, we're processing DWARF and have stored
        // files for each program counter.  If there is no entry, we're
        // processing STABS and can use the old method of mapping
        // addresses to files(which was basically iterating over a set
        // of addresses until we reached one that was greater than the
        // high PC of the current file, then moving on to the next file)
        NSNumber *fileIndex = [fileNameToFileIndex objectForKey:[dict objectForKey:kFunctionFileKey]];
        if (!WriteFormat(fd, "%llx %llx %d %d\n", addressVal, length,
                         [line unsignedIntValue], fileIndex ? [fileIndex unsignedIntValue] : fileIdx))
          return NO;
      } 
    } else {
      // PUBLIC <address> <stack-size> <name>
      if (!WriteFormat(fd, "PUBLIC %llx 0 %s\n", addressVal,
                       [symbol UTF8String]))
        return NO;
      insideFunction = false;
    }
  }

  return YES;
}

//=============================================================================
- (id)initWithContentsOfFile:(NSString *)path {
  if ((self = [super init])) {

    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
      [self autorelease];
      return nil;
    }

    sourcePath_ = [path copy];

    // Test for .DSYM bundle
    NSBundle *dsymBundle = [NSBundle bundleWithPath:sourcePath_];

    if (dsymBundle) {

      // we need to take the DSYM bundle path and remove it's
      // extension to get the name of the file inside the resources
      // directory of the bundle that actually has the DWARF
      // information
      // But, Xcode supports something called "Wrapper extension"(see
      // build settings), which would make the bundle name
      // /tmp/foo/test.kext.dSYM, but the dwarf binary name would
      // still be "test".  so, now we loop through until deleting the
      // extension doesn't change the string

      // e.g. suppose sourcepath_ is /tmp/foo/test.dSYM

      NSString *dwarfBinName = [sourcePath_ lastPathComponent];
      NSString *dwarfBinPath;

      // We use a do/while loop so we can handle files without an extension
      do {
        dwarfBinName = [dwarfBinName stringByDeletingPathExtension];
        // now, dwarfBinName is "test"
        dwarfBinPath = [dsymBundle pathForResource:dwarfBinName ofType:nil inDirectory:@"DWARF"];
        if (dwarfBinPath != nil)
          break;
      } while (![[dwarfBinName stringByDeletingPathExtension] isEqualToString:dwarfBinName]);

      if (dwarfBinPath == nil) {
        NSLog(@"The bundle passed on the command line does not appear to be a DWARF dSYM bundle");
        [self autorelease];
        return nil;
      }

      // otherwise we're good to go
      [sourcePath_ release];

      sourcePath_ = [dwarfBinPath copy];
      NSLog(@"Loading DWARF dSYM file from %@", sourcePath_);
    }

    sectionsForArch_ = new ArchSectionMap();

    if (![self loadModuleInfo]) {
      [self autorelease];
      return nil;
    }

    // If there's more than one, use the native one
    if ([headers_ count] > 1) {
      const NXArchInfo *localArchInfo = NXGetLocalArchInfo();

      if (localArchInfo) {
        cpu_type_t cpu = localArchInfo->cputype;
        NSString *arch;

        if (cpu & CPU_ARCH_ABI64)
          arch = ((cpu & ~CPU_ARCH_ABI64) == CPU_TYPE_X86) ?
            @"x86_64" : @"ppc64";
        else
          arch = (cpu == CPU_TYPE_X86) ? @"x86" : @"ppc";

        [self setArchitecture:arch];
      }
    } else {
      // Specify the default architecture
      [self setArchitecture:[[headers_ allKeys] objectAtIndex:0]];
    }
  }

  return self;
}

//=============================================================================
- (NSArray *)availableArchitectures {
  return [headers_ allKeys];
}

//=============================================================================
- (void)dealloc {
  [sourcePath_ release];
  [architecture_ release];
  [addresses_ release];
  [functionAddresses_ release];
  [sources_ release];
  [headers_ release];
  delete sectionsForArch_;

  [super dealloc];
}

//=============================================================================
- (BOOL)setArchitecture:(NSString *)architecture {
  NSString *normalized = [architecture lowercaseString];
  BOOL isValid = NO;

  if ([normalized isEqualToString:@"ppc"]) {
    isValid = YES;
  }
  else if ([normalized isEqualToString:@"i386"]) {
    normalized = @"x86";
    isValid = YES;
  }
  else if ([normalized isEqualToString:@"x86"]) {
    isValid = YES;
  }
  else if ([normalized isEqualToString:@"ppc64"]) {
    isValid = YES;
  }
  else if ([normalized isEqualToString:@"x86_64"]) {
    isValid = YES;
  }

  if (isValid) {
    if (![headers_ objectForKey:normalized])
      return NO;

    [architecture_ autorelease];
    architecture_ = [normalized copy];
  }

  return isValid;
}

//=============================================================================
- (NSString *)architecture {
  return architecture_;
}

//=============================================================================
- (BOOL)writeSymbolFile:(NSString *)destinationPath {
  const char *dest = [destinationPath fileSystemRepresentation];
  int fd;

  if ([[destinationPath substringToIndex:1] isEqualToString:@"-"])
    fd = STDOUT_FILENO;
  else
    fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (fd == -1)
    return NO;

  BOOL result = [self outputSymbolFile:fd];

  close(fd);

  return result;
}

@end

@implementation MachSection 

- (id)initWithMachSection:(section *)sect andNumber:(uint32_t)sectionNumber {
  if ((self = [super init])) {
    sect_ = sect;
    sectionNumber_ = sectionNumber;
  }

  return self;
}

- (section*)sectionPointer {
  return sect_;
}

- (uint32_t)sectionNumber {
  return sectionNumber_;
}
@end
