#![feature(cfg_target_feature)]
extern crate simd;
#[macro_use] extern crate cfg_if;
use simd::u8x16;

use std::{env, process};

cfg_if! {
    if #[cfg(target_arch = "aarch64")] {
        #[inline(always)]
        fn shuffle(x: u8x16, y: u8x16) -> u8x16 {
            use simd::aarch64::neon::*;
            y.table_lookup_1(x)
        }
    } else if #[cfg(all(target_arch = "arm",
                 target_feature = "neon"))] {
        #[inline(always)]
        fn shuffle(x: u8x16, y: u8x16) -> u8x16 {
            use simd::arm::neon::*;
            #[inline(always)]
            fn split(x: u8x16) -> (u8x8, u8x8) {
                unsafe {std::mem::transmute(x)}
            }
            fn join(x: u8x8, y: u8x8) -> u8x16 {
                unsafe {std::mem::transmute((x, y))}
            }

            let (t0, t1) = split(x);
            let (i0, i1) = split(y);
            join(i0.table_lookup_2(t0, t1),
                 i1.table_lookup_2(t0, t1))
        }
    } else if #[cfg(target_feature = "ssse3")] {
        #[inline(always)]
        fn shuffle(x: u8x16, y: u8x16) -> u8x16 {
            use simd::x86::ssse3::*;
            x.shuffle_bytes(y)
        }
    } else {
        // slow fallback, so tests work
        #[inline(always)]
        fn shuffle(x: u8x16, y: u8x16) -> u8x16 {
            u8x16::new(x.extract(y.extract(0) as u32),
                       x.extract(y.extract(1) as u32),
                       x.extract(y.extract(2) as u32),
                       x.extract(y.extract(3) as u32),
                       x.extract(y.extract(4) as u32),
                       x.extract(y.extract(5) as u32),
                       x.extract(y.extract(6) as u32),
                       x.extract(y.extract(7) as u32),
                       x.extract(y.extract(8) as u32),
                       x.extract(y.extract(9) as u32),
                       x.extract(y.extract(10) as u32),
                       x.extract(y.extract(11) as u32),
                       x.extract(y.extract(12) as u32),
                       x.extract(y.extract(13) as u32),
                       x.extract(y.extract(14) as u32),
                       x.extract(y.extract(15) as u32))
        }
    }
}
struct State {
    s: [u8; 16],
    flip_masks: [u8x16; 16],
    rotate_masks: [u8x16; 16],

    maxflips: i32,
    odd: u16,
    checksum: i32,
}
impl State {
    fn new() -> State {
        State {
            s: [0; 16],
            flip_masks: [u8x16::splat(0); 16],
            rotate_masks: [u8x16::splat(0); 16],

            maxflips: 0,
            odd: 0,
            checksum: 0,
        }
    }
    #[inline(never)]
    fn rotate_sisd(&mut self, n: usize) {
        let c = self.s[0];
        for i in 1..(n + 1) {
            self.s[i - 1] = self.s[i];
        }
        self.s[n] = c;
    }
    #[inline(never)]
    fn popmasks(&mut self) {
        let mut mask = [0_u8; 16];
        for i in 0..16 {
            for j in 0..16 { mask[j] = j as u8; }

            for x in 0..(i+1)/2 {
                mask.swap(x, i - x);
            }

            self.flip_masks[i] = u8x16::load(&mask, 0);

            for j in 0..16 { self.s[j] = j as u8; }
            self.rotate_sisd(i);
            self.rotate_masks[i] = self.load_s();
        }
    }
    fn rotate(&mut self, n: usize) {
        shuffle(self.load_s(), self.rotate_masks[n]).store(&mut self.s, 0)
    }

    fn load_s(&self) -> u8x16 {
        u8x16::load(&self.s, 0)
    }


    #[inline(never)]
    fn tk(&mut self, n: usize) {
        #[derive(Copy, Clone, Debug)]
        struct Perm {
            perm: u8x16,
            start: u8,
            odd: u16
        }

        let mut perms = [Perm { perm: u8x16::splat(0), start: 0 , odd: 0 }; 60];

        let mut i = 0;
        let mut c = [0_u8; 16];
        let mut perm_max = 0;

        while i < n {
            while i < n && perm_max < 60 {
                self.rotate(i);
                if c[i] as usize >= i {
                    c[i] = 0;
                    i += 1;
                    continue
                }

                c[i] += 1;
                i = 1;
                self.odd = !self.odd;
                if self.s[0] != 0 {
                    if self.s[self.s[0] as usize] != 0 {
                        perms[perm_max].perm = self.load_s();
                        perms[perm_max].start = self.s[0];
                        perms[perm_max].odd = self.odd;
                        perm_max += 1;
                    } else {
                        if self.maxflips == 0 { self.maxflips = 1 }
                        self.checksum += if self.odd != 0 { -1 } else { 1 };
                    }
                }
            }

            let mut k = 0;
            while k < std::cmp::max(1, perm_max) - 1 {
                let pk = &perms[k];
                let pk1 = &perms[k + 1];
                //println!("perm1 {:?}\nperm2 {:?}", pk.perm, pk1.perm);
                let mut perm1 = pk.perm;
                let mut perm2 = pk1.perm;

                let mut f1 = 0;
                let mut f2 = 0;
                let mut toterm1 = pk.start;
                let mut toterm2 = pk1.start;

                while toterm1 != 0 && toterm2 != 0 {
                    perm1 = shuffle(perm1, self.flip_masks[toterm1 as usize]);
                    perm2 = shuffle(perm2, self.flip_masks[toterm2 as usize]);
                    toterm1 = perm1.extract(0);
                    toterm2 = perm2.extract(0);

                    f1 += 1; f2 += 1;
                }
                while toterm1 != 0 {
                    perm1 = shuffle(perm1, self.flip_masks[toterm1 as usize]);
                    toterm1 = perm1.extract(0);
                    f1 += 1;
                }
                while toterm2 != 0 {
                    perm2 = shuffle(perm2, self.flip_masks[toterm2 as usize]);
                    toterm2 = perm2.extract(0);
                    f2 += 1;
                }

                if f1 > self.maxflips { self.maxflips = f1 }
                if f2 > self.maxflips { self.maxflips = f2 }
                self.checksum += if pk.odd != 0 { -f1 } else { f1 };
                self.checksum += if pk1.odd != 0 { -f2 } else { f2 };

                k += 2;
            }
            while k < perm_max {
                let pk = &perms[k];
                let mut perm = pk.perm;
                let mut f = 0;
                let mut toterm = pk.start;
                while toterm != 0 {
                    perm = shuffle(perm, self.flip_masks[toterm as usize]);
                    toterm = perm.extract(0);
                    f += 1;
                }
                if f > self.maxflips { self.maxflips = f }
                self.checksum += if pk.odd != 0 { -f } else { f };
                k += 1
            }
            perm_max = 0;
        }
    }
}

fn main() {
    let mut state = State::new();
    state.popmasks();

    let args = env::args().collect::<Vec<_>>();
    if args.len() < 2 {
        println!("usage: {} number", args[0]);
        process::exit(1)
    }
    let max_n = args[1].parse().unwrap();
    if max_n < 3 || max_n > 15 {
        println!("range: must be 3 <= n <= 14");
        process::exit(1);
    }
    for i in 0..max_n { state.s[i] = i as u8 }
    state.tk(max_n);

    println!("{}\nPfannkuchen({}) = {}", state.checksum, max_n, state.maxflips);
}
