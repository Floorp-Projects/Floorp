#![feature(test)]

// The benchmarks here verify that the complexity grows as O(*n*)
// where *n* is the size of the text to be wrapped.

extern crate test;
#[cfg(feature = "hyphenation")]
extern crate hyphenation;
extern crate textwrap;

use test::Bencher;
#[cfg(feature = "hyphenation")]
use hyphenation::Language;
#[cfg(feature = "hyphenation")]
use textwrap::Wrapper;

const LINE_LENGTH: usize = 60;

fn lorem_ipsum(length: usize) -> &'static str {
    let text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas feugiat non mi \
                rutrum consectetur. Nulla iaculis luctus ex suscipit posuere. Sed et tellus quis \
                elit volutpat pretium. Sed faucibus purus vitae feugiat tincidunt. Nulla \
                malesuada interdum tempus. Proin consectetur malesuada magna, id suscipit enim \
                tempus in. Sed sollicitudin velit tortor, quis condimentum nisl vulputate \
                lobortis. Curabitur id lectus arcu. Nullam quis aliquam nisi. Vestibulum quam \
                enim, elementum vel urna scelerisque, ultricies cursus urna. Mauris vestibulum, \
                augue non posuere viverra, risus tortor iaculis augue, eget convallis metus nisl \
                vestibulum nisi. Aenean auctor dui vel aliquet sagittis. Aliquam quis enim \
                mauris. Nunc eu leo et orci euismod bibendum vel eu tortor. Nam egestas volutpat \
                ex, a turpis duis.";
    text.split_at(length).0
}

#[bench]
fn lorem_100(b: &mut Bencher) {
    let text = lorem_ipsum(100);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
fn lorem_200(b: &mut Bencher) {
    let text = lorem_ipsum(200);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
fn lorem_400(b: &mut Bencher) {
    let text = lorem_ipsum(400);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
fn lorem_800(b: &mut Bencher) {
    let text = lorem_ipsum(800);
    b.iter(|| textwrap::fill(text, LINE_LENGTH))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_lorem_100(b: &mut Bencher) {
    let text = lorem_ipsum(100);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::new(LINE_LENGTH).word_splitter(Box::new(corpus));
    b.iter(|| wrapper.fill(text))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_lorem_200(b: &mut Bencher) {
    let text = lorem_ipsum(200);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::new(LINE_LENGTH).word_splitter(Box::new(corpus));
    b.iter(|| wrapper.fill(text))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_lorem_400(b: &mut Bencher) {
    let text = lorem_ipsum(400);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::new(LINE_LENGTH).word_splitter(Box::new(corpus));
    b.iter(|| wrapper.fill(text))
}

#[bench]
#[cfg(feature = "hyphenation")]
fn hyphenation_lorem_800(b: &mut Bencher) {
    let text = lorem_ipsum(800);
    let corpus = hyphenation::load(Language::Latin).unwrap();
    let wrapper = Wrapper::new(LINE_LENGTH).word_splitter(Box::new(corpus));
    b.iter(|| wrapper.fill(text))
}
