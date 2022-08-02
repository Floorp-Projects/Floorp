#![allow(unused_variables)]

use extend::ext;
use std::iter::FromIterator;

#[ext]
impl<T, K, F, C> C
where
    C: IntoIterator<Item = T>,
    K: Eq,
    F: Fn(&T) -> K,
{
    fn group_by<Out>(self, f: F) -> Out
    where
        Out: FromIterator<(K, Vec<T>)>,
    {
        todo!()
    }

    fn group_by_and_map_values<Out, G, T2>(self, f: F, g: G) -> Out
    where
        G: Fn(T) -> T2 + Copy,
        Out: FromIterator<(K, Vec<T2>)>,
    {
        todo!()
    }

    fn group_by_and_return_groups(self, f: F) -> Vec<Vec<T>> {
        todo!()
    }
}

fn main() {}
