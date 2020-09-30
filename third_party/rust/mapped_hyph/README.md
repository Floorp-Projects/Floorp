# mapped_hyph

mapped_hyph is a reimplementation of the hyphenation algorithm from the
[libhyphen](https://github.com/hunspell/hyphen) library
that is intended to reduce the in-memory footprint of loaded
hyphenation dictionaries, especially when the same dictionary
may be in use by multiple processes.

To reduce memory footprint, mapped_hyph uses hyphenation dictionaries that are
"precompiled" into a flat, position-independent binary format that is used
directly by the runtime hyphenation functions.
Therefore, dictionaries do not have to be parsed into a dynamic structure in memory;
the files can simply be mmap'd into the address space and immediately used.
In addition, a compiled dictionary mapped into a shared-memory block
can be made available to multiple processes for no added physical memory cost.

One deliberate simplification compared to libhyphen
is that mapped_hyph only accepts UTF-8 text and hyphenation dictionaries;
legacy non-Unicode encodings are not supported.

mapped_hyph has been created primarily for use by Gecko, replacing the use of libhyphen,
and so its features (and limitations) are based on this use case.
However, it is hoped that it will also be more generally useful.

## Functionality

Currently, mapped_hyph supports only "standard" hyphenation, where spelling does not
change around the hyphenation position. At present this is the only kind of
hyphenation supported in Gecko.

The compiled hyphenation dictionary format includes provision for replacement
strings and indexes, as used by libhyphen to support non-standard hyphenations
(e.g. German "Schiffahrt" -> "Schiff-fahrt"), but the `find_hyphen_values` function
will ignore any such hyphenation positions it finds.
(None of the hyphenation dictionaries shipping with Firefox includes such rules.)

## Licensing

mapped_hyph is dual licensed under the Apache-2.0 and MIT licenses;
see the file COPYRIGHT.

## Documentation

Use `cargo doc --open` to view (admittedly brief) documentation generated from
comments in the source.

## C and C++ bindings

See src/ffi.rs for C/C++ APIs that can be used to load hyphenation files
and to locate valid hyphenation positions in a word.

## Sample programs

See main.rs for a simple example program.

## Compiled dictionaries

The `hyf_compile` tool is used to generate `.hyf` files for mapped_hyph
from standard `.dic` (or `.pat`) files as used by libhyphen, LibreOffice, etc.

(A compiled version of the `hyph_en_US` dictionary from libhyphen is currently
included here, as it is handy for testing purposes.)

## Release Notes

### 0.4.0

* Added a boolean `compress` param to the pattern compiler to control whether
  it attempts to compress the compiled table by merging duplicate states (which
  takes significant extra time).

* Added FFI functions to compile hyphenation tables from a file path or a buffer,
  intended for use from Gecko.

### 0.3.0

* Switched from MPL2 to Apache2/MIT dual license.

* Misc bug-fixes and optimizations.

### 0.2.0

* Implemented a hyphenation table compiler in the `builder` submodule,
  and `hyf_compile` command-line tool.

* Moved C-callable API functions into an `ffi` submodule.

### 0.1.0

* Initial release.
