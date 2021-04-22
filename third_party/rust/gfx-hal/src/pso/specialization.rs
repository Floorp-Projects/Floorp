//! Pipeline specialization types.

use std::{borrow::Cow, ops::Range, slice};

/// Description of a specialization constant for the pipeline.
#[derive(Debug, Clone, Hash, PartialEq)]
pub struct SpecializationConstant {
    /// Constant identifier in shader source.
    pub id: u32,
    /// Value to override specialization constant.
    pub range: Range<u16>,
}

/// Information required for pipeline specialization.
///
/// Specialization allows for easy configuration of multiple similar pipelines.
/// For example, there may be a boolean exposed to the shader that switches
/// the [specularity](https://en.wikipedia.org/wiki/Specularity) on/off,
/// provided via a [specialization constant][SpecializationConstant].
///
/// That would produce separate PSO's for the "on" and "off" states
/// but they share most of the internal stuff and are fast to produce.
/// More importantly, they are fast to execute, since the driver
/// can optimize out the branch on that other PSO creation.
#[derive(Debug, Clone)]
pub struct Specialization<'a> {
    /// Array of descriptors of specialization constants to override.
    pub constants: Cow<'a, [SpecializationConstant]>,
    /// Raw data of the specialization constants
    pub data: Cow<'a, [u8]>,
}

impl Specialization<'_> {
    /// Empty specialization instance.
    pub const EMPTY: Self = Specialization {
        constants: Cow::Borrowed(&[]),
        data: Cow::Borrowed(&[]),
    };
}

impl Default for Specialization<'_> {
    fn default() -> Self {
        Specialization::EMPTY
    }
}

#[doc(hidden)]
#[derive(Debug, Default)]
pub struct SpecializationStorage {
    constants: Vec<SpecializationConstant>,
    data: Vec<u8>,
}

/// List of specialization constants.
#[doc(hidden)]
pub trait SpecConstList: Sized {
    fn fold(self, storage: &mut SpecializationStorage);
}

impl<T> From<T> for Specialization<'_>
where
    T: SpecConstList,
{
    fn from(list: T) -> Self {
        let mut storage = SpecializationStorage::default();
        list.fold(&mut storage);
        Specialization {
            data: Cow::Owned(storage.data),
            constants: Cow::Owned(storage.constants),
        }
    }
}

#[doc(hidden)]
#[derive(Debug)]
pub struct SpecConstListNil;

#[doc(hidden)]
#[derive(Debug)]
pub struct SpecConstListCons<H, T> {
    pub head: (u32, H),
    pub tail: T,
}

impl SpecConstList for SpecConstListNil {
    fn fold(self, _storage: &mut SpecializationStorage) {}
}

impl<H, T> SpecConstList for SpecConstListCons<H, T>
where
    T: SpecConstList,
{
    fn fold(self, storage: &mut SpecializationStorage) {
        let size = std::mem::size_of::<H>();
        assert!(storage.data.len() + size <= u16::MAX as usize);
        let offset = storage.data.len() as u16;
        storage.data.extend_from_slice(unsafe {
            // Inspecting bytes is always safe.
            let head_ptr: *const H = &self.head.1;
            slice::from_raw_parts(head_ptr as *const u8, size)
        });
        storage.constants.push(SpecializationConstant {
            id: self.head.0,
            range: offset..offset + size as u16,
        });
        self.tail.fold(storage)
    }
}

/// Macro for specifying list of specialization constants for `EntryPoint`.
#[macro_export]
macro_rules! spec_const_list {
    (@ $(,)?) => {
        $crate::pso::SpecConstListNil
    };

    (@ $head_id:expr => $head_constant:expr $(,$tail_id:expr => $tail_constant:expr)* $(,)?) => {
        $crate::pso::SpecConstListCons {
            head: ($head_id, $head_constant),
            tail: $crate::spec_const_list!(@ $($tail_id => $tail_constant),*),
        }
    };

    ($($id:expr => $constant:expr),* $(,)?) => {
        $crate::spec_const_list!(@ $($id => $constant),*).into()
    };

    ($($constant:expr),* $(,)?) => {
        {
            let mut counter = 0;
            $crate::spec_const_list!(@ $({ counter += 1; counter - 1 } => $constant),*).into()
        }
    };
}
