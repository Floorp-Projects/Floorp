# Rust Image Release Notes

Rust image aims to be a pure-Rust implementation of various popular image formats. Accompanying reading/write support, rust image provides basic imaging processing function. See `README.md` for further details.

## Known issues
 - Interlaced (progressive) or animated images are not well supported.
 - Images with *n* bit/channel (*n ≠ 8*) are not well supported.

## Changes

### Version 0.22

- The required Rust version is now `1.34.2`.
- Note the website and blog: [image-rs.org][1] and [blog.image-rs.org][2]
- `PixelMut` now only on `ImageBuffer` and removed from `GenericImage`
  interface. Prefer iterating manually in the generic case.
- Replaced an unsafe interface in the hdr decoder with a safe variant.
- Support loading 2-bit BMP images
- Add method to save an `ImageBuffer`/`DynamicImage` with specified format
- Update tiff to `0.3` with a writer
- Update png to `0.15`, fixes reading of interlaced sub-byte pixels
- Always use custom struct for `ImageDecoder::Reader`
- Added `apply_without_alpha` and `map_without_alpha` to `Pixel` trait
- Pixel information now with associated constants instead of static methods
- Changed color structs to tuple types with single component. Improves
  ergonomics of destructuring assignment and construction.
- Add lifetime parameter on `ImageDecoder` trait.
- Remove unecessary `'static` bounds on affine operations
- Add function to retrieve image dimensions without loading full image
- Allow different image types in overlay and replace
- Iterators over rows of `ImageBuffer`, mutable variants

[1]: https://www.image-rs.org
[2]: https://blog.image-rs.org

### Version 0.21.2

- Fixed a variety of crashes and opaque errors in webp
- Updated the png limits to be less restrictive
- Reworked even more `unsafe` operations into safe alternatives
- Derived Debug on FilterType and Deref on Pixel
- Removed a restriction on DXT to always require power of two dimensions
- Change the encoding of RGBA in bmp using bitfields
- Corrected various urls

### Version 0.21.1

- A fairly important bugfix backport
- Fixed a potentially memory safety issue in the hdr and tiff decoders, see #885
- See [the full advisory](docs/2019-04-23-memory-unsafety.md) for an analysis
- Fixes `ImageBuffer` index calculation for very, very large images
- Fix some crashes while parsing specific incomplete pnm images
- Added comprehensive fuzzing for the pam image types

### Version 0.21

- Updated README to use `GenericImageView`
- Removed outdated version number from CHANGES
- Compiles now with wasm-unknown-emscripten target
- Restructured `ImageDecoder` trait
- Updated README with a more colorful example for the Julia fractal
- Use Rust 1.24.1 as minimum supported version
- Support for loading GIF frames one at a time with `animation::Frames`
- The TGA decoder now recognizes 32 bpp as RGBA(8)
- Fixed `to_bgra` document comment
- Added release test script
- Removed unsafe code blocks several places
- Fixed overlay overflow bug issues with documented proofs

### Version 0.20

- Clippy lint pass
- Updated num-rational dependency
- Added BGRA and BGR color types
- Improved performance of image resizing
- Improved PBM decoding
- PNM P4 decoding now returns bits instead of bytes
- Fixed move of overlapping buffers in BMP decoder
- Fixed some document comments
- `GenericImage` and `GenericImageView` is now object-safe
- Moved TIFF code to its own library
- Fixed README examples
- Fixed ordering of interpolated parameters in TIFF decode error string
- Thumbnail now handles upscaling
- GIF encoding for multiple frames
- Improved subimages API
- Cargo fmt fixes

### Version 0.19

- Fixed panic when blending with alpha zero.
- Made `save` consistent.
- Consistent size calculation.
- Fixed bug in `apply_with_alpha`.
- Implemented `TGADecoder::read_scanline`.
- Use deprecated attribute for `pixels_mut`.
- Fixed bug in JPEG grayscale encoding.
- Fixed multi image TIFF.
- PNM encoder.
- Added `#[derive(Hash)]` for `ColorType`.
- Use `num-derive` for `#[derive(FromPrimitive)]`.
- Added `into_frames` implementation for GIF.
- Made rayon an optional dependency.
- Fixed issue where resizing image did not give exact width/height.
- Improved downscale.
- Added a way to expose options when saving files.
- Fixed some compiler warnings.
- Switched to lzw crate instead of using built-in version.
- Added `ExactSizeIterator` implementations to buffer structs.
- Added `resize_to_fill` method.
- DXT encoding support.
- Applied clippy suggestions.

### Version 0.4
 - Various improvements.
 - Additional supported image formats (BMP and ICO).
 - GIF and PNG codec moved into separate crates.

### Version 0.3
 - Replace `std::old_io` with `std::io`.

### Version 0.2
 - Support for interlaced PNG images.
 - Writing support for GIF images (full color and paletted).
 - Color quantizer that converts 32bit images to paletted including the alpha channel.
 - Initial support for reading TGA images.
 - Reading support for TIFF images (packbits and FAX compression not supported).
 - Various bug fixes and improvements.

### Version 0.1
- Initial release
- Basic reading support for png, jpeg, gif, ppm and webp.
- Basic writing support for png and jpeg.
- A collection of basic imaging processing function like `blur` or `invert`
