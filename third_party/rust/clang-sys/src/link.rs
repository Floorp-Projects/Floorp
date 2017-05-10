// Copyright 2016 Kyle Mayes
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//================================================
// Macros
//================================================

// link! _________________________________________

#[cfg(feature="runtime")]
macro_rules! link {
    (@LOAD: #[cfg($cfg:meta)] fn $name:ident($($pname:ident: $pty:ty), *) $(-> $ret:ty)*) => (
        #[cfg($cfg)]
        pub fn $name(library: &mut super::SharedLibrary) {
            let symbol = unsafe { library.library.get(stringify!($name).as_bytes()) }.ok();
            library.functions.$name = symbol.map(|s| *s);
        }

        #[cfg(not($cfg))]
        pub fn $name(_: &mut super::SharedLibrary) {}
    );

    (@LOAD: fn $name:ident($($pname:ident: $pty:ty), *) $(-> $ret:ty)*) => (
        link!(@LOAD: #[cfg(feature="runtime")] fn $name($($pname: $pty), *) $(-> $ret)*);
    );

    ($($(#[cfg($cfg:meta)])* pub fn $name:ident($($pname:ident: $pty:ty), *) $(-> $ret:ty)*;)+) => (
        use std::cell::{RefCell};
        use std::sync::{Arc};

        /// The set of functions loaded dynamically.
        #[derive(Debug)]
        pub struct Functions {
            $($(#[cfg($cfg)])* pub $name: Option<unsafe extern fn($($pname: $pty), *) $(-> $ret)*>,)+
        }

        impl Default for Functions {
            fn default() -> Functions {
                unsafe { std::mem::zeroed() }
            }
        }

        /// A dynamically loaded instance of the `libclang` library.
        #[derive(Debug)]
        pub struct SharedLibrary {
            library: libloading::Library,
            pub functions: Functions,
        }

        impl SharedLibrary {
            //- Constructors -----------------------------

            fn new(library: libloading::Library) -> SharedLibrary {
                SharedLibrary { library: library, functions: Functions::default() }
            }
        }

        thread_local!(static LIBRARY: RefCell<Option<Arc<SharedLibrary>>> = RefCell::new(None));

        /// Returns whether a `libclang` shared library is loaded on this thread.
        pub fn is_loaded() -> bool {
            LIBRARY.with(|l| l.borrow().is_some())
        }

        fn with_library<T, F>(f: F) -> Option<T> where F: FnOnce(&SharedLibrary) -> T {
            LIBRARY.with(|l| {
                match l.borrow().as_ref() {
                    Some(library) => Some(f(&library)),
                    _ => None,
                }
            })
        }

        $(
            #[cfg_attr(feature="clippy", allow(too_many_arguments))]
            $(#[cfg($cfg)])*
            pub unsafe fn $name($($pname: $pty), *) $(-> $ret)* {
                let f = with_library(|l| {
                    match l.functions.$name {
                        Some(f) => f,
                        _ => panic!(concat!("function not loaded: ", stringify!($name))),
                    }
                }).expect("a `libclang` shared library is not loaded on this thread");
                f($($pname), *)
            }

            $(#[cfg($cfg)])*
            pub mod $name {
                pub fn is_loaded() -> bool {
                    super::with_library(|l| l.functions.$name.is_some()).unwrap_or(false)
                }
            }
        )+

        mod load {
            $(link!(@LOAD: $(#[cfg($cfg)])* fn $name($($pname: $pty), *) $(-> $ret)*);)+
        }

        /// Loads a `libclang` shared library and returns the library instance.
        ///
        /// This function does not attempt to load any functions from the shared library. The caller
        /// is responsible for loading the functions they require.
        ///
        /// # Failures
        ///
        /// * a `libclang` shared library could not be found
        /// * the `libclang` shared library could not be opened
        pub fn load_manually() -> Result<SharedLibrary, String> {
            #[path="../build.rs"]
            mod build;

            let file = try!(build::find_shared_library());
            let library = libloading::Library::new(&file).map_err(|_| {
                format!("the `libclang` shared library could not be opened: {}", file.display())
            });
            let mut library = SharedLibrary::new(try!(library));
            $(load::$name(&mut library);)+
            Ok(library)
        }

        /// Loads a `libclang` shared library for use in the current thread.
        ///
        /// This functions attempts to load all the functions in the shared library. Whether a
        /// function has been loaded can be tested by calling the `is_loaded` function on the
        /// module with the same name as the function (e.g., `clang_createIndex::is_loaded()` for
        /// the `clang_createIndex` function).
        ///
        /// # Failures
        ///
        /// * a `libclang` shared library could not be found
        /// * the `libclang` shared library could not be opened
        #[allow(dead_code)]
        pub fn load() -> Result<(), String> {
            let library = Arc::new(try!(load_manually()));
            LIBRARY.with(|l| *l.borrow_mut() = Some(library));
            Ok(())
        }

        /// Unloads the `libclang` shared library in use in the current thread.
        ///
        /// # Failures
        ///
        /// * a `libclang` shared library is not in use in the current thread
        pub fn unload() -> Result<(), String> {
            let library = set_library(None);
            if library.is_some() {
                Ok(())
            } else {
                Err("a `libclang` shared library is not in use in the current thread".into())
            }
        }

        /// Returns the library instance stored in TLS.
        ///
        /// This functions allows for sharing library instances between threads.
        pub fn get_library() -> Option<Arc<SharedLibrary>> {
            LIBRARY.with(|l| l.borrow_mut().clone())
        }

        /// Sets the library instance stored in TLS and returns the previous library.
        ///
        /// This functions allows for sharing library instances between threads.
        pub fn set_library(library: Option<Arc<SharedLibrary>>) -> Option<Arc<SharedLibrary>> {
            LIBRARY.with(|l| mem::replace(&mut *l.borrow_mut(), library))
        }
    )
}

#[cfg(not(feature="runtime"))]
macro_rules! link {
    ($($(#[cfg($cfg:meta)])* pub fn $name:ident($($pname:ident: $pty:ty), *) $(-> $ret:ty)*;)+) => (
        extern { $($(#[cfg($cfg)])* pub fn $name($($pname: $pty), *) $(-> $ret)*;)+ }
    )
}
