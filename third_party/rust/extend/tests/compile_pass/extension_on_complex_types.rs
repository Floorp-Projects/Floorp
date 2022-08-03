use extend::ext;

#[ext]
impl<'a> &'a str {
    fn foo(self) {}
}

#[ext]
impl<T> [T; 3] {
    fn foo(self) {}
}

#[ext]
impl *const i32 {
    fn foo(self) {}
}

#[ext]
impl<T> [T] {
    fn foo(&self) {}
}

#[ext]
impl<'a, T> &'a [T] {
    fn foo(self) {}
}

#[ext]
impl (i32, i64) {
    fn foo(self) {}
}

#[ext]
impl fn(i32) -> bool {
    fn foo(self) {}
}

fn bare_fn(_: i32) -> bool {
    false
}

#[ext]
impl dyn Send + Sync + 'static {}

fn main() {
    "".foo();

    [1, 2, 3].foo();

    let ptr: *const i32 = &123;
    ptr.foo();

    &[1, 2, 3].foo();

    (1i32, 1i64).foo();

    (bare_fn as fn(i32) -> bool).foo();
}
