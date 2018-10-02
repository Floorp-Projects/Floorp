# Changelog
All notable changes to this project will be documented in this file.

Before 1.0, this project does not adhere to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

I'm sorry, I will try my best to ease breaking changes.  We're almost to 1.0, don't worry!

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
