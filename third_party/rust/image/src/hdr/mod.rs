//!  Decoding of Radiance HDR Images
//!
//!  A decoder for Radiance HDR images
//!
//!  # Related Links
//!
//!  * <http://radsite.lbl.gov/radiance/refer/filefmts.pdf>
//!  * <http://www.graphics.cornell.edu/~bjw/rgbe/rgbe.c>
//!

extern crate scoped_threadpool;

mod hdr_decoder;
mod hdr_encoder;

pub use self::hdr_decoder::*;
pub use self::hdr_encoder::*;
