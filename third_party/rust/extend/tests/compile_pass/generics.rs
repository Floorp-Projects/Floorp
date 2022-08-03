use extend::ext;

#[ext]
impl<'a, T: Clone> Vec<&'a T>
where
    T: 'a + Copy,
{
    fn size(&self) -> usize {
        self.len()
    }
}

fn main() {
    assert_eq!(3, vec![&1, &2, &3].size());
}
