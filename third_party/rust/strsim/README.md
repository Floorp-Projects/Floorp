# strsim-rs [![Crates.io](https://img.shields.io/crates/v/strsim.svg)](https://crates.io/crates/strsim) [![Crates.io](https://img.shields.io/crates/l/strsim.svg?maxAge=2592000)](https://github.com/dguo/strsim-rs/blob/master/LICENSE) [![Linux build status](https://travis-ci.org/dguo/strsim-rs.svg?branch=master)](https://travis-ci.org/dguo/strsim-rs) [![Windows build status](https://ci.appveyor.com/api/projects/status/ggue6i785618a39w?svg=true)](https://ci.appveyor.com/project/dguo/strsim-rs)

[Rust](https://www.rust-lang.org) implementations of [string similarity metrics]:
  - [Hamming]
  - [Levenshtein]
  - [Optimal string alignment]
  - [Damerau-Levenshtein]
  - [Jaro and Jaro-Winkler] - this implementation of Jaro-Winkler does not limit the common prefix length

### Installation
```toml
# Cargo.toml
[dependencies]
strsim = "0.6.0"
```

### [Documentation](https://docs.rs/strsim/)
You can change the version in the url to see the documentation for an older
version in the
[changelog](https://github.com/dguo/strsim-rs/blob/master/CHANGELOG.md).

### Usage
```rust
extern crate strsim;

use strsim::{hamming, levenshtein, osa_distance, damerau_levenshtein, jaro,
             jaro_winkler, levenshtein_against_vec, osa_distance_against_vec,
             damerau_levenshtein_against_vec, jaro_against_vec,
             jaro_winkler_against_vec};

fn main() {
    match hamming("hamming", "hammers") {
        Ok(distance) => assert_eq!(3, distance),
        Err(why) => panic!("{:?}", why)
    }

    assert_eq!(3, levenshtein("kitten", "sitting"));

    assert_eq!(3, osa_distance("ac", "cba"));

    assert_eq!(2, damerau_levenshtein("ac", "cba"));

    assert!((0.392 - jaro("Friedrich Nietzsche", "Jean-Paul Sartre")).abs() <
            0.001);

    assert!((0.911 - jaro_winkler("cheeseburger", "cheese fries")).abs() <
            0.001);

    // get vectors of values back
    let v = vec!["test", "test1", "test12", "test123", "", "tset", "tsvet"];

    assert_eq!(levenshtein_against_vec("test", &v),
               vec![0, 1, 2, 3, 4, 2, 3]);

    assert_eq!(osa_distance_against_vec("test", &v),
               vec![0, 1, 2, 3, 4, 1, 3]);

    assert_eq!(damerau_levenshtein_against_vec("test", &v),
               vec![0, 1, 2, 3, 4, 1, 2]);

    let jaro_distances = jaro_against_vec("test", &v);
    let jaro_expected = vec![1.0, 0.933333, 0.888889, 0.857143, 0.0, 0.916667];
    let jaro_delta: f64 = jaro_distances.iter()
                                        .zip(jaro_expected.iter())
                                        .map(|(x, y)| (x - y).abs() as f64)
                                        .fold(0.0, |x, y| x + y as f64);
    assert!(jaro_delta < 0.0001);

    let jaro_winkler_distances = jaro_winkler_against_vec("test", &v);
    let jaro_winkler_expected = vec![1.0, 0.96, 0.933333, 0.914286, 0.0, 0.925];
    let jaro_winkler_delta = jaro_winkler_distances.iter()
                                 .zip(jaro_winkler_expected.iter())
                                 .map(|(x, y)| (x - y).abs() as f64)
                                 .fold(0.0, |x, y| x + y as f64);
    assert!(jaro_winkler_delta < 0.0001);
}
```

### Development
If you don't want to install Rust itself, you can install [Docker], and run
`$ ./dev`. This should bring up a temporary container from which you can run
[cargo] commands.

### License
[MIT](https://github.com/dguo/strsim-rs/blob/master/LICENSE)

[string similarity metrics]:http://en.wikipedia.org/wiki/String_metric
[Damerau-Levenshtein]:http://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance
[Jaro and Jaro-Winkler]:http://en.wikipedia.org/wiki/Jaro%E2%80%93Winkler_distance
[Levenshtein]:http://en.wikipedia.org/wiki/Levenshtein_distance
[Hamming]:http://en.wikipedia.org/wiki/Hamming_distance
[Optimal string alignment]:https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance#Optimal_string_alignment_distance
[Docker]:https://docs.docker.com/engine/installation/
[cargo]:https://github.com/rust-lang/cargo

