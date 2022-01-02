macro_rules! expand_align {
    () => {
        s! {
            pub struct pthread_mutexattr_t {
                #[cfg(any(target_arch = "x86_64",
                          target_arch = "powerpc64",
                          target_arch = "mips64",
                          target_arch = "s390x",
                          target_arch = "sparc64",
                          target_arch = "riscv64",
                          target_arch = "riscv32",
                          all(target_arch = "aarch64",
                              target_env = "musl")))]
                __align: [::c_int; 0],
                #[cfg(not(any(target_arch = "x86_64",
                              target_arch = "powerpc64",
                              target_arch = "mips64",
                              target_arch = "s390x",
                              target_arch = "sparc64",
                              target_arch = "riscv64",
                              target_arch = "riscv32",
                              all(target_arch = "aarch64",
                                  target_env = "musl"))))]
                __align: [::c_long; 0],
                size: [u8; ::__SIZEOF_PTHREAD_MUTEXATTR_T],
            }

            pub struct pthread_rwlockattr_t {
                #[cfg(target_env = "musl")]
                __align: [::c_int; 0],
                #[cfg(not(target_env = "musl"))]
                __align: [::c_long; 0],
                size: [u8; ::__SIZEOF_PTHREAD_RWLOCKATTR_T],
            }

            pub struct pthread_condattr_t {
                __align: [::c_int; 0],
                size: [u8; ::__SIZEOF_PTHREAD_CONDATTR_T],
            }

            pub struct fanotify_event_metadata {
                __align: [::c_long; 0],
                pub event_len: __u32,
                pub vers: __u8,
                pub reserved: __u8,
                pub metadata_len: __u16,
                pub mask: __u64,
                pub fd: ::c_int,
                pub pid: ::c_int,
            }
        }

        s_no_extra_traits! {
            pub struct pthread_cond_t {
                #[cfg(target_env = "musl")]
                __align: [*const ::c_void; 0],
                #[cfg(not(target_env = "musl"))]
                __align: [::c_longlong; 0],
                size: [u8; ::__SIZEOF_PTHREAD_COND_T],
            }

            pub struct pthread_mutex_t {
                #[cfg(any(target_arch = "mips",
                          target_arch = "arm",
                          target_arch = "powerpc",
                          target_arch = "sparc",
                          all(target_arch = "x86_64",
                              target_pointer_width = "32")))]
                __align: [::c_long; 0],
                #[cfg(not(any(target_arch = "mips",
                              target_arch = "arm",
                              target_arch = "powerpc",
                              target_arch = "sparc",
                              all(target_arch = "x86_64",
                                  target_pointer_width = "32"))))]
                __align: [::c_longlong; 0],
                size: [u8; ::__SIZEOF_PTHREAD_MUTEX_T],
            }

            pub struct pthread_rwlock_t {
                #[cfg(any(target_arch = "mips",
                          target_arch = "arm",
                          target_arch = "powerpc",
                          target_arch = "sparc",
                          all(target_arch = "x86_64",
                              target_pointer_width = "32")))]
                __align: [::c_long; 0],
                #[cfg(not(any(target_arch = "mips",
                              target_arch = "arm",
                              target_arch = "powerpc",
                              target_arch = "sparc",
                              all(target_arch = "x86_64",
                                  target_pointer_width = "32"))))]
                __align: [::c_longlong; 0],
                size: [u8; ::__SIZEOF_PTHREAD_RWLOCK_T],
            }
        }
    };
}
