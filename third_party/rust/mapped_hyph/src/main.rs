// Copyright 2019 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

extern crate mapped_hyph;

use mapped_hyph::Hyphenator;

fn main() {
    let dic_path = "hyph_en_US.hyf";

    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);

    println!("{}", hyph.hyphenate_word("haha", '-'));
    println!("{}", hyph.hyphenate_word("hahaha", '-'));
    println!("{}", hyph.hyphenate_word("photo", '-'));
    println!("{}", hyph.hyphenate_word("photograph", '-'));
    println!("{}", hyph.hyphenate_word("photographer", '-'));
    println!("{}", hyph.hyphenate_word("photographic", '-'));
    println!("{}", hyph.hyphenate_word("photographical", '-'));
    println!("{}", hyph.hyphenate_word("photographically", '-'));
    println!("{}", hyph.hyphenate_word("supercalifragilisticexpialidocious", '-'));
    println!("{}", hyph.hyphenate_word("o'dwyer", '='));
    println!("{}", hyph.hyphenate_word("o'callahan", '='));
    println!("{}", hyph.hyphenate_word("o’dwyer", '='));
    println!("{}", hyph.hyphenate_word("o’callahan", '='));
    println!("{}", hyph.hyphenate_word("petti-fogging", '='));
    println!("{}", hyph.hyphenate_word("e-mailing", '='));
    println!("{}", hyph.hyphenate_word("-x-mailing", '='));
    println!("{}", hyph.hyphenate_word("-strikeout-", '='));

    let dic2 = match unsafe { mapped_hyph::load_file("tests/compound.hyf") } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", "tests/compound.hyf"),
    };

    let h2 = Hyphenator::new(&*dic2);
    println!("{}", h2.hyphenate_word("motorcycle", '='));

    let dic3 = match unsafe { mapped_hyph::load_file("tests/rhmin.hyf") } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let h3 = Hyphenator::new(&*dic3);
    println!("{}", h3.hyphenate_word("övéit", '='));
    println!("{}", h3.hyphenate_word("అంగడిధర", '='));

    let dic4 = match unsafe { mapped_hyph::load_file("tests/num.hyf") } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", "tests/num.hyf"),
    };
    let h4 = Hyphenator::new(&*dic4);

    println!("{}", h4.hyphenate_word("123foobar123", '='));
    println!("{}", h4.hyphenate_word("123foobarfoobar", '='));
    println!("{}", h4.hyphenate_word("foobarfoobar123", '='));
    println!("{}", h4.hyphenate_word("123foobarfoobar123", '='));
}
