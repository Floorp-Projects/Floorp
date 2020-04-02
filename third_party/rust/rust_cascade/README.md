# rust-cascade
A Bloom filter cascade implementation in rust. This can utilize one of two hash
functions:

* MurmurHash32, or
* SHA256, with an optional salt

This implementation is designed to match up with the Python [filter-cascade
project](https://pypi.org/project/filtercascade/)
[[github](https://github.com/mozilla/filter-cascade)]

See tests in src/lib.rs to get an idea of usage.
