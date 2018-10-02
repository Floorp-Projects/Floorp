ThinVec is a Vec that stores its length and capacity inline, making it take up
less space. Currently this crate mostly exists to facilitate gecko ffi. The
crate isn't quite ready for use elsewhere, as it currently unconditionally
uses the libc allocator.
