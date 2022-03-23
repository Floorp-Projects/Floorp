# audio_thread_priority

[![](https://img.shields.io/crates/v/audio_thread_priority.svg)](https://crates.io/crates/audio_thread_priority)
[![](https://docs.rs/audio_thread_priority/badge.svg)](https://docs.rs/audio_thread_priority)


Synopsis:

```rust

use audio_thread_priority::{promote_current_thread_to_real_time, demote_current_thread_from_real_time};

// ... on a thread that will compute audio and has to be real-time:
match promote_current_thread_to_real_time(512, 44100) {
  Ok(h) => {
    println!("this thread is now bumped to real-time priority.");

    // Do some real-time work...

    match demote_current_thread_from_real_time(h) {
      Ok(_) => {
        println!("this thread is now bumped back to normal.")
      }
      Err(_) => {
        println!("Could not bring the thread back to normal priority.")
      }
    };
  }
  Err(e) => {
    eprintln!("Error promoting thread to real-time: {}", e);
  }
}

```

This library can also be used from C or C++ using the included header and
compiling the rust code in the application. By default, a `.a` is compiled to
ease linking.

# License

MPL-2

