// Copyright 2013-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Interfaces to the operating system provided random number
//! generators.

use std::io;
use Rng;

/// A random number generator that retrieves randomness straight from
/// the operating system. Platform sources:
///
/// - Unix-like systems (Linux, Android, Mac OSX): read directly from
///   `/dev/urandom`, or from `getrandom(2)` system call if available.
/// - Windows: calls `CryptGenRandom`, using the default cryptographic
///   service provider with the `PROV_RSA_FULL` type.
/// - iOS: calls SecRandomCopyBytes as /dev/(u)random is sandboxed.
/// - PNaCl: calls into the `nacl-irt-random-0.1` IRT interface.
///
/// This does not block.
pub struct OsRng(imp::OsRng);

impl OsRng {
    /// Create a new `OsRng`.
    pub fn new() -> io::Result<OsRng> {
        imp::OsRng::new().map(OsRng)
    }
}

impl Rng for OsRng {
    fn next_u32(&mut self) -> u32 { self.0.next_u32() }
    fn next_u64(&mut self) -> u64 { self.0.next_u64() }
    fn fill_bytes(&mut self, v: &mut [u8]) { self.0.fill_bytes(v) }
}

#[cfg(all(unix, not(target_os = "ios"),
          not(target_os = "nacl")))]
mod imp {
    extern crate libc;

    use self::OsRngInner::*;

    use std::io;
    use std::fs::File;
    use Rng;
    use read::ReadRng;
    use std::mem;

    #[cfg(all(target_os = "linux",
              any(target_arch = "x86_64",
                  target_arch = "x86",
                  target_arch = "arm",
                  target_arch = "aarch64",
                  target_arch = "powerpc")))]
    fn getrandom(buf: &mut [u8]) -> libc::c_long {
        extern "C" {
            fn syscall(number: libc::c_long, ...) -> libc::c_long;
        }

        #[cfg(target_arch = "x86_64")]
        const NR_GETRANDOM: libc::c_long = 318;
        #[cfg(target_arch = "x86")]
        const NR_GETRANDOM: libc::c_long = 355;
        #[cfg(target_arch = "arm")]
        const NR_GETRANDOM: libc::c_long = 384;
        #[cfg(target_arch = "aarch64")]
        const NR_GETRANDOM: libc::c_long = 278;
        #[cfg(target_arch = "powerpc")]
        const NR_GETRANDOM: libc::c_long = 384;

        unsafe {
            syscall(NR_GETRANDOM, buf.as_mut_ptr(), buf.len(), 0)
        }
    }

    #[cfg(not(all(target_os = "linux",
                  any(target_arch = "x86_64",
                      target_arch = "x86",
                      target_arch = "arm",
                      target_arch = "aarch64",
                      target_arch = "powerpc"))))]
    fn getrandom(_buf: &mut [u8]) -> libc::c_long { -1 }

    fn getrandom_fill_bytes(v: &mut [u8]) {
        let mut read = 0;
        let len = v.len();
        while read < len {
            let result = getrandom(&mut v[read..]);
            if result == -1 {
                let err = io::Error::last_os_error();
                if err.kind() == io::ErrorKind::Interrupted {
                    continue
                } else {
                    panic!("unexpected getrandom error: {}", err);
                }
            } else {
                read += result as usize;
            }
        }
    }

    fn getrandom_next_u32() -> u32 {
        let mut buf: [u8; 4] = [0u8; 4];
        getrandom_fill_bytes(&mut buf);
        unsafe { mem::transmute::<[u8; 4], u32>(buf) }
    }

    fn getrandom_next_u64() -> u64 {
        let mut buf: [u8; 8] = [0u8; 8];
        getrandom_fill_bytes(&mut buf);
        unsafe { mem::transmute::<[u8; 8], u64>(buf) }
    }

    #[cfg(all(target_os = "linux",
              any(target_arch = "x86_64",
                  target_arch = "x86",
                  target_arch = "arm",
                  target_arch = "aarch64",
                  target_arch = "powerpc")))]
    fn is_getrandom_available() -> bool {
        use std::sync::atomic::{AtomicBool, ATOMIC_BOOL_INIT, Ordering};
        use std::sync::{Once, ONCE_INIT};

        static CHECKER: Once = ONCE_INIT;
        static AVAILABLE: AtomicBool = ATOMIC_BOOL_INIT;

        CHECKER.call_once(|| {
            let mut buf: [u8; 0] = [];
            let result = getrandom(&mut buf);
            let available = if result == -1 {
                let err = io::Error::last_os_error().raw_os_error();
                err != Some(libc::ENOSYS)
            } else {
                true
            };
            AVAILABLE.store(available, Ordering::Relaxed);
        });

        AVAILABLE.load(Ordering::Relaxed)
    }

    #[cfg(not(all(target_os = "linux",
                  any(target_arch = "x86_64",
                      target_arch = "x86",
                      target_arch = "arm",
                      target_arch = "aarch64",
                      target_arch = "powerpc"))))]
    fn is_getrandom_available() -> bool { false }

    pub struct OsRng {
        inner: OsRngInner,
    }

    enum OsRngInner {
        OsGetrandomRng,
        OsReadRng(ReadRng<File>),
    }

    impl OsRng {
        pub fn new() -> io::Result<OsRng> {
            if is_getrandom_available() {
                return Ok(OsRng { inner: OsGetrandomRng });
            }

            let reader = try!(File::open("/dev/urandom"));
            let reader_rng = ReadRng::new(reader);

            Ok(OsRng { inner: OsReadRng(reader_rng) })
        }
    }

    impl Rng for OsRng {
        fn next_u32(&mut self) -> u32 {
            match self.inner {
                OsGetrandomRng => getrandom_next_u32(),
                OsReadRng(ref mut rng) => rng.next_u32(),
            }
        }
        fn next_u64(&mut self) -> u64 {
            match self.inner {
                OsGetrandomRng => getrandom_next_u64(),
                OsReadRng(ref mut rng) => rng.next_u64(),
            }
        }
        fn fill_bytes(&mut self, v: &mut [u8]) {
            match self.inner {
                OsGetrandomRng => getrandom_fill_bytes(v),
                OsReadRng(ref mut rng) => rng.fill_bytes(v)
            }
        }
    }
}

#[cfg(target_os = "ios")]
mod imp {
    extern crate libc;

    use std::io;
    use std::mem;
    use Rng;
    use self::libc::{c_int, size_t};

    pub struct OsRng;

    enum SecRandom {}

    #[allow(non_upper_case_globals)]
    const kSecRandomDefault: *const SecRandom = 0 as *const SecRandom;

    #[link(name = "Security", kind = "framework")]
    extern {
        fn SecRandomCopyBytes(rnd: *const SecRandom,
                              count: size_t, bytes: *mut u8) -> c_int;
    }

    impl OsRng {
        pub fn new() -> io::Result<OsRng> {
            Ok(OsRng)
        }
    }

    impl Rng for OsRng {
        fn next_u32(&mut self) -> u32 {
            let mut v = [0u8; 4];
            self.fill_bytes(&mut v);
            unsafe { mem::transmute(v) }
        }
        fn next_u64(&mut self) -> u64 {
            let mut v = [0u8; 8];
            self.fill_bytes(&mut v);
            unsafe { mem::transmute(v) }
        }
        fn fill_bytes(&mut self, v: &mut [u8]) {
            let ret = unsafe {
                SecRandomCopyBytes(kSecRandomDefault, v.len() as size_t, v.as_mut_ptr())
            };
            if ret == -1 {
                panic!("couldn't generate random bytes: {}", io::Error::last_os_error());
            }
        }
    }
}

#[cfg(windows)]
mod imp {
    use std::io;
    use std::mem;
    use std::ptr;
    use Rng;

    type BOOL = i32;
    type LPCSTR = *const i8;
    type DWORD = u32;
    type HCRYPTPROV = usize;
    type BYTE = u8;

    const PROV_RSA_FULL: DWORD = 1;
    const CRYPT_SILENT: DWORD = 0x00000040;
    const CRYPT_VERIFYCONTEXT: DWORD = 0xF0000000;

    #[link(name = "advapi32")]
    extern "system" {
        fn CryptAcquireContextA(phProv: *mut HCRYPTPROV,
                                szContainer: LPCSTR,
                                szProvider: LPCSTR,
                                dwProvType: DWORD,
                                dwFlags: DWORD) -> BOOL;
        fn CryptGenRandom(hProv: HCRYPTPROV,
                          dwLen: DWORD,
                          pbBuffer: *mut BYTE) -> BOOL;
        fn CryptReleaseContext(hProv: HCRYPTPROV, dwFlags: DWORD) -> BOOL;
    }

    pub struct OsRng {
        hcryptprov: HCRYPTPROV
    }

    impl OsRng {
        pub fn new() -> io::Result<OsRng> {
            let mut hcp = 0;
            let ret = unsafe {
                CryptAcquireContextA(&mut hcp, ptr::null(), ptr::null(),
                                     PROV_RSA_FULL,
                                     CRYPT_VERIFYCONTEXT | CRYPT_SILENT)
            };

            if ret == 0 {
                Err(io::Error::last_os_error())
            } else {
                Ok(OsRng { hcryptprov: hcp })
            }
        }
    }

    impl Rng for OsRng {
        fn next_u32(&mut self) -> u32 {
            let mut v = [0u8; 4];
            self.fill_bytes(&mut v);
            unsafe { mem::transmute(v) }
        }
        fn next_u64(&mut self) -> u64 {
            let mut v = [0u8; 8];
            self.fill_bytes(&mut v);
            unsafe { mem::transmute(v) }
        }
        fn fill_bytes(&mut self, v: &mut [u8]) {
            let ret = unsafe {
                CryptGenRandom(self.hcryptprov, v.len() as DWORD,
                               v.as_mut_ptr())
            };
            if ret == 0 {
                panic!("couldn't generate random bytes: {}",
                       io::Error::last_os_error());
            }
        }
    }

    impl Drop for OsRng {
        fn drop(&mut self) {
            let ret = unsafe {
                CryptReleaseContext(self.hcryptprov, 0)
            };
            if ret == 0 {
                panic!("couldn't release context: {}",
                       io::Error::last_os_error());
            }
        }
    }
}

#[cfg(target_os = "nacl")]
mod imp {
    extern crate libc;

    use std::io;
    use std::mem;
    use Rng;

    pub struct OsRng(extern fn(dest: *mut libc::c_void,
                               bytes: libc::size_t,
                               read: *mut libc::size_t) -> libc::c_int);

    extern {
        fn nacl_interface_query(name: *const libc::c_char,
                                table: *mut libc::c_void,
                                table_size: libc::size_t) -> libc::size_t;
    }

    const INTERFACE: &'static [u8] = b"nacl-irt-random-0.1\0";

    #[repr(C)]
    struct NaClIRTRandom {
        get_random_bytes: Option<extern fn(dest: *mut libc::c_void,
                                           bytes: libc::size_t,
                                           read: *mut libc::size_t) -> libc::c_int>,
    }

    impl OsRng {
        pub fn new() -> io::Result<OsRng> {
            let mut iface = NaClIRTRandom {
                get_random_bytes: None,
            };
            let result = unsafe {
                nacl_interface_query(INTERFACE.as_ptr() as *const _,
                                     mem::transmute(&mut iface),
                                     mem::size_of::<NaClIRTRandom>() as libc::size_t)
            };
            if result != 0 {
                assert!(iface.get_random_bytes.is_some());
                let result = OsRng(iface.get_random_bytes.take().unwrap());
                Ok(result)
            } else {
                let error = io::ErrorKind::NotFound;
                let error = io::Error::new(error, "IRT random interface missing");
                Err(error)
            }
        }
    }

    impl Rng for OsRng {
        fn next_u32(&mut self) -> u32 {
            let mut v = [0u8; 4];
            self.fill_bytes(&mut v);
            unsafe { mem::transmute(v) }
        }
        fn next_u64(&mut self) -> u64 {
            let mut v = [0u8; 8];
            self.fill_bytes(&mut v);
            unsafe { mem::transmute(v) }
        }
        fn fill_bytes(&mut self, v: &mut [u8]) {
            let mut read = 0;
            loop {
                let mut r: libc::size_t = 0;
                let len = v.len();
                let error = (self.0)(v[read..].as_mut_ptr() as *mut _,
                                     (len - read) as libc::size_t,
                                     &mut r as *mut _);
                assert!(error == 0, "`get_random_bytes` failed!");
                read += r as usize;

                if read >= v.len() { break; }
            }
        }
    }
}


#[cfg(test)]
mod test {
    use std::sync::mpsc::channel;
    use Rng;
    use OsRng;
    use std::thread;

    #[test]
    fn test_os_rng() {
        let mut r = OsRng::new().unwrap();

        r.next_u32();
        r.next_u64();

        let mut v = [0u8; 1000];
        r.fill_bytes(&mut v);
    }

    #[test]
    fn test_os_rng_tasks() {

        let mut txs = vec!();
        for _ in 0..20 {
            let (tx, rx) = channel();
            txs.push(tx);

            thread::spawn(move|| {
                // wait until all the tasks are ready to go.
                rx.recv().unwrap();

                // deschedule to attempt to interleave things as much
                // as possible (XXX: is this a good test?)
                let mut r = OsRng::new().unwrap();
                thread::yield_now();
                let mut v = [0u8; 1000];

                for _ in 0..100 {
                    r.next_u32();
                    thread::yield_now();
                    r.next_u64();
                    thread::yield_now();
                    r.fill_bytes(&mut v);
                    thread::yield_now();
                }
            });
        }

        // start all the tasks
        for tx in txs.iter() {
            tx.send(()).unwrap();
        }
    }
}
