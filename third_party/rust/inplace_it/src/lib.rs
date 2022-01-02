//! # Inplace it!
//!
//! Place small arrays on the stack with a low cost!
//!
//! The only price you should pay for this is the price of choosing
//! a type based on the size of the requested array! This is just one `match`!
//!
//! ## What?
//!
//! This crate is created for one purpose: allocating small arrays on the stack.
//! The simplest way to use it is:
//!
//! ```rust
//! use inplace_it::{inplace_or_alloc_array, UninitializedSliceMemoryGuard};
//!
//! inplace_or_alloc_array(
//!     150, // size of needed array to allocate
//!     |mut uninit_guard: UninitializedSliceMemoryGuard<u16>| { // and this is consumer of uninitialized memory
//!         assert_eq!(160, uninit_guard.len());
//!
//!         {
//!             // You can borrow guard to reuse memory
//!             let borrowed_uninit_guard = uninit_guard.borrow();
//!             // Let's initialize memory
//!             // Note that borrowed_uninit_guard will be consumed (destroyed to produce initialized memory guard)
//!             let init_guard = borrowed_uninit_guard.init(|index| index as u16 + 1);
//!             // Memory now contains elements [1, 2, ..., 160]
//!             // Lets check it. Sum of [1, 2, ..., 160] = 12880
//!             let sum: u16 = init_guard.iter().sum();
//!             assert_eq!(sum, 12880);
//!         }
//!
//!         {
//!             // If you don't want to reuse memory, you can init new guard directly
//!             let init_guard = uninit_guard.init(|index| index as u16 * 2);
//!             // Memory now contains elements [0, 2, 4, ..., 318]
//!             // Lets check it. Sum of [0, 2, 4, ..., 318] = 25440
//!             let sum: u16 = init_guard.iter().sum();
//!             assert_eq!(sum, 25440);
//!         }
//!     }
//! )
//! ```
//!
//! ## Why?
//!
//! Because allocation on the stack (i.e. placing variables) is **MUCH FASTER** then usual
//! allocating in the heap.
//!

mod guards;
mod fixed_array;
mod alloc_array;

pub use guards::*;
pub use fixed_array::*;
pub use alloc_array::*;
