// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Deprecated re-exports (we can't add deprecation warnings otherwise)

#![allow(deprecated)]

use rngs;
use {RngCore, CryptoRng, SeedableRng, Error};
use rand_core::block::BlockRngCore;
use rand_isaac;
use rand_chacha;
use rand_hc;

#[cfg(feature="std")]
use std::io::Read;

#[derive(Clone, Debug)]
#[deprecated(since="0.6.0",
    note="import from rand_isaac crate instead, or use the newer Hc128Rng")]
pub struct IsaacRng(rand_isaac::IsaacRng);

impl RngCore for IsaacRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for IsaacRng {
    type Seed = <rand_isaac::IsaacRng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        IsaacRng(rand_isaac::IsaacRng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        rand_isaac::IsaacRng::from_rng(rng).map(IsaacRng)
    }
}

impl IsaacRng {
    pub fn new_from_u64(seed: u64) -> Self {
        IsaacRng(rand_isaac::IsaacRng::new_from_u64(seed))
    }
}


#[derive(Clone, Debug)]
#[deprecated(since="0.6.0",
    note="import from rand_isaac crate instead, or use newer Hc128Rng")]
pub struct Isaac64Rng(rand_isaac::Isaac64Rng);

impl RngCore for Isaac64Rng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for Isaac64Rng {
    type Seed = <rand_isaac::Isaac64Rng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        Isaac64Rng(rand_isaac::Isaac64Rng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        rand_isaac::Isaac64Rng::from_rng(rng).map(Isaac64Rng)
    }
}

impl Isaac64Rng {
    pub fn new_from_u64(seed: u64) -> Self {
        Isaac64Rng(rand_isaac::Isaac64Rng::new_from_u64(seed))
    }
}


#[derive(Clone, Debug)]
#[deprecated(since="0.6.0", note="import from rand_chacha crate instead")]
pub struct ChaChaRng(rand_chacha::ChaChaRng);

impl RngCore for ChaChaRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for ChaChaRng {
    type Seed = <rand_chacha::ChaChaRng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        ChaChaRng(rand_chacha::ChaChaRng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        rand_chacha::ChaChaRng::from_rng(rng).map(ChaChaRng)
    }
}

impl ChaChaRng {
    #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
    pub fn get_word_pos(&self) -> u128 {
        self.0.get_word_pos()
    }

    #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
    pub fn set_word_pos(&mut self, word_offset: u128) {
        self.0.set_word_pos(word_offset)
    }

    pub fn set_stream(&mut self, stream: u64) {
        self.0.set_stream(stream)
    }
}

impl CryptoRng for ChaChaRng {}


#[derive(Clone, Debug)]
#[deprecated(since="0.6.0", note="import from rand_hc crate instead")]
pub struct Hc128Rng(rand_hc::Hc128Rng);

impl RngCore for Hc128Rng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for Hc128Rng {
    type Seed = <rand_hc::Hc128Rng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        Hc128Rng(rand_hc::Hc128Rng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        rand_hc::Hc128Rng::from_rng(rng).map(Hc128Rng)
    }
}

impl CryptoRng for Hc128Rng {}


#[derive(Clone, Debug)]
#[deprecated(since="0.6.0", note="import from rand_xorshift crate instead")]
pub struct XorShiftRng(::rand_xorshift::XorShiftRng);

impl RngCore for XorShiftRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for XorShiftRng {
    type Seed = <::rand_xorshift::XorShiftRng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        XorShiftRng(::rand_xorshift::XorShiftRng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        ::rand_xorshift::XorShiftRng::from_rng(rng).map(XorShiftRng)
    }
}


#[derive(Clone, Debug)]
#[deprecated(since="0.6.0",
    note="import with rand::prelude::* or rand::rngs::StdRng instead")]
pub struct StdRng(rngs::StdRng);

impl RngCore for StdRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for StdRng {
    type Seed = <rngs::StdRng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        StdRng(rngs::StdRng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        rngs::StdRng::from_rng(rng).map(StdRng)
    }
}

impl CryptoRng for StdRng {}


#[cfg(feature="rand_os")]
#[derive(Clone, Debug)]
#[deprecated(since="0.6.0", note="import with rand::rngs::OsRng instead")]
pub struct OsRng(rngs::OsRng);

#[cfg(feature="rand_os")]
impl RngCore for OsRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

#[cfg(feature="rand_os")]
impl OsRng {
    pub fn new() -> Result<Self, Error> {
        rngs::OsRng::new().map(OsRng)
    }
}

#[cfg(feature="rand_os")]
impl CryptoRng for OsRng {}


#[cfg(feature="std")]
#[derive(Debug)]
#[deprecated(since="0.6.0", note="import with rand::rngs::EntropyRng instead")]
pub struct EntropyRng(rngs::EntropyRng);

#[cfg(feature="std")]
impl RngCore for EntropyRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

#[cfg(feature="std")]
impl EntropyRng {
    pub fn new() -> Self {
        EntropyRng(rngs::EntropyRng::new())
    }
}

#[cfg(feature="std")]
impl Default for EntropyRng {
    fn default() -> Self {
        EntropyRng::new()
    }
}

#[cfg(feature="std")]
impl CryptoRng for EntropyRng {}


#[derive(Clone, Debug)]
#[deprecated(since="0.6.0", note="import with rand::rngs::JitterRng instead")]
pub struct JitterRng(rngs::JitterRng);

impl RngCore for JitterRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl JitterRng {
    #[cfg(all(feature="std", not(target_arch = "wasm32")))]
    pub fn new() -> Result<JitterRng, rngs::TimerError> {
        rngs::JitterRng::new().map(JitterRng)
    }

    pub fn new_with_timer(timer: fn() -> u64) -> JitterRng {
        JitterRng(rngs::JitterRng::new_with_timer(timer))
    }

    pub fn set_rounds(&mut self, rounds: u8) {
        self.0.set_rounds(rounds)
    }

    pub fn test_timer(&mut self) -> Result<u8, rngs::TimerError> {
        self.0.test_timer()
    }

    #[cfg(feature="std")]
    pub fn timer_stats(&mut self, var_rounds: bool) -> i64 {
        self.0.timer_stats(var_rounds)
    }
}

impl CryptoRng for JitterRng {}


#[cfg(feature="std")]
#[derive(Clone, Debug)]
#[deprecated(since="0.6.0",
    note="import with rand::prelude::* or rand::rngs::ThreadRng instead")]
pub struct ThreadRng(rngs::ThreadRng);

#[cfg(feature="std")]
impl RngCore for ThreadRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

#[cfg(feature="std")]
impl CryptoRng for ThreadRng {}


#[cfg(feature="std")]
#[derive(Debug)]
#[deprecated(since="0.6.0", note="import with rand::rngs::adapter::ReadRng instead")]
pub struct ReadRng<R>(rngs::adapter::ReadRng<R>);

#[cfg(feature="std")]
impl<R: Read> RngCore for ReadRng<R> {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline(always)]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    #[inline(always)]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

#[cfg(feature="std")]
impl<R: Read> ReadRng<R> {
    pub fn new(r: R) -> ReadRng<R> {
        ReadRng(rngs::adapter::ReadRng::new(r))
    }
}


#[derive(Clone, Debug)]
pub struct ReseedingRng<R, Rsdr>(rngs::adapter::ReseedingRng<R, Rsdr>)
where R: BlockRngCore + SeedableRng,
      Rsdr: RngCore;

impl<R, Rsdr: RngCore> RngCore for ReseedingRng<R, Rsdr>
where R: BlockRngCore<Item = u32> + SeedableRng,
    <R as BlockRngCore>::Results: AsRef<[u32]> + AsMut<[u32]>
{
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest)
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl<R, Rsdr> ReseedingRng<R, Rsdr>
where R: BlockRngCore + SeedableRng,
      Rsdr: RngCore
{
    pub fn new(rng: R, threshold: u64, reseeder: Rsdr) -> Self {
        ReseedingRng(rngs::adapter::ReseedingRng::new(rng, threshold, reseeder))
    }

    pub fn reseed(&mut self) -> Result<(), Error> {
        self.0.reseed()
    }
}

impl<R, Rsdr> CryptoRng for ReseedingRng<R, Rsdr>
where R: BlockRngCore + SeedableRng + CryptoRng,
      Rsdr: RngCore + CryptoRng {}
