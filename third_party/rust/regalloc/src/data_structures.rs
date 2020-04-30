//! Data structures for the whole crate.

use rustc_hash::FxHashMap;
use rustc_hash::FxHashSet;
use smallvec::SmallVec;

use std::cmp::Ordering;
use std::collections::VecDeque;
use std::fmt;
use std::hash::Hash;
use std::marker::PhantomData;
use std::mem;
use std::ops::Index;
use std::ops::IndexMut;
use std::slice::{Iter, IterMut};

use crate::Function;

//=============================================================================
// Queues

pub type Queue<T> = VecDeque<T>;

//=============================================================================
// Maps

// NOTE: plain HashMap is nondeterministic, even in a single-threaded
// scenario, which can make debugging code that uses it really confusing.  So
// we use FxHashMap instead, as it *is* deterministic, and, allegedly, faster
// too.
pub type Map<K, V> = FxHashMap<K, V>;

//=============================================================================
// Sets of things

// Same comment as above for FxHashMap.
pub struct Set<T> {
    set: FxHashSet<T>,
}

impl<T: Eq + Ord + Hash + Copy + fmt::Debug> Set<T> {
    #[inline(never)]
    pub fn empty() -> Self {
        Self {
            set: FxHashSet::<T>::default(),
        }
    }

    #[inline(never)]
    pub fn unit(item: T) -> Self {
        let mut s = Self::empty();
        s.insert(item);
        s
    }

    #[inline(never)]
    pub fn two(item1: T, item2: T) -> Self {
        let mut s = Self::empty();
        s.insert(item1);
        s.insert(item2);
        s
    }

    #[inline(never)]
    pub fn card(&self) -> usize {
        self.set.len()
    }

    #[inline(never)]
    pub fn insert(&mut self, item: T) {
        self.set.insert(item);
    }

    #[inline(never)]
    pub fn delete(&mut self, item: T) {
        self.set.remove(&item);
    }

    #[inline(never)]
    pub fn is_empty(&self) -> bool {
        self.set.is_empty()
    }

    #[inline(never)]
    pub fn contains(&self, item: T) -> bool {
        self.set.contains(&item)
    }

    #[inline(never)]
    pub fn intersect(&mut self, other: &Self) {
        let mut res = FxHashSet::<T>::default();
        for item in self.set.iter() {
            if other.set.contains(item) {
                res.insert(*item);
            }
        }
        self.set = res;
    }

    #[inline(never)]
    pub fn union(&mut self, other: &Self) {
        for item in other.set.iter() {
            self.set.insert(*item);
        }
    }

    #[inline(never)]
    pub fn remove(&mut self, other: &Self) {
        for item in other.set.iter() {
            self.set.remove(item);
        }
    }

    #[inline(never)]
    pub fn intersects(&self, other: &Self) -> bool {
        !self.set.is_disjoint(&other.set)
    }

    #[inline(never)]
    pub fn is_subset_of(&self, other: &Self) -> bool {
        self.set.is_subset(&other.set)
    }

    #[inline(never)]
    pub fn to_vec(&self) -> Vec<T> {
        let mut res = Vec::<T>::new();
        for item in self.set.iter() {
            res.push(*item)
        }
        // Don't delete this.  It is important.
        res.sort_unstable();
        res
    }

    #[inline(never)]
    pub fn from_vec(vec: Vec<T>) -> Self {
        let mut res = Set::<T>::empty();
        for x in vec {
            res.insert(x);
        }
        res
    }

    #[inline(never)]
    pub fn equals(&self, other: &Self) -> bool {
        self.set == other.set
    }

    #[inline(never)]
    pub fn retain<F>(&mut self, f: F)
    where
        F: FnMut(&T) -> bool,
    {
        self.set.retain(f)
    }

    #[inline(never)]
    pub fn map<F, U>(&self, f: F) -> Set<U>
    where
        F: Fn(&T) -> U,
        U: Eq + Ord + Hash + Copy + fmt::Debug,
    {
        Set {
            set: self.set.iter().map(f).collect(),
        }
    }

    #[inline(never)]
    pub fn filter_map<F, U>(&self, f: F) -> Set<U>
    where
        F: Fn(&T) -> Option<U>,
        U: Eq + Ord + Hash + Copy + fmt::Debug,
    {
        Set {
            set: self.set.iter().filter_map(f).collect(),
        }
    }
}

impl<T: Eq + Ord + Hash + Copy + fmt::Debug> fmt::Debug for Set<T> {
    #[inline(never)]
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        // Print the elements in some way which depends only on what is
        // present in the set, and not on any other factor.  In particular,
        // <Debug for FxHashSet> has been observed to to print the elements
        // of a two element set in both orders on different occasions.
        let sorted_vec = self.to_vec();
        let mut s = "{".to_string();
        for i in 0..sorted_vec.len() {
            if i > 0 {
                s = s + &", ".to_string();
            }
            s = s + &format!("{:?}", &sorted_vec[i]);
        }
        s = s + &"}".to_string();
        write!(fmt, "{}", s)
    }
}

impl<T: Eq + Ord + Hash + Copy + Clone + fmt::Debug> Clone for Set<T> {
    #[inline(never)]
    fn clone(&self) -> Self {
        let mut res = Set::<T>::empty();
        for item in self.set.iter() {
            res.set.insert(item.clone());
        }
        res
    }
}

pub struct SetIter<'a, T> {
    set_iter: std::collections::hash_set::Iter<'a, T>,
}
impl<T> Set<T> {
    pub fn iter(&self) -> SetIter<T> {
        SetIter {
            set_iter: self.set.iter(),
        }
    }
}
impl<'a, T> Iterator for SetIter<'a, T> {
    type Item = &'a T;
    fn next(&mut self) -> Option<Self::Item> {
        self.set_iter.next()
    }
}

//=============================================================================
// Iteration boilerplate for entities.  The only purpose of this is to support
// constructions of the form
//
//   for ent in startEnt .dotdot( endPlus1Ent ) {
//   }
//
// until such time as `trait Step` is available in stable Rust.  At that point
// `fn dotdot` and all of the following can be removed, and the loops
// rewritten using the standard syntax:
//
//   for ent in startEnt .. endPlus1Ent {
//   }

pub trait Zero {
    fn zero() -> Self;
}

pub trait PlusN {
    fn plus_n(&self, n: usize) -> Self;
}

#[derive(Clone, Copy)]
pub struct Range<T> {
    first: T,
    len: usize,
}

impl<T: Copy + PartialOrd + PlusN> IntoIterator for Range<T> {
    type Item = T;
    type IntoIter = MyIterator<T>;
    fn into_iter(self) -> Self::IntoIter {
        MyIterator {
            range: self,
            next: self.first,
        }
    }
}

impl<T: Copy + Eq + Ord + PlusN> Range<T> {
    /// Create a new range object.
    pub fn new(from: T, len: usize) -> Range<T> {
        Range { first: from, len }
    }

    pub fn start(&self) -> T {
        self.first
    }

    pub fn first(&self) -> T {
        assert!(self.len() > 0);
        self.start()
    }

    pub fn last(&self) -> T {
        assert!(self.len() > 0);
        self.start().plus_n(self.len() - 1)
    }

    pub fn last_plus1(&self) -> T {
        self.start().plus_n(self.len())
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn contains(&self, t: T) -> bool {
        t >= self.first && t < self.first.plus_n(self.len)
    }
}

pub struct MyIterator<T> {
    range: Range<T>,
    next: T,
}
impl<T: Copy + PartialOrd + PlusN> Iterator for MyIterator<T> {
    type Item = T;
    fn next(&mut self) -> Option<Self::Item> {
        if self.next >= self.range.first.plus_n(self.range.len) {
            None
        } else {
            let res = Some(self.next);
            self.next = self.next.plus_n(1);
            res
        }
    }
}

//=============================================================================
// Vectors where both the index and element types can be specified (and at
// most 2^32-1 elems can be stored.  What if this overflows?)

pub struct TypedIxVec<TyIx, Ty> {
    vek: Vec<Ty>,
    ty_ix: PhantomData<TyIx>,
}

impl<TyIx, Ty> TypedIxVec<TyIx, Ty>
where
    Ty: Clone,
    TyIx: Copy + Eq + Ord + Zero + PlusN + Into<u32>,
{
    pub fn new() -> Self {
        Self {
            vek: Vec::new(),
            ty_ix: PhantomData::<TyIx>,
        }
    }
    pub fn from_vec(vek: Vec<Ty>) -> Self {
        Self {
            vek,
            ty_ix: PhantomData::<TyIx>,
        }
    }
    pub fn append(&mut self, other: &mut TypedIxVec<TyIx, Ty>) {
        // FIXME what if this overflows?
        self.vek.append(&mut other.vek);
    }
    pub fn iter(&self) -> Iter<Ty> {
        self.vek.iter()
    }
    pub fn iter_mut(&mut self) -> IterMut<Ty> {
        self.vek.iter_mut()
    }
    pub fn len(&self) -> u32 {
        // FIXME what if this overflows?
        self.vek.len() as u32
    }
    pub fn push(&mut self, item: Ty) {
        // FIXME what if this overflows?
        self.vek.push(item);
    }
    pub fn resize(&mut self, new_len: u32, value: Ty) {
        self.vek.resize(new_len as usize, value);
    }
    pub fn reserve(&mut self, additional: usize) {
        self.vek.reserve(additional);
    }
    pub fn elems(&self) -> &[Ty] {
        &self.vek[..]
    }
    pub fn elems_mut(&mut self) -> &mut [Ty] {
        &mut self.vek[..]
    }
    pub fn range(&self) -> Range<TyIx> {
        Range::new(TyIx::zero(), self.len() as usize)
    }
    pub fn remove(&mut self, idx: TyIx) -> Ty {
        self.vek.remove(idx.into() as usize)
    }
    pub fn sort_by<F: FnMut(&Ty, &Ty) -> Ordering>(&mut self, compare: F) {
        self.vek.sort_by(compare)
    }
    pub fn sort_unstable_by<F: FnMut(&Ty, &Ty) -> Ordering>(&mut self, compare: F) {
        self.vek.sort_unstable_by(compare)
    }
    pub fn clear(&mut self) {
        self.vek.clear();
    }
}

impl<TyIx, Ty> Index<TyIx> for TypedIxVec<TyIx, Ty>
where
    TyIx: Into<u32>,
{
    type Output = Ty;
    fn index(&self, ix: TyIx) -> &Ty {
        &self.vek[ix.into() as usize]
    }
}

impl<TyIx, Ty> IndexMut<TyIx> for TypedIxVec<TyIx, Ty>
where
    TyIx: Into<u32>,
{
    fn index_mut(&mut self, ix: TyIx) -> &mut Ty {
        &mut self.vek[ix.into() as usize]
    }
}

impl<TyIx, Ty> Clone for TypedIxVec<TyIx, Ty>
where
    Ty: Clone,
{
    // This is only needed for debug printing.
    fn clone(&self) -> Self {
        Self {
            vek: self.vek.clone(),
            ty_ix: PhantomData::<TyIx>,
        }
    }
}

//=============================================================================

macro_rules! generate_boilerplate {
    ($TypeIx:ident, $Type:ident, $PrintingPrefix:expr) => {
        #[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
        // Firstly, the indexing type (TypeIx)
        pub enum $TypeIx {
            $TypeIx(u32),
        }
        impl $TypeIx {
            #[allow(dead_code)]
            #[inline(always)]
            pub fn new(n: u32) -> Self {
                debug_assert!(n != u32::max_value());
                Self::$TypeIx(n)
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub const fn max_value() -> Self {
                Self::$TypeIx(u32::max_value() - 1)
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub const fn min_value() -> Self {
                Self::$TypeIx(u32::min_value())
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub const fn invalid_value() -> Self {
                Self::$TypeIx(u32::max_value())
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub fn is_valid(self) -> bool {
                self != Self::invalid_value()
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub fn is_invalid(self) -> bool {
                self == Self::invalid_value()
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub fn get(self) -> u32 {
                debug_assert!(self.is_valid());
                match self {
                    $TypeIx::$TypeIx(n) => n,
                }
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub fn plus(self, delta: u32) -> $TypeIx {
                debug_assert!(self.is_valid());
                $TypeIx::$TypeIx(self.get() + delta)
            }
            #[allow(dead_code)]
            #[inline(always)]
            pub fn minus(self, delta: u32) -> $TypeIx {
                debug_assert!(self.is_valid());
                $TypeIx::$TypeIx(self.get() - delta)
            }
            #[allow(dead_code)]
            pub fn dotdot(&self, last_plus1: $TypeIx) -> Range<$TypeIx> {
                debug_assert!(self.is_valid());
                let len = (last_plus1.get() - self.get()) as usize;
                Range::new(*self, len)
            }
        }
        impl fmt::Debug for $TypeIx {
            fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
                if self.is_invalid() {
                    write!(fmt, "{}<NONE>", $PrintingPrefix)
                } else {
                    write!(fmt, "{}{}", $PrintingPrefix, &self.get())
                }
            }
        }
        impl PlusN for $TypeIx {
            #[inline(always)]
            fn plus_n(&self, n: usize) -> Self {
                debug_assert!(self.is_valid());
                self.plus(n as u32)
            }
        }
        impl Into<u32> for $TypeIx {
            #[inline(always)]
            fn into(self) -> u32 {
                debug_assert!(self.is_valid());
                self.get()
            }
        }
        impl Zero for $TypeIx {
            #[inline(always)]
            fn zero() -> Self {
                $TypeIx::new(0)
            }
        }
    };
}

generate_boilerplate!(InstIx, Inst, "i");

generate_boilerplate!(BlockIx, Block, "b");

generate_boilerplate!(RangeFragIx, RangeFrag, "f");

generate_boilerplate!(VirtualRangeIx, VirtualRange, "vr");

generate_boilerplate!(RealRangeIx, RealRange, "rr");

impl<TyIx, Ty: fmt::Debug> fmt::Debug for TypedIxVec<TyIx, Ty> {
    // This is something of a hack in the sense that it doesn't show the
    // indices, but oh well ..
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}", self.vek)
    }
}

//=============================================================================
// Definitions of register classes, registers and stack slots, and printing
// thereof. Note that this register class definition is meant to be
// architecture-independent: it simply captures common integer/float/vector
// types that machines are likely to use. TODO: investigate whether we need a
// more flexible register-class definition mechanism.

#[derive(PartialEq, Eq, Debug, Clone, Copy)]
pub enum RegClass {
    I32,
    F32,
    I64,
    F64,
    V128,
}

/// The number of register classes that exist.
/// N.B.: must be <= 7 (fit into 3 bits) for 32-bit VReg/RReg packed format!
pub const NUM_REG_CLASSES: usize = 5;

impl RegClass {
    /// Convert a register class to a u32 index.
    pub fn rc_to_u32(self) -> u32 {
        match self {
            RegClass::I32 => 0,
            RegClass::F32 => 1,
            RegClass::I64 => 2,
            RegClass::F64 => 3,
            RegClass::V128 => 4,
        }
    }
    /// Convert a register class to a usize index.
    pub fn rc_to_usize(self) -> usize {
        self.rc_to_u32() as usize
    }
    /// Construct a register class from a u32.
    pub fn rc_from_u32(rc: u32) -> RegClass {
        match rc {
            0 => RegClass::I32,
            1 => RegClass::F32,
            2 => RegClass::I64,
            3 => RegClass::F64,
            4 => RegClass::V128,
            _ => panic!("rc_from_u32"),
        }
    }

    pub fn short_name(self) -> &'static str {
        match self {
            RegClass::I32 => "I",
            RegClass::I64 => "J",
            RegClass::F32 => "F",
            RegClass::F64 => "D",
            RegClass::V128 => "V",
        }
    }

    pub fn long_name(self) -> &'static str {
        match self {
            RegClass::I32 => "I32",
            RegClass::I64 => "I32",
            RegClass::F32 => "F32",
            RegClass::F64 => "F32",
            RegClass::V128 => "V128",
        }
    }
}

// Reg represents both real and virtual registers.  For compactness and speed,
// these fields are packed into a single u32.  The format is:
//
// Virtual Reg:   1  rc:3                index:28
// Real Reg:      0  rc:3  uu:12  enc:8  index:8
//
// `rc` is the register class.  `uu` means "unused".  `enc` is the hardware
// encoding for the reg.  `index` is a zero based index which has the
// following meanings:
//
// * for a Virtual Reg, `index` is just the virtual register number.
// * for a Real Reg, `index` is the entry number in the associated
//   `RealRegUniverse`.
//
// This scheme gives us:
//
// * a compact (32-bit) representation for registers
// * fast equality tests for registers
// * ability to handle up to 2^28 (268.4 million) virtual regs per function
// * ability to handle up to 8 register classes
// * ability to handle targets with up to 256 real registers
// * ability to emit instructions containing real regs without having to
//   look up encodings in any side tables, since a real reg carries its
//   encoding
// * efficient bitsets and arrays of virtual registers, since each has a
//   zero-based index baked in
// * efficient bitsets and arrays of real registers, for the same reason
//
// This scheme makes it impossible to represent overlapping register classes,
// but that doesn't seem important.  AFAIK only ARM32 VFP/Neon has that.

#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct Reg {
    do_not_access_this_directly: u32,
}

static INVALID_REG: u32 = 0xffffffff;

impl Reg {
    pub fn is_virtual(self) -> bool {
        self.is_valid() && (self.do_not_access_this_directly & 0x8000_0000) != 0
    }
    pub fn is_real(self) -> bool {
        self.is_valid() && !self.is_virtual()
    }
    pub fn new_real(rc: RegClass, enc: u8, index: u8) -> Self {
        let n = (0 << 31) | (rc.rc_to_u32() << 28) | ((enc as u32) << 8) | ((index as u32) << 0);
        Reg {
            do_not_access_this_directly: n,
        }
    }
    pub fn new_virtual(rc: RegClass, index: u32) -> Self {
        if index >= (1 << 28) {
            panic!("new_virtual(): index too large");
        }
        let n = (1 << 31) | (rc.rc_to_u32() << 28) | (index << 0);
        Reg {
            do_not_access_this_directly: n,
        }
    }
    pub fn invalid() -> Reg {
        Reg {
            do_not_access_this_directly: INVALID_REG,
        }
    }
    pub fn is_invalid(self) -> bool {
        self.do_not_access_this_directly == INVALID_REG
    }
    pub fn is_valid(self) -> bool {
        !self.is_invalid()
    }
    pub fn is_virtual_or_invalid(self) -> bool {
        self.is_virtual() || self.is_invalid()
    }
    pub fn is_real_or_invalid(self) -> bool {
        self.is_real() || self.is_invalid()
    }
    pub fn get_class(self) -> RegClass {
        debug_assert!(self.is_valid());
        RegClass::rc_from_u32((self.do_not_access_this_directly >> 28) & 0x7)
    }
    pub fn get_index(self) -> usize {
        debug_assert!(self.is_valid());
        // Return type is usize because typically we will want to use the
        // result for indexing into a Vec
        if self.is_virtual() {
            (self.do_not_access_this_directly & ((1 << 28) - 1)) as usize
        } else {
            (self.do_not_access_this_directly & ((1 << 8) - 1)) as usize
        }
    }
    pub fn get_hw_encoding(self) -> u8 {
        debug_assert!(self.is_valid());
        if self.is_virtual() {
            panic!("Virtual register does not have a hardware encoding")
        } else {
            ((self.do_not_access_this_directly >> 8) & ((1 << 8) - 1)) as u8
        }
    }
    pub fn as_virtual_reg(self) -> Option<VirtualReg> {
        // Allow invalid virtual regs as well.
        if self.is_virtual_or_invalid() {
            Some(VirtualReg { reg: self })
        } else {
            None
        }
    }
    pub fn as_real_reg(self) -> Option<RealReg> {
        // Allow invalid real regs as well.
        if self.is_real_or_invalid() {
            Some(RealReg { reg: self })
        } else {
            None
        }
    }
    pub fn show_with_rru(self, univ: &RealRegUniverse) -> String {
        if self.is_real() && self.get_index() < univ.regs.len() {
            univ.regs[self.get_index()].1.clone()
        } else if self.is_valid() {
            format!("{:?}", self)
        } else {
            "rINVALID".to_string()
        }
    }
}

impl fmt::Debug for Reg {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        if self.is_valid() {
            write!(
                fmt,
                "{}{}{}",
                if self.is_virtual() { "v" } else { "r" },
                self.get_index(),
                self.get_class().short_name(),
            )
        } else {
            write!(fmt, "rINVALID")
        }
    }
}

// RealReg and VirtualReg are merely wrappers around Reg, which try to
// dynamically ensure that they are really wrapping the correct flavour of
// register.

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct RealReg {
    reg: Reg,
}
impl Reg /* !!not RealReg!! */ {
    pub fn to_real_reg(self) -> RealReg {
        if self.is_virtual() {
            panic!("Reg::to_real_reg: this is a virtual register")
        } else {
            RealReg { reg: self }
        }
    }
}
impl RealReg {
    pub fn get_class(self) -> RegClass {
        self.reg.get_class()
    }
    pub fn get_index(self) -> usize {
        self.reg.get_index()
    }
    pub fn get_hw_encoding(self) -> usize {
        self.reg.get_hw_encoding() as usize
    }
    pub fn to_reg(self) -> Reg {
        self.reg
    }
    pub fn invalid() -> RealReg {
        RealReg {
            reg: Reg::invalid(),
        }
    }
    pub fn is_valid(self) -> bool {
        self.reg.is_valid()
    }
    pub fn is_invalid(self) -> bool {
        self.reg.is_invalid()
    }
    pub fn maybe_valid(self) -> Option<RealReg> {
        if self == RealReg::invalid() {
            None
        } else {
            Some(self)
        }
    }
}
impl fmt::Debug for RealReg {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}", self.reg)
    }
}

#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct VirtualReg {
    reg: Reg,
}
impl Reg /* !!not VirtualReg!! */ {
    pub fn to_virtual_reg(self) -> VirtualReg {
        if self.is_virtual() {
            VirtualReg { reg: self }
        } else {
            panic!("Reg::to_virtual_reg: this is a real register")
        }
    }
}
impl VirtualReg {
    pub fn get_class(self) -> RegClass {
        self.reg.get_class()
    }
    pub fn get_index(self) -> usize {
        self.reg.get_index()
    }
    pub fn to_reg(self) -> Reg {
        self.reg
    }
    pub fn invalid() -> VirtualReg {
        VirtualReg {
            reg: Reg::invalid(),
        }
    }
    pub fn is_valid(self) -> bool {
        self.reg.is_valid()
    }
    pub fn is_invalid(self) -> bool {
        self.reg.is_invalid()
    }
    pub fn maybe_valid(self) -> Option<VirtualReg> {
        if self == VirtualReg::invalid() {
            None
        } else {
            Some(self)
        }
    }
}
impl fmt::Debug for VirtualReg {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}", self.reg)
    }
}

impl Reg {
    /// Apply a vreg-rreg mapping to a Reg.  This is used for registers used in
    /// a read-role.
    pub fn apply_uses(&mut self, mapper: &RegUsageMapper) {
        self.apply(|vreg| mapper.get_use(vreg));
    }

    /// Apply a vreg-rreg mapping to a Reg.  This is used for registers used in
    /// a write-role.
    pub fn apply_defs(&mut self, mapper: &RegUsageMapper) {
        self.apply(|vreg| mapper.get_def(vreg));
    }

    /// Apply a vreg-rreg mapping to a Reg.  This is used for registers used in
    /// a modify-role.
    pub fn apply_mods(&mut self, mapper: &RegUsageMapper) {
        self.apply(|vreg| mapper.get_mod(vreg));
    }

    fn apply<F: Fn(VirtualReg) -> Option<RealReg>>(&mut self, f: F) {
        if let Some(vreg) = self.as_virtual_reg() {
            if let Some(rreg) = f(vreg) {
                debug_assert!(rreg.get_class() == vreg.get_class());
                *self = rreg.to_reg();
            } else {
                panic!("Reg::apply: no mapping for {:?}", self);
            }
        }
    }
}

/// A "writable register". This is a zero-cost wrapper that can be used to
/// create a distinction, at the Rust type level, between a plain "register"
/// and a "writable register".
///
/// There is nothing that ensures that Writable<..> is only wrapped around Reg
/// and its variants (`RealReg`, `VirtualReg`).  That however is its intended
/// and currently its only use.
///
/// Writable<..> can be used by the client to ensure that, internally, it only
/// generates instructions that write to registers that should be written. The
/// `InstRegUses` below, which must be implemented for every instruction,
/// requires a `Writable<Reg>` (not just `Reg`) in its `defined` and
/// `modified` sets. While we cannot hide the constructor for `Writable<..>`
/// from certain parts of the client while exposing it to others, the client
/// *can* adopt conventions to e.g. only ever call the Writable<..>
/// constructor from its central vreg-management logic, and decide that any
/// invocation of this constructor in a machine backend (for example) is an
/// error.
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Debug)]
pub struct Writable<R: Copy + Clone + PartialEq + Eq + Hash + PartialOrd + Ord + fmt::Debug> {
    reg: R,
}

impl<R: Copy + Clone + PartialEq + Eq + Hash + PartialOrd + Ord + fmt::Debug> Writable<R> {
    /// Create a Writable<R> from an R. The client should carefully audit where
    /// it calls this constructor to ensure correctness (see `Writable<..>`
    /// struct documentation).
    pub fn from_reg(reg: R) -> Writable<R> {
        Writable { reg }
    }

    /// Get the inner Reg.
    pub fn to_reg(&self) -> R {
        self.reg
    }

    pub fn map<F, U>(&self, f: F) -> Writable<U>
    where
        F: Fn(R) -> U,
        U: Copy + Clone + PartialEq + Eq + Hash + PartialOrd + Ord + fmt::Debug,
    {
        Writable { reg: f(self.reg) }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub enum SpillSlot {
    SpillSlot(u32),
}
impl SpillSlot {
    pub fn new(n: u32) -> Self {
        SpillSlot::SpillSlot(n)
    }
    pub fn get(self) -> u32 {
        match self {
            SpillSlot::SpillSlot(n) => n,
        }
    }
    pub fn get_usize(self) -> usize {
        self.get() as usize
    }
    pub fn round_up(self, num_slots: u32) -> SpillSlot {
        assert!(num_slots > 0);
        SpillSlot::new((self.get() + num_slots - 1) / num_slots * num_slots)
    }
    pub fn inc(self, num_slots: u32) -> SpillSlot {
        SpillSlot::new(self.get() + num_slots)
    }
}
impl fmt::Debug for SpillSlot {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "S{}", self.get())
    }
}

//=============================================================================
// Register uses: low level interface

// This minimal struct is visible from outside the regalloc.rs interface.  It
// is intended to be a safe wrapper around `RegVecs`, which isn't externally
// visible.  It is used to collect unsanitized reg use info from client
// instructions.
pub struct RegUsageCollector<'a> {
    pub reg_vecs: &'a mut RegVecs,
}
impl<'a> RegUsageCollector<'a> {
    pub fn new(reg_vecs: &'a mut RegVecs) -> Self {
        Self { reg_vecs }
    }
    pub fn add_use(&mut self, r: Reg) {
        self.reg_vecs.uses.push(r);
    }
    pub fn add_uses(&mut self, regs: &Set<Reg>) {
        for reg in regs.iter() {
            self.add_use(*reg);
        }
    }
    pub fn add_def(&mut self, r: Writable<Reg>) {
        self.reg_vecs.defs.push(r.to_reg());
    }
    pub fn add_defs(&mut self, regs: &Set<Writable<Reg>>) {
        for reg in regs.iter() {
            self.add_def(*reg);
        }
    }
    pub fn add_mod(&mut self, r: Writable<Reg>) {
        self.reg_vecs.mods.push(r.to_reg());
    }
    pub fn add_mods(&mut self, regs: &Set<Writable<Reg>>) {
        for reg in regs.iter() {
            self.add_mod(*reg);
        }
    }
    // The presence of the following two is a hack, needed to support fuzzing
    // in the test framework.  Real clients should not call them.
    pub fn get_use_def_mod_vecs_test_framework_only(&self) -> (Vec<Reg>, Vec<Reg>, Vec<Reg>) {
        (
            self.reg_vecs.uses.clone(),
            self.reg_vecs.defs.clone(),
            self.reg_vecs.mods.clone(),
        )
    }
    pub fn get_empty_reg_vecs_test_framework_only(sanitized: bool) -> RegVecs {
        RegVecs::new(sanitized)
    }
}

// Everything else is not visible outside the regalloc.rs interface.

// There is one of these per function.  Note that `defs` and `mods` lose the
// `Writable` constraint at this point.  This is for convenience of having all
// three vectors be the same type, but comes at the cost of the loss of being
// able to differentiate readonly vs read/write registers in the Rust type
// system.
#[derive(Debug)]
pub struct RegVecs {
    pub uses: Vec<Reg>,
    pub defs: Vec<Reg>,
    pub mods: Vec<Reg>,
    sanitized: bool,
}

impl RegVecs {
    pub fn new(sanitized: bool) -> Self {
        Self {
            uses: vec![],
            defs: vec![],
            mods: vec![],
            sanitized,
        }
    }
    pub fn is_sanitized(&self) -> bool {
        self.sanitized
    }
    pub fn set_sanitized(&mut self, sanitized: bool) {
        self.sanitized = sanitized;
    }
    pub fn clear(&mut self) {
        self.uses.clear();
        self.defs.clear();
        self.mods.clear();
    }
}

// There is one of these per insn, so try and keep it as compact as possible.
// I think this should fit in 16 bytes.
#[derive(Clone, Debug)]
pub struct RegVecBounds {
    // These are the group start indices in RegVecs.{uses, defs, mods}.
    pub uses_start: u32,
    pub defs_start: u32,
    pub mods_start: u32,
    // And these are the group lengths.  This does limit each instruction to
    // mentioning only 256 registers in any group, but that does not seem like a
    // problem.
    pub uses_len: u8,
    pub defs_len: u8,
    pub mods_len: u8,
}

impl RegVecBounds {
    pub fn new() -> Self {
        Self {
            uses_start: 0,
            defs_start: 0,
            mods_start: 0,
            uses_len: 0,
            defs_len: 0,
            mods_len: 0,
        }
    }
}

// This is the primary structure.  We compute just one of these for an entire
// function.
pub struct RegVecsAndBounds {
    // The three vectors of registers.  These can be arbitrarily long.
    pub vecs: RegVecs,
    // Admin info which tells us the location, for each insn, of its register
    // groups in `vecs`.
    pub bounds: TypedIxVec<InstIx, RegVecBounds>,
}

impl RegVecsAndBounds {
    pub fn new(vecs: RegVecs, bounds: TypedIxVec<InstIx, RegVecBounds>) -> Self {
        Self { vecs, bounds }
    }
    pub fn is_sanitized(&self) -> bool {
        self.vecs.sanitized
    }
    pub fn num_insns(&self) -> u32 {
        self.bounds.len()
    }
}

//=============================================================================
// Register uses: convenience interface

// Some call sites want to get reg use information as three Sets.  This is a
// "convenience facility" which is easier to use but much slower than working
// with a whole-function `RegVecsAndBounds`.  It shouldn't be used on critical
// paths.
#[derive(Debug)]
pub struct RegSets {
    pub uses: Set<Reg>, // registers that are read.
    pub defs: Set<Reg>, // registers that are written.
    pub mods: Set<Reg>, // registers that are modified.
    sanitized: bool,
}

impl RegSets {
    pub fn new(sanitized: bool) -> Self {
        Self {
            uses: Set::<Reg>::empty(),
            defs: Set::<Reg>::empty(),
            mods: Set::<Reg>::empty(),
            sanitized,
        }
    }
    pub fn is_sanitized(&self) -> bool {
        self.sanitized
    }
}

impl RegVecsAndBounds {
    /* !!not RegSets!! */
    #[inline(never)]
    // Convenience function.  Try to avoid using this.
    pub fn get_reg_sets_for_iix(&self, iix: InstIx) -> RegSets {
        let bounds = &self.bounds[iix];
        let mut regsets = RegSets::new(self.vecs.sanitized);
        for i in bounds.uses_start as usize..bounds.uses_start as usize + bounds.uses_len as usize {
            regsets.uses.insert(self.vecs.uses[i]);
        }
        for i in bounds.defs_start as usize..bounds.defs_start as usize + bounds.defs_len as usize {
            regsets.defs.insert(self.vecs.defs[i]);
        }
        for i in bounds.mods_start as usize..bounds.mods_start as usize + bounds.mods_len as usize {
            regsets.mods.insert(self.vecs.mods[i]);
        }
        regsets
    }
}

//=============================================================================
// Register maps: virtual-to-real mapping.

/// This data structure holds the mappings needed to map an instruction's uses,
/// mods and defs from virtual to real registers.
#[derive(Debug)]
pub struct RegUsageMapper {
    /// Dense vector-map indexed by virtual register number. This is consulted
    /// directly for use-queries and augmented with the overlay for def-queries.
    slots: Vec<RealReg>,

    /// Overlay for def-queries. This is a set of updates that occurs "during"
    /// the instruction in question, and will be applied to the slots array
    /// once we are done processing this instruction (in preparation for
    /// the next one).
    overlay: SmallVec<[(VirtualReg, RealReg); 16]>,
}

impl RegUsageMapper {
    /// Allocate a reg-usage mapper with the given predicted vreg capacity.
    pub(crate) fn new(vreg_capacity: usize) -> RegUsageMapper {
        RegUsageMapper {
            slots: Vec::with_capacity(vreg_capacity),
            overlay: SmallVec::new(),
        }
    }

    /// Is the overlay past the sorted-size threshold?
    fn is_overlay_large_enough_to_sort(&self) -> bool {
        // Use the SmallVec spill-to-heap threshold as a threshold for "large
        // enough to sort"; this has the effect of amortizing the cost of
        // sorting along with the cost of copying out to heap memory, and also
        // ensures that when we access heap (more likely to miss in cache), we
        // do it with O(log N) accesses instead of O(N).
        self.overlay.spilled()
    }

    /// Update the overlay.
    pub(crate) fn set_overlay(&mut self, vreg: VirtualReg, rreg: Option<RealReg>) {
        let rreg = rreg.unwrap_or(RealReg::invalid());
        self.overlay.push((vreg, rreg));
    }

    /// Finish updates to the overlay, sorting if necessary.
    pub(crate) fn finish_overlay(&mut self) {
        if self.overlay.len() == 0 || !self.is_overlay_large_enough_to_sort() {
            return;
        }

        // Sort stably, so that later updates continue to come after earlier
        // ones.
        self.overlay.sort_by_key(|pair| pair.0);
        // Remove duplicates by collapsing runs of same-vreg pairs down to
        // the last one.
        let mut last_vreg = self.overlay[0].0;
        let mut out = 0;
        for i in 1..self.overlay.len() {
            let this_vreg = self.overlay[i].0;
            if this_vreg != last_vreg {
                out += 1;
            }
            if i != out {
                self.overlay[out] = self.overlay[i];
            }
            last_vreg = this_vreg;
        }
        let new_len = out + 1;
        self.overlay.truncate(new_len);
    }

    /// Merge the overlay into the main map.
    pub(crate) fn merge_overlay(&mut self) {
        // Take the SmallVec and swap with empty to allow `&mut self` method
        // call below.
        let mappings = mem::replace(&mut self.overlay, SmallVec::new());
        for (vreg, rreg) in mappings.into_iter() {
            self.set_direct_internal(vreg, rreg);
        }
    }

    /// Make a direct update to the mapping. Only usable when the overlay
    /// is empty.
    pub(crate) fn set_direct(&mut self, vreg: VirtualReg, rreg: Option<RealReg>) {
        debug_assert!(self.overlay.is_empty());
        let rreg = rreg.unwrap_or(RealReg::invalid());
        self.set_direct_internal(vreg, rreg);
    }

    fn set_direct_internal(&mut self, vreg: VirtualReg, rreg: RealReg) {
        let idx = vreg.get_index();
        if idx >= self.slots.len() {
            self.slots.resize(idx + 1, RealReg::invalid());
        }
        self.slots[idx] = rreg;
    }

    /// Perform a lookup directly in the main map. Returns `None` for
    /// not-present.
    fn lookup_direct(&self, vreg: VirtualReg) -> Option<RealReg> {
        let idx = vreg.get_index();
        if idx >= self.slots.len() {
            None
        } else {
            Some(self.slots[idx])
        }
    }

    /// Perform a lookup in the overlay. Returns `None` for not-present. No
    /// fallback to main map (that happens in callers). Returns `Some` even
    /// if mapped to `RealReg::invalid()`, because this is a tombstone
    /// (represents deletion) in the overlay.
    fn lookup_overlay(&self, vreg: VirtualReg) -> Option<RealReg> {
        if self.is_overlay_large_enough_to_sort() {
            // Do a binary search; we are guaranteed to have at most one
            // matching because duplicates were collapsed after sorting.
            if let Ok(idx) = self.overlay.binary_search_by_key(&vreg, |pair| pair.0) {
                return Some(self.overlay[idx].1);
            }
        } else {
            // Search in reverse order to find later updates first.
            for &(this_vreg, this_rreg) in self.overlay.iter().rev() {
                if this_vreg == vreg {
                    return Some(this_rreg);
                }
            }
        }
        None
    }

    /// Sanity check: check that all slots are empty. Typically for use at the
    /// end of processing as a debug-assert.
    pub(crate) fn is_empty(&self) -> bool {
        self.overlay.iter().all(|pair| pair.1.is_invalid())
            && self.slots.iter().all(|rreg| rreg.is_invalid())
    }

    // -- client-facing API: --

    /// Return the `RealReg` if mapped, or `None`, for `vreg` occuring as a use
    /// on the current instruction.
    pub fn get_use(&self, vreg: VirtualReg) -> Option<RealReg> {
        self.lookup_direct(vreg)
            // Convert Some(RealReg::invalid()) to None.
            .and_then(|reg| reg.maybe_valid())
    }

    /// Return the `RealReg` if mapped, or `None`, for `vreg` occuring as a def
    /// on the current instruction.
    pub fn get_def(&self, vreg: VirtualReg) -> Option<RealReg> {
        self.lookup_overlay(vreg)
            .or_else(|| self.lookup_direct(vreg))
            // Convert Some(RealReg::invalid()) to None.
            .and_then(|reg| reg.maybe_valid())
    }

    /// Return the `RealReg` if mapped, or `None`, for a `vreg` occuring as a
    /// mod on the current instruction.
    pub fn get_mod(&self, vreg: VirtualReg) -> Option<RealReg> {
        let result = self.get_use(vreg);
        debug_assert_eq!(result, self.get_def(vreg));
        result
    }
}

//=============================================================================
// Definitions of the "real register universe".

// A "Real Register Universe" is a read-only structure that contains all
// information about real registers on a given host.  It serves several
// purposes:
//
// * defines the mapping from real register indices to the registers
//   themselves
//
// * defines the size of the initial section of that mapping that is available
//   to the register allocator for use, so that it can treat the registers
//   under its control as a zero based, contiguous array.  This is important
//   for its efficiency.
//
// * gives meaning to Set<RealReg>, which otherwise would merely be a bunch of
//   bits.

#[derive(Debug)]
pub struct RealRegUniverse {
    // The registers themselves.  All must be real registers, and all must
    // have their index number (.get_index()) equal to the array index here,
    // since this is the only place where we map index numbers to actual
    // registers.
    pub regs: Vec<(RealReg, String)>,

    // This is the size of the initial section of `regs` that is available to
    // the allocator.  It must be < `regs`.len().
    pub allocable: usize,

    // Information about groups of allocable registers. Used to quickly address
    // only a group of allocable registers belonging to the same register class.
    // Indexes into `allocable_by_class` are RegClass values, such as
    // RegClass::F32. If the resulting entry is `None` then there are no
    // registers in that class.  Otherwise the value is a `RegClassInfo`, which
    // provides a register range and possibly information about fixed uses.
    pub allocable_by_class: [Option<RegClassInfo>; NUM_REG_CLASSES],
}

/// Information about a single register class in the `RealRegUniverse`.
#[derive(Clone, Copy, Debug)]
pub struct RegClassInfo {
    // Range of allocatable registers in this register class, in terms of
    // register indices.
    //
    // A range (first, last) specifies the range of entries in
    // `RealRegUniverse.regs` corresponding to that class.  The range includes
    // both `first` and `last`.
    //
    // In all cases, `last` must be < `RealRegUniverse.allocable`.  In other
    // words, all ranges together in `allocable_by_class` must describe only the
    // allocable prefix of `regs`.
    //
    // For example, let's say
    //    allocable_by_class[RegClass::F32] ==
    //      Some(RegClassInfo { first: 10, last: 14, .. })
    // Then regs[10], regs[11], regs[12], regs[13], and regs[14] give all
    // registers of register class RegClass::F32.
    //
    // The effect of the above is that registers in `regs` must form
    // contiguous groups. This is checked by RealRegUniverse::check_is_sane().
    pub first: usize,
    pub last: usize,

    // A register, if any, that is *guaranteed* not to be used as a fixed use
    // in any code, and so that the register allocator can statically reserve
    // for its own use as a temporary. Some register allocators may need such
    // a register for various maneuvers, for example a spillslot-to-spillslot
    // move when no (other) registers are free.
    pub suggested_scratch: Option<usize>,
}

impl RealRegUniverse {
    /// Show it in a pretty way.
    pub fn show(&self) -> Vec<String> {
        let mut res = vec![];
        // Show the allocables
        for class_num in 0..NUM_REG_CLASSES {
            let class_info = match &self.allocable_by_class[class_num] {
                None => continue,
                Some(info) => info,
            };
            let class = RegClass::rc_from_u32(class_num as u32);
            let mut class_str = "class ".to_string()
                + &class.long_name().to_string()
                + &"(".to_string()
                + &class.short_name().to_string()
                + &") at ".to_string();
            class_str = class_str + &format!("[{} .. {}]: ", class_info.first, class_info.last);
            for ix in class_info.first..=class_info.last {
                class_str = class_str + &self.regs[ix].1;
                if let Some(suggested_ix) = class_info.suggested_scratch {
                    if ix == suggested_ix {
                        class_str = class_str + "*";
                    }
                }
                class_str = class_str + " ";
            }
            res.push(class_str);
        }
        // And the non-allocables
        if self.allocable < self.regs.len() {
            let mut stragglers = format!(
                "not allocable at [{} .. {}]: ",
                self.allocable,
                self.regs.len() - 1
            );
            for ix in self.allocable..self.regs.len() {
                stragglers = stragglers + &self.regs[ix].1 + &" ".to_string();
            }
            res.push(stragglers);
        }
        res
    }

    /// Check that the given universe satisfies various invariants, and panic
    /// if not.  All the invariants are important.
    pub fn check_is_sane(&self) {
        let regs_len = self.regs.len();
        let regs_allocable = self.allocable;
        // The universe must contain at most 256 registers.  That's because
        // `Reg` only has an 8-bit index value field, so if the universe
        // contained more than 256 registers, we'd never be able to index into
        // entries 256 and above.  This is no limitation in practice since all
        // targets we're interested in contain (many) fewer than 256 regs in
        // total.
        let mut ok = regs_len <= 256;
        // The number of allocable registers must not exceed the number of
        // `regs` presented.  In general it will be less, since the universe
        // will list some registers (stack pointer, etc) which are not
        // available for allocation.
        if ok {
            ok = regs_allocable <= regs_len;
        }
        // All registers must have an index value which points back at the
        // `regs` slot they are in.  Also they really must be real regs.
        if ok {
            for i in 0..regs_len {
                let (reg, _name) = &self.regs[i];
                if ok && (reg.to_reg().is_virtual() || reg.get_index() != i) {
                    ok = false;
                }
            }
        }
        // The allocatable regclass groupings defined by `allocable_first` and
        // `allocable_last` must be contiguous.
        if ok {
            let mut regclass_used = [false; NUM_REG_CLASSES];
            for rc in 0..NUM_REG_CLASSES {
                regclass_used[rc] = false;
            }
            for i in 0..regs_allocable {
                let (reg, _name) = &self.regs[i];
                let rc = reg.get_class().rc_to_u32() as usize;
                regclass_used[rc] = true;
            }
            // Scan forward through each grouping, checking that the listed
            // registers really are of the claimed class.  Also count the
            // total number visited.  This seems a fairly reliable way to
            // ensure that the groupings cover all allocated registers exactly
            // once, and that all classes are contiguous groups.
            let mut regs_visited = 0;
            for rc in 0..NUM_REG_CLASSES {
                match &self.allocable_by_class[rc] {
                    &None => {
                        if regclass_used[rc] {
                            ok = false;
                        }
                    }
                    &Some(RegClassInfo {
                        first,
                        last,
                        suggested_scratch,
                    }) => {
                        if !regclass_used[rc] {
                            ok = false;
                        }
                        if ok {
                            for i in first..last + 1 {
                                let (reg, _name) = &self.regs[i];
                                if ok && RegClass::rc_from_u32(rc as u32) != reg.get_class() {
                                    ok = false;
                                }
                                regs_visited += 1;
                            }
                        }
                        if ok {
                            if let Some(s) = suggested_scratch {
                                if s < first || s > last {
                                    ok = false;
                                }
                            }
                        }
                    }
                }
            }
            if ok && regs_visited != regs_allocable {
                ok = false;
            }
        }
        // So finally ..
        if !ok {
            panic!("RealRegUniverse::check_is_sane: invalid RealRegUniverse");
        }
    }
}

//=============================================================================
// Representing and printing of live range fragments.

#[derive(Copy, Clone, Hash, PartialEq, Eq, Ord)]
// There are four "points" within an instruction that are of interest, and
// these have a total ordering: R < U < D < S.  They are:
//
// * R(eload): this is where any reload insns for the insn itself are
//   considered to live.
//
// * U(se): this is where the insn is considered to use values from those of
//   its register operands that appear in a Read or Modify role.
//
// * D(ef): this is where the insn is considered to define new values for
//   those of its register operands that appear in a Write or Modify role.
//
// * S(pill): this is where any spill insns for the insn itself are considered
//   to live.
//
// Instructions in the incoming Func may only exist at the U and D points,
// and so their associated live range fragments will only mention the U and D
// points.  However, when adding spill code, we need a way to represent live
// ranges involving the added spill and reload insns, in which case R and S
// come into play:
//
// * A reload for instruction i is considered to be live from i.R to i.U.
//
// * A spill for instruction i is considered to be live from i.D to i.S.

pub enum Point {
    Reload,
    Use,
    Def,
    Spill,
}

impl Point {
    pub fn min_value() -> Self {
        Self::Reload
    }
    pub fn max_value() -> Self {
        Self::Spill
    }
    pub fn is_reload(self) -> bool {
        match self {
            Point::Reload => true,
            _ => false,
        }
    }
    pub fn is_use(self) -> bool {
        match self {
            Point::Use => true,
            _ => false,
        }
    }
    pub fn is_def(self) -> bool {
        match self {
            Point::Def => true,
            _ => false,
        }
    }
    pub fn is_spill(self) -> bool {
        match self {
            Point::Spill => true,
            _ => false,
        }
    }
    pub fn is_use_or_def(self) -> bool {
        self.is_use() || self.is_def()
    }
}

impl PartialOrd for Point {
    // In short .. R < U < D < S.  This is probably what would be #derive'd
    // anyway, but we need to be sure.
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        // This is a bit idiotic, but hey .. hopefully LLVM can turn it into a
        // no-op.
        fn convert(pt: &Point) -> u32 {
            match pt {
                Point::Reload => 0,
                Point::Use => 1,
                Point::Def => 2,
                Point::Spill => 3,
            }
        }
        convert(self).partial_cmp(&convert(other))
    }
}

// See comments below on `RangeFrag` for the meaning of `InstPoint`.
#[derive(Copy, Clone, Hash, PartialEq, Eq, Ord)]
pub struct InstPoint {
    pub iix: InstIx,
    pub pt: Point,
}

impl InstPoint {
    #[inline(always)]
    pub fn new(iix: InstIx, pt: Point) -> Self {
        InstPoint { iix, pt }
    }
    #[inline(always)]
    pub fn new_reload(iix: InstIx) -> Self {
        InstPoint {
            iix,
            pt: Point::Reload,
        }
    }
    #[inline(always)]
    pub fn new_use(iix: InstIx) -> Self {
        InstPoint {
            iix,
            pt: Point::Use,
        }
    }
    #[inline(always)]
    pub fn new_def(iix: InstIx) -> Self {
        InstPoint {
            iix,
            pt: Point::Def,
        }
    }
    #[inline(always)]
    pub fn new_spill(iix: InstIx) -> Self {
        InstPoint {
            iix,
            pt: Point::Spill,
        }
    }
    pub fn step(&self) -> Self {
        match self.pt {
            Point::Reload => InstPoint::new(self.iix, Point::Use),
            Point::Use => InstPoint::new(self.iix, Point::Def),
            Point::Def => InstPoint::new(self.iix, Point::Spill),
            Point::Spill => InstPoint::new(self.iix.plus(1), Point::Reload),
        }
    }
    pub fn max_value() -> Self {
        Self {
            iix: InstIx::max_value(),
            pt: Point::max_value(),
        }
    }
    pub fn min_value() -> Self {
        Self {
            iix: InstIx::min_value(),
            pt: Point::min_value(),
        }
    }
}

impl PartialOrd for InstPoint {
    // Again .. don't assume anything about the #derive'd version.  These have
    // to be ordered using `iix` as the primary key and `pt` as the
    // secondary.
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        match self.iix.partial_cmp(&other.iix) {
            Some(Ordering::Less) => Some(Ordering::Less),
            Some(Ordering::Greater) => Some(Ordering::Greater),
            Some(Ordering::Equal) => self.pt.partial_cmp(&other.pt),
            None => panic!("InstPoint::partial_cmp: fail #1"),
        }
    }
}

impl fmt::Debug for InstPoint {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(
            fmt,
            "{:?}{}",
            self.iix,
            match self.pt {
                Point::Reload => ".r",
                Point::Use => ".u",
                Point::Def => ".d",
                Point::Spill => ".s",
            }
        )
    }
}

// A handy summary hint for a RangeFrag.
#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum RangeFragKind {
    Local,   // Fragment exists entirely inside one block
    LiveIn,  // Fragment is live in to a block, but ends inside it
    LiveOut, // Fragment is live out of a block, but starts inside it
    Thru,    // Fragment is live through the block (starts and ends outside it)
    // Multi is a hack that is required by BT's experimental RangeFrag
    // compression.  I hope to be able to get rid of it in subsequent work to
    // clean up and improve the compression.
    Multi, // Fragment spans multiple blocks ("is compressed")
}

impl fmt::Debug for RangeFragKind {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            RangeFragKind::Local => write!(fmt, "Local"),
            RangeFragKind::LiveIn => write!(fmt, "LiveIn"),
            RangeFragKind::LiveOut => write!(fmt, "LiveOut"),
            RangeFragKind::Thru => write!(fmt, "Thru"),
            // Hack, per comment above
            RangeFragKind::Multi => write!(fmt, "Multi"),
        }
    }
}

//=============================================================================
// Metrics.  Meaning, estimated hotness, etc, numbers, which don't have any
// effect on the correctness of the resulting allocation, but which are
// important for getting a good allocation, basically by giving preference for
// the hottest values getting a register.

/* Required metrics:
   Block (a basic block):
   - Estimated relative execution frequency ("EEF")
     Calculated from loop nesting depth, depth inside an if-tree, etc
     Suggested: u16

   RangeFrag (Live Range Fragment):
   - Length (in instructions).  Can be calculated, = end - start + 1.
   - Number of uses (of the associated Reg)
     Suggested: u16

   LR (Live Range, = a set of Live Range Fragments):
   - spill cost (can be calculated)
     = sum, for each frag:
            frag.#uses / frag.len * frag.block.estFreq
       with the proviso that spill/reload LRs must have spill cost of infinity
     Do this with a f32 so we don't have to worry about scaling/overflow.
*/

// A Live Range Fragment (RangeFrag) describes a consecutive sequence of one or
// more instructions, in which a Reg is "live".  The sequence must exist
// entirely inside only one basic block.
//
// However, merely indicating the start and end instruction numbers isn't
// enough: we must also include a "Use or Def" indication.  These indicate two
// different "points" within each instruction: the Use position, where
// incoming registers are read, and the Def position, where outgoing registers
// are written.  The Use position is considered to come before the Def
// position, as described for `Point` above.
//
// When we come to generate spill/restore live ranges, Point::S and Point::R
// also come into play.  Live ranges (and hence, RangeFrags) that do not perform
// spills or restores should not use either of Point::S or Point::R.
//
// The set of positions denoted by
//
//    {0 .. #insns-1} x {Reload point, Use point, Def point, Spill point}
//
// is exactly the set of positions that we need to keep track of when mapping
// live ranges to registers.  This the reason for the type InstPoint.  Note
// that InstPoint values have a total ordering, at least within a single basic
// block: the insn number is used as the primary key, and the Point part is
// the secondary key, with Reload < Use < Def < Spill.
//
// Finally, a RangeFrag has a `count` field, which is a u16 indicating how often
// the associated storage unit (Reg) is mentioned inside the RangeFrag.  It is
// assumed that the RangeFrag is associated with some Reg.  If not, the `count`
// field is meaningless.
//
// The `bix` field is actually redundant, since the containing `Block` can be
// inferred, laboriously, from `first` and `last`, providing you have a
// `Block` table to hand.  It is included here for convenience.

#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct RangeFrag {
    pub bix: BlockIx,
    pub kind: RangeFragKind,
    pub first: InstPoint,
    pub last: InstPoint,
    pub count: u16,
}

impl fmt::Debug for RangeFrag {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(
            fmt,
            "(RF: {:?}, count={}, {:?}, {:?}-{:?})",
            self.bix, self.count, self.kind, self.first, self.last
        )
    }
}

impl RangeFrag {
    pub fn new_multi_block<F: Function>(
        f: &F,
        bix: BlockIx,
        first: InstPoint,
        last: InstPoint,
        count: u16,
    ) -> Self {
        debug_assert!(first <= last);
        if first == last {
            debug_assert!(count == 1);
        }
        let first_iix_in_block = f.block_insns(bix).first();
        let last_iix_in_block = f.block_insns(bix).last();
        let first_pt_in_block = InstPoint::new_use(first_iix_in_block);
        let last_pt_in_block = InstPoint::new_def(last_iix_in_block);
        let kind = match (first == first_pt_in_block, last == last_pt_in_block) {
            (false, false) => RangeFragKind::Local,
            (false, true) => RangeFragKind::LiveOut,
            (true, false) => RangeFragKind::LiveIn,
            (true, true) => RangeFragKind::Thru,
        };
        RangeFrag {
            bix,
            kind,
            first,
            last,
            count,
        }
    }

    pub fn new<F: Function>(
        f: &F,
        bix: BlockIx,
        first: InstPoint,
        last: InstPoint,
        count: u16,
    ) -> Self {
        debug_assert!(f.block_insns(bix).len() >= 1);
        debug_assert!(f.block_insns(bix).contains(first.iix));
        debug_assert!(f.block_insns(bix).contains(last.iix));
        RangeFrag::new_multi_block(f, bix, first, last, count)
    }
}

// Comparison of RangeFrags.  They form a partial order.

pub fn cmp_range_frags(f1: &RangeFrag, f2: &RangeFrag) -> Option<Ordering> {
    if f1.last < f2.first {
        return Some(Ordering::Less);
    }
    if f1.first > f2.last {
        return Some(Ordering::Greater);
    }
    if f1.first == f2.first && f1.last == f2.last {
        return Some(Ordering::Equal);
    }
    None
}

impl RangeFrag {
    pub fn contains(&self, ipt: &InstPoint) -> bool {
        self.first <= *ipt && *ipt <= self.last
    }
}

//=============================================================================
// Vectors of RangeFragIxs, sorted so that the associated RangeFrags are in
// ascending order (per their InstPoint fields).
//
// The "fragment environment" (sometimes called 'fenv' or 'frag_env') to which
// the RangeFragIxs refer, is not stored here.

// Helper for SmallVec
//fn smallVec_from_TypedIxVec(src: &TypedIxVec<A, B>) -> SmallVec<C> // hmm

#[derive(Clone)]
pub struct SortedRangeFragIxs {
    pub frag_ixs: SmallVec<[RangeFragIx; 4]>,
}

impl fmt::Debug for SortedRangeFragIxs {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        self.frag_ixs.fmt(fmt)
    }
}

impl SortedRangeFragIxs {
    pub fn cmp_debug_only(&self, other: &SortedRangeFragIxs) -> Ordering {
        self.frag_ixs.cmp(&other.frag_ixs)
    }

    pub fn check(&self, fenv: &TypedIxVec<RangeFragIx, RangeFrag>) {
        let mut ok = true;
        for i in 1..self.frag_ixs.len() {
            let prev_frag = &fenv[self.frag_ixs[i - 1]];
            let this_frag = &fenv[self.frag_ixs[i - 0]];
            if cmp_range_frags(prev_frag, this_frag) != Some(Ordering::Less) {
                ok = false;
                break;
            }
        }
        if !ok {
            panic!("SortedRangeFragIxs::check: vector not ok");
        }
    }

    pub fn sort(&mut self, fenv: &TypedIxVec<RangeFragIx, RangeFrag>) {
        self.frag_ixs.sort_unstable_by(|fix_a, fix_b| {
            match cmp_range_frags(&fenv[*fix_a], &fenv[*fix_b]) {
                Some(Ordering::Less) => Ordering::Less,
                Some(Ordering::Greater) => Ordering::Greater,
                Some(Ordering::Equal) | None => {
                    panic!("SortedRangeFragIxs::new: overlapping Frags!")
                }
            }
        });
    }

    pub fn new(
        frag_ixs: SmallVec<[RangeFragIx; 4]>,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
    ) -> Self {
        let mut res = SortedRangeFragIxs { frag_ixs };
        // check the source is ordered, and clone (or sort it)
        res.sort(fenv);
        res.check(fenv);
        res
    }

    pub fn unit(fix: RangeFragIx, fenv: &TypedIxVec<RangeFragIx, RangeFrag>) -> Self {
        let mut res = SortedRangeFragIxs {
            frag_ixs: SmallVec::<[RangeFragIx; 4]>::new(),
        };
        res.frag_ixs.push(fix);
        res.check(fenv);
        res
    }
}

//=============================================================================
// Representing spill costs.  A spill cost can either be infinite, in which
// case the associated VirtualRange may not be spilled, because it's already a
// spill/reload range.  Or it can be finite, in which case it must be a 32-bit
// floating point number, which is (in the IEEE754 meaning of the terms)
// non-infinite, non-NaN and it must be non negative.  In fact it's
// meaningless for a VLR to have a zero spill cost (how could that really be
// the case?) but we allow it here for convenience.

#[derive(Copy, Clone)]
pub enum SpillCost {
    Infinite,    // Infinite, positive
    Finite(f32), // Finite, non-negative
}

impl fmt::Debug for SpillCost {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            SpillCost::Infinite => write!(fmt, "INFINITY"),
            SpillCost::Finite(c) => write!(fmt, "{:<.3}", c),
        }
    }
}

impl SpillCost {
    pub fn zero() -> Self {
        SpillCost::Finite(0.0)
    }
    pub fn infinite() -> Self {
        SpillCost::Infinite
    }
    pub fn finite(cost: f32) -> Self {
        // "`is_normal` returns true if the number is neither zero, infinite,
        // subnormal, or NaN."
        assert!(cost.is_normal() || cost == 0.0);
        // And also it can't be negative.
        assert!(cost >= 0.0);
        // Somewhat arbitrarily ..
        assert!(cost < 1e18);
        SpillCost::Finite(cost)
    }
    pub fn is_zero(&self) -> bool {
        match self {
            SpillCost::Infinite => false,
            SpillCost::Finite(c) => *c == 0.0,
        }
    }
    pub fn is_infinite(&self) -> bool {
        match self {
            SpillCost::Infinite => true,
            SpillCost::Finite(_) => false,
        }
    }
    pub fn is_finite(&self) -> bool {
        !self.is_infinite()
    }
    pub fn is_less_than(&self, other: &Self) -> bool {
        match (self, other) {
            // Dubious .. both are infinity
            (SpillCost::Infinite, SpillCost::Infinite) => false,
            // finite < inf
            (SpillCost::Finite(_), SpillCost::Infinite) => true,
            // inf is not < finite
            (SpillCost::Infinite, SpillCost::Finite(_)) => false,
            // straightforward
            (SpillCost::Finite(c1), SpillCost::Finite(c2)) => c1 < c2,
        }
    }
    pub fn partial_cmp_debug_only(&self, other: &Self) -> Option<Ordering> {
        // NB!  This is only for debugging; it serves only to give an arbitrary
        // structural partial ordering on `SpillCost` so it can participate in
        // sorting.  The induced ordering should not be used to make any
        // judgements about spill costs.
        match (self, other) {
            (SpillCost::Infinite, SpillCost::Infinite) => Some(Ordering::Equal),
            (SpillCost::Finite(_), SpillCost::Infinite) => Some(Ordering::Less),
            (SpillCost::Infinite, SpillCost::Finite(_)) => Some(Ordering::Greater),
            (SpillCost::Finite(c1), SpillCost::Finite(c2)) => c1.partial_cmp(c2),
        }
    }
    pub fn add(&mut self, other: &Self) {
        match (*self, other) {
            (SpillCost::Finite(c1), SpillCost::Finite(c2)) => {
                // The 10^18 limit above gives us a lot of headroom here, since max
                // f32 is around 10^37.
                *self = SpillCost::Finite(c1 + c2);
            }
            (_, _) => {
                // All other cases produce an infinity.
                *self = SpillCost::Infinite;
            }
        }
    }
}

//=============================================================================
// Representing and printing live ranges.  These are represented by two
// different but closely related types, RealRange and VirtualRange.

// RealRanges are live ranges for real regs (RealRegs).  VirtualRanges are
// live ranges for virtual regs (VirtualRegs).  VirtualRanges are the
// fundamental unit of allocation.  Both RealRange and VirtualRange pair the
// relevant kind of Reg with a vector of RangeFragIxs in which it is live.
// The RangeFragIxs are indices into some vector of RangeFrags (a "fragment
// environment", 'fenv'), which is not specified here.  They are sorted so as
// to give ascending order to the RangeFrags which they refer to.
//
// VirtualRanges contain metrics.  Not all are initially filled in:
//
// * `size` is the number of instructions in total spanned by the LR.  It must
//   not be zero.
//
// * `spill_cost` is an abstractified measure of the cost of spilling the LR.
//   The only constraint (w.r.t. correctness) is that normal LRs have a `Some`
//   value, whilst `None` is reserved for live ranges created for spills and
//   reloads and interpreted to mean "infinity".  This is needed to guarantee
//   that allocation can always succeed in the worst case, in which all of the
//   original live ranges of the program are spilled.
//
// RealRanges don't carry any metrics info since we are not trying to allocate
// them.  We merely need to work around them.
//
// I find it helpful to think of a live range, both RealRange and
// VirtualRange, as a "renaming equivalence class".  That is, if you rename
// `reg` at some point inside `sorted_frags`, then you must rename *all*
// occurrences of `reg` inside `sorted_frags`, since otherwise the program will
// no longer work.
//
// Invariants for RealRange/VirtualRange RangeFrag sets (their `sfrags` fields):
//
// * Either `sorted_frags` contains just one RangeFrag, in which case it *must*
//   be RangeFragKind::Local.
//
// * Or `sorted_frags` contains more than one RangeFrag, in which case: at
//   least one must be RangeFragKind::LiveOut, at least one must be
//   RangeFragKind::LiveIn, and there may be zero or more RangeFragKind::Thrus.

#[derive(Clone)]
pub struct RealRange {
    pub rreg: RealReg,
    pub sorted_frags: SortedRangeFragIxs,
}

impl fmt::Debug for RealRange {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "(RR: {:?}, {:?})", self.rreg, self.sorted_frags)
    }
}

impl RealRange {
    pub fn cmp_debug_only(&self, other: &RealRange) -> Ordering {
        match self.rreg.cmp(&other.rreg) {
            Ordering::Equal => self.sorted_frags.cmp_debug_only(&other.sorted_frags),
            oth1 => oth1,
        }
    }
    pub fn show_with_rru(&self, univ: &RealRegUniverse) -> String {
        format!(
            "(RR: {}, {:?})",
            self.rreg.to_reg().show_with_rru(univ),
            self.sorted_frags
        )
    }
}

// VirtualRanges are live ranges for virtual regs (VirtualRegs).  This does
// carry metrics info and also the identity of the RealReg to which it
// eventually got allocated.  (Or in the backtracking allocator, the identity
// of the RealReg to which it is *currently* assigned; that may be undone at
// some later point.)

#[derive(Clone)]
pub struct VirtualRange {
    pub vreg: VirtualReg,
    pub rreg: Option<RealReg>,
    pub sorted_frags: SortedRangeFragIxs,
    pub size: u16,
    pub total_cost: u32,
    pub spill_cost: SpillCost, // == total_cost / size
}

impl fmt::Debug for VirtualRange {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "(VR: {:?},", self.vreg)?;
        if self.rreg.is_some() {
            write!(fmt, " -> {:?}", self.rreg.unwrap())?;
        }
        write!(
            fmt,
            " sz={}, tc={}, sc={:?}, {:?})",
            self.size, self.total_cost, self.spill_cost, self.sorted_frags
        )
    }
}

impl VirtualRange {
    pub fn cmp_debug_only(&self, other: &VirtualRange) -> Ordering {
        match self.vreg.cmp(&other.vreg) {
            Ordering::Equal => match self.rreg.cmp(&other.rreg) {
                Ordering::Equal => match self.sorted_frags.cmp_debug_only(&other.sorted_frags) {
                    Ordering::Equal => match self.size.cmp(&other.size) {
                        Ordering::Equal => self
                            .spill_cost
                            .partial_cmp_debug_only(&other.spill_cost)
                            .unwrap(),
                        oth4 => oth4,
                    },
                    oth3 => oth3,
                },
                oth2 => oth2,
            },
            oth1 => oth1,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn vreg(idx: u32) -> VirtualReg {
        Reg::new_virtual(RegClass::I64, idx).to_virtual_reg()
    }
    fn rreg(idx: u8) -> RealReg {
        Reg::new_real(RegClass::I64, /* enc = */ 0, /* index = */ idx).to_real_reg()
    }

    #[test]
    fn test_reg_use_mapper() {
        let mut mapper = RegUsageMapper::new(/* estimated vregs = */ 16);
        assert_eq!(None, mapper.get_use(vreg(0)));
        assert_eq!(None, mapper.get_def(vreg(0)));
        assert_eq!(None, mapper.get_mod(vreg(0)));

        mapper.set_direct(vreg(0), Some(rreg(1)));
        mapper.set_direct(vreg(1), Some(rreg(2)));

        assert_eq!(Some(rreg(1)), mapper.get_use(vreg(0)));
        assert_eq!(Some(rreg(1)), mapper.get_def(vreg(0)));
        assert_eq!(Some(rreg(1)), mapper.get_mod(vreg(0)));
        assert_eq!(Some(rreg(2)), mapper.get_use(vreg(1)));
        assert_eq!(Some(rreg(2)), mapper.get_def(vreg(1)));
        assert_eq!(Some(rreg(2)), mapper.get_mod(vreg(1)));

        mapper.set_overlay(vreg(0), Some(rreg(3)));
        mapper.set_overlay(vreg(2), Some(rreg(4)));
        mapper.finish_overlay();

        assert_eq!(Some(rreg(1)), mapper.get_use(vreg(0)));
        assert_eq!(Some(rreg(3)), mapper.get_def(vreg(0)));
        // vreg 0 not valid for mod (use and def differ).
        assert_eq!(Some(rreg(2)), mapper.get_use(vreg(1)));
        assert_eq!(Some(rreg(2)), mapper.get_def(vreg(1)));
        assert_eq!(Some(rreg(2)), mapper.get_mod(vreg(1)));
        assert_eq!(None, mapper.get_use(vreg(2)));
        assert_eq!(Some(rreg(4)), mapper.get_def(vreg(2)));
        // vreg 2 not valid for mod (use and def differ).

        mapper.merge_overlay();
        assert_eq!(Some(rreg(3)), mapper.get_use(vreg(0)));
        assert_eq!(Some(rreg(2)), mapper.get_use(vreg(1)));
        assert_eq!(Some(rreg(4)), mapper.get_use(vreg(2)));
        assert_eq!(None, mapper.get_use(vreg(3)));

        // Check tombstoning behavior.
        mapper.set_overlay(vreg(0), None);
        mapper.finish_overlay();
        assert_eq!(Some(rreg(3)), mapper.get_use(vreg(0)));
        assert_eq!(None, mapper.get_def(vreg(0)));
        mapper.merge_overlay();

        // Check large (sorted) overlay mode.
        for i in (2..50).rev() {
            mapper.set_overlay(vreg(i), Some(rreg((i + 100) as u8)));
        }
        mapper.finish_overlay();
        assert_eq!(None, mapper.get_use(vreg(0)));
        assert_eq!(Some(rreg(2)), mapper.get_use(vreg(1)));
        assert_eq!(Some(rreg(4)), mapper.get_use(vreg(2)));
        for i in 2..50 {
            assert_eq!(Some(rreg((i + 100) as u8)), mapper.get_def(vreg(i)));
        }
        mapper.merge_overlay();

        for i in (0..100).rev() {
            mapper.set_overlay(vreg(i), None);
        }
        mapper.finish_overlay();
        for i in 0..100 {
            assert_eq!(None, mapper.get_def(vreg(i)));
        }
        assert_eq!(false, mapper.is_empty());
        mapper.merge_overlay();
        assert_eq!(true, mapper.is_empty());

        // Check multiple-update behavior in small mode.
        mapper.set_overlay(vreg(1), Some(rreg(1)));
        mapper.set_overlay(vreg(1), Some(rreg(2)));
        mapper.finish_overlay();
        assert_eq!(Some(rreg(2)), mapper.get_def(vreg(1)));
        mapper.merge_overlay();
        assert_eq!(Some(rreg(2)), mapper.get_use(vreg(1)));

        mapper.set_overlay(vreg(1), Some(rreg(2)));
        mapper.set_overlay(vreg(1), None);
        mapper.finish_overlay();
        assert_eq!(None, mapper.get_def(vreg(1)));
        mapper.merge_overlay();
        assert_eq!(None, mapper.get_use(vreg(1)));

        // Check multiple-update behavior in sorted mode.
        for i in 0..100 {
            mapper.set_overlay(vreg(2), Some(rreg(i)));
        }
        for i in 0..100 {
            mapper.set_overlay(vreg(2), Some(rreg(2 * i)));
        }
        mapper.finish_overlay();
        assert_eq!(Some(rreg(198)), mapper.get_def(vreg(2)));
        mapper.merge_overlay();
        assert_eq!(Some(rreg(198)), mapper.get_use(vreg(2)));

        for i in 0..100 {
            mapper.set_overlay(vreg(2), Some(rreg(i)));
        }
        for _ in 0..100 {
            mapper.set_overlay(vreg(2), None);
        }
        mapper.finish_overlay();
        assert_eq!(None, mapper.get_def(vreg(50)));
        mapper.merge_overlay();
        assert_eq!(None, mapper.get_use(vreg(50)));
    }
}

//=============================================================================
// Test cases

// sewardj 2020Mar04: these are commented out for now, as they no longer
// compile.  They may be useful later though, once BT acquires an interval
// tree implementation for its CommitmentMap.

/*
#[test]
fn test_sorted_frag_ranges() {
  // Create a RangeFrag and RangeFragIx from two InstPoints.
  fn gen_fix(
    fenv: &mut TypedIxVec<RangeFragIx, RangeFrag>, first: InstPoint,
    last: InstPoint,
  ) -> RangeFragIx {
    assert!(first <= last);
    let res = RangeFragIx::new(fenv.len() as u32);
    let frag = RangeFrag {
      bix: BlockIx::new(123),
      kind: RangeFragKind::Local,
      first,
      last,
      count: 0,
    };
    fenv.push(frag);
    res
  }

  fn get_range_frag(
    fenv: &TypedIxVec<RangeFragIx, RangeFrag>, fix: RangeFragIx,
  ) -> &RangeFrag {
    &fenv[fix]
  }

  // Structural equality, at least.  Not equality in the sense of
  // deferencing the contained RangeFragIxes.
  fn sorted_range_eq(
    fixs1: &SortedRangeFragIxs, fixs2: &SortedRangeFragIxs,
  ) -> bool {
    if fixs1.frag_ixs.len() != fixs2.frag_ixs.len() {
      return false;
    }
    for (mf1, mf2) in fixs1.frag_ixs.iter().zip(&fixs2.frag_ixs) {
      if mf1 != mf2 {
        return false;
      }
    }
    true
  }

  let iix3 = InstIx::new(3);
  let iix4 = InstIx::new(4);
  let iix5 = InstIx::new(5);
  let iix6 = InstIx::new(6);
  let iix7 = InstIx::new(7);
  let iix10 = InstIx::new(10);
  let iix12 = InstIx::new(12);

  let fp_3u = InstPoint::new_use(iix3);
  let fp_3d = InstPoint::new_def(iix3);

  let fp_4u = InstPoint::new_use(iix4);

  let fp_5u = InstPoint::new_use(iix5);
  let fp_5d = InstPoint::new_def(iix5);

  let fp_6u = InstPoint::new_use(iix6);
  let fp_6d = InstPoint::new_def(iix6);

  let fp_7u = InstPoint::new_use(iix7);
  let fp_7d = InstPoint::new_def(iix7);

  let fp_10u = InstPoint::new_use(iix10);
  let fp_12u = InstPoint::new_use(iix12);

  let mut fenv = TypedIxVec::<RangeFragIx, RangeFrag>::new();

  let fix_3u = gen_fix(&mut fenv, fp_3u, fp_3u);
  let fix_3d = gen_fix(&mut fenv, fp_3d, fp_3d);
  let fix_4u = gen_fix(&mut fenv, fp_4u, fp_4u);
  let fix_3u_5u = gen_fix(&mut fenv, fp_3u, fp_5u);
  let fix_3d_5d = gen_fix(&mut fenv, fp_3d, fp_5d);
  let fix_3d_5u = gen_fix(&mut fenv, fp_3d, fp_5u);
  let fix_3u_5d = gen_fix(&mut fenv, fp_3u, fp_5d);
  let fix_6u_6d = gen_fix(&mut fenv, fp_6u, fp_6d);
  let fix_7u_7d = gen_fix(&mut fenv, fp_7u, fp_7d);
  let fix_10u = gen_fix(&mut fenv, fp_10u, fp_10u);
  let fix_12u = gen_fix(&mut fenv, fp_12u, fp_12u);

  // Boundary checks for point ranges, 3u vs 3d
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3u),
      get_range_frag(&fenv, fix_3u)
    ) == Some(Ordering::Equal)
  );
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3u),
      get_range_frag(&fenv, fix_3d)
    ) == Some(Ordering::Less)
  );
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3d),
      get_range_frag(&fenv, fix_3u)
    ) == Some(Ordering::Greater)
  );

  // Boundary checks for point ranges, 3d vs 4u
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3d),
      get_range_frag(&fenv, fix_3d)
    ) == Some(Ordering::Equal)
  );
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3d),
      get_range_frag(&fenv, fix_4u)
    ) == Some(Ordering::Less)
  );
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_4u),
      get_range_frag(&fenv, fix_3d)
    ) == Some(Ordering::Greater)
  );

  // Partially overlapping
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3d_5d),
      get_range_frag(&fenv, fix_3u_5u)
    ) == None
  );
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3u_5u),
      get_range_frag(&fenv, fix_3d_5d)
    ) == None
  );

  // Completely overlapping: one contained within the other
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3d_5u),
      get_range_frag(&fenv, fix_3u_5d)
    ) == None
  );
  assert!(
    cmp_range_frags(
      get_range_frag(&fenv, fix_3u_5d),
      get_range_frag(&fenv, fix_3d_5u)
    ) == None
  );

  // Create a SortedRangeFragIxs from a bunch of RangeFrag indices
  fn new_sorted_frag_ranges(
    fenv: &TypedIxVec<RangeFragIx, RangeFrag>, frags: &Vec<RangeFragIx>,
  ) -> SortedRangeFragIxs {
    SortedRangeFragIxs::new(&frags, fenv)
  }

  // Construction tests
  // These fail due to overlap
  //let _ = new_sorted_frag_ranges(&fenv, &vec![fix_3u_3u, fix_3u_3u]);
  //let _ = new_sorted_frag_ranges(&fenv, &vec![fix_3u_5u, fix_3d_5d]);

  // These fail due to not being in order
  //let _ = new_sorted_frag_ranges(&fenv, &vec![fix_4u_4u, fix_3u_3u]);

  // Simple non-overlap tests for add()

  let smf_empty = new_sorted_frag_ranges(&fenv, &vec![]);
  let smf_6_7_10 =
    new_sorted_frag_ranges(&fenv, &vec![fix_6u_6d, fix_7u_7d, fix_10u]);
  let smf_3_12 = new_sorted_frag_ranges(&fenv, &vec![fix_3u, fix_12u]);
  let smf_3_6_7_10_12 = new_sorted_frag_ranges(
    &fenv,
    &vec![fix_3u, fix_6u_6d, fix_7u_7d, fix_10u, fix_12u],
  );
  let mut tmp;

  tmp = smf_empty.clone();
  tmp.add(&smf_empty, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_empty));

  tmp = smf_3_12.clone();
  tmp.add(&smf_empty, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_3_12));

  tmp = smf_empty.clone();
  tmp.add(&smf_3_12, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_3_12));

  tmp = smf_6_7_10.clone();
  tmp.add(&smf_3_12, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_3_6_7_10_12));

  tmp = smf_3_12.clone();
  tmp.add(&smf_6_7_10, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_3_6_7_10_12));

  // Tests for can_add()
  assert!(true == smf_empty.can_add(&smf_empty, &fenv));
  assert!(true == smf_empty.can_add(&smf_3_12, &fenv));
  assert!(true == smf_3_12.can_add(&smf_empty, &fenv));
  assert!(false == smf_3_12.can_add(&smf_3_12, &fenv));

  assert!(true == smf_6_7_10.can_add(&smf_3_12, &fenv));

  assert!(true == smf_3_12.can_add(&smf_6_7_10, &fenv));

  // Tests for del()
  let smf_6_7 = new_sorted_frag_ranges(&fenv, &vec![fix_6u_6d, fix_7u_7d]);
  let smf_6_10 = new_sorted_frag_ranges(&fenv, &vec![fix_6u_6d, fix_10u]);
  let smf_7 = new_sorted_frag_ranges(&fenv, &vec![fix_7u_7d]);
  let smf_10 = new_sorted_frag_ranges(&fenv, &vec![fix_10u]);

  tmp = smf_empty.clone();
  tmp.del(&smf_empty, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_empty));

  tmp = smf_3_12.clone();
  tmp.del(&smf_empty, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_3_12));

  tmp = smf_empty.clone();
  tmp.del(&smf_3_12, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_empty));

  tmp = smf_6_7_10.clone();
  tmp.del(&smf_3_12, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_6_7_10));

  tmp = smf_3_12.clone();
  tmp.del(&smf_6_7_10, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_3_12));

  tmp = smf_6_7_10.clone();
  tmp.del(&smf_6_7, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_10));

  tmp = smf_6_7_10.clone();
  tmp.del(&smf_10, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_6_7));

  tmp = smf_6_7_10.clone();
  tmp.del(&smf_7, &fenv);
  assert!(sorted_range_eq(&tmp, &smf_6_10));

  // Tests for can_add_if_we_first_del()
  let smf_10_12 = new_sorted_frag_ranges(&fenv, &vec![fix_10u, fix_12u]);

  assert!(
    true
      == smf_6_7_10
        .can_add_if_we_first_del(/*d=*/ &smf_10_12, /*a=*/ &smf_3_12, &fenv)
  );

  assert!(
    false
      == smf_6_7_10
        .can_add_if_we_first_del(/*d=*/ &smf_10_12, /*a=*/ &smf_7, &fenv)
  );
}
*/
