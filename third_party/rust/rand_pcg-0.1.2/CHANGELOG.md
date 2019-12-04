# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2019-02-23
- require `bincode` 1.1.2 for i128 auto-detection
- make `bincode` a dev-dependency again #663
- clean up tests and Serde support

## [0.1.1] - 2018-10-04
- make `bincode` an explicit dependency when using Serde

## [0.1.0] - 2018-10-04
Initial release, including:

- `Lcg64Xsh32` aka `Pcg32`
- `Mcg128Xsl64` aka `Pcg64Mcg`
