// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    collections::hash_map::DefaultHasher,
    fs::File,
    hash::{Hash, Hasher},
    io::Write,
    path::Path,
};

/// Write a data item `data` for the fuzzing target `target` to the fuzzing corpus. The caller needs
/// to make sure that `target` is the correct fuzzing target name for the data written.
///
/// # Panics
///
/// Panics if the corpus directory does not exist or if the corpus item cannot be written.
pub fn write_item_to_fuzzing_corpus(target: &str, data: &[u8]) {
    // This bakes in the assumption that we're executing in the root of the neqo workspace.
    // Unfortunately, `cargo fuzz` doesn't provide a way to learn the location of the corpus
    // directory.
    let corpus = Path::new("../fuzz/corpus").join(target);
    if !corpus.exists() {
        std::fs::create_dir_all(&corpus).expect("failed to create corpus directory");
    }

    // Hash the data to get a unique name for the corpus item.
    let mut hasher = DefaultHasher::new();
    data.hash(&mut hasher);
    let item_name = hex::encode(hasher.finish().to_be_bytes());
    let item_path = corpus.join(item_name);
    if item_path.exists() {
        // Don't overwrite existing corpus items.
        return;
    }

    // Write the data to the corpus item.
    let mut file = File::create(item_path).expect("failed to create corpus item");
    Write::write_all(&mut file, data).expect("failed to write to corpus item");
}
