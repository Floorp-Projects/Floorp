// SPDX-License-Identifier: MPL-2.0

//! Implementations of PRGs specified in [[draft-irtf-cfrg-vdaf-05]].
//!
//! [draft-irtf-cfrg-vdaf-05]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/

use crate::vdaf::{CodecError, Decode, Encode};
#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
use aes::{
    cipher::{generic_array::GenericArray, BlockEncrypt, KeyInit},
    Block,
};
#[cfg(feature = "crypto-dependencies")]
use aes::{
    cipher::{KeyIvInit, StreamCipher},
    Aes128,
};
#[cfg(feature = "crypto-dependencies")]
use cmac::{Cmac, Mac};
#[cfg(feature = "crypto-dependencies")]
use ctr::Ctr64BE;
use sha3::{
    digest::{ExtendableOutput, Update, XofReader},
    CShake128, CShake128Core, CShake128Reader,
};
#[cfg(feature = "crypto-dependencies")]
use std::fmt::Formatter;
use std::{
    fmt::Debug,
    io::{Cursor, Read},
};
use subtle::{Choice, ConstantTimeEq};

/// Input of [`Prg`].
#[derive(Clone, Debug, Eq)]
pub struct Seed<const SEED_SIZE: usize>(pub(crate) [u8; SEED_SIZE]);

impl<const SEED_SIZE: usize> Seed<SEED_SIZE> {
    /// Generate a uniform random seed.
    pub fn generate() -> Result<Self, getrandom::Error> {
        let mut seed = [0; SEED_SIZE];
        getrandom::getrandom(&mut seed)?;
        Ok(Self::from_bytes(seed))
    }

    pub(crate) fn from_bytes(seed: [u8; SEED_SIZE]) -> Self {
        Self(seed)
    }
}

impl<const SEED_SIZE: usize> AsRef<[u8; SEED_SIZE]> for Seed<SEED_SIZE> {
    fn as_ref(&self) -> &[u8; SEED_SIZE] {
        &self.0
    }
}

impl<const SEED_SIZE: usize> ConstantTimeEq for Seed<SEED_SIZE> {
    fn ct_eq(&self, other: &Self) -> Choice {
        self.0.ct_eq(&other.0)
    }
}

impl<const SEED_SIZE: usize> PartialEq for Seed<SEED_SIZE> {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl<const SEED_SIZE: usize> Encode for Seed<SEED_SIZE> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&self.0[..]);
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(SEED_SIZE)
    }
}

impl<const SEED_SIZE: usize> Decode for Seed<SEED_SIZE> {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let mut seed = [0; SEED_SIZE];
        bytes.read_exact(&mut seed)?;
        Ok(Seed(seed))
    }
}

/// A stream of pseudorandom bytes derived from a seed.
pub trait SeedStream {
    /// Fill `buf` with the next `buf.len()` bytes of output.
    fn fill(&mut self, buf: &mut [u8]);
}

/// A pseudorandom generator (PRG) with the interface specified in [[draft-irtf-cfrg-vdaf-05]].
///
/// [draft-irtf-cfrg-vdaf-05]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/
pub trait Prg<const SEED_SIZE: usize>: Clone + Debug {
    /// The type of stream produced by this PRG.
    type SeedStream: SeedStream;

    /// Construct an instance of [`Prg`] with the given seed.
    fn init(seed_bytes: &[u8; SEED_SIZE], custom: &[u8]) -> Self;

    /// Update the PRG state by passing in the next fragment of the info string. The final info
    /// string is assembled from the concatenation of sequence of fragments passed to this method.
    fn update(&mut self, data: &[u8]);

    /// Finalize the PRG state, producing a seed stream.
    fn into_seed_stream(self) -> Self::SeedStream;

    /// Finalize the PRG state, producing a seed.
    fn into_seed(self) -> Seed<SEED_SIZE> {
        let mut new_seed = [0; SEED_SIZE];
        let mut seed_stream = self.into_seed_stream();
        seed_stream.fill(&mut new_seed);
        Seed(new_seed)
    }

    /// Construct a seed stream from the given seed and info string.
    fn seed_stream(seed: &Seed<SEED_SIZE>, custom: &[u8], binder: &[u8]) -> Self::SeedStream {
        let mut prg = Self::init(seed.as_ref(), custom);
        prg.update(binder);
        prg.into_seed_stream()
    }
}

/// The PRG based on AES128 as specified in previous versions of draft-irtf-cfrg-vdaf.
///
/// This PRG has been removed as of [[draft-irtf-cfrg-vdaf-04]], and is deprecated. [`PrgSha3`]
/// should be used instead. cSHAKE128 is a safer choice than AES-128 for VDAFs that assume the PRG
/// acts like a random oracle.
///
/// [draft-irtf-cfrg-vdaf-04]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/04/
#[derive(Clone, Debug)]
#[cfg(feature = "crypto-dependencies")]
#[cfg_attr(docsrs, doc(cfg(feature = "crypto-dependencies")))]
#[deprecated(since = "0.11.0", note = "Superseded by PrgSha3")]
pub struct PrgAes128(Cmac<Aes128>);

#[cfg(feature = "crypto-dependencies")]
#[allow(deprecated)]
impl Prg<16> for PrgAes128 {
    type SeedStream = SeedStreamAes128;

    fn init(seed_bytes: &[u8; 16], custom: &[u8]) -> Self {
        let mut mac = <Cmac<_> as Mac>::new_from_slice(seed_bytes).unwrap();
        let custom_len = u16::try_from(custom.len()).expect("customization string is too long");
        Mac::update(&mut mac, &custom_len.to_be_bytes());
        Mac::update(&mut mac, custom);
        Self(mac)
    }

    fn update(&mut self, data: &[u8]) {
        Mac::update(&mut self.0, data);
    }

    fn into_seed_stream(self) -> SeedStreamAes128 {
        let key = self.0.finalize().into_bytes();
        SeedStreamAes128::new(&key, &[0; 16])
    }
}

/// The key stream produced by AES128 in CTR-mode.
#[cfg(feature = "crypto-dependencies")]
#[cfg_attr(docsrs, doc(cfg(feature = "crypto-dependencies")))]
pub struct SeedStreamAes128(Ctr64BE<Aes128>);

#[cfg(feature = "crypto-dependencies")]
impl SeedStreamAes128 {
    pub(crate) fn new(key: &[u8], iv: &[u8]) -> Self {
        SeedStreamAes128(<Ctr64BE<Aes128> as KeyIvInit>::new(key.into(), iv.into()))
    }
}

#[cfg(feature = "crypto-dependencies")]
impl SeedStream for SeedStreamAes128 {
    fn fill(&mut self, buf: &mut [u8]) {
        buf.fill(0);
        self.0.apply_keystream(buf);
    }
}

#[cfg(feature = "crypto-dependencies")]
impl Debug for SeedStreamAes128 {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        // Ctr64BE<Aes128> does not implement Debug, but [`ctr::CtrCore`][1] does, and we get that
        // with [`cipher::StreamCipherCoreWrapper::get_core`][2].
        //
        // [1]: https://docs.rs/ctr/latest/ctr/struct.CtrCore.html
        // [2]: https://docs.rs/cipher/latest/cipher/struct.StreamCipherCoreWrapper.html
        self.0.get_core().fmt(f)
    }
}

/// The PRG based on SHA-3 as specified in [[draft-irtf-cfrg-vdaf-05]].
///
/// [draft-irtf-cfrg-vdaf-05]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/
#[derive(Clone, Debug)]
pub struct PrgSha3(CShake128);

impl Prg<16> for PrgSha3 {
    type SeedStream = SeedStreamSha3;

    fn init(seed_bytes: &[u8; 16], custom: &[u8]) -> Self {
        let mut prg = Self(CShake128::from_core(CShake128Core::new(custom)));
        Update::update(&mut prg.0, seed_bytes);
        prg
    }

    fn update(&mut self, data: &[u8]) {
        Update::update(&mut self.0, data);
    }

    fn into_seed_stream(self) -> SeedStreamSha3 {
        SeedStreamSha3::new(self.0.finalize_xof())
    }
}

/// The key stream produced by the cSHAKE128 XOF.
pub struct SeedStreamSha3(CShake128Reader);

impl SeedStreamSha3 {
    pub(crate) fn new(reader: CShake128Reader) -> Self {
        Self(reader)
    }
}

impl SeedStream for SeedStreamSha3 {
    fn fill(&mut self, buf: &mut [u8]) {
        XofReader::read(&mut self.0, buf);
    }
}

/// Factory to produce multiple [`PrgFixedKeyAes128`] instances with the same fixed key and
/// different seeds.
#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
#[cfg_attr(
    docsrs,
    doc(cfg(all(feature = "crypto-dependencies", feature = "experimental")))
)]
pub struct PrgFixedKeyAes128Key {
    cipher: Aes128,
}

#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
impl PrgFixedKeyAes128Key {
    /// Derive a fixed key from the customization string and binder string.
    pub fn new(custom: &[u8], binder: &[u8]) -> Self {
        let mut deriver = CShake128::from_core(CShake128Core::new(custom));
        deriver.update(binder);
        let mut key = GenericArray::from([0; 16]);
        XofReader::read(&mut deriver.finalize_xof(), &mut key);
        Self {
            cipher: Aes128::new(&key),
        }
    }

    /// Combine a fixed key with a seed to produce a new stream of pseudorandom bytes.
    pub fn with_seed(&self, seed: &[u8; 16]) -> SeedStreamFixedKeyAes128 {
        SeedStreamFixedKeyAes128 {
            cipher: self.cipher.clone(),
            base_block: (*seed).into(),
            length_consumed: 0,
        }
    }
}

/// PrgFixedKeyAes128 as specified in [[draft-irtf-cfrg-vdaf-05]]. This PRG is NOT RECOMMENDED for
/// general use; see Section 9 ("Security Considerations") for details.
///
/// This PRG combines SHA-3 and a fixed-key mode of operation for AES-128. The key is "fixed" in
/// the sense that it is derived (using cSHAKE128) from the customization and binder strings, and
/// depending on the application, these strings can be hard-coded. The seed is used to construct
/// each block of input passed to a hash function built from AES-128.
///
/// [draft-irtf-cfrg-vdaf-05]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/
#[derive(Clone, Debug)]
#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
#[cfg_attr(
    docsrs,
    doc(cfg(all(feature = "crypto-dependencies", feature = "experimental")))
)]
pub struct PrgFixedKeyAes128 {
    fixed_key_deriver: CShake128,
    base_block: Block,
}

#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
impl Prg<16> for PrgFixedKeyAes128 {
    type SeedStream = SeedStreamFixedKeyAes128;

    fn init(seed_bytes: &[u8; 16], custom: &[u8]) -> Self {
        Self {
            fixed_key_deriver: CShake128::from_core(CShake128Core::new(custom)),
            base_block: (*seed_bytes).into(),
        }
    }

    fn update(&mut self, data: &[u8]) {
        Update::update(&mut self.fixed_key_deriver, data);
    }

    fn into_seed_stream(self) -> SeedStreamFixedKeyAes128 {
        let mut fixed_key = GenericArray::from([0; 16]);
        XofReader::read(&mut self.fixed_key_deriver.finalize_xof(), &mut fixed_key);
        SeedStreamFixedKeyAes128 {
            base_block: self.base_block,
            cipher: Aes128::new(&fixed_key),
            length_consumed: 0,
        }
    }
}

/// Seed stream for [`PrgFixedKeyAes128`].
#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
#[cfg_attr(
    docsrs,
    doc(cfg(all(feature = "crypto-dependencies", feature = "experimental")))
)]
pub struct SeedStreamFixedKeyAes128 {
    cipher: Aes128,
    base_block: Block,
    length_consumed: u64,
}

#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
impl SeedStream for SeedStreamFixedKeyAes128 {
    fn fill(&mut self, buf: &mut [u8]) {
        let next_length_consumed = self.length_consumed + u64::try_from(buf.len()).unwrap();
        let mut offset = usize::try_from(self.length_consumed % 16).unwrap();
        let mut index = 0;
        let mut block = Block::from([0; 16]);

        // NOTE(cjpatton) We might be able to speed this up by unrolling this loop and encrypting
        // multiple blocks at the same time via `self.cipher.encrypt_blocks()`.
        for block_counter in self.length_consumed / 16..(next_length_consumed + 15) / 16 {
            block.clone_from(&self.base_block);
            for (b, i) in block.iter_mut().zip(block_counter.to_le_bytes().iter()) {
                *b ^= i;
            }
            self.hash_block(&mut block);
            let read = std::cmp::min(16 - offset, buf.len() - index);
            buf[index..index + read].copy_from_slice(&block[offset..offset + read]);
            offset = 0;
            index += read;
        }

        self.length_consumed = next_length_consumed;
    }
}

#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
impl SeedStreamFixedKeyAes128 {
    fn hash_block(&self, block: &mut Block) {
        let sigma = Block::from([
            // hi
            block[8],
            block[9],
            block[10],
            block[11],
            block[12],
            block[13],
            block[14],
            block[15],
            // xor(hi, lo)
            block[8] ^ block[0],
            block[9] ^ block[1],
            block[10] ^ block[2],
            block[11] ^ block[3],
            block[12] ^ block[4],
            block[13] ^ block[5],
            block[14] ^ block[6],
            block[15] ^ block[7],
        ]);
        self.cipher.encrypt_block_b2b(&sigma, block);
        for (b, s) in block.iter_mut().zip(sigma.iter()) {
            *b ^= s;
        }
    }
}

/// Types implementing `CoinToss` can be randomly sampled from a [`SeedStream`].
pub trait CoinToss {
    /// Randomly generate an object using bytes from a PRG's output stream.
    fn sample<S>(seed_stream: &mut S) -> Self
    where
        S: SeedStream;
}

impl<const N: usize> CoinToss for [u8; N] {
    fn sample<S>(seed_stream: &mut S) -> Self
    where
        S: SeedStream,
    {
        let mut output = [0; N];
        seed_stream.fill(&mut output);
        output
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{field::Field128, prng::Prng};
    use serde::{Deserialize, Serialize};
    use std::{convert::TryInto, io::Cursor};

    #[derive(Deserialize, Serialize)]
    struct PrgTestVector {
        #[serde(with = "hex")]
        seed: Vec<u8>,
        #[serde(with = "hex")]
        custom: Vec<u8>,
        #[serde(with = "hex")]
        binder: Vec<u8>,
        length: usize,
        #[serde(with = "hex")]
        derived_seed: Vec<u8>,
        #[serde(with = "hex")]
        expanded_vec_field128: Vec<u8>,
    }

    // Test correctness of dervied methods.
    fn test_prg<P, const SEED_SIZE: usize>()
    where
        P: Prg<SEED_SIZE>,
    {
        let seed = Seed::generate().unwrap();
        let custom = b"algorithm and usage";
        let binder = b"bind to artifact";

        let mut prg = P::init(seed.as_ref(), custom);
        prg.update(binder);

        let mut want = Seed([0; SEED_SIZE]);
        prg.clone().into_seed_stream().fill(&mut want.0[..]);
        let got = prg.clone().into_seed();
        assert_eq!(got, want);

        let mut want = [0; 45];
        prg.clone().into_seed_stream().fill(&mut want);
        let mut got = [0; 45];
        P::seed_stream(&seed, custom, binder).fill(&mut got);
        assert_eq!(got, want);
    }

    #[test]
    #[allow(deprecated)]
    fn prg_aes128() {
        let t: PrgTestVector =
            serde_json::from_str(include_str!("test_vec/05/PrgAes128.json")).unwrap();
        let mut prg = PrgAes128::init(&t.seed.try_into().unwrap(), &t.custom);
        prg.update(&t.binder);

        assert_eq!(
            prg.clone().into_seed(),
            Seed(t.derived_seed.try_into().unwrap())
        );

        let mut bytes = Cursor::new(t.expanded_vec_field128.as_slice());
        let mut want = Vec::with_capacity(t.length);
        while (bytes.position() as usize) < t.expanded_vec_field128.len() {
            want.push(Field128::decode(&mut bytes).unwrap())
        }
        let got: Vec<Field128> = Prng::from_seed_stream(prg.clone().into_seed_stream())
            .take(t.length)
            .collect();
        assert_eq!(got, want);

        test_prg::<PrgAes128, 16>();
    }

    #[test]
    fn prg_sha3() {
        let t: PrgTestVector =
            serde_json::from_str(include_str!("test_vec/05/PrgSha3.json")).unwrap();
        let mut prg = PrgSha3::init(&t.seed.try_into().unwrap(), &t.custom);
        prg.update(&t.binder);

        assert_eq!(
            prg.clone().into_seed(),
            Seed(t.derived_seed.try_into().unwrap())
        );

        let mut bytes = Cursor::new(t.expanded_vec_field128.as_slice());
        let mut want = Vec::with_capacity(t.length);
        while (bytes.position() as usize) < t.expanded_vec_field128.len() {
            want.push(Field128::decode(&mut bytes).unwrap())
        }
        let got: Vec<Field128> = Prng::from_seed_stream(prg.clone().into_seed_stream())
            .take(t.length)
            .collect();
        assert_eq!(got, want);

        test_prg::<PrgSha3, 16>();
    }

    #[cfg(feature = "experimental")]
    #[test]
    fn prg_fixed_key_aes128() {
        let t: PrgTestVector =
            serde_json::from_str(include_str!("test_vec/05/PrgFixedKeyAes128.json")).unwrap();
        let mut prg = PrgFixedKeyAes128::init(&t.seed.try_into().unwrap(), &t.custom);
        prg.update(&t.binder);

        assert_eq!(
            prg.clone().into_seed(),
            Seed(t.derived_seed.try_into().unwrap())
        );

        let mut bytes = Cursor::new(t.expanded_vec_field128.as_slice());
        let mut want = Vec::with_capacity(t.length);
        while (bytes.position() as usize) < t.expanded_vec_field128.len() {
            want.push(Field128::decode(&mut bytes).unwrap())
        }
        let got: Vec<Field128> = Prng::from_seed_stream(prg.clone().into_seed_stream())
            .take(t.length)
            .collect();
        assert_eq!(got, want);

        test_prg::<PrgFixedKeyAes128, 16>();
    }

    #[cfg(feature = "experimental")]
    #[test]
    fn prg_fixed_key_aes128_incomplete_block() {
        let seed = Seed::generate().unwrap();
        let mut expected = [0; 32];
        PrgFixedKeyAes128::seed_stream(&seed, b"custom", b"binder").fill(&mut expected);

        for len in 0..=32 {
            let mut buf = vec![0; len];
            PrgFixedKeyAes128::seed_stream(&seed, b"custom", b"binder").fill(&mut buf);
            assert_eq!(buf, &expected[..len]);
        }
    }

    #[cfg(feature = "experimental")]
    #[test]
    fn prg_fixed_key_aes128_alternate_apis() {
        let custom = b"customization string";
        let binder = b"AAAAAAAAAAAAAAAAAAAAAAAA";
        let seed_1 = Seed::generate().unwrap();
        let seed_2 = Seed::generate().unwrap();

        let mut stream_1_trait_api = PrgFixedKeyAes128::seed_stream(&seed_1, custom, binder);
        let mut output_1_trait_api = [0u8; 32];
        stream_1_trait_api.fill(&mut output_1_trait_api);
        let mut stream_2_trait_api = PrgFixedKeyAes128::seed_stream(&seed_2, custom, binder);
        let mut output_2_trait_api = [0u8; 32];
        stream_2_trait_api.fill(&mut output_2_trait_api);

        let fixed_key = PrgFixedKeyAes128Key::new(custom, binder);
        let mut stream_1_alternate_api = fixed_key.with_seed(seed_1.as_ref());
        let mut output_1_alternate_api = [0u8; 32];
        stream_1_alternate_api.fill(&mut output_1_alternate_api);
        let mut stream_2_alternate_api = fixed_key.with_seed(seed_2.as_ref());
        let mut output_2_alternate_api = [0u8; 32];
        stream_2_alternate_api.fill(&mut output_2_alternate_api);

        assert_eq!(output_1_trait_api, output_1_alternate_api);
        assert_eq!(output_2_trait_api, output_2_alternate_api);
    }
}
