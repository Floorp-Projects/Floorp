# rustc-hash

A speedy hash algorithm used within rustc. The hashmap in liballoc by
default uses SipHash which isn't quite as speedy as we want. In the
compiler we're not really worried about DOS attempts, so we use a fast
non-cryptographic hash.

This is the same as the algorithm used by Firefox -- which is a
homespun one not based on any widely-known algorithm -- though
modified to produce 64-bit hash values instead of 32-bit hash
values. It consistently out-performs an FNV-based hash within rustc
itself -- the collision rate is similar or slightly worse than FNV,
but the speed of the hash function itself is much higher because it
works on up to 8 bytes at a time.

## Usage

```
use rustc_hash::FxHashMap;
let map: FxHashMap<u32, u32> = FxHashMap::default();
```
