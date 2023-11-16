//! This module implements the incremental distributed point function (IDPF) described in
//! [[draft-irtf-cfrg-vdaf-07]].
//!
//! [draft-irtf-cfrg-vdaf-07]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/07/

use crate::{
    codec::{CodecError, Decode, Encode, ParameterizedDecode},
    field::{FieldElement, FieldElementExt},
    vdaf::{
        xof::{Seed, XofFixedKeyAes128Key},
        VdafError, VERSION,
    },
};
use bitvec::{
    bitvec,
    boxed::BitBox,
    prelude::{Lsb0, Msb0},
    slice::BitSlice,
    vec::BitVec,
    view::BitView,
};
use rand_core::RngCore;
use std::{
    collections::{HashMap, VecDeque},
    fmt::Debug,
    io::{Cursor, Read},
    ops::{Add, AddAssign, ControlFlow, Index, Sub},
};
use subtle::{Choice, ConditionallyNegatable, ConditionallySelectable, ConstantTimeEq};

/// IDPF-related errors.
#[derive(Debug, thiserror::Error)]
pub enum IdpfError {
    /// Error from incompatible shares at different levels.
    #[error("tried to merge shares from incompatible levels")]
    MismatchedLevel,

    /// Invalid parameter, indicates an invalid input to either [`Idpf::gen`] or [`Idpf::eval`].
    #[error("invalid parameter: {0}")]
    InvalidParameter(String),
}

/// An index used as the input to an IDPF evaluation.
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct IdpfInput {
    /// The index as a boxed bit slice.
    index: BitBox,
}

impl IdpfInput {
    /// Convert a slice of bytes into an IDPF input, where the bits of each byte are processed in
    /// MSB-to-LSB order. (Subsequent bytes are processed in their natural order.)
    pub fn from_bytes(bytes: &[u8]) -> IdpfInput {
        let bit_slice_u8_storage = bytes.view_bits::<Msb0>();
        let mut bit_vec_usize_storage = bitvec![0; bit_slice_u8_storage.len()];
        bit_vec_usize_storage.clone_from_bitslice(bit_slice_u8_storage);
        IdpfInput {
            index: bit_vec_usize_storage.into_boxed_bitslice(),
        }
    }

    /// Convert a slice of booleans into an IDPF input.
    pub fn from_bools(bools: &[bool]) -> IdpfInput {
        let bits = bools.iter().collect::<BitVec>();
        IdpfInput {
            index: bits.into_boxed_bitslice(),
        }
    }

    /// Create a new IDPF input by appending to this input.
    pub fn clone_with_suffix(&self, suffix: &[bool]) -> IdpfInput {
        let mut vec = BitVec::with_capacity(self.index.len() + suffix.len());
        vec.extend_from_bitslice(&self.index);
        vec.extend(suffix);
        IdpfInput {
            index: vec.into_boxed_bitslice(),
        }
    }

    /// Get the length of the input in bits.
    pub fn len(&self) -> usize {
        self.index.len()
    }

    /// Check if the input is empty, i.e. it does not contain any bits.
    pub fn is_empty(&self) -> bool {
        self.index.is_empty()
    }

    /// Get an iterator over the bits that make up this input.
    pub fn iter(&self) -> impl DoubleEndedIterator<Item = bool> + '_ {
        self.index.iter().by_vals()
    }

    /// Convert the IDPF into a byte slice. If the length of the underlying bit vector is not a
    /// multiple of `8`, then the least significant bits of the last byte are `0`-padded.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut vec = BitVec::<u8, Msb0>::with_capacity(self.index.len());
        vec.extend_from_bitslice(&self.index);
        vec.set_uninitialized(false);
        vec.into_vec()
    }

    /// Return the `level`-bit prefix of this IDPF input.
    pub fn prefix(&self, level: usize) -> Self {
        Self {
            index: self.index[..=level].to_owned().into(),
        }
    }
}

impl From<BitVec<usize, Lsb0>> for IdpfInput {
    fn from(bit_vec: BitVec<usize, Lsb0>) -> Self {
        IdpfInput {
            index: bit_vec.into_boxed_bitslice(),
        }
    }
}

impl From<BitBox<usize, Lsb0>> for IdpfInput {
    fn from(bit_box: BitBox<usize, Lsb0>) -> Self {
        IdpfInput { index: bit_box }
    }
}

impl<I> Index<I> for IdpfInput
where
    BitSlice: Index<I>,
{
    type Output = <BitSlice as Index<I>>::Output;

    fn index(&self, index: I) -> &Self::Output {
        &self.index[index]
    }
}

/// Trait for values to be programmed into an IDPF.
///
/// Values must form an Abelian group, so that they can be secret-shared, and the group operation
/// must be represented by [`Add`]. Values must be encodable and decodable, without need for a
/// decoding parameter. Values can be pseudorandomly generated, with a uniform probability
/// distribution, from XOF output.
pub trait IdpfValue:
    Add<Output = Self>
    + AddAssign
    + Sub<Output = Self>
    + ConditionallyNegatable
    + Encode
    + Decode
    + Sized
{
    /// Any run-time parameters needed to produce a value.
    type ValueParameter;

    /// Generate a pseudorandom value from a seed stream.
    fn generate<S>(seed_stream: &mut S, parameter: &Self::ValueParameter) -> Self
    where
        S: RngCore;

    /// Returns the additive identity.
    fn zero(parameter: &Self::ValueParameter) -> Self;

    /// Conditionally select between two values. Implementations must perform this operation in
    /// constant time.
    ///
    /// This is the same as in [`subtle::ConditionallySelectable`], but without the [`Copy`] bound.
    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self;
}

impl<F> IdpfValue for F
where
    F: FieldElement,
{
    type ValueParameter = ();

    fn generate<S>(seed_stream: &mut S, _: &()) -> Self
    where
        S: RngCore,
    {
        // This is analogous to `Prng::get()`, but does not make use of a persistent buffer of
        // output.
        let mut buffer = [0u8; 64];
        assert!(
            buffer.len() >= F::ENCODED_SIZE,
            "field is too big for buffer"
        );
        loop {
            seed_stream.fill_bytes(&mut buffer[..F::ENCODED_SIZE]);
            match F::from_random_rejection(&buffer[..F::ENCODED_SIZE]) {
                ControlFlow::Break(x) => return x,
                ControlFlow::Continue(()) => continue,
            }
        }
    }

    fn zero(_: &()) -> Self {
        <Self as FieldElement>::zero()
    }

    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        <F as ConditionallySelectable>::conditional_select(a, b, choice)
    }
}

/// An output from evaluation of an IDPF at some level and index.
#[derive(Debug, PartialEq, Eq)]
pub enum IdpfOutputShare<VI, VL> {
    /// An IDPF output share corresponding to an inner tree node.
    Inner(VI),
    /// An IDPF output share corresponding to a leaf tree node.
    Leaf(VL),
}

impl<VI, VL> IdpfOutputShare<VI, VL>
where
    VI: IdpfValue,
    VL: IdpfValue,
{
    /// Combine two output share values into one.
    pub fn merge(self, other: Self) -> Result<IdpfOutputShare<VI, VL>, IdpfError> {
        match (self, other) {
            (IdpfOutputShare::Inner(mut self_value), IdpfOutputShare::Inner(other_value)) => {
                self_value += other_value;
                Ok(IdpfOutputShare::Inner(self_value))
            }
            (IdpfOutputShare::Leaf(mut self_value), IdpfOutputShare::Leaf(other_value)) => {
                self_value += other_value;
                Ok(IdpfOutputShare::Leaf(self_value))
            }
            (_, _) => Err(IdpfError::MismatchedLevel),
        }
    }
}

fn extend(seed: &[u8; 16], xof_fixed_key: &XofFixedKeyAes128Key) -> ([[u8; 16]; 2], [Choice; 2]) {
    let mut seed_stream = xof_fixed_key.with_seed(seed);

    let mut seeds = [[0u8; 16], [0u8; 16]];
    seed_stream.fill_bytes(&mut seeds[0]);
    seed_stream.fill_bytes(&mut seeds[1]);

    let mut byte = [0u8];
    seed_stream.fill_bytes(&mut byte);
    let control_bits = [(byte[0] & 1).into(), ((byte[0] >> 1) & 1).into()];

    (seeds, control_bits)
}

fn convert<V>(
    seed: &[u8; 16],
    xof_fixed_key: &XofFixedKeyAes128Key,
    parameter: &V::ValueParameter,
) -> ([u8; 16], V)
where
    V: IdpfValue,
{
    let mut seed_stream = xof_fixed_key.with_seed(seed);

    let mut next_seed = [0u8; 16];
    seed_stream.fill_bytes(&mut next_seed);

    (next_seed, V::generate(&mut seed_stream, parameter))
}

/// Helper method to update seeds, update control bits, and output the correction word for one level
/// of the IDPF key generation process.
fn generate_correction_word<V>(
    input_bit: Choice,
    value: V,
    parameter: &V::ValueParameter,
    keys: &mut [[u8; 16]; 2],
    control_bits: &mut [Choice; 2],
    extend_xof_fixed_key: &XofFixedKeyAes128Key,
    convert_xof_fixed_key: &XofFixedKeyAes128Key,
) -> IdpfCorrectionWord<V>
where
    V: IdpfValue,
{
    // Expand both keys into two seeds and two control bits each.
    let (seed_0, control_bits_0) = extend(&keys[0], extend_xof_fixed_key);
    let (seed_1, control_bits_1) = extend(&keys[1], extend_xof_fixed_key);

    let (keep, lose) = (input_bit, !input_bit);

    let cw_seed = xor_seeds(
        &conditional_select_seed(lose, &seed_0),
        &conditional_select_seed(lose, &seed_1),
    );
    let cw_control_bits = [
        control_bits_0[0] ^ control_bits_1[0] ^ input_bit ^ Choice::from(1),
        control_bits_0[1] ^ control_bits_1[1] ^ input_bit,
    ];
    let cw_control_bits_keep =
        Choice::conditional_select(&cw_control_bits[0], &cw_control_bits[1], keep);

    let previous_control_bits = *control_bits;
    let control_bits_0_keep =
        Choice::conditional_select(&control_bits_0[0], &control_bits_0[1], keep);
    let control_bits_1_keep =
        Choice::conditional_select(&control_bits_1[0], &control_bits_1[1], keep);
    control_bits[0] = control_bits_0_keep ^ (cw_control_bits_keep & previous_control_bits[0]);
    control_bits[1] = control_bits_1_keep ^ (cw_control_bits_keep & previous_control_bits[1]);

    let seed_0_keep = conditional_select_seed(keep, &seed_0);
    let seed_1_keep = conditional_select_seed(keep, &seed_1);
    let seeds_corrected = [
        conditional_xor_seeds(&seed_0_keep, &cw_seed, previous_control_bits[0]),
        conditional_xor_seeds(&seed_1_keep, &cw_seed, previous_control_bits[1]),
    ];

    let (new_key_0, elements_0) =
        convert::<V>(&seeds_corrected[0], convert_xof_fixed_key, parameter);
    let (new_key_1, elements_1) =
        convert::<V>(&seeds_corrected[1], convert_xof_fixed_key, parameter);

    keys[0] = new_key_0;
    keys[1] = new_key_1;

    let mut cw_value = value - elements_0 + elements_1;
    cw_value.conditional_negate(control_bits[1]);

    IdpfCorrectionWord {
        seed: cw_seed,
        control_bits: cw_control_bits,
        value: cw_value,
    }
}

/// Helper function to evaluate one level of an IDPF. This updates the seed and control bit
/// arguments that are passed in.
#[allow(clippy::too_many_arguments)]
fn eval_next<V>(
    is_leader: bool,
    parameter: &V::ValueParameter,
    key: &mut [u8; 16],
    control_bit: &mut Choice,
    correction_word: &IdpfCorrectionWord<V>,
    input_bit: Choice,
    extend_xof_fixed_key: &XofFixedKeyAes128Key,
    convert_xof_fixed_key: &XofFixedKeyAes128Key,
) -> V
where
    V: IdpfValue,
{
    let (mut seeds, mut control_bits) = extend(key, extend_xof_fixed_key);

    seeds[0] = conditional_xor_seeds(&seeds[0], &correction_word.seed, *control_bit);
    control_bits[0] ^= correction_word.control_bits[0] & *control_bit;
    seeds[1] = conditional_xor_seeds(&seeds[1], &correction_word.seed, *control_bit);
    control_bits[1] ^= correction_word.control_bits[1] & *control_bit;

    let seed_corrected = conditional_select_seed(input_bit, &seeds);
    *control_bit = Choice::conditional_select(&control_bits[0], &control_bits[1], input_bit);

    let (new_key, elements) = convert::<V>(&seed_corrected, convert_xof_fixed_key, parameter);
    *key = new_key;

    let mut out =
        elements + V::conditional_select(&V::zero(parameter), &correction_word.value, *control_bit);
    out.conditional_negate(Choice::from((!is_leader) as u8));
    out
}

/// This defines a family of IDPFs (incremental distributed point functions) with certain types of
/// values at inner tree nodes and at leaf tree nodes.
///
/// IDPF keys can be generated by providing an input and programmed outputs for each tree level to
/// [`Idpf::gen`].
pub struct Idpf<VI, VL>
where
    VI: IdpfValue,
    VL: IdpfValue,
{
    inner_node_value_parameter: VI::ValueParameter,
    leaf_node_value_parameter: VL::ValueParameter,
}

impl<VI, VL> Idpf<VI, VL>
where
    VI: IdpfValue,
    VL: IdpfValue,
{
    /// Construct an [`Idpf`] instance with the given run-time parameters needed for inner and leaf
    /// values.
    pub fn new(
        inner_node_value_parameter: VI::ValueParameter,
        leaf_node_value_parameter: VL::ValueParameter,
    ) -> Self {
        Self {
            inner_node_value_parameter,
            leaf_node_value_parameter,
        }
    }

    pub(crate) fn gen_with_random<M: IntoIterator<Item = VI>>(
        &self,
        input: &IdpfInput,
        inner_values: M,
        leaf_value: VL,
        binder: &[u8],
        random: &[[u8; 16]; 2],
    ) -> Result<(IdpfPublicShare<VI, VL>, [Seed<16>; 2]), VdafError> {
        let bits = input.len();

        let initial_keys: [Seed<16>; 2] =
            [Seed::from_bytes(random[0]), Seed::from_bytes(random[1])];

        let extend_dst = [
            VERSION, 1, /* algorithm class */
            0, 0, 0, 0, /* algorithm ID */
            0, 0, /* usage */
        ];
        let convert_dst = [
            VERSION, 1, /* algorithm class */
            0, 0, 0, 0, /* algorithm ID */
            0, 1, /* usage */
        ];
        let extend_xof_fixed_key = XofFixedKeyAes128Key::new(&extend_dst, binder);
        let convert_xof_fixed_key = XofFixedKeyAes128Key::new(&convert_dst, binder);

        let mut keys = [initial_keys[0].0, initial_keys[1].0];
        let mut control_bits = [Choice::from(0u8), Choice::from(1u8)];
        let mut inner_correction_words = Vec::with_capacity(bits - 1);

        for (level, value) in inner_values.into_iter().enumerate() {
            if level >= bits - 1 {
                return Err(IdpfError::InvalidParameter(
                    "too many values were supplied".to_string(),
                )
                .into());
            }
            inner_correction_words.push(generate_correction_word::<VI>(
                Choice::from(input[level] as u8),
                value,
                &self.inner_node_value_parameter,
                &mut keys,
                &mut control_bits,
                &extend_xof_fixed_key,
                &convert_xof_fixed_key,
            ));
        }
        if inner_correction_words.len() != bits - 1 {
            return Err(
                IdpfError::InvalidParameter("too few values were supplied".to_string()).into(),
            );
        }
        let leaf_correction_word = generate_correction_word::<VL>(
            Choice::from(input[bits - 1] as u8),
            leaf_value,
            &self.leaf_node_value_parameter,
            &mut keys,
            &mut control_bits,
            &extend_xof_fixed_key,
            &convert_xof_fixed_key,
        );
        let public_share = IdpfPublicShare {
            inner_correction_words,
            leaf_correction_word,
        };

        Ok((public_share, initial_keys))
    }

    /// The IDPF key generation algorithm.
    ///
    /// Generate and return a sequence of IDPF shares for `input`. The parameters `inner_values`
    /// and `leaf_value` provide the output values for each successive level of the prefix tree.
    pub fn gen<M>(
        &self,
        input: &IdpfInput,
        inner_values: M,
        leaf_value: VL,
        binder: &[u8],
    ) -> Result<(IdpfPublicShare<VI, VL>, [Seed<16>; 2]), VdafError>
    where
        M: IntoIterator<Item = VI>,
    {
        if input.is_empty() {
            return Err(
                IdpfError::InvalidParameter("invalid number of bits: 0".to_string()).into(),
            );
        }
        let mut random = [[0u8; 16]; 2];
        for random_seed in random.iter_mut() {
            getrandom::getrandom(random_seed)?;
        }
        self.gen_with_random(input, inner_values, leaf_value, binder, &random)
    }

    /// Evaluate an IDPF share on `prefix`, starting from a particular tree level with known
    /// intermediate values.
    #[allow(clippy::too_many_arguments)]
    fn eval_from_node(
        &self,
        is_leader: bool,
        public_share: &IdpfPublicShare<VI, VL>,
        start_level: usize,
        mut key: [u8; 16],
        mut control_bit: Choice,
        prefix: &IdpfInput,
        binder: &[u8],
        cache: &mut dyn IdpfCache,
    ) -> Result<IdpfOutputShare<VI, VL>, IdpfError> {
        let bits = public_share.inner_correction_words.len() + 1;

        let extend_dst = [
            VERSION, 1, /* algorithm class */
            0, 0, 0, 0, /* algorithm ID */
            0, 0, /* usage */
        ];
        let convert_dst = [
            VERSION, 1, /* algorithm class */
            0, 0, 0, 0, /* algorithm ID */
            0, 1, /* usage */
        ];
        let extend_xof_fixed_key = XofFixedKeyAes128Key::new(&extend_dst, binder);
        let convert_xof_fixed_key = XofFixedKeyAes128Key::new(&convert_dst, binder);

        let mut last_inner_output = None;
        for ((correction_word, input_bit), level) in public_share.inner_correction_words
            [start_level..]
            .iter()
            .zip(prefix[start_level..].iter())
            .zip(start_level..)
        {
            last_inner_output = Some(eval_next(
                is_leader,
                &self.inner_node_value_parameter,
                &mut key,
                &mut control_bit,
                correction_word,
                Choice::from(*input_bit as u8),
                &extend_xof_fixed_key,
                &convert_xof_fixed_key,
            ));
            let cache_key = &prefix[..=level];
            cache.insert(cache_key, &(key, control_bit.unwrap_u8()));
        }

        if prefix.len() == bits {
            let leaf_output = eval_next(
                is_leader,
                &self.leaf_node_value_parameter,
                &mut key,
                &mut control_bit,
                &public_share.leaf_correction_word,
                Choice::from(prefix[bits - 1] as u8),
                &extend_xof_fixed_key,
                &convert_xof_fixed_key,
            );
            // Note: there's no point caching this node's key, because we will always run the
            // eval_next() call for the leaf level.
            Ok(IdpfOutputShare::Leaf(leaf_output))
        } else {
            Ok(IdpfOutputShare::Inner(last_inner_output.unwrap()))
        }
    }

    /// The IDPF key evaluation algorithm.
    ///
    /// Evaluate an IDPF share on `prefix`.
    pub fn eval(
        &self,
        agg_id: usize,
        public_share: &IdpfPublicShare<VI, VL>,
        key: &Seed<16>,
        prefix: &IdpfInput,
        binder: &[u8],
        cache: &mut dyn IdpfCache,
    ) -> Result<IdpfOutputShare<VI, VL>, IdpfError> {
        let bits = public_share.inner_correction_words.len() + 1;
        if agg_id > 1 {
            return Err(IdpfError::InvalidParameter(format!(
                "invalid aggregator ID {agg_id}"
            )));
        }
        let is_leader = agg_id == 0;
        if prefix.is_empty() {
            return Err(IdpfError::InvalidParameter("empty prefix".to_string()));
        }
        if prefix.len() > bits {
            return Err(IdpfError::InvalidParameter(format!(
                "prefix length ({}) exceeds configured number of bits ({})",
                prefix.len(),
                bits,
            )));
        }

        // Check for cached keys first, starting from the end of our desired path down the tree, and
        // walking back up. If we get a hit, stop there and evaluate the remainder of the tree path
        // going forward.
        if prefix.len() > 1 {
            // Skip checking for `prefix` in the cache, because we don't store field element
            // values along with keys and control bits. Instead, start looking one node higher
            // up, so we can recompute everything for the last level of `prefix`.
            let mut cache_key = &prefix[..prefix.len() - 1];
            while !cache_key.is_empty() {
                if let Some((key, control_bit)) = cache.get(cache_key) {
                    // Evaluate the IDPF starting from the cached data at a previously-computed
                    // node, and return the result.
                    return self.eval_from_node(
                        is_leader,
                        public_share,
                        /* start_level */ cache_key.len(),
                        key,
                        Choice::from(control_bit),
                        prefix,
                        binder,
                        cache,
                    );
                }
                cache_key = &cache_key[..cache_key.len() - 1];
            }
        }
        // Evaluate starting from the root node.
        self.eval_from_node(
            is_leader,
            public_share,
            /* start_level */ 0,
            key.0,
            /* control_bit */ Choice::from((!is_leader) as u8),
            prefix,
            binder,
            cache,
        )
    }
}

/// An IDPF public share. This contains the list of correction words used by all parties when
/// evaluating the IDPF.
#[derive(Debug, Clone)]
pub struct IdpfPublicShare<VI, VL> {
    /// Correction words for each inner node level.
    inner_correction_words: Vec<IdpfCorrectionWord<VI>>,
    /// Correction word for the leaf node level.
    leaf_correction_word: IdpfCorrectionWord<VL>,
}

impl<VI, VL> ConstantTimeEq for IdpfPublicShare<VI, VL>
where
    VI: ConstantTimeEq,
    VL: ConstantTimeEq,
{
    fn ct_eq(&self, other: &Self) -> Choice {
        self.inner_correction_words
            .ct_eq(&other.inner_correction_words)
            & self.leaf_correction_word.ct_eq(&other.leaf_correction_word)
    }
}

impl<VI, VL> PartialEq for IdpfPublicShare<VI, VL>
where
    VI: ConstantTimeEq,
    VL: ConstantTimeEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl<VI, VL> Eq for IdpfPublicShare<VI, VL>
where
    VI: ConstantTimeEq,
    VL: ConstantTimeEq,
{
}

impl<VI, VL> Encode for IdpfPublicShare<VI, VL>
where
    VI: Encode,
    VL: Encode,
{
    fn encode(&self, bytes: &mut Vec<u8>) {
        // Control bits need to be written within each byte in LSB-to-MSB order, and assigned into
        // bytes in big-endian order. Thus, the first four levels will have their control bits
        // encoded in the last byte, and the last levels will have their control bits encoded in the
        // first byte.
        let mut control_bits: BitVec<u8, Lsb0> =
            BitVec::with_capacity(self.inner_correction_words.len() * 2 + 2);
        for correction_words in self.inner_correction_words.iter() {
            control_bits.extend(correction_words.control_bits.iter().map(|x| bool::from(*x)));
        }
        control_bits.extend(
            self.leaf_correction_word
                .control_bits
                .iter()
                .map(|x| bool::from(*x)),
        );
        control_bits.set_uninitialized(false);
        let mut packed_control = control_bits.into_vec();
        bytes.append(&mut packed_control);

        for correction_words in self.inner_correction_words.iter() {
            Seed(correction_words.seed).encode(bytes);
            correction_words.value.encode(bytes);
        }
        Seed(self.leaf_correction_word.seed).encode(bytes);
        self.leaf_correction_word.value.encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        let control_bits_count = (self.inner_correction_words.len() + 1) * 2;
        let mut len = (control_bits_count + 7) / 8 + (self.inner_correction_words.len() + 1) * 16;
        for correction_words in self.inner_correction_words.iter() {
            len += correction_words.value.encoded_len()?;
        }
        len += self.leaf_correction_word.value.encoded_len()?;
        Some(len)
    }
}

impl<VI, VL> ParameterizedDecode<usize> for IdpfPublicShare<VI, VL>
where
    VI: Decode,
    VL: Decode,
{
    fn decode_with_param(bits: &usize, bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let packed_control_len = (bits + 3) / 4;
        let mut packed = vec![0u8; packed_control_len];
        bytes.read_exact(&mut packed)?;
        let unpacked_control_bits: BitVec<u8, Lsb0> = BitVec::from_vec(packed);

        let mut inner_correction_words = Vec::with_capacity(bits - 1);
        for chunk in unpacked_control_bits[0..(bits - 1) * 2].chunks(2) {
            let control_bits = [(chunk[0] as u8).into(), (chunk[1] as u8).into()];
            let seed = Seed::decode(bytes)?.0;
            let value = VI::decode(bytes)?;
            inner_correction_words.push(IdpfCorrectionWord {
                seed,
                control_bits,
                value,
            })
        }

        let control_bits = [
            (unpacked_control_bits[(bits - 1) * 2] as u8).into(),
            (unpacked_control_bits[bits * 2 - 1] as u8).into(),
        ];
        let seed = Seed::decode(bytes)?.0;
        let value = VL::decode(bytes)?;
        let leaf_correction_word = IdpfCorrectionWord {
            seed,
            control_bits,
            value,
        };

        // Check that unused packed bits are zero.
        if unpacked_control_bits[bits * 2..].any() {
            return Err(CodecError::UnexpectedValue);
        }

        Ok(IdpfPublicShare {
            inner_correction_words,
            leaf_correction_word,
        })
    }
}

#[derive(Debug, Clone)]
struct IdpfCorrectionWord<V> {
    seed: [u8; 16],
    control_bits: [Choice; 2],
    value: V,
}

impl<V> ConstantTimeEq for IdpfCorrectionWord<V>
where
    V: ConstantTimeEq,
{
    fn ct_eq(&self, other: &Self) -> Choice {
        self.seed.ct_eq(&other.seed)
            & self.control_bits.ct_eq(&other.control_bits)
            & self.value.ct_eq(&other.value)
    }
}

impl<V> PartialEq for IdpfCorrectionWord<V>
where
    V: ConstantTimeEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl<V> Eq for IdpfCorrectionWord<V> where V: ConstantTimeEq {}

fn xor_seeds(left: &[u8; 16], right: &[u8; 16]) -> [u8; 16] {
    let mut seed = [0u8; 16];
    for (a, (b, c)) in left.iter().zip(right.iter().zip(seed.iter_mut())) {
        *c = a ^ b;
    }
    seed
}

fn and_seeds(left: &[u8; 16], right: &[u8; 16]) -> [u8; 16] {
    let mut seed = [0u8; 16];
    for (a, (b, c)) in left.iter().zip(right.iter().zip(seed.iter_mut())) {
        *c = a & b;
    }
    seed
}

fn or_seeds(left: &[u8; 16], right: &[u8; 16]) -> [u8; 16] {
    let mut seed = [0u8; 16];
    for (a, (b, c)) in left.iter().zip(right.iter().zip(seed.iter_mut())) {
        *c = a | b;
    }
    seed
}

/// Take a control bit, and fan it out into a byte array that can be used as a mask for XOF seeds,
/// without branching. If the control bit input is 0, all bytes will be equal to 0, and if the
/// control bit input is 1, all bytes will be equal to 255.
fn control_bit_to_seed_mask(control: Choice) -> [u8; 16] {
    let mask = -(control.unwrap_u8() as i8) as u8;
    [mask; 16]
}

/// Take two seeds and a control bit, and return the first seed if the control bit is zero, or the
/// XOR of the two seeds if the control bit is one. This does not branch on the control bit.
fn conditional_xor_seeds(
    normal_input: &[u8; 16],
    switched_input: &[u8; 16],
    control: Choice,
) -> [u8; 16] {
    xor_seeds(
        normal_input,
        &and_seeds(switched_input, &control_bit_to_seed_mask(control)),
    )
}

/// Returns one of two seeds, depending on the value of a selector bit. Does not branch on the
/// selector input or make selector-dependent memory accesses.
fn conditional_select_seed(select: Choice, seeds: &[[u8; 16]; 2]) -> [u8; 16] {
    or_seeds(
        &and_seeds(&control_bit_to_seed_mask(!select), &seeds[0]),
        &and_seeds(&control_bit_to_seed_mask(select), &seeds[1]),
    )
}

/// An interface that provides memoization of IDPF computations.
///
/// Each instance of a type implementing `IdpfCache` should only be used with one IDPF key and
/// public share.
///
/// In typical use, IDPFs will be evaluated repeatedly on inputs of increasing length, as part of a
/// protocol executed by multiple participants. Each IDPF evaluation computes keys and control
/// bits corresponding to tree nodes along a path determined by the input to the IDPF. Thus, the
/// values from nodes further up in the tree may be cached and reused in evaluations of subsequent
/// longer inputs. If one IDPF input is a prefix of another input, then the first input's path down
/// the tree is a prefix of the other input's path.
pub trait IdpfCache {
    /// Fetch cached values for the node identified by the IDPF input.
    fn get(&self, input: &BitSlice) -> Option<([u8; 16], u8)>;

    /// Store values corresponding to the node identified by the IDPF input.
    fn insert(&mut self, input: &BitSlice, values: &([u8; 16], u8));
}

/// A no-op [`IdpfCache`] implementation that always reports a cache miss.
#[derive(Default)]
pub struct NoCache {}

impl NoCache {
    /// Construct a `NoCache` object.
    pub fn new() -> NoCache {
        NoCache::default()
    }
}

impl IdpfCache for NoCache {
    fn get(&self, _: &BitSlice) -> Option<([u8; 16], u8)> {
        None
    }

    fn insert(&mut self, _: &BitSlice, _: &([u8; 16], u8)) {}
}

/// A simple [`IdpfCache`] implementation that caches intermediate results in an in-memory hash map,
/// with no eviction.
#[derive(Default)]
pub struct HashMapCache {
    map: HashMap<BitBox, ([u8; 16], u8)>,
}

impl HashMapCache {
    /// Create a new unpopulated `HashMapCache`.
    pub fn new() -> HashMapCache {
        HashMapCache::default()
    }

    /// Create a new unpopulated `HashMapCache`, with a set pre-allocated capacity.
    pub fn with_capacity(capacity: usize) -> HashMapCache {
        Self {
            map: HashMap::with_capacity(capacity),
        }
    }
}

impl IdpfCache for HashMapCache {
    fn get(&self, input: &BitSlice) -> Option<([u8; 16], u8)> {
        self.map.get(input).cloned()
    }

    fn insert(&mut self, input: &BitSlice, values: &([u8; 16], u8)) {
        if !self.map.contains_key(input) {
            self.map
                .insert(input.to_owned().into_boxed_bitslice(), *values);
        }
    }
}

/// A simple [`IdpfCache`] implementation that caches intermediate results in memory, with
/// first-in-first-out eviction, and lookups via linear probing.
pub struct RingBufferCache {
    ring: VecDeque<(BitBox, [u8; 16], u8)>,
}

impl RingBufferCache {
    /// Create a new unpopulated `RingBufferCache`.
    pub fn new(capacity: usize) -> RingBufferCache {
        Self {
            ring: VecDeque::with_capacity(std::cmp::max(capacity, 1)),
        }
    }
}

impl IdpfCache for RingBufferCache {
    fn get(&self, input: &BitSlice) -> Option<([u8; 16], u8)> {
        // iterate back-to-front, so that we check the most recently pushed entry first.
        for entry in self.ring.iter().rev() {
            if input == entry.0 {
                return Some((entry.1, entry.2));
            }
        }
        None
    }

    fn insert(&mut self, input: &BitSlice, values: &([u8; 16], u8)) {
        // evict first (to avoid growing the storage)
        if self.ring.len() == self.ring.capacity() {
            self.ring.pop_front();
        }
        self.ring
            .push_back((input.to_owned().into_boxed_bitslice(), values.0, values.1));
    }
}

#[cfg(test)]
mod tests {
    use std::{
        collections::HashMap,
        convert::{TryFrom, TryInto},
        io::Cursor,
        ops::{Add, AddAssign, Sub},
        str::FromStr,
        sync::Mutex,
    };

    use assert_matches::assert_matches;
    use bitvec::{
        bitbox,
        prelude::{BitBox, Lsb0},
        slice::BitSlice,
        vec::BitVec,
    };
    use num_bigint::BigUint;
    use rand::random;
    use subtle::{Choice, ConditionallyNegatable, ConditionallySelectable};

    use super::{
        HashMapCache, Idpf, IdpfCache, IdpfCorrectionWord, IdpfInput, IdpfOutputShare,
        IdpfPublicShare, NoCache, RingBufferCache,
    };
    use crate::{
        codec::{
            decode_u32_items, encode_u32_items, CodecError, Decode, Encode, ParameterizedDecode,
        },
        field::{Field128, Field255, Field64, FieldElement},
        prng::Prng,
        vdaf::{poplar1::Poplar1IdpfValue, xof::Seed},
    };

    #[test]
    fn idpf_input_conversion() {
        let input_1 = IdpfInput::from_bools(&[
            false, true, false, false, false, false, false, true, false, true, false, false, false,
            false, true, false,
        ]);
        let input_2 = IdpfInput::from_bytes(b"AB");
        assert_eq!(input_1, input_2);
        let bits = bitbox![0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0];
        assert_eq!(input_1[..], bits);
    }

    /// A lossy IDPF cache, for testing purposes, that randomly returns cache misses.
    #[derive(Default)]
    struct LossyCache {
        map: HashMap<BitBox, ([u8; 16], u8)>,
    }

    impl LossyCache {
        /// Create a new unpopulated `LossyCache`.
        fn new() -> LossyCache {
            LossyCache::default()
        }
    }

    impl IdpfCache for LossyCache {
        fn get(&self, input: &BitSlice) -> Option<([u8; 16], u8)> {
            if random() {
                self.map.get(input).cloned()
            } else {
                None
            }
        }

        fn insert(&mut self, input: &BitSlice, values: &([u8; 16], u8)) {
            if !self.map.contains_key(input) {
                self.map
                    .insert(input.to_owned().into_boxed_bitslice(), *values);
            }
        }
    }

    /// A wrapper [`IdpfCache`] implementation that records `get()` calls, for testing purposes.
    struct SnoopingCache<T> {
        inner: T,
        get_calls: Mutex<Vec<BitBox>>,
        insert_calls: Mutex<Vec<(BitBox, [u8; 16], u8)>>,
    }

    impl<T> SnoopingCache<T> {
        fn new(inner: T) -> SnoopingCache<T> {
            SnoopingCache {
                inner,
                get_calls: Mutex::new(Vec::new()),
                insert_calls: Mutex::new(Vec::new()),
            }
        }
    }

    impl<T> IdpfCache for SnoopingCache<T>
    where
        T: IdpfCache,
    {
        fn get(&self, input: &BitSlice) -> Option<([u8; 16], u8)> {
            self.get_calls
                .lock()
                .unwrap()
                .push(input.to_owned().into_boxed_bitslice());
            self.inner.get(input)
        }

        fn insert(&mut self, input: &BitSlice, values: &([u8; 16], u8)) {
            self.insert_calls.lock().unwrap().push((
                input.to_owned().into_boxed_bitslice(),
                values.0,
                values.1,
            ));
            self.inner.insert(input, values)
        }
    }

    #[test]
    fn test_idpf_poplar() {
        let input = bitbox![0, 1, 1, 0, 1].into();
        let nonce: [u8; 16] = random();
        let idpf = Idpf::new((), ());
        let (public_share, keys) = idpf
            .gen(
                &input,
                Vec::from([Poplar1IdpfValue::new([Field64::one(), Field64::one()]); 4]),
                Poplar1IdpfValue::new([Field255::one(), Field255::one()]),
                &nonce,
            )
            .unwrap();

        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::one(), Field64::one()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![1].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::zero(), Field64::zero()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 1].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::one(), Field64::one()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 0].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::zero(), Field64::zero()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![1, 0].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::zero(), Field64::zero()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![1, 1].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::zero(), Field64::zero()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 1, 1].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::one(), Field64::one()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 1, 1, 0].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::one(), Field64::one()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 1, 1, 0, 1].into(),
            &nonce,
            &IdpfOutputShare::Leaf(Poplar1IdpfValue::new([Field255::one(), Field255::one()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 1, 1, 0, 0].into(),
            &nonce,
            &IdpfOutputShare::Leaf(Poplar1IdpfValue::new([Field255::zero(), Field255::zero()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![1, 0, 1, 0, 0].into(),
            &nonce,
            &IdpfOutputShare::Leaf(Poplar1IdpfValue::new([Field255::zero(), Field255::zero()])),
            &mut NoCache::new(),
            &mut NoCache::new(),
        );
    }

    fn check_idpf_poplar_evaluation(
        public_share: &IdpfPublicShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>,
        keys: &[Seed<16>; 2],
        prefix: &IdpfInput,
        binder: &[u8],
        expected_output: &IdpfOutputShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>,
        cache_0: &mut dyn IdpfCache,
        cache_1: &mut dyn IdpfCache,
    ) {
        let idpf = Idpf::new((), ());
        let share_0 = idpf
            .eval(0, public_share, &keys[0], prefix, binder, cache_0)
            .unwrap();
        let share_1 = idpf
            .eval(1, public_share, &keys[1], prefix, binder, cache_1)
            .unwrap();
        let output = share_0.merge(share_1).unwrap();
        assert_eq!(&output, expected_output);
    }

    #[test]
    fn test_idpf_poplar_medium() {
        // This test on 40 byte inputs takes about a second in debug mode. (and ten milliseconds in
        // release mode)
        const INPUT_LEN: usize = 320;
        let mut bits = bitbox![0; INPUT_LEN];
        for mut bit in bits.iter_mut() {
            bit.set(random());
        }
        let input = bits.clone().into();

        let mut inner_values = Vec::with_capacity(INPUT_LEN - 1);
        let mut prng = Prng::new().unwrap();
        for _ in 0..INPUT_LEN - 1 {
            inner_values.push(Poplar1IdpfValue::new([
                Field64::one(),
                prng.next().unwrap(),
            ]));
        }
        let leaf_values =
            Poplar1IdpfValue::new([Field255::one(), Prng::new().unwrap().next().unwrap()]);

        let nonce: [u8; 16] = random();
        let idpf = Idpf::new((), ());
        let (public_share, keys) = idpf
            .gen(&input, inner_values.clone(), leaf_values, &nonce)
            .unwrap();
        let mut cache_0 = RingBufferCache::new(3);
        let mut cache_1 = RingBufferCache::new(3);

        for (level, values) in inner_values.iter().enumerate() {
            let mut prefix = BitBox::from_bitslice(&bits[..=level]).into();
            check_idpf_poplar_evaluation(
                &public_share,
                &keys,
                &prefix,
                &nonce,
                &IdpfOutputShare::Inner(*values),
                &mut cache_0,
                &mut cache_1,
            );
            let flipped_bit = !prefix[level];
            prefix.index.set(level, flipped_bit);
            check_idpf_poplar_evaluation(
                &public_share,
                &keys,
                &prefix,
                &nonce,
                &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::zero(), Field64::zero()])),
                &mut cache_0,
                &mut cache_1,
            );
        }
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &input,
            &nonce,
            &IdpfOutputShare::Leaf(leaf_values),
            &mut cache_0,
            &mut cache_1,
        );
        let mut modified_bits = bits.clone();
        modified_bits.set(INPUT_LEN - 1, !bits[INPUT_LEN - 1]);
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &modified_bits.into(),
            &nonce,
            &IdpfOutputShare::Leaf(Poplar1IdpfValue::new([Field255::zero(), Field255::zero()])),
            &mut cache_0,
            &mut cache_1,
        );
    }

    #[test]
    fn idpf_poplar_cache_behavior() {
        let bits = bitbox![0, 1, 1, 1, 0, 1, 0, 0];
        let input = bits.into();

        let mut inner_values = Vec::with_capacity(7);
        let mut prng = Prng::new().unwrap();
        for _ in 0..7 {
            inner_values.push(Poplar1IdpfValue::new([
                Field64::one(),
                prng.next().unwrap(),
            ]));
        }
        let leaf_values =
            Poplar1IdpfValue::new([Field255::one(), Prng::new().unwrap().next().unwrap()]);

        let nonce: [u8; 16] = random();
        let idpf = Idpf::new((), ());
        let (public_share, keys) = idpf
            .gen(&input, inner_values.clone(), leaf_values, &nonce)
            .unwrap();
        let mut cache_0 = SnoopingCache::new(HashMapCache::new());
        let mut cache_1 = HashMapCache::new();

        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![1, 1, 0, 0].into(),
            &nonce,
            &IdpfOutputShare::Inner(Poplar1IdpfValue::new([Field64::zero(), Field64::zero()])),
            &mut cache_0,
            &mut cache_1,
        );
        assert_eq!(
            cache_0
                .get_calls
                .lock()
                .unwrap()
                .drain(..)
                .collect::<Vec<_>>(),
            vec![bitbox![1, 1, 0], bitbox![1, 1], bitbox![1]],
        );
        assert_eq!(
            cache_0
                .insert_calls
                .lock()
                .unwrap()
                .drain(..)
                .map(|(input, _, _)| input)
                .collect::<Vec<_>>(),
            vec![
                bitbox![1],
                bitbox![1, 1],
                bitbox![1, 1, 0],
                bitbox![1, 1, 0, 0]
            ],
        );

        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0].into(),
            &nonce,
            &IdpfOutputShare::Inner(inner_values[0]),
            &mut cache_0,
            &mut cache_1,
        );
        assert_eq!(
            cache_0
                .get_calls
                .lock()
                .unwrap()
                .drain(..)
                .collect::<Vec<BitBox>>(),
            Vec::<BitBox>::new(),
        );
        assert_eq!(
            cache_0
                .insert_calls
                .lock()
                .unwrap()
                .drain(..)
                .map(|(input, _, _)| input)
                .collect::<Vec<_>>(),
            vec![bitbox![0]],
        );

        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &bitbox![0, 1].into(),
            &nonce,
            &IdpfOutputShare::Inner(inner_values[1]),
            &mut cache_0,
            &mut cache_1,
        );
        assert_eq!(
            cache_0
                .get_calls
                .lock()
                .unwrap()
                .drain(..)
                .collect::<Vec<_>>(),
            vec![bitbox![0]],
        );
        assert_eq!(
            cache_0
                .insert_calls
                .lock()
                .unwrap()
                .drain(..)
                .map(|(input, _, _)| input)
                .collect::<Vec<_>>(),
            vec![bitbox![0, 1]],
        );

        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &input,
            &nonce,
            &IdpfOutputShare::Leaf(leaf_values),
            &mut cache_0,
            &mut cache_1,
        );
        assert_eq!(
            cache_0
                .get_calls
                .lock()
                .unwrap()
                .drain(..)
                .collect::<Vec<_>>(),
            vec![
                bitbox![0, 1, 1, 1, 0, 1, 0],
                bitbox![0, 1, 1, 1, 0, 1],
                bitbox![0, 1, 1, 1, 0],
                bitbox![0, 1, 1, 1],
                bitbox![0, 1, 1],
                bitbox![0, 1],
            ],
        );
        assert_eq!(
            cache_0
                .insert_calls
                .lock()
                .unwrap()
                .drain(..)
                .map(|(input, _, _)| input)
                .collect::<Vec<_>>(),
            vec![
                bitbox![0, 1, 1],
                bitbox![0, 1, 1, 1],
                bitbox![0, 1, 1, 1, 0],
                bitbox![0, 1, 1, 1, 0, 1],
                bitbox![0, 1, 1, 1, 0, 1, 0],
            ],
        );

        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &input,
            &nonce,
            &IdpfOutputShare::Leaf(leaf_values),
            &mut cache_0,
            &mut cache_1,
        );
        assert_eq!(
            cache_0
                .get_calls
                .lock()
                .unwrap()
                .drain(..)
                .collect::<Vec<_>>(),
            vec![bitbox![0, 1, 1, 1, 0, 1, 0]],
        );
        assert!(cache_0.insert_calls.lock().unwrap().is_empty());
    }

    #[test]
    fn idpf_poplar_lossy_cache() {
        let bits = bitbox![1, 0, 0, 1, 1, 0, 1, 0];
        let input = bits.into();

        let mut inner_values = Vec::with_capacity(7);
        let mut prng = Prng::new().unwrap();
        for _ in 0..7 {
            inner_values.push(Poplar1IdpfValue::new([
                Field64::one(),
                prng.next().unwrap(),
            ]));
        }
        let leaf_values =
            Poplar1IdpfValue::new([Field255::one(), Prng::new().unwrap().next().unwrap()]);

        let nonce: [u8; 16] = random();
        let idpf = Idpf::new((), ());
        let (public_share, keys) = idpf
            .gen(&input, inner_values.clone(), leaf_values, &nonce)
            .unwrap();
        let mut cache_0 = LossyCache::new();
        let mut cache_1 = LossyCache::new();

        for (level, values) in inner_values.iter().enumerate() {
            check_idpf_poplar_evaluation(
                &public_share,
                &keys,
                &input[..=level].to_owned().into(),
                &nonce,
                &IdpfOutputShare::Inner(*values),
                &mut cache_0,
                &mut cache_1,
            );
        }
        check_idpf_poplar_evaluation(
            &public_share,
            &keys,
            &input,
            &nonce,
            &IdpfOutputShare::Leaf(leaf_values),
            &mut cache_0,
            &mut cache_1,
        );
    }

    #[test]
    fn test_idpf_poplar_error_cases() {
        let nonce: [u8; 16] = random();
        let idpf = Idpf::new((), ());
        // Zero bits does not make sense.
        idpf.gen(
            &bitbox![].into(),
            Vec::<Poplar1IdpfValue<Field64>>::new(),
            Poplar1IdpfValue::new([Field255::zero(); 2]),
            &nonce,
        )
        .unwrap_err();

        let (public_share, keys) = idpf
            .gen(
                &bitbox![0;10].into(),
                Vec::from([Poplar1IdpfValue::new([Field64::zero(); 2]); 9]),
                Poplar1IdpfValue::new([Field255::zero(); 2]),
                &nonce,
            )
            .unwrap();

        // Wrong number of values.
        idpf.gen(
            &bitbox![0; 10].into(),
            Vec::from([Poplar1IdpfValue::new([Field64::zero(); 2]); 8]),
            Poplar1IdpfValue::new([Field255::zero(); 2]),
            &nonce,
        )
        .unwrap_err();
        idpf.gen(
            &bitbox![0; 10].into(),
            Vec::from([Poplar1IdpfValue::new([Field64::zero(); 2]); 10]),
            Poplar1IdpfValue::new([Field255::zero(); 2]),
            &nonce,
        )
        .unwrap_err();

        // Evaluating with empty prefix.
        assert!(idpf
            .eval(
                0,
                &public_share,
                &keys[0],
                &bitbox![].into(),
                &nonce,
                &mut NoCache::new(),
            )
            .is_err());
        // Evaluating with too-long prefix.
        assert!(idpf
            .eval(
                0,
                &public_share,
                &keys[0],
                &bitbox![0; 11].into(),
                &nonce,
                &mut NoCache::new(),
            )
            .is_err());
    }

    #[test]
    fn idpf_poplar_public_share_round_trip() {
        let public_share = IdpfPublicShare {
            inner_correction_words: Vec::from([
                IdpfCorrectionWord {
                    seed: [0xab; 16],
                    control_bits: [Choice::from(1), Choice::from(0)],
                    value: Poplar1IdpfValue::new([
                        Field64::try_from(83261u64).unwrap(),
                        Field64::try_from(125159u64).unwrap(),
                    ]),
                },
                IdpfCorrectionWord{
                    seed: [0xcd;16],
                    control_bits: [Choice::from(0), Choice::from(1)],
                    value: Poplar1IdpfValue::new([
                        Field64::try_from(17614120u64).unwrap(),
                        Field64::try_from(20674u64).unwrap(),
                    ]),
                },
            ]),
            leaf_correction_word: IdpfCorrectionWord {
                seed: [0xff; 16],
                control_bits: [Choice::from(1), Choice::from(1)],
                value: Poplar1IdpfValue::new([
                    Field255::one(),
                    Field255::get_decoded(
                        b"\xf0\xde\xbc\x9a\x78\x56\x34\x12\xf0\xde\xbc\x9a\x78\x56\x34\x12\xf0\xde\xbc\x9a\x78\x56\x34\x12\xf0\xde\xbc\x9a\x78\x56\x34\x12", // field element correction word, continued
                    ).unwrap(),
                ]),
            },
        };
        let message = hex::decode(concat!(
            "39",                               // packed control bit correction words (0b00111001)
            "abababababababababababababababab", // seed correction word, first level
            "3d45010000000000",                 // field element correction word
            "e7e8010000000000",                 // field element correction word, continued
            "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd", // seed correction word, second level
            "28c50c0100000000",                 // field element correction word
            "c250000000000000",                 // field element correction word, continued
            "ffffffffffffffffffffffffffffffff", // seed correction word, third level
            "0100000000000000000000000000000000000000000000000000000000000000", // field element correction word, leaf field
            "f0debc9a78563412f0debc9a78563412f0debc9a78563412f0debc9a78563412", // field element correction word, continued
        ))
        .unwrap();
        let encoded = public_share.get_encoded();
        let decoded = IdpfPublicShare::get_decoded_with_param(&3, &message).unwrap();
        assert_eq!(public_share, decoded);
        assert_eq!(message, encoded);
        assert_eq!(public_share.encoded_len().unwrap(), encoded.len());

        // check serialization of packed control bits when they span multiple bytes:
        let public_share = IdpfPublicShare {
            inner_correction_words: Vec::from([
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(0)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(0), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
                IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [Choice::from(1), Choice::from(1)],
                    value: Poplar1IdpfValue::new([Field64::zero(), Field64::zero()]),
                },
            ]),
            leaf_correction_word: IdpfCorrectionWord {
                seed: [0; 16],
                control_bits: [Choice::from(0), Choice::from(1)],
                value: Poplar1IdpfValue::new([Field255::zero(), Field255::zero()]),
            },
        };
        let message = hex::decode(concat!(
            "dffb02", // packed correction word control bits: 0b11011111, 0b11111011, 0b10
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000",
            "0000000000000000",
            "00000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000000000000000000000",
        ))
        .unwrap();
        let encoded = public_share.get_encoded();
        let decoded = IdpfPublicShare::get_decoded_with_param(&9, &message).unwrap();
        assert_eq!(public_share, decoded);
        assert_eq!(message, encoded);
    }

    #[test]
    fn idpf_poplar_public_share_control_bit_codec() {
        let test_cases = [
            (&[false, true][..], &[0b10][..]),
            (
                &[false, false, true, false, false, true][..],
                &[0b10_0100u8][..],
            ),
            (
                &[
                    true, true, false, true, false, false, false, false, true, true,
                ][..],
                &[0b0000_1011, 0b11][..],
            ),
            (
                &[
                    true, true, false, true, false, true, true, true, false, true, false, true,
                    false, false, true, false,
                ][..],
                &[0b1110_1011, 0b0100_1010][..],
            ),
            (
                &[
                    true, true, true, true, true, false, true, true, false, true, true, true,
                    false, true, false, true, false, false, true, false, true, true,
                ][..],
                &[0b1101_1111, 0b1010_1110, 0b11_0100][..],
            ),
        ];

        for (control_bits, serialized_control_bits) in test_cases {
            let public_share = IdpfPublicShare::<
                Poplar1IdpfValue<Field64>,
                Poplar1IdpfValue<Field255>,
            > {
                inner_correction_words: control_bits[..control_bits.len() - 2]
                    .chunks(2)
                    .map(|chunk| IdpfCorrectionWord {
                        seed: [0; 16],
                        control_bits: [Choice::from(chunk[0] as u8), Choice::from(chunk[1] as u8)],
                        value: Poplar1IdpfValue::new([Field64::zero(); 2]),
                    })
                    .collect(),
                leaf_correction_word: IdpfCorrectionWord {
                    seed: [0; 16],
                    control_bits: [
                        Choice::from(control_bits[control_bits.len() - 2] as u8),
                        Choice::from(control_bits[control_bits.len() - 1] as u8),
                    ],
                    value: Poplar1IdpfValue::new([Field255::zero(); 2]),
                },
            };

            let mut serialized_public_share = serialized_control_bits.to_owned();
            let idpf_bits = control_bits.len() / 2;
            let size_seeds = 16 * idpf_bits;
            let size_field_vecs =
                Field64::ENCODED_SIZE * 2 * (idpf_bits - 1) + Field255::ENCODED_SIZE * 2;
            serialized_public_share.resize(
                serialized_control_bits.len() + size_seeds + size_field_vecs,
                0,
            );

            assert_eq!(public_share.get_encoded(), serialized_public_share);
            assert_eq!(
                IdpfPublicShare::get_decoded_with_param(&idpf_bits, &serialized_public_share)
                    .unwrap(),
                public_share
            );
        }
    }

    #[test]
    fn idpf_poplar_public_share_unused_bits() {
        let mut buf = vec![0u8; 4096];

        buf[0] = 1 << 2;
        let err =
            IdpfPublicShare::<Field64, Field255>::decode_with_param(&1, &mut Cursor::new(&buf))
                .unwrap_err();
        assert_matches!(err, CodecError::UnexpectedValue);

        buf[0] = 1 << 4;
        let err =
            IdpfPublicShare::<Field64, Field255>::decode_with_param(&2, &mut Cursor::new(&buf))
                .unwrap_err();
        assert_matches!(err, CodecError::UnexpectedValue);

        buf[0] = 1 << 6;
        let err =
            IdpfPublicShare::<Field64, Field255>::decode_with_param(&3, &mut Cursor::new(&buf))
                .unwrap_err();
        assert_matches!(err, CodecError::UnexpectedValue);

        buf[0] = 0;
        buf[1] = 1 << 2;
        let err =
            IdpfPublicShare::<Field64, Field255>::decode_with_param(&5, &mut Cursor::new(&buf))
                .unwrap_err();
        assert_matches!(err, CodecError::UnexpectedValue);
    }

    /// Stores a test vector for the IDPF key generation algorithm.
    struct IdpfTestVector {
        /// The number of bits in IDPF inputs.
        bits: usize,
        /// The binder string used when generating and evaluating keys.
        binder: Vec<u8>,
        /// The IDPF input provided to the key generation algorithm.
        alpha: IdpfInput,
        /// The IDPF output values, at each inner level, provided to the key generation algorithm.
        beta_inner: Vec<Poplar1IdpfValue<Field64>>,
        /// The IDPF output values for the leaf level, provided to the key generation algorithm.
        beta_leaf: Poplar1IdpfValue<Field255>,
        /// The two keys returned by the key generation algorithm.
        keys: [[u8; 16]; 2],
        /// The public share returned by the key generation algorithm.
        public_share: Vec<u8>,
    }

    /// Load a test vector for Idpf key generation.
    fn load_idpfpoplar_test_vector() -> IdpfTestVector {
        let test_vec: serde_json::Value =
            serde_json::from_str(include_str!("vdaf/test_vec/07/IdpfPoplar_0.json")).unwrap();
        let test_vec_obj = test_vec.as_object().unwrap();

        let bits = test_vec_obj
            .get("bits")
            .unwrap()
            .as_u64()
            .unwrap()
            .try_into()
            .unwrap();

        let alpha_str = test_vec_obj.get("alpha").unwrap().as_str().unwrap();
        let alpha_bignum = BigUint::from_str(alpha_str).unwrap();
        let zero_bignum = BigUint::from(0u8);
        let one_bignum = BigUint::from(1u8);
        let alpha_bits = (0..bits)
            .map(|level| (&alpha_bignum >> (bits - level - 1)) & &one_bignum != zero_bignum)
            .collect::<BitVec>();
        let alpha = alpha_bits.into();

        let beta_inner_level_array = test_vec_obj.get("beta_inner").unwrap().as_array().unwrap();
        let beta_inner = beta_inner_level_array
            .iter()
            .map(|array| {
                Poplar1IdpfValue::new([
                    Field64::from(array[0].as_str().unwrap().parse::<u64>().unwrap()),
                    Field64::from(array[1].as_str().unwrap().parse::<u64>().unwrap()),
                ])
            })
            .collect::<Vec<_>>();

        let beta_leaf_array = test_vec_obj.get("beta_leaf").unwrap().as_array().unwrap();
        let beta_leaf = Poplar1IdpfValue::new([
            Field255::from(
                beta_leaf_array[0]
                    .as_str()
                    .unwrap()
                    .parse::<BigUint>()
                    .unwrap(),
            ),
            Field255::from(
                beta_leaf_array[1]
                    .as_str()
                    .unwrap()
                    .parse::<BigUint>()
                    .unwrap(),
            ),
        ]);

        let keys_array = test_vec_obj.get("keys").unwrap().as_array().unwrap();
        let keys = [
            hex::decode(keys_array[0].as_str().unwrap())
                .unwrap()
                .try_into()
                .unwrap(),
            hex::decode(keys_array[1].as_str().unwrap())
                .unwrap()
                .try_into()
                .unwrap(),
        ];

        let public_share_hex = test_vec_obj.get("public_share").unwrap();
        let public_share = hex::decode(public_share_hex.as_str().unwrap()).unwrap();

        let binder_hex = test_vec_obj.get("binder").unwrap();
        let binder = hex::decode(binder_hex.as_str().unwrap()).unwrap();

        IdpfTestVector {
            bits,
            binder,
            alpha,
            beta_inner,
            beta_leaf,
            keys,
            public_share,
        }
    }

    #[test]
    fn idpf_poplar_generate_test_vector() {
        let test_vector = load_idpfpoplar_test_vector();
        let idpf = Idpf::new((), ());
        let (public_share, keys) = idpf
            .gen_with_random(
                &test_vector.alpha,
                test_vector.beta_inner,
                test_vector.beta_leaf,
                &test_vector.binder,
                &test_vector.keys,
            )
            .unwrap();

        assert_eq!(keys[0].0, test_vector.keys[0]);
        assert_eq!(keys[1].0, test_vector.keys[1]);

        let expected_public_share =
            IdpfPublicShare::get_decoded_with_param(&test_vector.bits, &test_vector.public_share)
                .unwrap();
        for (level, (correction_words, expected_correction_words)) in public_share
            .inner_correction_words
            .iter()
            .zip(expected_public_share.inner_correction_words.iter())
            .enumerate()
        {
            assert_eq!(
                correction_words, expected_correction_words,
                "layer {level} did not match\n{correction_words:#x?}\n{expected_correction_words:#x?}"
            );
        }
        assert_eq!(
            public_share.leaf_correction_word,
            expected_public_share.leaf_correction_word
        );

        assert_eq!(
            public_share, expected_public_share,
            "public share did not match\n{public_share:#x?}\n{expected_public_share:#x?}"
        );
        let encoded_public_share = public_share.get_encoded();
        assert_eq!(encoded_public_share, test_vector.public_share);
    }

    #[test]
    fn idpf_input_from_bytes_to_bytes() {
        let test_cases: &[&[u8]] = &[b"hello", b"banana", &[1], &[127], &[1, 2, 3, 4], &[]];
        for test_case in test_cases {
            assert_eq!(&IdpfInput::from_bytes(test_case).to_bytes(), test_case);
        }
    }

    #[test]
    fn idpf_input_from_bools_to_bytes() {
        let input = IdpfInput::from_bools(&[true; 7]);
        assert_eq!(input.to_bytes(), &[254]);
        let input = IdpfInput::from_bools(&[true; 9]);
        assert_eq!(input.to_bytes(), &[255, 128]);
    }

    /// Demonstrate use of an IDPF with values that need run-time parameters for random generation.
    #[test]
    fn idpf_with_value_parameters() {
        use super::IdpfValue;

        /// A test-only type for use as an [`IdpfValue`].
        #[derive(Debug, Clone, Copy)]
        struct MyUnit;

        impl IdpfValue for MyUnit {
            type ValueParameter = ();

            fn generate<S>(_: &mut S, _: &Self::ValueParameter) -> Self
            where
                S: rand_core::RngCore,
            {
                MyUnit
            }

            fn zero(_: &()) -> Self {
                MyUnit
            }

            fn conditional_select(_: &Self, _: &Self, _: Choice) -> Self {
                MyUnit
            }
        }

        impl Encode for MyUnit {
            fn encode(&self, _: &mut Vec<u8>) {}
        }

        impl Decode for MyUnit {
            fn decode(_: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
                Ok(MyUnit)
            }
        }

        impl ConditionallySelectable for MyUnit {
            fn conditional_select(_: &Self, _: &Self, _: Choice) -> Self {
                MyUnit
            }
        }

        impl ConditionallyNegatable for MyUnit {
            fn conditional_negate(&mut self, _: Choice) {}
        }

        impl Add for MyUnit {
            type Output = Self;

            fn add(self, _: Self) -> Self::Output {
                MyUnit
            }
        }

        impl AddAssign for MyUnit {
            fn add_assign(&mut self, _: Self) {}
        }

        impl Sub for MyUnit {
            type Output = Self;

            fn sub(self, _: Self) -> Self::Output {
                MyUnit
            }
        }

        /// A test-only type for use as an [`IdpfValue`], representing a variable-length vector of
        /// field elements. The length must be fixed before generating IDPF keys, but we assume it
        /// is not known at compile time.
        #[derive(Debug, Clone)]
        struct MyVector(Vec<Field128>);

        impl IdpfValue for MyVector {
            type ValueParameter = usize;

            fn generate<S>(seed_stream: &mut S, length: &Self::ValueParameter) -> Self
            where
                S: rand_core::RngCore,
            {
                let mut output = vec![<Field128 as FieldElement>::zero(); *length];
                for element in output.iter_mut() {
                    *element = <Field128 as IdpfValue>::generate(seed_stream, &());
                }
                MyVector(output)
            }

            fn zero(length: &usize) -> Self {
                MyVector(vec![<Field128 as FieldElement>::zero(); *length])
            }

            fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
                debug_assert_eq!(a.0.len(), b.0.len());
                let mut output = vec![<Field128 as FieldElement>::zero(); a.0.len()];
                for ((a_elem, b_elem), output_elem) in
                    a.0.iter().zip(b.0.iter()).zip(output.iter_mut())
                {
                    *output_elem = <Field128 as ConditionallySelectable>::conditional_select(
                        a_elem, b_elem, choice,
                    );
                }
                MyVector(output)
            }
        }

        impl Encode for MyVector {
            fn encode(&self, bytes: &mut Vec<u8>) {
                encode_u32_items(bytes, &(), &self.0);
            }
        }

        impl Decode for MyVector {
            fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
                decode_u32_items(&(), bytes).map(MyVector)
            }
        }

        impl ConditionallyNegatable for MyVector {
            fn conditional_negate(&mut self, choice: Choice) {
                for element in self.0.iter_mut() {
                    element.conditional_negate(choice);
                }
            }
        }

        impl Add for MyVector {
            type Output = Self;

            fn add(self, rhs: Self) -> Self::Output {
                debug_assert_eq!(self.0.len(), rhs.0.len());
                let mut output = vec![<Field128 as FieldElement>::zero(); self.0.len()];
                for ((left_elem, right_elem), output_elem) in
                    self.0.iter().zip(rhs.0.iter()).zip(output.iter_mut())
                {
                    *output_elem = left_elem + right_elem;
                }
                MyVector(output)
            }
        }

        impl AddAssign for MyVector {
            fn add_assign(&mut self, rhs: Self) {
                debug_assert_eq!(self.0.len(), rhs.0.len());
                for (self_elem, right_elem) in self.0.iter_mut().zip(rhs.0.iter()) {
                    *self_elem += *right_elem;
                }
            }
        }

        impl Sub for MyVector {
            type Output = Self;

            fn sub(self, rhs: Self) -> Self::Output {
                debug_assert_eq!(self.0.len(), rhs.0.len());
                let mut output = vec![<Field128 as FieldElement>::zero(); self.0.len()];
                for ((left_elem, right_elem), output_elem) in
                    self.0.iter().zip(rhs.0.iter()).zip(output.iter_mut())
                {
                    *output_elem = left_elem - right_elem;
                }
                MyVector(output)
            }
        }

        // Use a unit type for inner nodes, thus emulating a DPF. Use a newtype around a `Vec` for
        // the leaf nodes, to test out values that require runtime parameters.
        let idpf = Idpf::new((), 3);
        let binder = b"binder";
        let (public_share, [key_0, key_1]) = idpf
            .gen(
                &IdpfInput::from_bytes(b"ae"),
                [MyUnit; 15],
                MyVector(Vec::from([
                    Field128::from(1),
                    Field128::from(2),
                    Field128::from(3),
                ])),
                binder,
            )
            .unwrap();

        let zero_share_0 = idpf
            .eval(
                0,
                &public_share,
                &key_0,
                &IdpfInput::from_bytes(b"ou"),
                binder,
                &mut NoCache::new(),
            )
            .unwrap();
        let zero_share_1 = idpf
            .eval(
                1,
                &public_share,
                &key_1,
                &IdpfInput::from_bytes(b"ou"),
                binder,
                &mut NoCache::new(),
            )
            .unwrap();
        let zero_output = zero_share_0.merge(zero_share_1).unwrap();
        assert_matches!(zero_output, IdpfOutputShare::Leaf(value) => {
            assert_eq!(value.0.len(), 3);
            assert_eq!(value.0[0], <Field128 as FieldElement>::zero());
            assert_eq!(value.0[1], <Field128 as FieldElement>::zero());
            assert_eq!(value.0[2], <Field128 as FieldElement>::zero());
        });

        let programmed_share_0 = idpf
            .eval(
                0,
                &public_share,
                &key_0,
                &IdpfInput::from_bytes(b"ae"),
                binder,
                &mut NoCache::new(),
            )
            .unwrap();
        let programmed_share_1 = idpf
            .eval(
                1,
                &public_share,
                &key_1,
                &IdpfInput::from_bytes(b"ae"),
                binder,
                &mut NoCache::new(),
            )
            .unwrap();
        let programmed_output = programmed_share_0.merge(programmed_share_1).unwrap();
        assert_matches!(programmed_output, IdpfOutputShare::Leaf(value) => {
            assert_eq!(value.0.len(), 3);
            assert_eq!(value.0[0], Field128::from(1));
            assert_eq!(value.0[1], Field128::from(2));
            assert_eq!(value.0[2], Field128::from(3));
        });
    }
}
