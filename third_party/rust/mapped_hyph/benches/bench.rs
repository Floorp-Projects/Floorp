#![feature(test)]
extern crate test;

#[macro_use]
extern crate lazy_static;

extern crate mapped_hyph;
use mapped_hyph::Hyphenator;

lazy_static! {
    static ref WORDS: Vec<String> = {
        use std::fs::File;
        use std::io::{BufRead,BufReader};
        let file = File::open("/usr/share/dict/words").unwrap();
        BufReader::new(file).lines().map(|l| l.unwrap()).collect()
    };
}

#[bench]
fn bench_words(b: &mut test::Bencher) {
    b.iter(|| {
        let dic_path = "hyph_en_US.hyf";
        let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
            Some(dic) => dic,
            _ => panic!("failed to load dictionary {}", dic_path),
        };
        let hyph = Hyphenator::new(&*dic);
        let mut values: Vec<u8> = vec![0; 1000];
        for w in WORDS.iter() {
            test::black_box(hyph.find_hyphen_values(&w, &mut values));
        }
    });
}
