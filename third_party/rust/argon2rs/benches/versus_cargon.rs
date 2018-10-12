// benches argon2rs against the reference c implementation at
// https://github.com/p-h-c/phc-winner-argon2

#![feature(test)]

extern crate test;
extern crate argon2rs;
extern crate cargon;

use argon2rs::{Argon2, defaults};
use argon2rs::Variant::Argon2i;
use std::ptr;

const PASSWORD: &'static [u8] = b"cargo bench --feature=simd";
const SALT: &'static [u8] = b"cargo test --release";

#[bench]
fn bench_argon2rs_i(b: &mut test::Bencher) {
    let a2 = Argon2::default(Argon2i);
    let mut out = [0; defaults::LENGTH];
    b.iter(|| a2.hash(&mut out, PASSWORD, SALT, &[], &[]));
}

#[bench]
fn bench_cargon_i(b: &mut test::Bencher) {
    let a2 = Argon2::default(Argon2i);
    let mut out = [0; defaults::LENGTH];
    let mut ctx = mk_cargon(a2, &mut out, PASSWORD, SALT, &[], &[]);
    b.iter(|| unsafe { cargon::argon2_ctx(&mut ctx, Argon2i as usize) });
}

#[bench]
fn bench_argon2rs_threaded(b: &mut test::Bencher) {
    let a2 = Argon2::new(defaults::PASSES, 4, defaults::KIB, Argon2i).unwrap();
    let mut out = [0; defaults::LENGTH];
    b.iter(|| a2.hash(&mut out, PASSWORD, SALT, &[], &[]));
}

#[bench]
fn bench_cargon_threaded(b: &mut test::Bencher) {
    let a2 = Argon2::new(defaults::PASSES, 4, defaults::KIB, Argon2i).unwrap();
    let mut out = [0; defaults::LENGTH];
    let mut ctx = mk_cargon(a2, &mut out, PASSWORD, SALT, &[], &[]);
    b.iter(|| unsafe { cargon::argon2_ctx(&mut ctx, Argon2i as usize) });
}

fn mk_cargon(a2: Argon2, out: &mut [u8], p: &[u8], s: &[u8], k: &[u8], x: &[u8])
             -> cargon::CargonContext {
    let (_, kib, passes, lanes) = a2.params();
    cargon::CargonContext {
        out: out.as_mut_ptr(),
        outlen: out.len() as u32,
        pwd: p.as_ptr(),
        pwdlen: p.len() as u32,
        salt: s.as_ptr(),
        saltlen: s.len() as u32,
        secret: k.as_ptr(),
        secretlen: k.len() as u32,
        ad: x.as_ptr(),
        adlen: x.len() as u32,

        t_cost: passes,
        m_cost: kib,
        lanes: lanes,
        threads: lanes,
        version: 0x10,
        allocate_fptr: ptr::null(),
        deallocate_fptr: ptr::null(),
        flags: cargon::ARGON2_FLAG_CLEAR_MEMORY,
    }
}

#[test]
fn ensure_identical_hashes() {
    fn comp(lanes: u32) {
        let mut outrs = [0; defaults::LENGTH];
        let mut outca = [0; defaults::LENGTH];
        let a2 = Argon2::new(defaults::PASSES, lanes, defaults::KIB, Argon2i)
            .unwrap();
        a2.hash(&mut outrs, PASSWORD, SALT, &[], &[]);

        let mut ctx = mk_cargon(a2, &mut outca, PASSWORD, SALT, &[], &[]);
        unsafe {
            cargon::argon2_ctx(&mut ctx, Argon2i as usize);
        }
        assert_eq!(outrs, outca);
    }
    comp(1);
    comp(4);
}
