use std::num::NonZeroU32;

/// Tracks the generation of an entry in an arena. Encapsulates NonZeroU32 to
/// reduce the number of redundant checks needed, as well as enforcing checked
/// arithmetic on advancing a generation.
///
/// Uses NonZeroU32 to help `Index` stay the same size when put inside an
/// `Option`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub(crate) struct Generation(NonZeroU32);

impl Generation {
    #[must_use]
    pub(crate) fn first() -> Self {
        // This is safe because 1 is not zero.
        Generation(unsafe { NonZeroU32::new_unchecked(1) })
    }

    #[must_use]
    pub(crate) fn next(self) -> Self {
        let last_generation = self.0.get();
        let next_generation = last_generation.checked_add(1).unwrap_or(1);

        // This is safe because value that would overflow is instead made 1.
        Generation(unsafe { NonZeroU32::new_unchecked(next_generation) })
    }

    pub(crate) fn to_u32(self) -> u32 {
        self.0.get()
    }

    pub(crate) fn from_u32(gen: u32) -> Self {
        Generation(NonZeroU32::new(gen).expect("generation IDs must be nonzero!"))
    }
}

#[cfg(test)]
mod test {
    use super::Generation;

    use std::num::NonZeroU32;

    #[test]
    fn first_and_next() {
        let first = Generation::first();
        assert_eq!(first.0.get(), 1);

        let second = first.next();
        assert_eq!(second.0.get(), 2);
    }

    #[test]
    fn wrap_on_overflow() {
        let max = Generation(NonZeroU32::new(std::u32::MAX).unwrap());
        assert_eq!(max.0.get(), std::u32::MAX);

        let next = max.next();
        assert_eq!(next.0.get(), 1);
    }
}
