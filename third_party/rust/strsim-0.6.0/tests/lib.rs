extern crate strsim;

use strsim::{hamming, levenshtein, osa_distance, damerau_levenshtein, jaro,
             jaro_winkler, levenshtein_against_vec, osa_distance_against_vec,
             damerau_levenshtein_against_vec, jaro_against_vec,
             jaro_winkler_against_vec};

#[test]
fn hamming_works() {
    match hamming("hamming", "hammers") {
        Ok(distance) => assert_eq!(3, distance),
        Err(why) => panic!("{:?}", why)
    }
}

#[test]
fn levenshtein_works() {
    assert_eq!(3, levenshtein("kitten", "sitting"));
}

#[test]
fn osa_distance_works() {
    assert_eq!(3, osa_distance("ac", "cba"));
}

#[test]
fn damerau_levenshtein_works() {
    assert_eq!(2, damerau_levenshtein("ac", "cba"));
}

#[test]
fn jaro_works() {
    assert!((0.392 - jaro("Friedrich Nietzsche", "Jean-Paul Sartre")).abs() <
            0.001);
}

#[test]
fn jaro_winkler_works() {
    assert!((0.911 - jaro_winkler("cheeseburger", "cheese fries")).abs() <
            0.001);
}

#[test]
fn levenshtein_against_vec_works() {
   let v = vec!["test", "test1", "test12", "test123", "", "tset", "tsvet"];
   let result = levenshtein_against_vec("test", &v);
   let expected = vec![0, 1, 2, 3, 4, 2, 3];
   assert_eq!(expected, result);
}

#[test]
fn osa_distance_against_vec_works() {
   let v = vec!["test", "test1", "test12", "test123", "", "tset", "tsvet"];
   let result = osa_distance_against_vec("test", &v);
   let expected = vec![0, 1, 2, 3, 4, 1, 3];
   assert_eq!(expected, result);
}

#[test]
fn damerau_levenshtein_against_vec_works() {
   let v = vec!["test", "test1", "test12", "test123", "", "tset", "tsvet"];
   let result = damerau_levenshtein_against_vec("test", &v);
   let expected = vec![0, 1, 2, 3, 4, 1, 2];
   assert_eq!(expected, result);
}

#[test]
fn jaro_against_vec_works() {
   let v = vec!["test", "test1", "test12", "test123", "", "tset"];
   let result = jaro_against_vec("test", &v);
   let expected = vec![1.0, 0.933333, 0.888889, 0.857143, 0.0, 0.916667];
   let delta: f64 = result.iter()
                          .zip(expected.iter())
                          .map(|(x, y)| (x - y).abs() as f64)
                          .fold(0.0, |x, y| x + y as f64);
   assert!(delta.abs() < 0.0001);
}

#[test]
fn jaro_winkler_against_vec_works() {
   let v = vec!["test", "test1", "test12", "test123", "", "tset"];
   let result = jaro_winkler_against_vec("test", &v);
   let expected = vec![1.0, 0.96, 0.933333, 0.914286, 0.0, 0.925];
   let delta: f64 = result.iter()
                          .zip(expected.iter())
                          .map(|(x, y)| (x - y).abs() as f64)
                          .fold(0.0, |x, y| x + y as f64);
   assert!(delta.abs() < 0.0001);
}
