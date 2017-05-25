tempdir
=======

A Rust library for creating a temporary directory and deleting its entire
contents when the directory is dropped.

[![Build Status](https://travis-ci.org/rust-lang-nursery/tempdir.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/tempdir)

[Documentation](https://doc.rust-lang.org/tempdir)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
tempdir = "0.3"
```

and this to your crate root:

```rust
extern crate tempdir;
```

## Example

This sample method does the following:

1. Create a temporary directory in the default location with the given prefix.
2. Determine a file path in the directory and print it out.
3. Create a file inside the temp folder.
4. Write to the file and sync it to disk.
5. Close the directory, deleting the contents in the process.

```rust
fn write_temp_folder_with_files() -> Result<(), io::Error> {
    if let Ok(dir) = TempDir::new("my_directory_prefix") {
        let file_path = dir.path().join("foo.txt");
        println!("{:?}", file_path);

        let mut f = try!(File::create(file_path));
        try!(f.write_all(b"Hello, world!"));
        try!(f.sync_all());
        try!(dir.close());
    }
    Ok(())
}
```

**Note:** Closing the directory is actually optional, as it would be done on
drop. The benefit of closing here is that it allows possible errors to be
handled.
