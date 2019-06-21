Preamble
--------

The different ways you can append and get message arguments can be a bit bewildering. I've iterated a few times on the design and didn't want to lose backwards compatibility.

This guide is to help you on your way. In addition, many of the examples in the examples directory append and read arguments.

Code generation
---------------

First - if you can get D-Bus introspection data, you can use the the `dbus-codegen` tool to generate some boilerplate code for you. E g, if you want to talk to NetworkManager:

```rust
cargo install dbus-codegen
dbus-codegen-rust -s -g -m None -d org.freedesktop.NetworkManager -p /org/freedesktop/NetworkManager > networkmanager.rs
```

You would then use this code like:

```rust
// main.rs
mod networkmanager;

/* ... */

// Start a connection to the system bus.
let c = Connection::get_private(BusType::System)?;

// Make a "ConnPath" struct that just contains a Connection, a destination and a path.
let p = c.with_path("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", 5000);

// Bring our generated code into scope.
use networkmanager::OrgFreedesktopNetworkManager;

// Now we can call methods on our connpath from the "org.freedesktop.NetworkManager" interface.
let devices = c.get_all_devices()?;
```

There is also pre-generated code for standard D-Bus interfaces in the `stdintf` module. A similar example:

```rust
let c = Connection::get_private(BusType::Session)?;

// Make a "ConnPath" struct that just contains a Connection, a destination and a path.
let p = c.with_path("org.mpris.MediaPlayer2.rhythmbox", "/org/mpris/MediaPlayer2", 5000);

// The ConnPath struct implements many traits, e g `org.freedesktop.DBus.Properties`. Bring the trait into scope.
use stdintf::org_freedesktop_dbus::Properties;

// Now we can call org.freedesktop.DBus.Properties.Get just like an ordinary method and get the result back.
let metadata = p.get("org.mpris.MediaPlayer2.Player", "Metadata")?;
```

For more details, see `dbus-codegen-rust --help` and the `README.md` in the dbus-codegen directory.

Now, if you want to make a service yourself, the generated code is more complex. And for some use cases, codegen isn't really an option, so let's move on:

Append / get basic types
------------------------

If you just want to get/append simple types, just use `append1` / `append2` / `append3`, and 
`read1` / `read2` / `read3`. The imaginary method below takes one byte parameter and one string parameter, and returns one string parameter and one int parameter.

```rust
let m = Message::new_method_call(dest, path, intf, member)?.append2(5u8, "Foo");
let r = c.send_with_reply_and_block(m, 2000)?;
let (data1, data2): (&str, i32) = c.read2()?;
```

Arrays and dictionaries
-----------------------

D-Bus arrays and dictionaries usually correspond to `Vec` and `HashMap`. You can just append and get them like basic types:

```rust
let v = vec![3i32, 4i32, 5i32];
let mut map = HashMap::new();
map.insert("Funghi", 5u16);
map.insert("Mold", 8u16);

let m = Message::new_method_call(dest, path, intf, member)?.append2(v, map);
let r = c.send_with_reply_and_block(m, 2000)?;
let (data1, data2): (Vec<i32>, HashMap<&str, u16>) = r.read2()?;
```

Or combine them as you wish, e g, use a `Vec<Vec<u8>>`, a `HashMap<u64, Vec<String>>` or `HashMap<String, HashMap<String, i32>>` to construct more difficult types.

Slices can sometimes be used as arrays - e g, `&[&str]` can be appended, but only very simple types can be used with `get` and `read`, e g `&[u8]`.

This is the easiest way to get started, but in case you want to avoid the overhead of creating `Vec` or `HashMap`s, the "Array and Dict types" and "Iter / IterAppend" sections offer useful alternatives.

Variants
--------

Things are getting slightly more complex with Variants, because they are not strongly typed and thus not fit as well into Rust's strongly typed as arrays and dicts.

If you know the type beforehand, it's still easy:

```rust
let v = Variant("This is a variant containing a &str");
let m = Message::new_method_call(dest, path, intf, member)?.append1(v);
let r = c.send_with_reply_and_block(m, 2000)?;
let z: Variant<i32> = r.read1()?;
println!("Method returned {}", z.0);
```

The `Variant` struct is just a wrapper with a public interior, so you can easily both read from it and write to it with the `.0` accessor.

Sometimes you don't know the type beforehand. We can solve this in two ways (choose whichever is more appropriate for your use case), either through the trait object `Box<RefArg>` or through `Iter` / `IterAppend` (see later sections).

Through trait objects:

```rust
let x = Box::new(5000i32) as Box<RefArg>;
let m = Message::new_method_call(dest, path, intf, member)?.append1(Variant(x));
let r = c.send_with_reply_and_block(m, 2000)?;
let z: Variant<Box<RefArg>> = r.read1()?;
```

Ok, so we retrieved our `Box<RefArg>`. We now need to use the `RefArg` methods to probe it, to see what's inside. Easiest is to use `as_i64` or `as_str` if you want to test for integer or string types. Use `as_iter` if the variant contains a complex type you need to iterate over.
For floating point values, use `arg::cast` (this requires that the RefArg is `static` though, due to Rust type system limitations).
Match over `arg_type` if you need to know the exact type. 


```rust
let z: Variant<Box<RefArg + 'static>> = r.read1()?;
let value = &z.0;

if let Some(s) = value.as_str() { println!("It's a string: {}", s); }
else if let Some(i) = value.as_i64() { println!("It's an integer: {}", i); }
else if let Some(f) = arg::cast::<f64>(value) { println!("It's a float: {}", f); }
else { println!("Don't know how to handle a {:?}", value.arg_type()) }
```

Dicts and variants are sometimes combined, e g, you might need to read a D-Bus dictionary of String to Variants. You can then read these as `HashMap<String, Variant<Box<RefArg>>>`.

Structs
-------

D-Bus structs are implemented as Rust tuples. You can append and get tuples like you do with other types of arguments.

TODO: Example

Declare method arguments
------------------------

When you make a `Tree`, you want to declare what input and output arguments your method expects - so that correct D-Bus introspection data can be generated. You'll use the same types as you learned earlier in this guide:

```rust
factory.method( /* ... */ )
.inarg::<HashMap<i32, Vec<(i32, bool, String)>>,_>("request")
.outarg::<&str,_>("reply")
```

The types are just for generating a correct signature, they are never instantiated. Many different types can generate the same signature - e g, `Array<u8, _>`, `Vec<u8>` and `&[u8]` will all generate the same signature. `Variant` will generate the same type signature regardless of what's inside, so just write `Variant<()>` for simplicity.


Iter / IterAppend
-----------------

Iter and IterAppend are more low-level, direct methods to get and append arguments. They can, e g, come handy if you have more than five arguments to read.

E g, for appending a variant with IterAppend you can use `IterAppend::new(&msg).append_variant(|i| i.append(5000i32))` to append what you need to your variant inside the closure.
To read a variant you can use `let i = msg.read1::<Variant<Iter>>::()?` and then examine the methods on `i.0` to probe the variant.

Array and Dict types
--------------------

These provide slightly better flexibility than using `Vec` and `HashMap` by instead integrating with `Iterator`. Here's an example where you can append and get a dictionary without having to create a HashMap:

```rust
let x = &[("Hello", true), ("World", false)];
let m = Message::new_method_call(dest, path, intf, member)?.append1(Dict::new(x));
let r = c.send_with_reply_and_block(m, 2000)?;
let z: Dict<i32, &str, _> = r.read1()?;
for (key, value) in z { /* do something */ }
```

An edge case where this is necessary is having floating point keys in a dictionary - this is supported in D-Bus but not in Rust's `HashMap`. I have never seen this in practice, though.

Unusual types
-------------

The types `Path`, `Signature` and `OwnedFd` are not often used, but they can be appended and read as other argument types. `Path` and `Signature` will return strings with a borrowed lifetime - use `.into_static()` if you want to untie that lifetime.

For `OwnedFd`, which a wrapper around a file descriptor, remember that the file descriptor will be closed when it goes out of scope.

MessageItem
-----------

MessageItem was the first design - an enum representing a D-Bus argument. It still works, but I doubt you'll ever need to use it. Newer methods provide better type safety, speed, and ergonomics.


