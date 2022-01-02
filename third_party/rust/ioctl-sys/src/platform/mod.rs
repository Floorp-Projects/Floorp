#[doc(hidden)]
pub const NRBITS: u32 = 8;
#[doc(hidden)]
pub const TYPEBITS: u32 = 8;

#[cfg(any(target_os = "linux", target_os = "android"))]
#[path = "linux.rs"]
mod consts;

#[cfg(target_os = "macos")]
#[path = "macos.rs"]
mod consts;

#[doc(hidden)]
pub use self::consts::*;

#[doc(hidden)]
pub const NRSHIFT: u32 = 0;
#[doc(hidden)]
pub const TYPESHIFT: u32 = NRSHIFT + NRBITS as u32;
#[doc(hidden)]
pub const SIZESHIFT: u32 = TYPESHIFT + TYPEBITS as u32;
#[doc(hidden)]
pub const DIRSHIFT: u32 = SIZESHIFT + SIZEBITS as u32;

#[doc(hidden)]
pub const NRMASK: u32 = (1 << NRBITS) - 1;
#[doc(hidden)]
pub const TYPEMASK: u32 = (1 << TYPEBITS) - 1;
#[doc(hidden)]
pub const SIZEMASK: u32 = (1 << SIZEBITS) - 1;
#[doc(hidden)]
pub const DIRMASK: u32 = (1 << DIRBITS) - 1;

/// Encode an ioctl command.
#[macro_export]
macro_rules! ioc {
    ($dir:expr, $ty:expr, $nr:expr, $sz:expr) => {
        (($dir as u32) << $crate::DIRSHIFT)
            | (($ty as u32) << $crate::TYPESHIFT)
            | (($nr as u32) << $crate::NRSHIFT)
            | (($sz as u32) << $crate::SIZESHIFT)
    };
}

/// Encode an ioctl command that has no associated data.
#[macro_export]
macro_rules! io {
    ($ty:expr, $nr:expr) => {
        $crate::ioc!($crate::NONE, $ty, $nr, 0)
    };
}

/// Encode an ioctl command that reads.
#[macro_export]
macro_rules! ior {
    ($ty:expr, $nr:expr, $sz:expr) => {
        $crate::ioc!($crate::READ, $ty, $nr, $sz)
    };
}

/// Encode an ioctl command that writes.
#[macro_export]
macro_rules! iow {
    ($ty:expr, $nr:expr, $sz:expr) => {
        $crate::ioc!($crate::WRITE, $ty, $nr, $sz)
    };
}

/// Encode an ioctl command that both reads and writes.
#[macro_export]
macro_rules! iorw {
    ($ty:expr, $nr:expr, $sz:expr) => {
        $crate::ioc!($crate::READ | $crate::WRITE, $ty, $nr, $sz)
    };
}

/// Declare a wrapper function around an ioctl.
#[macro_export]
macro_rules! ioctl {
    (bad $name:ident with $nr:expr) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int, data: *mut u8) -> ::std::os::raw::c_int {
            $crate::ioctl(fd, $nr as ::std::os::raw::c_ulong, data)
        }
    };
    (bad read $name:ident with $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int, data: *mut $ty) -> ::std::os::raw::c_int {
            $crate::ioctl(fd, $nr as ::std::os::raw::c_ulong, data)
        }
    };
    (bad write $name:ident with $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int, data: *const $ty) -> ::std::os::raw::c_int {
            $crate::ioctl(fd, $nr as ::std::os::raw::c_ulong, data)
        }
    };
    (none $name:ident with $ioty:expr, $nr:expr) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int) -> ::std::os::raw::c_int {
            $crate::ioctl(fd, $crate::io!($ioty, $nr) as ::std::os::raw::c_ulong)
        }
    };
    (try none $name:ident with $ioty:expr, $nr:expr) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int) -> std::result::Result<(), std::io::Error> {
            $crate::check_res($crate::ioctl(
                fd,
                $crate::io!($ioty, $nr) as ::std::os::raw::c_ulong,
            ))
        }
    };
    (arg $name:ident with $ioty:expr, $nr:expr) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            arg: ::std::os::raw::c_ulong,
        ) -> ::std::os::raw::c_int {
            $crate::ioctl(fd, $crate::io!($ioty, $nr) as ::std::os::raw::c_ulong, arg)
        }
    };
    (read $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int, val: *mut $ty) -> ::std::os::raw::c_int {
            $crate::ioctl(
                fd,
                $crate::ior!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                val,
            )
        }
    };
    (try read $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            val: *mut $ty,
        ) -> std::result::Result<(), std::io::Error> {
            $crate::check_res($crate::ioctl(
                fd,
                $crate::ior!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                val,
            ))
        }
    };
    (try read0 $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int) -> std::result::Result<$ty, std::io::Error> {
            let mut val: $ty = std::mem::zeroed();
            $crate::check_res($crate::ioctl(
                fd,
                $crate::ior!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                &mut val as *mut $ty,
            ))
            .map(|_| val)
        }
    };
    (write $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int, val: *const $ty) -> ::std::os::raw::c_int {
            $crate::ioctl(
                fd,
                $crate::iow!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                val,
            )
        }
    };
    (try write $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            val: *const $ty,
        ) -> std::result::Result<(), std::io::Error> {
            $crate::check_res($crate::ioctl(
                fd,
                $crate::iow!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                val,
            ))
        }
    };
    (readwrite $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: ::std::os::raw::c_int, val: *mut $ty) -> ::std::os::raw::c_int {
            $crate::ioctl(
                fd,
                $crate::iorw!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                val,
            )
        }
    };
    (try readwrite $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            val: *mut $ty,
        ) -> std::result::Result<(), std::io::Error> {
            $crate::check_res($crate::ioctl(
                fd,
                $crate::iorw!($ioty, $nr, ::std::mem::size_of::<$ty>()) as ::std::os::raw::c_ulong,
                val,
            ))
        }
    };
    (read buf $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            val: *mut $ty,
            len: usize,
        ) -> ::std::os::raw::c_int {
            $crate::ioctl(
                fd,
                $crate::ior!($ioty, $nr, len) as ::std::os::raw::c_ulong,
                val,
            )
        }
    };
    (write buf $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            val: *const $ty,
            len: usize,
        ) -> ::std::os::raw::c_int {
            $crate::ioctl(
                fd,
                $crate::iow!($ioty, $nr, len) as ::std::os::raw::c_ulong,
                val,
            )
        }
    };
    (readwrite buf $name:ident with $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(
            fd: ::std::os::raw::c_int,
            val: *const $ty,
            len: usize,
        ) -> ::std::os::raw::c_int {
            $crate::ioctl(
                fd,
                $crate::iorw!($ioty, $nr, len) as ::std::os::raw::c_ulong,
                val,
            )
        }
    };
}

/// Extracts the "direction" (read/write/none) from an encoded ioctl command.
#[inline(always)]
pub fn ioc_dir(nr: u32) -> u8 {
    ((nr >> DIRSHIFT) & DIRMASK) as u8
}

/// Extracts the type from an encoded ioctl command.
#[inline(always)]
pub fn ioc_type(nr: u32) -> u32 {
    (nr >> TYPESHIFT) & TYPEMASK
}

/// Extracts the ioctl number from an encoded ioctl command.
#[inline(always)]
pub fn ioc_nr(nr: u32) -> u32 {
    (nr >> NRSHIFT) & NRMASK
}

/// Extracts the size from an encoded ioctl command.
#[inline(always)]
pub fn ioc_size(nr: u32) -> u32 {
    ((nr >> SIZESHIFT) as u32) & SIZEMASK
}

#[doc(hidden)]
pub const IN: u32 = (WRITE as u32) << DIRSHIFT;
#[doc(hidden)]
pub const OUT: u32 = (READ as u32) << DIRSHIFT;
#[doc(hidden)]
pub const INOUT: u32 = ((READ | WRITE) as u32) << DIRSHIFT;
#[doc(hidden)]
pub const SIZE_MASK: u32 = SIZEMASK << SIZESHIFT;
