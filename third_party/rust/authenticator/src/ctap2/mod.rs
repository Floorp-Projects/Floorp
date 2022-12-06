#[allow(dead_code)] // TODO(MS): Remove me asap
pub mod commands;
pub use commands::get_assertion::AssertionObject;

pub(crate) mod attestation;

pub mod client_data;
pub mod server;
pub(crate) mod utils;
