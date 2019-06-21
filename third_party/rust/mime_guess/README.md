# mime_guess [![Build Status](https://travis-ci.org/abonander/mime_guess.svg?branch=master)](https://travis-ci.org/abonander/mime_guess) [![Crates.io](https://img.shields.io/crates/v/mime_guess.svg)](https://crates.io/crates/mime_guess)

MIME/MediaType guessing by file extension. Uses a [compile-time](https://crates.io/crates/phf_codegen) [perfect hash map](https://crates.io/crates/phf) of known file extension -> MIME type mappings.

#### [Documentation](https://docs.rs/mime_guess/)

**NOTE**: this crate will remain in `2.0.0-alpha.x` until `mime` stabilizes (releases 1.0, [discussion here](https://github.com/hyperium/mime/issues/52)). Backwards-compatible changes will
be backported to the `1.x-maint` branch but upgrades to `mime` will constitute an alpha version bump.

#### Note: MIME Types Returned Are Not Stable/Guaranteed
The media types returned for a given extension are not considered to be part of the crate's
 stable API and are often updated in patch (#.#.x) releases to be as correct as possible.
 
Additionally, only the extensions of paths/filenames are inspected in order to guess the MIME type. The
file that may or may not reside at that path may or may not be a valid file of the returned MIME type.
Be wary of unsafe or un-validated assumptions about file structure or length.

Contributing
-----------

#### Adding or correcting MIME types for extensions

Is the MIME type for a file extension wrong or missing? Great! Well, not great for us, but great for you if you'd like to open a pull request! 

The file extension -> MIME type mappings are listed in `src/mime_types.rs`. **The list is sorted alphabetically by file extension, and all extensions are lowercase (where applicable).** This is necessary only for the sanity of the crate maintainers; extension search is case-insensitive.

Simply add or update the appropriate string pair(s) to make the correction(s) needed. Run `cargo test` to make sure the library continues to work correctly.

#### (Important!) Citing the corrected MIME type 

When opening a pull request, please include a link to an official document or RFC noting the correct MIME type for the file type in question. Though we're only guessing here, we like to be as correct as we can. It makes it much easier to vet your contribution if we don't have to search for corroborating material.

#### Changes to the API or operation of the crate

We're open to changes to the crate's API or its inner workings, breaking or not, if it improves the overall operation, efficiency, or ergonomics of the crate. However, it would be a good idea to open an issue on the repository so we can discuss your proposed changes and decide how best to approach them.


License
-------

MIT (See the `LICENSE` file in this repository for more information.)
