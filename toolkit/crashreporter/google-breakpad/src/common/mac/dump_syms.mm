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

using google_breakpad::FileID;

static NSString *kAddressSymbolKey = @"symbol";
static NSString *kAddressConvertedSymbolKey = @"converted_symbol";
static NSString *kAddressSourceLineKey = @"line";
static NSString *kFunctionSizeKey = @"size";
static NSString *kHeaderBaseAddressKey = @"baseAddr";
static NSString *kHeaderSizeKey = @"size";
static NSString *kHeaderOffsetKey = @"offset";  // Offset to the header
static NSString *kHeaderIs64BitKey = @"is64";
static NSString *kHeaderCPUTypeKey = @"cpuType";
static NSString *kUnknownSymbol = @"???";

// The section for __TEXT, __text seems to be always 1.  This is useful
// for pruning out extraneous non-function symbols.
static const int kTextSection = 1;

@interface DumpSymbols(PrivateMethods)
- (NSArray *)convertCPlusPlusSymbols:(NSArray *)symbols;
- (void)convertSymbols;
- (void)addFunction:(NSString *)name line:(int)line address:(uint64_t)address section:(int)section;
- (BOOL)processSymbolItem:(struct nlist_64 *)list stringTable:(char *)table;
- (BOOL)loadSymbolInfo:(void *)base offset:(uint32_t)offset;
- (BOOL)loadSymbolInfo64:(void *)base offset:(uint32_t)offset;
- (BOOL)loadSymbolInfoForArchitecture;
- (void)generateSectionDictionary:(struct mach_header*)header;
- (BOOL)loadHeader:(void *)base offset:(uint32_t)offset;
- (BOOL)loadHeader64:(void *)base offset:(uint32_t)offset;
- (BOOL)loadModuleInfo;
@end

@implementation DumpSymbols
//=============================================================================
- (NSArray *)convertCPlusPlusSymbols:(NSArray *)symbols {
  NSMutableArray *symbols_demangled = [[NSMutableArray alloc]
					initWithCapacity:[symbols count]];
  // __cxa_demangle will realloc this if needed
  char *buffer = (char *)malloc(1024);
  size_t buffer_size = 1024;
  int result;

  NSEnumerator *enumerator = [symbols objectEnumerator];
  id symbolObject;
  while ((symbolObject = [enumerator nextObject])) {
    const char *symbol = [symbolObject UTF8String];
    buffer = abi::__cxa_demangle(symbol, buffer, &buffer_size, &result);
    if (result == 0) {
      [symbols_demangled addObject:[NSString stringWithUTF8String:buffer]];
    } else {
      // unable to demangle - use mangled name instead
      [symbols_demangled addObject:symbolObject];
    }
  }
  free(buffer);

  return symbols_demangled;
}

//=============================================================================
- (void)convertSymbols {
  unsigned int count = [cppAddresses_ count];
  NSMutableArray *symbols = [[NSMutableArray alloc] initWithCapacity:count];

  // Sort addresses for processing
  NSArray *addresses = [cppAddresses_ sortedArrayUsingSelector:
    @selector(compare:)];

  for (unsigned int i = 0; i < count; ++i) {
    NSMutableDictionary *dict = [addresses_ objectForKey:
      [addresses objectAtIndex:i]];
    NSString *symbol = [dict objectForKey:kAddressSymbolKey];

    // Make sure that the symbol is valid
    if ([symbol length] < 1)
      symbol = kUnknownSymbol;

    [symbols addObject:symbol];
  }

  // In order to deal with crashing problems in c++filt, we setup
  // a while loop to handle the case where convertCPlusPlusSymbols
  // only returns partial results.
  // We then attempt to continue from the point where c++filt failed
  // and add the partial results to the total results until we're
  // completely done.
  
  unsigned int totalIndex = 0;
  unsigned int totalCount = count;
  
  while (totalIndex < totalCount) {
    NSRange range = NSMakeRange(totalIndex, totalCount - totalIndex);
    NSArray *subarray = [symbols subarrayWithRange:range];
    NSArray *converted = [self convertCPlusPlusSymbols:subarray];
    unsigned int convertedCount = [converted count];

    if (convertedCount == 0) {
      break; // we give up at this point
    }
    
    for (unsigned int convertedIndex = 0;
      convertedIndex < convertedCount && totalIndex < totalCount;
       ++totalIndex, ++convertedIndex) {
      NSMutableDictionary *dict = [addresses_ objectForKey:
        [addresses objectAtIndex:totalIndex]];
      NSString *symbol = [converted objectAtIndex:convertedIndex];

      // Only add if this is a non-zero length symbol
      if ([symbol length])
        [dict setObject:symbol forKey:kAddressConvertedSymbolKey];
    }
  }
  
  [symbols release];
}

//=============================================================================
- (void)addFunction:(NSString *)name line:(int)line address:(uint64_t)address section:(int)section {
  NSNumber *addressNum = [NSNumber numberWithUnsignedLongLong:address];

  if (!address)
    return;

  // If the function starts with "_Z" or "__Z" then add it to the list of
  // addresses to run through the c++filt
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
  }
  
  if (isCPP) {
    if (!cppAddresses_)
      cppAddresses_ = [[NSMutableArray alloc] init];
    [cppAddresses_ addObject:addressNum];
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
  uint32_t mainSection = [[sectionNumbers_ objectForKey:@"__TEXT__text" ] unsignedLongValue];

  // Extract debugging information:
  // Doc: http://developer.apple.com/documentation/DeveloperTools/gdb/stabs/stabs_toc.html
  // Header: /usr/include/mach-o/stab.h:
  if (list->n_type == N_SO)  {
    NSString *src = [NSString stringWithUTF8String:&table[n_strx]];
    NSString *ext = [src pathExtension];
    NSNumber *address = [NSNumber numberWithUnsignedLongLong:list->n_value];

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
  } else if (list->n_type == N_SLINE && list->n_sect == mainSection ) {
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
  
  if (!sectionNumbers_)
    sectionNumbers_ = [[NSMutableDictionary alloc] init];
  
  // loop through every segment command, then through every section
  // contained inside each of them
  for (uint32_t i = 0; cmd && (i < count); ++i) {
    if (cmd->cmd == segmentCommand) {            
      struct segment_command *seg = (struct segment_command *)cmd;
      section *sect = (section *)((uint32_t)cmd + sizeof(segment_command));
      uint32_t nsects = SwapLongIfNeeded(seg->nsects);
      
      for (uint32_t j = 0; j < nsects; ++j) {
        //printf("%d: %s %s\n", sectionNumber, seg->segname, sect->sectname );
        NSString *segSectName = [NSString stringWithFormat:@"%s%s",
          seg->segname, sect->sectname ];
        
        [sectionNumbers_ setValue:[NSNumber numberWithUnsignedLong:sectionNumber]
          forKey:segSectName ];
        
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
  [self convertSymbols];

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
  unsigned int sourceCount = [sources count];
  for (unsigned int i = 0; i < sourceCount; ++i) {
    NSString *file = [sources_ objectForKey:[sources objectAtIndex:i]];
    if (!WriteFormat(fd, "FILE %d %s\n", i + 1, [file UTF8String]))
      return NO;
  }

  // Symbols
  char terminatingChar = '\n';
  uint32_t fileIdx = 0, nextFileIdx = 0;
  uint64_t nextSourceFileAddress = 0;
  NSNumber *nextAddress;
  uint64_t nextAddressVal;
  unsigned int addressCount = [sortedAddresses count];

  for (unsigned int i = 0; i < addressCount; ++i) {
    NSNumber *address = [sortedAddresses objectAtIndex:i];
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

    if ([symbol hasPrefix:@"_GLOBAL__I__"])
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

      // Source line
      uint64_t length = nextAddressVal - addressVal;
      if (!WriteFormat(fd, "%llx %llx %d %d\n", addressVal, length,
                       [line unsignedIntValue], fileIdx))
        return NO;
    } else {
      // PUBLIC <address> <stack-size> <name>
      if (!WriteFormat(fd, "PUBLIC %llx 0 %s\n", addressVal,
                       [symbol UTF8String]))
        return NO;
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
    architecture_ = [architecture copy];
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
