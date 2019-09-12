ChangeLog for rust-argon2
=========================

This documents all notable changes to
[rust-argon2](https://github.com/sru-systems/rust-argon2).

## 0.5.1

- Use crossbeam utils 0.6 instead of crossbeam 0.5


## 0.5.0

- Replace blake2-rfc with blake2b_simd.


## 0.4.0

- Replace rustc-serialize dependency with base64 and hex.
- Update base64 dependency.
- Update crossbeam dependency.
- Update hex dependency.
- Allow updating to minor versions of blake2-rfc.


## 0.3.0

- Embed Config struct in Context struct.


## 0.2.0

- Use ThreadMode enum instead of explicit thread number.
- Use a Config struct instead of explicit configuration arguments.
- Use references instead of vectors for byte data in the Context struct.
- Deprecate the following functions:
  - hash_encoded_defaults
  - hash_encoded_old
  - hash_encoded_std
  - hash_raw_defaults
  - hash_raw_old
  - hash_raw_std
  - verify_raw_old
  - verify_raw_std
