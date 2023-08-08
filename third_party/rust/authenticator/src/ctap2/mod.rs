#[allow(dead_code)] // TODO(MS): Remove me asap
pub mod commands;
pub use commands::get_assertion::GetAssertionResult;

pub mod attestation;

pub mod client_data;
pub(crate) mod preflight;
pub mod server;
pub(crate) mod utils;
