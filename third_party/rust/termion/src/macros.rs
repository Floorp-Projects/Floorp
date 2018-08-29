/// Create a CSI-introduced sequence.
macro_rules! csi {
    ($( $l:expr ),*) => { concat!("\x1B[", $( $l ),*) };
}

/// Derive a CSI sequence struct.
macro_rules! derive_csi_sequence {
    ($doc:expr, $name:ident, $value:expr) => {
        #[doc = $doc]
        #[derive(Copy, Clone)]
        pub struct $name;

        impl fmt::Display for $name {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                write!(f, csi!($value))
            }
        }
    };
}
