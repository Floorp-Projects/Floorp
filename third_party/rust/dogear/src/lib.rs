// Copyright 2018-2019 Mozilla

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

#![allow(unknown_lints)]
#![warn(rust_2018_idioms)]

#[macro_use]
mod driver;
mod error;
mod guid;
mod merge;
mod store;
mod tree;

#[cfg(test)]
mod tests;

pub use crate::driver::*;
pub use crate::error::*;
pub use crate::guid::*;
pub use crate::merge::*;
pub use crate::store::*;
pub use crate::tree::*;
