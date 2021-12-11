# Changelog
All notable changes to this project will be documented in this file.

Before 1.0, this project does not adhere to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

Goblin is now 0.1, which means we will try our best to ease breaking changes. Tracking issue is here: https://github.com/m4b/goblin/issues/97

## [0.1.3] - 2019-12-28
### Removed
- alloc feature, stabilized in 1.36 @philipc https://github.com/m4b/goblin/pull/196
### Added
elf: support empty PT_DYNAMIC references, @jan-auer https://github.com/m4b/goblin/pull/193
elf: move various elf::Sym impls out of alloc gate, @lzutao https://github.com/m4b/goblin/pull/198
### Fixed
elf: parsing 0 section header had regression introduced in 779d0ce, fixed by @philipc https://github.com/m4b/goblin/pull/200

## [0.1.2] - 2019-12-02
### Fixed
mach: don't return data for zerofill sections, @philipc https://github.com/m4b/goblin/pull/195

## [0.1.1] - 2019-11-10
### Fixed
elf: Don't fail entire elf parse when interpreter is malformed string, @jsgf https://github.com/m4b/goblin/pull/192

## [0.1.0] - 2019-11-3
### Added
- update to scroll 0.10 api
### Changed
- BREAKING: rename export to lib in Reexport::DLLOrdinal from @lzybkr
- pe: only parse ExceptionData for machine X86_64, thanks @wyxloading
### Fixed
pe: Fix resolution of redirect unwind info, thanks @jan-auer https://github.com/m4b/goblin/pull/183
pe: fix reexport dll and ordinal, thanks @lzybkr: d62889f469846af0cceb789b415f1e14f5f9e402

## [0.0.24] - 2019-7-13
### Added
- archive: new public enum type to determine which kind of archive was parsed
### Fixed
- archive: thanks @raindev
    * fix parsing of windows style archives: https://github.com/m4b/goblin/pull/174
    * stricter parsing of archives with multiple indexes: https://github.com/m4b/goblin/pull/175

## [0.0.23] - 2019-6-30
### Added
- pe: add write support for COFF object files!!! This is huge; we now support at a basic level writing out all major binary object formats, thanks @philipc: https://github.com/m4b/goblin/pull/159
- elf: add more e_ident constants
- mach: add segment protection constants
- elf: add risc-v relocation constants
- elf: add constants for arm64_32 (ILP32 ABI on 64-bit arm)
- pe: coff relocations and other auxiliary symbol records

### Fixed
- mach: fix 0 length data sections in mach-o segments, seen in some object files, thanks @raindev: https://github.com/m4b/goblin/pull/172
- build: alloc build was fixed: https://github.com/m4b/goblin/pull/170
- pe: fix `set_name_offset` compilation for 32-bit: https://github.com/m4b/goblin/pull/163

## [0.0.22] - 2019-4-13
### Added
- Beautify debugging by using `debug_struct` in `Debug` implementation of many structs.
- PE: fix rva mask, thanks @wickawacka: https://github.com/m4b/goblin/pull/152
- PE: add PE exception tables, thanks @jan-auer: https://github.com/m4b/goblin/pull/136

### Changed
- Bump lowest Rust version to 1.31.1 and transition project to Rust 2018 edition.
- BREAKING: Rename module `goblin::elf::dyn` to `goblin::elf::dynamic` due to `dyn`
  become a keyword in Rust 2018 edition.
- BREAKING: Rename `mach::exports::SymbolKind::to_str(kind: SymbolKind)` -> `to_str(&self)`.
- BREAKING: Rename `strtab::Strtab::to_vec(self)` -> `to_vec(&self).`

### Removed
- BREAKING: `goblin::error::Error::description` would be removed. Use `to_string()` method instead.

### Fixed
- elf: handle some invalid sizes, thanks @philipc: https://github.com/m4b/goblin/pull/121

## [0.0.21] - 2019-2-21
### Added
- elf: add symbol visibility. thanks @pchickey: https://github.com/m4b/goblin/pull/119

## [0.0.20] - 2019-2-10
### Added
- elf: parse section header relocs even when not an object file. thanks @Techno-Coder: https://github.com/m4b/goblin/pull/118
- pe: make utils public, add better examples for data directory usage. thanks @Pzixel: https://github.com/m4b/goblin/pull/116

## [0.0.19] - 2018-10-23
### Added
- elf: fix regression when parsing dynamic symbols from some binaries, thanks @philipc: https://github.com/m4b/goblin/issues/111

## [0.0.18] - 2018-10-14
### Changed
 - BREAKING: updated required compiler to 1.20 (due to scroll 1.20 requirement)
 - BREAKING: elf: removed bias field, as it was misleading/useless/incorrect
 - BREAKING: elf: add lazy relocation iterators: Thanks @ibabushkin https://github.com/m4b/goblin/pull/102
 - BREAKING: mach: remove repr(packed) from dylib and fvmlib (this should not affect anyone): https://github.com/m4b/goblin/issues/105
### Added
 - elf: use gnu/sysv hash table to compute sizeof dynsyms more accurately: again _huge_ thanks to @philipc https://github.com/m4b/goblin/pull/109
 - elf: handle multiple load biases: _huge_ thanks @philipc: https://github.com/m4b/goblin/pull/107
 - mach: add arm64e constants: Thanks @mitsuhiko https://github.com/m4b/goblin/pull/103
 - PE: calculate read bytes using alignment: Thanks @tathanhdinh https://github.com/m4b/goblin/pull/101
 - PE: get proper names for PE sections: Thanks @roblabla https://github.com/m4b/goblin/pull/100

## [0.0.17] - 2018-7-16
### Changed
 - BREAKING: updated required compiler to 1.19 (technically only required for tests, but assume this is required for building as well)
 - fixed nightly alloc api issues: https://github.com/m4b/goblin/issues/94

## [0.0.16] - 2018-7-14
### Changed
 - BREAKING: pe.export: name is now optional to reflect realities of PE parsing, and add more robustness to parser. many thanks to @tathanhdinh! https://github.com/m4b/goblin/pull/88
 - elf.note: treat alignment similar to other tools, e.g., readelf. Thanks @xcoldhandsx: https://github.com/m4b/goblin/pull/91
### Added
 - elf: more inline annotations on various methods, thanks@amanieu: https://github.com/m4b/goblin/pull/87

## [0.0.15] - 2018-4-22
### Changed
 - BREAKING: elf.reloc: u64/i64 used for r_offset/r_addend, and addend is now proper optional, thanks @amanieu! https://github.com/m4b/goblin/pull/86/
 - update to scroll 0.9
 - pe32+: parse better, thanks @kjempelodott, https://github.com/m4b/goblin/pull/82
### Added
 - mach: add constants for `n_types` when `N_STAB` field is being used, thanks @jrmuizel! https://github.com/m4b/goblin/pull/85
 - elf: implement support for compressed headers, thanks @rocallahan! https://github.com/m4b/goblin/pull/83
 - new nightly "alloc" feature: allows compiling the goblin parser on nightly with extern crate + no_std, thanks @philipc! https://github.com/m4b/goblin/pull/77
 - mach.segments: do not panic on bad internal data bounds: https://github.com/m4b/goblin/issues/74
 - mach: correctly add weak dylibs to import libs: https://github.com/m4b/goblin/issues/73

## [0.0.14] - 2018-1-15
### Changed
- BREAKING: elf: `iter_notes` renamed to `iter_note_headers`
- BREAKING: mach: remove `is_little_endian()`, `ctx()`, and `container()` methods from header, as they were completely invalid for big-endian architectures since the header was parsed according to the endianness of the binary correctly into memory, and hence would always report `MH_MAGIC` or `MH_MAGIC64` as the magic value.
- elf: courtesy of @jan-auer, note iterator now properly iterates over multiple PH_NOTEs
### Added
- mach: added hotly requested feature - goblin now has new functionality to parse big-endian, powerpc 32-bit mach-o binaries correctly
- mach: new function to correctly extract the parsing context for a mach-o binary, `parse_magic_and_ctx`
- elf: note iterator has new `iter_note_sections` method

## [0.0.13] - 2017-12-10
### Changed
- BREAKING: remove deprecated goblin::parse method
- BREAKING: ELF `to_range` removed on program and section headers; use `vm_range` and `file_range` for respective ranges
- Technically BREAKING: @philipc added Symtab and symbol iterator to ELF, but is basically the same, unless you were explicitly relying on the backing vector
- use scroll 0.8.0 and us scroll_derive via scroll
- fix notes including \0 terminator (causes breakage because tools like grep treat resulting output as a binary output...)
### Added
- pe: add PE characteristics constants courtesy @philipc
- mach: SizeWith for RelocationInfo
- mach: IOWrite and Pwrite impls for Nlist

## [0.0.12] - 2017-10-29
### Changed
- fix proper std feature flag to log; this was an oversight in last version
- proper cputype and cpusubtype constants to mach, along with mappings, courtesy of @mitsuhiko
- new osx and ios version constants
- all mach load commands now implement IOread and IOwrite from scroll
- add new elf::note module and associated structs + constants, and `iter_notes` method to Elf object
- remove all unused muts; this will make nightly and future stables no longer warn

### Added
- fix macho nstab treatment, thanks @philipc !
- mach header cpusubtype bug fixed, thanks @mitsuhiko !

## [0.0.11] - 2017-08-24
### Added
- goblin::Object::parse; add deprecation to goblin::parse
- MAJOR archive now parses bsd style archives AND is zero-copy by @willglynn
- MAJOR macho import parser bug fixed by @willglynn
- added writer impls for Section and Segment
- add get_unsafe to strtab for Option<&str> returns
- relocations method on mach
- more elf relocations
- mach relocations
- convenience functions for many elf structures that elf writer will appreciate
- mach relocation iteration
- update to scroll 0.7
- add cread/ioread impls for various structs

### Changed
- BREAKING: sections() and section iterator now return (Section, &[u8])
- Segment, Section, RelocationIterator are now in segment module
- removed lifetime from section, removed data and raw data, and embedded ctx
- all scroll::Error have been removed from public API ref #33
- better mach symbol iteration
- better mach section iteration
- remove wow_so_meta_doge due to linker issues
- Strtab.get now returns a Option<Result>, when index is bad
- elf.soname is &str
- elf.libraries is now Vec<&str>

## [0.0.10] - 2017-05-09
### Added
- New goblin::Object for enum containing the parsed binary container, or convenience goblin::parse(&[u8) for parsing bytes into respective container format
### Changed
- All binaries formats now have lifetimes
- Elf has a lifetime
- Strtab.new now requires a &'a[u8]
- Strtab.get now returns a scroll::Result<&'a str> (use strtab[index] if you want old behavior and don't care about panics); returning scroll::Error is a bug, fixed in next release

## [0.0.9] - 2017-04-05
### Changed
- Archive has a lifetime
- Mach has a lifetime
