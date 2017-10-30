// Copyright 2014 The html5ever Project Developers. See the
// COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg(all(test, feature = "unstable"))]
#![feature(test)]

extern crate test;
extern crate unicode_bidi;

use test::Bencher;

use unicode_bidi::BidiInfo;


const LTR_TEXTS: &[&str] = &[
    include_str!("udhr_data/ltr/udhr_acu_1.txt"),
    include_str!("udhr_data/ltr/udhr_auc.txt"),
    include_str!("udhr_data/ltr/udhr_eng.txt"),
    include_str!("udhr_data/ltr/udhr_knc.txt"),
    include_str!("udhr_data/ltr/udhr_krl.txt"),
    include_str!("udhr_data/ltr/udhr_kwi.txt"),
    include_str!("udhr_data/ltr/udhr_lot.txt"),
    include_str!("udhr_data/ltr/udhr_mly_latn.txt"),
    include_str!("udhr_data/ltr/udhr_piu.txt"),
    include_str!("udhr_data/ltr/udhr_qug.txt"),
    include_str!("udhr_data/ltr/udhr_snn.txt"),
    include_str!("udhr_data/ltr/udhr_tiv.txt"),
    include_str!("udhr_data/ltr/udhr_uig_latn.txt"),
];

const BIDI_TEXTS: &[&str] = &[
    include_str!("udhr_data/bidi/udhr_aii.txt"),
    include_str!("udhr_data/bidi/udhr_arb.txt"),
    include_str!("udhr_data/bidi/udhr_mly_arab.txt"),
    include_str!("udhr_data/bidi/udhr_pes_1.txt"),
    include_str!("udhr_data/bidi/udhr_skr.txt"),
    include_str!("udhr_data/bidi/udhr_urd.txt"),
    include_str!("udhr_data/bidi/udhr_pes_2.txt"),
    include_str!("udhr_data/bidi/udhr_uig_arab.txt"),
    include_str!("udhr_data/bidi/udhr_urd_2.txt"),
    include_str!("udhr_data/bidi/udhr_heb.txt"),
    include_str!("udhr_data/bidi/udhr_pbu.txt"),
    include_str!("udhr_data/bidi/udhr_pnb.txt"),
    include_str!("udhr_data/bidi/udhr_ydd.txt"),
];


fn bench_bidi_info_new(b: &mut Bencher, texts: &[&str]) {
    for text in texts {
        b.iter(|| { BidiInfo::new(text, None); });
    }
}

fn bench_reorder_line(b: &mut Bencher, texts: &[&str]) {
    for text in texts {
        let bidi_info = BidiInfo::new(text, None);
        b.iter(
            || for para in &bidi_info.paragraphs {
                let line = para.range.clone();
                bidi_info.reorder_line(para, line);
            }
        );
    }
}


#[bench]
fn bench_1_bidi_info_new_for_ltr_texts(b: &mut Bencher) {
    bench_bidi_info_new(b, LTR_TEXTS);
}

#[bench]
fn bench_2_bidi_info_new_for_bidi_texts(b: &mut Bencher) {
    bench_bidi_info_new(b, BIDI_TEXTS);
}

#[bench]
fn bench_3_reorder_line_for_ltr_texts(b: &mut Bencher) {
    bench_reorder_line(b, LTR_TEXTS);
}

#[bench]
fn bench_4_reorder_line_for_bidi_texts(b: &mut Bencher) {
    bench_reorder_line(b, BIDI_TEXTS);
}
