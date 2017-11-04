#![feature(test)]

// The benchmarks here verify that the complexity grows as O(*n*)
// where *n* is the number of characters in the text to be wrapped.

extern crate test;
extern crate rand;
extern crate lipsum;
extern crate textwrap;
#[cfg(feature = "hyphenation")]
extern crate hyphenation;

use test::Bencher;
#[cfg(feature = "hyphenation")]
use hyphenation::Language;
#[cfg(feature = "hyphenation")]
use textwrap::Wrapper;
use rand::XorShiftRng;
use lipsum::MarkovChain;

const LINE_LENGTH: usize = 60;

/// Generate a lorem ipsum text with the given number of characters.
fn lorem_ipsum(length: usize) -> String {
    // The average word length in the lorem ipsum text is somewhere
    // between 6 and 7. So we conservatively divide by 5 to have a
    // long enough text that we can truncate below.
    let rng = XorShiftRng::new_unseeded();
    let mut chain = MarkovChain::new_with_rng(rng);
    chain.learn(lipsum::LOREM_IPSUM);
    chain.learn(lipsum::LIBER_PRIMUS);

    let mut text = chain.generate_from(length / 5, ("Lorem", "ipsum"));
    text.truncate(length);
    text
}

#[bench]
fn fill_100(b: &mut Bencher) {
    let text = &lorem_ipsum(100);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
fn fill_200(b: &mut Bencher) {
    let text = &lorem_ipsum(200);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
fn fill_400(b: &mut Bencher) {
    let text = &lorem_ipsum(400);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
fn fill_800(b: &mut Bencher) {
    let text = &lorem_ipsum(800);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}


#[bench]
fn wrap_100(b: &mut Bencher) {
    let text = &lorem_ipsum(100);
    b.iter(|| textwrap::wrap(text, LINE_LENGTH))
}

#[bench]
fn wrap_200(b: &mut Bencher) {
    let text = &lorem_ipsum(200);
    b.iter(|| textwrap::wrap(text, LINE_LENGTH))
}

#[bench]
fn wrap_400(b: &mut Bencher) {
    let text = &lorem_ipsum(400);
    b.iter(|| textwrap::wrap(text, LINE_LENGTH))
}

#[bench]
fn wrap_800(b: &mut Bencher) {
    let text = &lorem_ipsum(800);
    b.iter(|| textwrap::wrap(text, LINE_LENGTH))
}


#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_fill_100(b: &mut Bencher) {
    let text = &lorem_ipsum(100);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::with_splitter(LINE_LENGTH, corpus);
    b.iter(|| wrapper.fill(text))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_fill_200(b: &mut Bencher) {
    let text = &lorem_ipsum(200);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::with_splitter(LINE_LENGTH, corpus);
    b.iter(|| wrapper.fill(text))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_fill_400(b: &mut Bencher) {
    let text = &lorem_ipsum(400);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::with_splitter(LINE_LENGTH, corpus);
    b.iter(|| wrapper.fill(text))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_fill_800(b: &mut Bencher) {
    let text = &lorem_ipsum(800);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::with_splitter(LINE_LENGTH, corpus);
    b.iter(|| wrapper.fill(text))
}
