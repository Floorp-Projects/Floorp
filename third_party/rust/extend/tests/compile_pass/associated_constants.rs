use extend::ext;

#[ext]
impl Option<String> {
    const FOO: usize = 1;
}

fn main() {
    assert_eq!(Option::<String>::FOO, 1);
}
