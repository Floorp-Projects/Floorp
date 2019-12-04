#![cfg_attr(not(test), no_std)]

#![allow(clippy::unreadable_literal)]
#![allow(clippy::cast_lossless)]

#[cfg(test)]
extern crate core;

pub mod sip;
pub mod sip128;

#[cfg(test)]
mod tests;

#[cfg(test)]
mod tests128;
