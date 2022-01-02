// Any copyright to the test code below is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

use mapped_hyph::Hyphenator;

#[test]
fn basic_tests() {
    let dic_path = "hyph_en_US.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("haha", '-'), "haha");
    assert_eq!(hyph.hyphenate_word("hahaha", '-'), "ha-haha");
    assert_eq!(hyph.hyphenate_word("photo", '-'), "photo");
    assert_eq!(hyph.hyphenate_word("photograph", '-'), "pho-to-graph");
    assert_eq!(hyph.hyphenate_word("photographer", '-'), "pho-tog-ra-pher");
    assert_eq!(hyph.hyphenate_word("photographic", '-'), "pho-to-graphic");
    assert_eq!(hyph.hyphenate_word("photographical", '-'), "pho-to-graph-i-cal");
    assert_eq!(hyph.hyphenate_word("photographically", '-'), "pho-to-graph-i-cally");
    assert_eq!(hyph.hyphenate_word("supercalifragilisticexpialidocious", '-'), "su-per-cal-ifrag-ilis-tic-ex-pi-ali-do-cious");
}

// Testcases adapted from tests included with libhyphen.
// (Using only the UTF-8 dictionaries/tests, and omitting those that require
// the extended hyphenation algorithm.)

#[test]
fn base() {
    let dic_path = "tests/base.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    use std::fs::File;
    use std::io::{BufRead,BufReader};
    let words: Vec<String> = {
        let file = File::open("tests/base.word").unwrap();
        BufReader::new(file).lines().map(|l| l.unwrap()).collect()
    };
    let hyphs: Vec<String> = {
        let file = File::open("tests/base.hyph").unwrap();
        BufReader::new(file).lines().map(|l| l.unwrap()).collect()
    };
    for i in 0 .. words.len() {
        assert_eq!(hyph.hyphenate_word(&words[i], '='), hyphs[i]);
    }
}

#[test]
fn compound() {
    let dic_path = "tests/compound.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("motorcycle", '-'), "mo-tor-cy-cle");
}

#[test]
fn compound4() {
    let dic_path = "tests/compound4.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("motorcycle", '-'), "motor-cycle");
}

#[test]
fn compound5() {
    let dic_path = "tests/compound5.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("postea", '-'), "post-e-a");
}

#[test]
fn compound6() {
    let dic_path = "tests/compound6.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("meaque", '-'), "me-a-que");
}

#[test]
fn settings2() {
    let dic_path = "tests/settings2.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("őőőőőőő", '='), "ő=ő=ő=ő=ő=ő=ő");
}

#[test]
fn settings3() {
    let dic_path = "tests/settings3.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("őőőőőőő", '='), "őő=ő=ő=ő=őő");
}

#[test]
fn hyphen() {
    let dic_path = "tests/hyphen.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("foobar'foobar-foobar’foobar", '='), "foobar'foobar-foobar’foobar");
}

#[test]
fn lhmin() {
    let dic_path = "tests/lhmin.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("miért", '='), "mi=ért");
}

#[test]
fn rhmin() {
    let dic_path = "tests/rhmin.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("övéit", '='), "övéit");
    assert_eq!(hyph.hyphenate_word("అంగడిధర", '='), "అం=గ=డిధర");
}

#[test]
fn num() {
    let dic_path = "tests/num.hyf";
    let dic = match unsafe { mapped_hyph::load_file(dic_path) } {
        Some(dic) => dic,
        _ => panic!("failed to load dictionary {}", dic_path),
    };
    let hyph = Hyphenator::new(&*dic);
    assert_eq!(hyph.hyphenate_word("foobar", '='), "foobar");
    assert_eq!(hyph.hyphenate_word("foobarfoobar", '='), "foobar=foobar");
    assert_eq!(hyph.hyphenate_word("barfoobarfoo", '='), "barfoo=barfoo");
    assert_eq!(hyph.hyphenate_word("123foobarfoobar", '='), "123foobar=foobar");
    assert_eq!(hyph.hyphenate_word("foobarfoobar123", '='), "foobar=foobar123");
    assert_eq!(hyph.hyphenate_word("123foobarfoobar123", '='), "123foobar=foobar123");
    assert_eq!(hyph.hyphenate_word("123barfoobarfoo", '='), "123barfoo=barfoo");
    assert_eq!(hyph.hyphenate_word("barfoobarfoo123", '='), "barfoo=barfoo123");
    assert_eq!(hyph.hyphenate_word("123barfoobarfoo123", '='), "123barfoo=barfoo123");
}
