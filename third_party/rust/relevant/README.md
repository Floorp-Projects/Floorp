# Relevant

A small utility type to emulate must-use types.
They are different from `#[must_use]` attribute in that the one who have an instance must either send it somewhere else or `dispose` it manually.

This must be desired for types that need manual destruction which can't be implemented with `Drop` trait.
For example resource handler created from some source and that must be returned to the same source.

## Usage

The type `Relevant` is non-droppable. As limitation of current implementation it panics when dropped.
To make type non-droppable it must contain non-droppable type (`Relevant` type for example).

### Example

```rust

struct SourceOfFoos {
    handle: u64,
}

/// Foo must be destroyed manually.
struct Foo {
   handle: u64,
   relevant: Relevant,
}

/// Function from C library to create `Foo`
/// Access to same source must be synchronized.
extern "C" create_foo(source: u64) -> u64;

/// Function from C library to destroy `Foo`.
/// Access to same source must be synchronized.
extern "C" destroy_foo(source: u64, foo: u64) -> u64;

impl SourceOfFoos {
    fn create_foo(&mut self) -> Foo {
        Foo {
            handle: create_foo(self.handle),
            relevant: Relevant,
        }
    }

    fn destroy_foo(&mut self, foo: Foo) {
        destroy_foo(self.handle, foo.handle);
        foo.relevant.dispose();
    }
}

```

Now it is not possible to accidentally drop `Foo` and leak handle.
Of course it always possible to explicitly `std::mem::forget` relevant type.
But it will be deliberate leak.
