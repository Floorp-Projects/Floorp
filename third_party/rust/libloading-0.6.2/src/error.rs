use std::ffi::CString;

pub struct DlDescription(pub(crate) CString);

impl std::fmt::Debug for DlDescription {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        std::fmt::Debug::fmt(&self.0, f)
    }
}

pub struct WindowsError(pub(crate) std::io::Error);

impl std::fmt::Debug for WindowsError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        std::fmt::Debug::fmt(&self.0, f)
    }
}

#[derive(Debug)]
#[non_exhaustive]
pub enum Error {
    /// The `dlopen` call failed.
    DlOpen { desc: DlDescription },
    /// The `dlopen` call failed and system did not report an error.
    DlOpenUnknown,
    /// The `dlsym` call failed.
    DlSym { desc: DlDescription },
    /// The `dlsym` call failed and system did not report an error.
    DlSymUnknown,
    /// The `dlclose` call failed.
    DlClose { desc: DlDescription },
    /// The `dlclose` call failed and system did not report an error.
    DlCloseUnknown,
    /// The `LoadLibraryW` call failed.
    LoadLibraryW { source: WindowsError },
    /// The `LoadLibraryW` call failed and system did not report an error.
    LoadLibraryWUnknown,
    /// The `GetProcAddress` call failed.
    GetProcAddress { source: WindowsError },
    /// The `GetProcAddressUnknown` call failed and system did not report an error.
    GetProcAddressUnknown,
    /// The `FreeLibrary` call failed.
    FreeLibrary { source: WindowsError },
    /// The `FreeLibrary` call failed and system did not report an error.
    FreeLibraryUnknown,
    /// The requested type cannot possibly work.
    IncompatibleSize,
    /// Could not create a new CString.
    CreateCString { source: std::ffi::NulError },
    /// Could not create a new CString from bytes with trailing null.
    CreateCStringWithTrailing { source: std::ffi::FromBytesWithNulError },
}

impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        use Error::*;
        match *self {
            CreateCString { ref source } => Some(source),
            CreateCStringWithTrailing { ref source } => Some(source),
            LoadLibraryW { ref source } => Some(&source.0),
            GetProcAddress { ref source } => Some(&source.0),
            FreeLibrary { ref source } => Some(&source.0),
            _ => None,
        }
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        use Error::*;
        match *self {
            DlOpen { ref desc } => write!(f, "{}", desc.0.to_string_lossy()),
            DlOpenUnknown => write!(f, "dlopen failed, but system did not report the error"),
            DlSym { ref desc } => write!(f, "{}", desc.0.to_string_lossy()),
            DlSymUnknown => write!(f, "dlsym failed, but system did not report the error"),
            DlClose { ref desc } => write!(f, "{}", desc.0.to_string_lossy()),
            DlCloseUnknown => write!(f, "dlclose failed, but system did not report the error"),
            LoadLibraryW { .. } => write!(f, "LoadLibraryW failed"),
            LoadLibraryWUnknown =>
                write!(f, "LoadLibraryW failed, but system did not report the error"),
            GetProcAddress { .. } => write!(f, "GetProcAddress failed"),
            GetProcAddressUnknown =>
                write!(f, "GetProcAddress failed, but system did not report the error"),
            FreeLibrary { .. } => write!(f, "FreeLibrary failed"),
            FreeLibraryUnknown =>
                write!(f, "FreeLibrary failed, but system did not report the error"),
            CreateCString { .. } => write!(f, "could not create a C string from bytes"),
            CreateCStringWithTrailing { .. } =>
                write!(f, "could not create a C string from bytes with trailing null"),
            IncompatibleSize => write!(f, "requested type cannot possibly work"),
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn error_send() {
        fn assert_send<T: Send>() {}
        assert_send::<super::Error>();
    }

    #[test]
    fn error_sync() {
        fn assert_sync<T: Sync>() {}
        assert_sync::<super::Error>();
    }

    #[test]
    fn error_display() {
        fn assert_display<T: std::fmt::Display>() {}
        assert_display::<super::Error>();
    }
}
