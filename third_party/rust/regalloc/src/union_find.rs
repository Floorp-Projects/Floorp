#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

//! An implementation of a fast union-find implementation for "T: ToFromU32" items
//! in some dense range [0, N-1].

use std::marker::PhantomData;

//=============================================================================
// ToFromU32

// First, we need this.  You can store anything you like in this union-find
// mechanism, so long as it is really a u32.  Reminds me of that old joke
// about the Model T Ford being available in any colour you want, so long as
// it is black.
pub trait ToFromU32<T: Sized = Self> {
    fn to_u32(x: Self) -> u32;
    fn from_u32(x: u32) -> Self;
}
//impl ToFromU32 for i32 {
//  fn to_u32(x: i32) -> u32 {
//    x as u32
//  }
//  fn from_u32(x: u32) -> i32 {
//    x as i32
//  }
//}
impl ToFromU32 for u32 {
    fn to_u32(x: u32) -> u32 {
        x
    }
    fn from_u32(x: u32) -> u32 {
        x
    }
}

//=============================================================================
// UnionFind

// This is a fast union-find implementation for "T: ToFromU32" items in some
// dense range [0, N-1].  The allowed operations are:
//
// (1) create a new `UnionFind`er
//
// (2) mark two elements as being in the same equivalence class
//
// (3) get the equivalence classes wrapped up in an opaque structure
//     `UnionFindEquivClasses`, which makes it possible to cheaply find and
//     iterate through the equivalence class of any item.
//
// (4) get an iterator over the "equivalence class leaders".  Iterating this
//     produces one value from each equivalence class.  By presenting each of
//     these to (3), it is possible to enumerate all the equivalence classes
//     exactly once.
//
// `UnionFind` and the operations `union` and `find` are loosely based on the
// discussion in Chapter 8 of "Data Structures and Algorithm Analysis in C"
// (Mark Allen Weiss, 1993).  `UnionFindEquivClasses` and the algorithm to
// construct it is home-grown; although I'm sure the same idea has been
// implemented many times before.

pub struct UnionFind<T: ToFromU32> {
    // These are the trees that we are building.  A value that is negative means
    // that this node is a tree root, and the negation of its value is the size
    // of the tree.  A value that is positive (which must be in the range [0,
    // N-1]) indicates that this tree is a subtree and that its parent has the
    // given index.
    //
    // One consequence of this representation is that at most 2^31-1 values can
    // be supported.  Doesn't seem like much of a limitation in practice, given
    // that all of this allocator's data structures are limited to 2^32 entries.
    /*priv*/
    parent_or_size: Vec<i32>,

    // Keep the typechecker happy
    /*priv*/
    anchor: PhantomData<T>,
}

/*priv*/
const UF_MAX_SIZE: u32 = 0x7FFF_FFF0;

impl<T: ToFromU32> UnionFind<T> {
    pub fn new(size: usize) -> Self {
        // Test a slightly conservative limit to avoid any off-by-one errors.
        if size > UF_MAX_SIZE as usize {
            panic!("UnionFind::new: too many elements; max = 2^31 - 16.");
        }
        let mut parent_or_size = Vec::<i32>::new();
        parent_or_size.resize(size, -1);
        Self {
            parent_or_size,
            anchor: PhantomData,
        }
    }

    // Find, with path compression.  Returns the index of tree root for the
    // given element.  This is not for external use.  There's no boundary
    // checking since Rust will do that anyway.
    /*priv*/
    fn find(&mut self, elem: u32) -> u32 {
        let elem_parent_or_size: i32 = self.parent_or_size[elem as usize];
        if elem_parent_or_size < 0 {
            // We're at a tree root.
            return elem;
        } else {
            // Recurse up to the root.  On the way back out, make all nodes point
            // directly at the root index.
            let elem_parent = elem_parent_or_size as u32;
            let res = self.find(elem_parent);
            assert!(res < UF_MAX_SIZE);
            self.parent_or_size[elem as usize] = res as i32;
            return res;
        }
    }

    // Union, by size (weight).  This is publicly visible.
    pub fn union(&mut self, elem1t: T, elem2t: T) {
        let elem1 = ToFromU32::to_u32(elem1t);
        let elem2 = ToFromU32::to_u32(elem2t);
        if elem1 == elem2 {
            // Ideally, we'd alert the callers they're mistakenly do `union` on
            // identical values repeatedly, but fuzzing hits this repeatedly.
            return;
        }
        let root1: u32 = self.find(elem1);
        let root2: u32 = self.find(elem2);
        if root1 == root2 {
            // `elem1` and `elem2` are already in the same tree.  Do nothing.
            return;
        }
        let size1: i32 = self.parent_or_size[root1 as usize];
        let size2: i32 = self.parent_or_size[root2 as usize];
        // "They are both roots"
        assert!(size1 < 0 && size2 < 0);
        // Make the root of the smaller tree point at the root of the bigger tree.
        // Update the root of the bigger tree to reflect its increased size.  That
        // only requires adding the two `size` values, since they are both
        // negative, so adding them will (correctly) drive it more negative.
        if size1 < size2 {
            self.parent_or_size[root1 as usize] = root2 as i32;
            self.parent_or_size[root2 as usize] += size1;
        } else {
            self.parent_or_size[root2 as usize] = root1 as i32;
            self.parent_or_size[root1 as usize] += size2;
        }
    }
}

// This is a compact representation for all the equivalence classes in a
// `UnionFind`, that can be constructed in more-or-less linear time (meaning,
// O(universe size), and allows iteration over the elements of each
// equivalence class in time linear in the size of the equivalence class (you
// can't ask for better).  It doesn't support queries of the form "are these
// two elements in the same equivalence class" in linear time, but we don't
// care about that.  What we care about is being able to find and visit the
// equivalence class of an element quickly.
//
// The fields are non-public.  What is publically available is the ability to
// get an iterator (for the equivalence class elements), given a starting
// element.

/*priv*/
const UFEC_NULL: u32 = 0xFFFF_FFFF;

/*priv*/
#[derive(Clone)]
struct LLElem {
    // This list element
    elem: u32,
    // Pointer to the rest of the list (index in `llelems`), or UFEC_NULL.
    tail: u32,
}

pub struct UnionFindEquivClasses<T: ToFromU32> {
    // Linked list start "pointers".  Has .len() == universe size.  Entries must
    // not be UFEC_NULL since each element is at least a member of its own
    // equivalence class.
    /*priv*/
    heads: Vec<u32>,

    // Linked list elements.  Has .len() == universe size.
    /*priv*/
    lists: Vec<LLElem>,

    // Keep the typechecker happy
    /*priv*/
    anchor: PhantomData<T>,
    // This struct doesn't have a `new` method since construction is done by a
    // carefully designed algorithm, `UnionFind::get_equiv_classes`.
}

impl<T: ToFromU32> UnionFind<T> {
    // This requires mutable `self` because it needs to do a bunch of `find`
    // operations, and those modify `self` in order to perform path compression.
    // We could avoid this by using a non-path-compressing `find` operation, but
    // that could have the serious side effect of making the big-O complexity of
    // `get_equiv_classes` worse.  Hence we play safe and accept the mutability
    // requirement.
    pub fn get_equiv_classes(&mut self) -> UnionFindEquivClasses<T> {
        let nElemsUSize = self.parent_or_size.len();
        // The construction algorithm requires that all elements have a value
        // strictly less than 2^31.  The union-find machinery, that builds
        // `parent_or_size` that we read here, however relies on a slightly
        // tighter bound, which which we reiterate here due to general paranoia:
        assert!(nElemsUSize < UF_MAX_SIZE as usize);
        let nElems = nElemsUSize as u32;

        // Avoid reallocation; we know how big these need to be.
        let mut heads = Vec::<u32>::new();
        heads.resize(nElems as usize, UFEC_NULL); // all invalid

        let mut lists = Vec::<LLElem>::new();
        lists.resize(
            nElems as usize,
            LLElem {
                elem: 0,
                tail: UFEC_NULL,
            },
        );

        // As explanation, let there be N elements (`nElems`) which have been
        // partitioned into M <= N equivalence classes by calls to `union`.
        //
        // When we are finished, `lists` will contain M independent linked lists,
        // each of which represents one equivalence class, and which is terminated
        // by UFEC_NULL.  And `heads` is used to point to the starting point of
        // each elem's equivalence class, as follows:
        //
        // * if heads[elem][bit 31] == 1, then heads[i][bits 30:0] contain the
        //   index in lists[] of the first element in `elem`s equivalence class.
        //
        // * if heads[elem][bit 31] == 0, then heads[i][bits 30:0] contain tell us
        //   what `elem`s equivalence class leader is.  That is, heads[i][bits
        //   30:0] tells us the index in `heads` of the entry that contains the
        //   first element in `elem`s equivalence class.
        //
        // With this arrangement, we can:
        //
        // * detect whether `elem` is an equivalence class leader, by inspecting
        //   heads[elem][bit 31]
        //
        // * find the start of `elem`s equivalence class list, either by using
        //   heads[elem][bits 30:0] directly if heads[elem][bit 31] == 1, or
        //   using a single indirection if heads[elem][bit 31] == 0.
        //
        // For a universe of size N, this makes it possible to:
        //
        // * find the equivalence class list of any elem in O(1) time.
        //
        // * find and iterate through any single equivalence class in time O(1) +
        //   O(size of the equivalence class).
        //
        // * find all the equivalence class headers in O(N) time.
        //
        // * find all the equivalence class headers, and then iterate through each
        //   equivalence class exactly once, in time k1.O(N) + k2.O(N).  The first
        //   term is the cost of finding all the headers.  The second term is the
        //   cost of visiting all elements of each equivalence class exactly once.
        //
        // The construction algorithm requires two forward passes over
        // `parent_or_size`.
        //
        // In the first pass, we visit each element.  If a element is a tree root,
        // its `heads` entry is left at UFEC_NULL.  If a element isn't a tree
        // root, we use `find` to find the root element, and set
        // `heads[elem][30:0]` to be the tree root, and heads[elem][31] to 0.
        // Hence, after the first pass, `heads` maps each non-root element to its
        // equivalence class leader.
        //
        // The second pass builds the lists.  We again visit each element.  If a
        // element is a tree root, it is added as a list element, and its `heads`
        // entry is updated to point at the list element.  If a element isn't a
        // tree root, we find its root in constant time by inspecting its `head`
        // entry.  The element is added to the the root element's list, and the
        // root element's `head` entry is accordingly updated.  Hence, after the
        // second pass, the `head` entry for root elements points to a linked list
        // that contains all elements in that tree.  And the `head` entry for
        // non-root elements is unchanged from the first pass, that is, it points
        // to the `head` entry for that element's root element.
        //
        // Note that the heads[] entry for any class leader (tree root) can never
        // be UFEC_NULL, since all elements must at least be in an equivalence
        // class of size 1.  Hence there is no confusion possible resulting from
        // using the heads bit 31 entries as a direct/indirect flag.

        // First pass
        for i in 0..nElems {
            if self.parent_or_size[i as usize] >= 0 {
                // i is non-root
                let root_i: u32 = self.find(i);
                assert!(root_i < 0x8000_0000u32);
                heads[i as usize] = root_i; // .direct flag == 0
            }
        }

        // Second pass
        let mut list_bump = 0u32;
        for i in 0..nElems {
            if self.parent_or_size[i as usize] < 0 {
                // i is root
                lists[list_bump as usize] = LLElem {
                    elem: i,
                    tail: if heads[i as usize] == UFEC_NULL {
                        UFEC_NULL
                    } else {
                        heads[i as usize] & 0x7FFF_FFFF
                    },
                };
                assert!(list_bump < 0x8000_0000u32);
                heads[i as usize] = list_bump | 0x8000_0000u32; // .direct flag == 1
                list_bump += 1;
            } else {
                // i is non-root
                let i_root = heads[i as usize];
                lists[list_bump as usize] = LLElem {
                    elem: i,
                    tail: if heads[i_root as usize] == UFEC_NULL {
                        UFEC_NULL
                    } else {
                        heads[i_root as usize] & 0x7FFF_FFFF
                    },
                };
                assert!(list_bump < 0x8000_0000u32);
                heads[i_root as usize] = list_bump | 0x8000_0000u32; // .direct flag == 1
                list_bump += 1;
            }
        }
        assert!(list_bump == nElems);

        // It's a wrap!
        assert!(heads.len() == nElemsUSize);
        assert!(lists.len() == nElemsUSize);
        //{
        //  for i in 0 .. heads.len() {
        //    println!("{}:  heads {:x}  lists.elem {} .tail {:x}", i,
        //             heads[i], lists[i].elem, lists[i].tail);
        //  }
        //}
        UnionFindEquivClasses {
            heads,
            lists,
            anchor: PhantomData,
        }
    }
}

// We may want to find the equivalence class for some given element, and
// iterate through its elements.  This iterator provides that.

pub struct UnionFindEquivClassElemsIter<'a, T: ToFromU32> {
    // The equivalence classes
    /*priv*/
    ufec: &'a UnionFindEquivClasses<T>,
    // Index into `ufec.lists`, or UFEC_NULL.
    /*priv*/
    next: u32,
}

impl<T: ToFromU32> UnionFindEquivClasses<T> {
    pub fn equiv_class_elems_iter<'a>(&'a self, item: T) -> UnionFindEquivClassElemsIter<'a, T> {
        let mut itemU32 = ToFromU32::to_u32(item);
        assert!((itemU32 as usize) < self.heads.len());
        if (self.heads[itemU32 as usize] & 0x8000_0000) == 0 {
            // .direct flag is not set.  This is not a class leader.  We must
            // indirect.
            itemU32 = self.heads[itemU32 as usize];
        }
        // Now `itemU32` must point at a class leader.
        assert!((self.heads[itemU32 as usize] & 0x8000_0000) == 0x8000_0000);
        let next = self.heads[itemU32 as usize] & 0x7FFF_FFFF;
        // Now `next` points at the first element in the list.
        UnionFindEquivClassElemsIter { ufec: &self, next }
    }
}

impl<'a, T: ToFromU32> Iterator for UnionFindEquivClassElemsIter<'a, T> {
    type Item = T;
    fn next(&mut self) -> Option<Self::Item> {
        if self.next == UFEC_NULL {
            None
        } else {
            let res: T = ToFromU32::from_u32(self.ufec.lists[self.next as usize].elem);
            self.next = self.ufec.lists[self.next as usize].tail;
            Some(res)
        }
    }
}

// In order to visit all equivalence classes exactly once, we need something
// else: a way to enumerate their leaders (some value arbitrarily drawn from
// each one).  This provides that.

pub struct UnionFindEquivClassLeadersIter<'a, T: ToFromU32> {
    // The equivalence classes
    /*priv*/
    ufec: &'a UnionFindEquivClasses<T>,
    // Index into `ufec.heads` of the next unvisited item.
    /*priv*/
    next: u32,
}

impl<T: ToFromU32> UnionFindEquivClasses<T> {
    pub fn equiv_class_leaders_iter<'a>(&'a self) -> UnionFindEquivClassLeadersIter<'a, T> {
        UnionFindEquivClassLeadersIter {
            ufec: &self,
            next: 0,
        }
    }
}

impl<'a, T: ToFromU32> Iterator for UnionFindEquivClassLeadersIter<'a, T> {
    type Item = T;
    fn next(&mut self) -> Option<Self::Item> {
        // Scan forwards through `ufec.heads` to find the next unvisited one which
        // is a leader (a tree root).
        loop {
            if self.next as usize >= self.ufec.heads.len() {
                return None;
            }
            if (self.ufec.heads[self.next as usize] & 0x8000_0000) == 0x8000_0000 {
                // This is a leader.
                let res = ToFromU32::from_u32(self.next);
                self.next += 1;
                return Some(res);
            }
            // No luck, keep one searching.
            self.next += 1;
        }
        /*NOTREACHED*/
    }
}

// ====== Testing machinery for UnionFind ======

#[cfg(test)]
mod union_find_test_utils {
    use super::UnionFindEquivClasses;
    // Test that the eclass for `elem` is `expected` (modulo ordering).
    pub fn test_eclass(eclasses: &UnionFindEquivClasses<u32>, elem: u32, expected: &Vec<u32>) {
        let mut expected_sorted = expected.clone();
        let mut actual = vec![];
        for ecm in eclasses.equiv_class_elems_iter(elem) {
            actual.push(ecm);
        }
        expected_sorted.sort();
        actual.sort();
        assert!(actual == expected_sorted);
    }
    // Test that the eclass leaders are exactly `expected`.
    pub fn test_leaders(
        univ_size: u32,
        eclasses: &UnionFindEquivClasses<u32>,
        expected: &Vec<u32>,
    ) {
        let mut actual = vec![];
        for leader in eclasses.equiv_class_leaders_iter() {
            actual.push(leader);
        }
        assert!(actual == *expected);
        // Now use the headers to enumerate each eclass exactly once, and collect
        // up the elements.  The resulting vector should be some permutation of
        // [0 .. univ_size-1].
        let mut univ_actual = vec![];
        for leader in eclasses.equiv_class_leaders_iter() {
            for elem in eclasses.equiv_class_elems_iter(leader) {
                univ_actual.push(elem);
            }
        }
        univ_actual.sort();
        let mut univ_expected = vec![];
        for i in 0..univ_size {
            univ_expected.push(i);
        }
        assert!(univ_actual == univ_expected);
    }
}

#[test]
fn test_union_find() {
    const UNIV_SIZE: u32 = 8;
    let mut uf = UnionFind::new(UNIV_SIZE as usize);
    let mut uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![3]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![4]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![5]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![6]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 1, 2, 3, 4, 5, 6, 7]);

    uf.union(2, 4);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![4, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![3]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![4, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![5]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![6]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 1, 2, 3, 5, 6, 7]);

    uf.union(5, 3);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![4, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![5, 3]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![4, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![5, 3]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![6]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 1, 2, 5, 6, 7]);

    uf.union(2, 5);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![6]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 1, 2, 6, 7]);

    uf.union(7, 1);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![7, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![6]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7, 1]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 2, 6, 7]);

    uf.union(6, 7);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![7, 6, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![5, 4, 3, 2]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![7, 6, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7, 6, 1]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 2, 6]);

    uf.union(4, 1);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7, 6, 5, 4, 3, 2, 1]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0, 6]);

    uf.union(0, 3);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0]);

    // Pointless, because the classes are already maximal.
    uf.union(1, 2);
    uf_eclasses = uf.get_equiv_classes();
    union_find_test_utils::test_eclass(&uf_eclasses, 0, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 1, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 2, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 3, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 4, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 5, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 6, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_eclass(&uf_eclasses, 7, &vec![7, 6, 5, 4, 3, 2, 1, 0]);
    union_find_test_utils::test_leaders(UNIV_SIZE, &uf_eclasses, &vec![0]);
}
