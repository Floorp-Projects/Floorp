
nodrop
======

Recent Changes (nodrop)
-----------------------

- 0.1.12

  - Remove dependency on crate odds.

- 0.1.11

  - Remove erronous assertion in test (#77)

- 0.1.10

  - Update for stable ``needs_drop`` (Rust 1.21, was nightly only)

- 0.1.9

  - Fix issue in recent nightly where ``repr(u8)`` did not work. Use
    a better way to get rid of the enum layout optimization.

- 0.1.8
  
  - Add crate feature ``use_union`` that uses untagged unions to implement NoDrop.
    Finally we have an implementation without hacks, without a runtime flag,
    and without an actual ``Drop`` impl (which was needed to suppress drop).
    The crate feature requires nightly and is unstable.

- 0.1.7

  - Remove crate feature ``no_drop_flag``, because it doesn't compile on nightly
    anymore. Drop flags are gone anyway!

- 0.1.6

  - Add feature std, which you can opt out of to use ``no_std``.

- 0.1.5

  - Added crate feature ``use_needs_drop`` which is a nightly-only
    optimization, which skips overwriting if the inner value does not need
    drop.

License
=======

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
http://opensource.org/licenses/MIT, at your
option. This file may not be copied, modified, or distributed
except according to those terms.


