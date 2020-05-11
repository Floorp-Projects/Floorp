fn main() {
    let cell = once_cell::sync::OnceCell::<u32>::new();
    cell.get_or_init(|| {
        cell.get_or_init(|| 1);
        2
    });
}
