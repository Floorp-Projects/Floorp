use std::num::NonZeroU32;

/// Contains a reference to a free slot in an arena, encapsulating NonZeroU32
/// to prevent off-by-one errors and leaking unsafety.
///
/// Uses NonZeroU32 to stay small when put inside an `Option`.
#[derive(Debug, Clone, Copy)]
#[repr(transparent)]
pub(crate) struct FreePointer(NonZeroU32);

impl FreePointer {
    #[must_use]
    pub(crate) fn from_slot(slot: u32) -> Self {
        let value = slot
            .checked_add(1)
            .expect("u32 overflowed calculating free pointer from u32");

        // This is safe because any u32 + 1 that didn't overflow must not be
        // zero.
        FreePointer(unsafe { NonZeroU32::new_unchecked(value) })
    }

    #[must_use]
    #[allow(clippy::integer_arithmetic)]
    pub(crate) fn slot(self) -> u32 {
        // This will never underflow due to the field being guaranteed non-zero.
        self.0.get() - 1
    }
}

#[cfg(test)]
mod test {
    use super::FreePointer;

    #[test]
    fn from_slot() {
        let ptr = FreePointer::from_slot(0);
        assert_eq!(ptr.slot(), 0);
    }

    #[test]
    #[should_panic(expected = "u32 overflowed calculating free pointer from u32")]
    fn panic_on_overflow() {
        let _ = FreePointer::from_slot(std::u32::MAX);
    }
}
