//! AVL trees with a private allocation pool.
//!
//! AVL tree internals are public, so that backtracking.rs can do custom
//! traversals of the tree as it wishes.

use std::cmp::Ordering;

//=============================================================================
// Data structures for AVLTree

#[derive(Clone, PartialEq)]
pub enum AVLTag {
    Free,  // This pool entry is not in use
    None,  // This pool entry is in use.  Neither subtree is higher.
    Left,  // This pool entry is in use.  The left subtree is higher.
    Right, // This pool entry is in use.  The right subtree is higher.
}

#[derive(Clone)]
pub struct AVLNode<T> {
    pub tag: AVLTag,
    pub left: u32,
    pub right: u32,
    pub item: T,
}
impl<T> AVLNode<T> {
    fn new(tag: AVLTag, left: u32, right: u32, item: T) -> Self {
        Self {
            tag,
            left,
            right,
            item,
        }
    }
}

pub const AVL_NULL: u32 = 0xFFFF_FFFF;

pub struct AVLTree<T> {
    // The storage area.  There can be at most 2^32-2 entries, since AVL_NULL
    // (== 2^32-1) is used to mean "the null pointer".
    pub pool: Vec<AVLNode<T>>,
    // A default value for the stored item.  We don't care what this is;
    // unfortunately Rust forces us to have one so that additions to the free
    // list will be fully initialised.
    default: T,
    // The freelist head.  This is a list of available entries.  Each item on
    // the freelist must have its .tag be AVLTag::Free, and will use its .left
    // field as the link to the next freelist item.  A freelist link value of
    // AVL_NULL denotes the end of the list.  If `freelist` itself is AVL_NULL
    // then the list is empty.
    freelist: u32,
    // Last but not least, the root node.
    pub root: u32,
}

//=============================================================================
// Storage management functions for AVLTree

impl<T: Copy> AVLTree<T> {
    // Create a new tree and its associated storage pool.  This requires knowing
    // the default item value.
    pub fn new(default: T) -> Self {
        let pool = vec![];
        let freelist = AVL_NULL;
        let root = AVL_NULL;
        Self {
            pool,
            default,
            freelist,
            root,
        }
    }

    // Private function: free a tree node and put it back on the storage pool's
    // freelist.
    fn free(&mut self, index: u32) {
        assert!(index != AVL_NULL);
        assert!(self.pool[index as usize].tag != AVLTag::Free);
        self.pool[index as usize] =
            AVLNode::new(AVLTag::Free, self.freelist, AVL_NULL, self.default);
        self.freelist = index;
    }

    // Private function: allocate a tree node from the storage pool, resizing
    // the pool if necessary.  This will decline to expand the tree past about
    // 1.75 billion items.
    fn alloc(&mut self) -> u32 {
        // Check to see if the freelist is empty, and if so allocate a bunch more
        // slots.
        if self.freelist == AVL_NULL {
            let start = self.pool.len();
            let fill_item = AVLNode::new(AVLTag::Free, AVL_NULL, AVL_NULL, self.default);
            // What happens if this OOMs?  At least guard against u32 overflow by
            // doing this:
            if start >= 0x7000_0000 {
                // 1.75 billion elements in the tree should be enough for any
                // reasonable use of this register allocator.
                panic!("AVLTree<T>::alloc: too many items");
            }
            self.pool.resize(2 * start + 2, fill_item);
            let end_plus_1 = self.pool.len();
            debug_assert!(end_plus_1 >= 2);
            self.pool[end_plus_1 - 1].left = self.freelist;
            let mut i = end_plus_1 - 2;
            while i >= start {
                // The entry is already marked as free, but we must set the link.
                self.pool[i].left = i as u32 + 1;
                if i == 0 {
                    break;
                }
                i -= 1;
            }
            self.freelist = start as u32;
            debug_assert!(self.freelist != AVL_NULL);
        }
        // And now allocate.
        let new = self.freelist;
        assert!(self.pool[new as usize].tag == AVLTag::Free);
        // The caller is responsible for filling in the entry.  But at least set
        // the tag to non-Free, for sanity.
        self.pool[new as usize].tag = AVLTag::None;
        self.freelist = self.pool[new as usize].left;
        new
    }
}

//=============================================================================
// Tree-wrangling machinery for AVLTree (private)

// For the public interface, see below.

// The functions 'insert' and 'delete', and all supporting functions reachable
// from them, are derived from a public domain implementation by Georg Kraml.
// Unfortunately the relevant web site is long gone, and can only be found on
// the Wayback Machine.
//
// https://web.archive.org/web/20010419134337/
//   http://www.kraml.at/georg/avltree/index.html
//
// https://web.archive.org/web/20030926063347/
//   http://www.kraml.at/georg/avltree/avlmonolithic.c
//
// https://web.archive.org/web/20030401124003/http://www.kraml.at/src/howto/
//
// For relicensing clearance, see Mozilla bug 1620332, at
// https://bugzilla.mozilla.org/show_bug.cgi?id=1620332.

// Did a given insertion/deletion succeed, and what do we do next?
#[derive(Clone, Copy, PartialEq)]
enum AVLRes {
    Error,
    OK,
    Balance,
}

impl<T: Copy + PartialOrd> AVLTree<T> {
    // Private function: rotleft: perform counterclockwise rotation
    // Takes the root of the tree to rotate, returns the new root
    fn rotleft(&mut self, old_root: u32) -> u32 {
        let new_root = self.pool[old_root as usize].right;
        self.pool[old_root as usize].right = self.pool[new_root as usize].left;
        self.pool[new_root as usize].left = old_root;
        new_root
    }

    // Private function: rotright: perform clockwise rotation
    // Takes the root of the tree to rotate, returns the new root
    fn rotright(&mut self, old_root: u32) -> u32 {
        let new_root = self.pool[old_root as usize].left;
        self.pool[old_root as usize].left = self.pool[new_root as usize].right;
        self.pool[new_root as usize].right = old_root;
        new_root
    }

    // Private function: leftgrown: helper function for `insert`
    //
    //  Parameters:
    //
    //    root        Root of a tree. This node's left
    //                subtree has just grown due to item insertion; its
    //                "tag" flag needs adjustment, and the local tree
    //                (the subtree of which this node is the root node) may
    //                have become unbalanced.
    //
    //  Return values:
    //
    //    The new root of the subtree, plus either:
    //
    //    OK          The local tree could be rebalanced or was balanced
    //                from the start. The parent activations of the avlinsert
    //                activation that called this function may assume the
    //                entire tree is valid.
    //    or
    //    BALANCE     The local tree was balanced, but has grown in height.
    //                Do not assume the entire tree is valid.
    fn leftgrown(&mut self, mut root: u32) -> (u32, AVLRes) {
        match self.pool[root as usize].tag {
            AVLTag::Left => {
                if self.pool[self.pool[root as usize].left as usize].tag == AVLTag::Left {
                    self.pool[root as usize].tag = AVLTag::None;
                    let t = self.pool[root as usize].left;
                    self.pool[t as usize].tag = AVLTag::None;
                    root = self.rotright(root);
                } else {
                    match self.pool
                        [self.pool[self.pool[root as usize].left as usize].right as usize]
                        .tag
                    {
                        AVLTag::Left => {
                            self.pool[root as usize].tag = AVLTag::Right;
                            let t = self.pool[root as usize].left;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Right => {
                            self.pool[root as usize].tag = AVLTag::None;
                            let t = self.pool[root as usize].left;
                            self.pool[t as usize].tag = AVLTag::Left;
                        }
                        AVLTag::None => {
                            self.pool[root as usize].tag = AVLTag::None;
                            let t = self.pool[root as usize].left;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Free => panic!("AVLTree::leftgrown(1): unallocated node in tree"),
                    }
                    {
                        let t = self.pool[self.pool[root as usize].left as usize].right;
                        self.pool[t as usize].tag = AVLTag::None;
                    }
                    self.pool[root as usize].left = self.rotleft(self.pool[root as usize].left);
                    root = self.rotright(root);
                }
                return (root, AVLRes::OK);
            }
            AVLTag::Right => {
                self.pool[root as usize].tag = AVLTag::None;
                return (root, AVLRes::OK);
            }
            AVLTag::None => {
                self.pool[root as usize].tag = AVLTag::Left;
                return (root, AVLRes::Balance);
            }
            AVLTag::Free => panic!("AVLTree::leftgrown(2): unallocated node in tree"),
        }
    }

    // Private function: rightgrown: helper function for `insert`
    //
    //  See leftgrown for details.
    fn rightgrown(&mut self, mut root: u32) -> (u32, AVLRes) {
        match self.pool[root as usize].tag {
            AVLTag::Left => {
                self.pool[root as usize].tag = AVLTag::None;
                return (root, AVLRes::OK);
            }
            AVLTag::Right => {
                if self.pool[self.pool[root as usize].right as usize].tag == AVLTag::Right {
                    self.pool[root as usize].tag = AVLTag::None;
                    let t = self.pool[root as usize].right as usize;
                    self.pool[t].tag = AVLTag::None;
                    root = self.rotleft(root);
                } else {
                    match self.pool
                        [self.pool[self.pool[root as usize].right as usize].left as usize]
                        .tag
                    {
                        AVLTag::Right => {
                            self.pool[root as usize].tag = AVLTag::Left;
                            let t = self.pool[root as usize].right;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Left => {
                            self.pool[root as usize].tag = AVLTag::None;
                            let t = self.pool[root as usize].right;
                            self.pool[t as usize].tag = AVLTag::Right;
                        }
                        AVLTag::None => {
                            self.pool[root as usize].tag = AVLTag::None;
                            let t = self.pool[root as usize].right;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Free => panic!("AVLTree::rightgrown(1): unallocated node in tree"),
                    }
                    {
                        let t = self.pool[self.pool[root as usize].right as usize].left;
                        self.pool[t as usize].tag = AVLTag::None;
                    }
                    self.pool[root as usize].right = self.rotright(self.pool[root as usize].right);
                    root = self.rotleft(root);
                }
                return (root, AVLRes::OK);
            }
            AVLTag::None => {
                self.pool[root as usize].tag = AVLTag::Right;
                return (root, AVLRes::Balance);
            }
            AVLTag::Free => panic!("AVLTree::rightgrown(2): unallocated node in tree"),
        }
    }

    // Private function: insert_wrk: insert a node into the AVL tree
    // (worker function)
    //
    //  Parameters:
    //
    //    root        Root of the tree in whch to insert `d`.
    //
    //    item        Item to be inserted.
    //
    //  Return values:
    //
    //  Root of the new tree, plus one of:
    //
    //    nonzero     The item has been inserted. The excact value of
    //                nonzero yields is of no concern to user code; when
    //                avlinsert recursively calls itself, the number
    //                returned tells the parent activation if the AVL tree
    //                may have become unbalanced; specifically:
    //
    //      OK        None of the subtrees of the node that n points to
    //                has grown, the AVL tree is valid.
    //
    //      BALANCE   One of the subtrees of the node that n points to
    //                has grown, the node's "skew" flag needs adjustment,
    //                and the AVL tree may have become unbalanced.
    //
    //    zero        The datum provided could not be inserted, either due
    //                to AVLKEY collision (the tree already contains another
    //                item with which the same AVLKEY is associated), or
    //                due to insufficient memory.
    fn insert_wrk<F>(&mut self, mut root: u32, item: T, mb_cmp: Option<&F>) -> (u32, AVLRes)
    where
        F: Fn(T, T) -> Option<Ordering>,
    {
        if root == AVL_NULL {
            root = self.alloc();
            self.pool[root as usize] = AVLNode::new(AVLTag::None, AVL_NULL, AVL_NULL, item);
            return (root, AVLRes::Balance);
        }

        let cmp_arg_left: T = item;
        let cmp_arg_right: T = self.pool[root as usize].item;
        let cmp_res = match mb_cmp {
            None => cmp_arg_left.partial_cmp(&cmp_arg_right),
            Some(cmp) => cmp(cmp_arg_left, cmp_arg_right),
        };
        match cmp_res {
            None => panic!("AVLTree::insert_wrk: unordered elements"),
            Some(Ordering::Less) => {
                let (new_left, res) = self.insert_wrk(self.pool[root as usize].left, item, mb_cmp);
                self.pool[root as usize].left = new_left;
                if res == AVLRes::Balance {
                    return self.leftgrown(root);
                }
                return (root, res);
            }
            Some(Ordering::Greater) => {
                let (new_right, res) =
                    self.insert_wrk(self.pool[root as usize].right, item, mb_cmp);
                self.pool[root as usize].right = new_right;
                if res == AVLRes::Balance {
                    return self.rightgrown(root);
                }
                return (root, res);
            }
            Some(Ordering::Equal) => {
                return (root, AVLRes::Error);
            }
        }
    }

    // Private function: leftshrunk: helper function for delete and
    // findlowest
    //
    //  Parameters:
    //
    //    n           Address of a pointer to a node. The node's left
    //                subtree has just shrunk due to item removal; its
    //                "skew" flag needs adjustment, and the local tree
    //                (the subtree of which this node is the root node) may
    //                have become unbalanced.
    //
    //   Return values:
    //
    //    OK          The parent activation of the delete activation
    //                that called this function may assume the entire
    //                tree is valid.
    //
    //    BALANCE     Do not assume the entire tree is valid.
    fn leftshrunk(&mut self, mut n: u32) -> (u32, AVLRes) {
        match self.pool[n as usize].tag {
            AVLTag::Left => {
                self.pool[n as usize].tag = AVLTag::None;
                return (n, AVLRes::Balance);
            }
            AVLTag::Right => {
                if self.pool[self.pool[n as usize].right as usize].tag == AVLTag::Right {
                    self.pool[n as usize].tag = AVLTag::None;
                    let t = self.pool[n as usize].right;
                    self.pool[t as usize].tag = AVLTag::None;
                    n = self.rotleft(n);
                    return (n, AVLRes::Balance);
                } else if self.pool[self.pool[n as usize].right as usize].tag == AVLTag::None {
                    self.pool[n as usize].tag = AVLTag::Right;
                    let t = self.pool[n as usize].right;
                    self.pool[t as usize].tag = AVLTag::Left;
                    n = self.rotleft(n);
                    return (n, AVLRes::OK);
                } else {
                    match self.pool[self.pool[self.pool[n as usize].right as usize].left as usize]
                        .tag
                    {
                        AVLTag::Left => {
                            self.pool[n as usize].tag = AVLTag::None;
                            let t = self.pool[n as usize].right;
                            self.pool[t as usize].tag = AVLTag::Right;
                        }
                        AVLTag::Right => {
                            self.pool[n as usize].tag = AVLTag::Left;
                            let t = self.pool[n as usize].right;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::None => {
                            self.pool[n as usize].tag = AVLTag::None;
                            let t = self.pool[n as usize].right;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Free => {
                            panic!("AVLTree::leftshrunk(1): unallocated node in tree");
                        }
                    }
                    {
                        let t = self.pool[self.pool[n as usize].right as usize].left;
                        self.pool[t as usize].tag = AVLTag::None;
                    }
                    {
                        let t = self.rotright(self.pool[n as usize].right);
                        self.pool[n as usize].right = t;
                    }
                    n = self.rotleft(n);
                    return (n, AVLRes::Balance);
                }
            }
            AVLTag::None => {
                self.pool[n as usize].tag = AVLTag::Right;
                return (n, AVLRes::OK);
            }
            AVLTag::Free => {
                panic!("AVLTree::leftshrunk(2): unallocated node in tree");
            }
        }
    }

    // Private function: rightshrunk: helper function for delete and
    // findhighest
    //
    //  See leftshrunk for details.
    fn rightshrunk(&mut self, mut n: u32) -> (u32, AVLRes) {
        match self.pool[n as usize].tag {
            AVLTag::Right => {
                self.pool[n as usize].tag = AVLTag::None;
                return (n, AVLRes::Balance);
            }
            AVLTag::Left => {
                if self.pool[self.pool[n as usize].left as usize].tag == AVLTag::Left {
                    self.pool[n as usize].tag = AVLTag::None;
                    let t = self.pool[n as usize].left;
                    self.pool[t as usize].tag = AVLTag::None;
                    n = self.rotright(n);
                    return (n, AVLRes::Balance);
                } else if self.pool[self.pool[n as usize].left as usize].tag == AVLTag::None {
                    self.pool[n as usize].tag = AVLTag::Left;
                    let t = self.pool[n as usize].left;
                    self.pool[t as usize].tag = AVLTag::Right;
                    n = self.rotright(n);
                    return (n, AVLRes::OK);
                } else {
                    match self.pool[self.pool[self.pool[n as usize].left as usize].right as usize]
                        .tag
                    {
                        AVLTag::Left => {
                            self.pool[n as usize].tag = AVLTag::Right;
                            let t = self.pool[n as usize].left;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Right => {
                            self.pool[n as usize].tag = AVLTag::None;
                            let t = self.pool[n as usize].left;
                            self.pool[t as usize].tag = AVLTag::Left;
                        }
                        AVLTag::None => {
                            self.pool[n as usize].tag = AVLTag::None;
                            let t = self.pool[n as usize].left;
                            self.pool[t as usize].tag = AVLTag::None;
                        }
                        AVLTag::Free => {
                            panic!("AVLTree::rightshrunk(1): unallocated node in tree");
                        }
                    }
                    {
                        let t = self.pool[self.pool[n as usize].left as usize].right;
                        self.pool[t as usize].tag = AVLTag::None;
                    }
                    {
                        let t = self.rotleft(self.pool[n as usize].left);
                        self.pool[n as usize].left = t;
                    }
                    n = self.rotright(n);
                    return (n, AVLRes::Balance);
                }
            }
            AVLTag::None => {
                self.pool[n as usize].tag = AVLTag::Left;
                return (n, AVLRes::OK);
            }
            AVLTag::Free => {
                panic!("AVLTree::rightshrunk(2): unallocated node in tree");
            }
        }
    }

    // Private function: findhighest: replace a node with a subtree's
    // highest-ranking item.
    //
    //  Parameters:
    //
    //    target      Pointer to node to be replaced.
    //
    //    n           Address of pointer to subtree.
    //
    //    res         Pointer to variable used to tell the caller whether
    //                further checks are necessary; analog to the return
    //                values of leftgrown and leftshrunk (see there).
    //
    //  Return values:
    //
    //    True        A node was found; the target node has been replaced.
    //
    //    False       The target node could not be replaced because
    //                the subtree provided was empty.
    //
    fn findhighest(&mut self, target: u32, mut n: u32) -> Option<(u32, AVLRes)> {
        if n == AVL_NULL {
            return None;
        }
        let mut res = AVLRes::Balance;
        if self.pool[n as usize].right != AVL_NULL {
            let rec = self.findhighest(target, self.pool[n as usize].right);
            if let Some((new_n_right, new_res)) = rec {
                self.pool[n as usize].right = new_n_right;
                res = new_res;
                if res == AVLRes::Balance {
                    let (new_n, new_res) = self.rightshrunk(n);
                    n = new_n;
                    res = new_res;
                }
                return Some((n, res));
            } else {
                return None;
            }
        }
        self.pool[target as usize].item = self.pool[n as usize].item;
        let tmp = n;
        n = self.pool[n as usize].left;
        self.free(tmp);
        Some((n, res))
    }

    // Private function: findlowest: replace node with a subtree's
    // lowest-ranking item.
    //
    //  See findhighest for the details.
    fn findlowest(&mut self, target: u32, mut n: u32) -> Option<(u32, AVLRes)> {
        if n == AVL_NULL {
            return None;
        }
        let mut res = AVLRes::Balance;
        if self.pool[n as usize].left != AVL_NULL {
            let rec = self.findlowest(target, self.pool[n as usize].left);
            if let Some((new_n_left, new_res)) = rec {
                self.pool[n as usize].left = new_n_left;
                res = new_res;
                if res == AVLRes::Balance {
                    let (new_n, new_res) = self.leftshrunk(n);
                    n = new_n;
                    res = new_res;
                }
                return Some((n, res));
            } else {
                return None;
            }
        }
        self.pool[target as usize].item = self.pool[n as usize].item;
        let tmp = n;
        n = self.pool[n as usize].right;
        self.free(tmp);
        Some((n, res))
    }

    // Private function: delete_wrk: delete an item from the tree.
    // (worker function)
    //
    //  Parameters:
    //
    //    n           Address of a pointer to a node.
    //
    //    key         AVLKEY of item to be removed.
    //
    //  Return values:
    //
    //    nonzero     The item has been removed. The exact value of
    //                nonzero yields if of no concern to user code; when
    //                delete recursively calls itself, the number
    //                returned tells the parent activation if the AVL tree
    //                may have become unbalanced; specifically:
    //
    //      OK        None of the subtrees of the node that n points to
    //                has shrunk, the AVL tree is valid.
    //
    //      BALANCE   One of the subtrees of the node that n points to
    //                has shrunk, the node's "skew" flag needs adjustment,
    //                and the AVL tree may have become unbalanced.
    //
    //   zero         The tree does not contain an item yielding the
    //                AVLKEY value provided by the caller.
    fn delete_wrk<F>(&mut self, mut root: u32, item: T, mb_cmp: Option<&F>) -> (u32, AVLRes)
    where
        F: Fn(T, T) -> Option<Ordering>,
    {
        let mut tmp = AVLRes::Balance;
        if root == AVL_NULL {
            return (root, AVLRes::Error);
        }

        let cmp_arg_left: T = item;
        let cmp_arg_right: T = self.pool[root as usize].item;
        let cmp_res = match mb_cmp {
            None => cmp_arg_left.partial_cmp(&cmp_arg_right),
            Some(cmp) => cmp(cmp_arg_left, cmp_arg_right),
        };
        match cmp_res {
            None => panic!("AVLTree::delete_wrk: unordered elements"),
            Some(Ordering::Less) => {
                let root_left = self.pool[root as usize].left;
                let (new_root_left, new_tmp) = self.delete_wrk(root_left, item, mb_cmp);
                self.pool[root as usize].left = new_root_left;
                tmp = new_tmp;
                if tmp == AVLRes::Balance {
                    let (new_root, new_res) = self.leftshrunk(root);
                    root = new_root;
                    tmp = new_res;
                }
                return (root, tmp);
            }
            Some(Ordering::Greater) => {
                let root_right = self.pool[root as usize].right;
                let (new_root_right, new_tmp) = self.delete_wrk(root_right, item, mb_cmp);
                self.pool[root as usize].right = new_root_right;
                tmp = new_tmp;
                if tmp == AVLRes::Balance {
                    let (new_root, new_res) = self.rightshrunk(root);
                    root = new_root;
                    tmp = new_res;
                }
                return (root, tmp);
            }
            Some(Ordering::Equal) => {
                if self.pool[root as usize].left != AVL_NULL {
                    let root_left = self.pool[root as usize].left;
                    if let Some((new_root_left, new_tmp)) = self.findhighest(root, root_left) {
                        self.pool[root as usize].left = new_root_left;
                        tmp = new_tmp;
                        if new_tmp == AVLRes::Balance {
                            let (new_root, new_res) = self.leftshrunk(root);
                            root = new_root;
                            tmp = new_res;
                        }
                    }
                    return (root, tmp);
                }
                if self.pool[root as usize].right != AVL_NULL {
                    let root_right = self.pool[root as usize].right;
                    if let Some((new_root_right, new_tmp)) = self.findlowest(root, root_right) {
                        self.pool[root as usize].right = new_root_right;
                        tmp = new_tmp;
                        if new_tmp == AVLRes::Balance {
                            let (new_root, new_res) = self.rightshrunk(root);
                            root = new_root;
                            tmp = new_res;
                        }
                    }
                    return (root, tmp);
                }
                self.free(root);
                root = AVL_NULL;
                return (root, AVLRes::Balance);
            }
        }
    }

    // Private fn: count the number of items in the tree.  Warning: costs O(N) !
    #[cfg(test)]
    fn count_wrk(&self, n: u32) -> usize {
        if n == AVL_NULL {
            return 0;
        }
        1 + self.count_wrk(self.pool[n as usize].left) + self.count_wrk(self.pool[n as usize].right)
    }

    // Private fn: find the max depth of the tree.  Warning: costs O(N) !
    #[cfg(test)]
    fn depth_wrk(&self, n: u32) -> usize {
        if n == AVL_NULL {
            return 0;
        }
        let d_left = self.depth_wrk(self.pool[n as usize].left);
        let d_right = self.depth_wrk(self.pool[n as usize].right);
        1 + if d_left > d_right { d_left } else { d_right }
    }
}

// Machinery for iterating over the tree, enumerating nodes in ascending order.
// Unfortunately AVLTreeIter has to be public.
pub struct AVLTreeIter<'t, 's, T> {
    tree: &'t AVLTree<T>,
    stack: &'s mut Vec<u32>,
}

impl<'t, 's, T> AVLTreeIter<'t, 's, T> {
    fn new(tree: &'t AVLTree<T>, stack: &'s mut Vec<u32>) -> Self {
        let mut iter = AVLTreeIter { tree, stack };
        if tree.root != AVL_NULL {
            iter.stack.push(tree.root);
            iter.visit_left_children(tree.root);
        }
        iter
    }

    fn visit_left_children(&mut self, root: u32) {
        let mut cur = root;
        loop {
            let left = self.tree.pool[cur as usize].left;
            if left == AVL_NULL {
                break;
            }
            self.stack.push(left);
            cur = left;
        }
    }
}

impl<'s, 't, T: Copy> Iterator for AVLTreeIter<'s, 't, T> {
    type Item = T;
    fn next(&mut self) -> Option<Self::Item> {
        let ret = match self.stack.pop() {
            Some(ret) => ret,
            None => return None,
        };
        let right = self.tree.pool[ret as usize].right;
        if right != AVL_NULL {
            self.stack.push(right);
            self.visit_left_children(right);
        }
        Some(self.tree.pool[ret as usize].item)
    }
}

//=============================================================================
// Public interface for AVLTree

impl<T: Copy + PartialOrd> AVLTree<T> {
    // The core functions (insert, delete, contains) take a comparator argument
    //
    //   mb_cmp: Option<&F>
    //   where
    //     F: Fn(T, T) -> Option<Ordering>
    //
    // which allows control over how node comparison is done.  If this is None,
    // then comparison is done directly using PartialOrd for the T values.
    //
    // If this is Some(cmp), then comparison is done by passing the two T values
    // to `cmp`.  In this case, the routines will complain (panic) if `cmp`
    // indicates that its arguments are unordered.

    // Insert a value in the tree.  Returns true if an insert happened, false if
    // the item was already present.
    pub fn insert<F>(&mut self, item: T, mb_cmp: Option<&F>) -> bool
    where
        F: Fn(T, T) -> Option<Ordering>,
    {
        let (new_root, res) = self.insert_wrk(self.root, item, mb_cmp);
        if res == AVLRes::Error {
            return false; // already in tree
        } else {
            self.root = new_root;
            return true;
        }
    }

    // Remove an item from the tree.  Returns a bool which indicates whether the
    // value was in there in the first place.  (meaning, true == a removal
    // actually occurred).
    pub fn delete<F>(&mut self, item: T, mb_cmp: Option<&F>) -> bool
    where
        F: Fn(T, T) -> Option<Ordering>,
    {
        let (new_root, res) = self.delete_wrk(self.root, item, mb_cmp);
        if res == AVLRes::Error {
            return false;
        } else {
            self.root = new_root;
            return true;
        }
    }

    // Determine whether an item is in the tree.
    // sewardj 2020Mar31: this is not used; I assume all users of the trees
    // do their own custom traversals.  Remove #[cfg(test)] if any real uses
    // appear.
    #[cfg(test)]
    pub fn contains<F>(&self, item: T, mb_cmp: Option<&F>) -> bool
    where
        F: Fn(T, T) -> Option<Ordering>,
    {
        let mut n = self.root;
        // Lookup needs to be really fast, so have two versions of the loop, one
        // for direct comparison, one for indirect.
        match mb_cmp {
            None => {
                // Do comparisons directly on the items.
                loop {
                    if n == AVL_NULL {
                        return false;
                    }
                    let cmp_arg_left: T = item;
                    let cmp_arg_right: T = self.pool[n as usize].item;
                    match cmp_arg_left.partial_cmp(&cmp_arg_right) {
                        Some(Ordering::Less) => {
                            n = self.pool[n as usize].left;
                        }
                        Some(Ordering::Greater) => {
                            n = self.pool[n as usize].right;
                        }
                        Some(Ordering::Equal) => {
                            return true;
                        }
                        None => {
                            panic!("AVLTree::contains(1): unordered elements in search!");
                        }
                    }
                }
            }
            Some(cmp) => {
                // Do comparisons by handing off to the supplied function.
                loop {
                    if n == AVL_NULL {
                        return false;
                    }
                    let cmp_arg_left: T = item;
                    let cmp_arg_right: T = self.pool[n as usize].item;
                    match cmp(cmp_arg_left, cmp_arg_right) {
                        Some(Ordering::Less) => {
                            n = self.pool[n as usize].left;
                        }
                        Some(Ordering::Greater) => {
                            n = self.pool[n as usize].right;
                        }
                        Some(Ordering::Equal) => {
                            return true;
                        }
                        None => {
                            panic!("AVLTree::contains(2): unordered elements in search!");
                        }
                    }
                }
            }
        }
    }

    // Count the number of items in the tree.  Warning: costs O(N) !
    #[cfg(test)]
    fn count(&self) -> usize {
        self.count_wrk(self.root)
    }

    // Private fn: find the max depth of the tree.  Warning: costs O(N) !
    #[cfg(test)]
    fn depth(&self) -> usize {
        self.depth_wrk(self.root)
    }

    pub fn to_vec(&self) -> Vec<T> {
        // BEGIN helper fn
        fn walk<U: Copy>(res: &mut Vec<U>, root: u32, pool: &Vec<AVLNode<U>>) {
            let root_left = pool[root as usize].left;
            if root_left != AVL_NULL {
                walk(res, root_left, pool);
            }
            res.push(pool[root as usize].item);
            let root_right = pool[root as usize].right;
            if root_right != AVL_NULL {
                walk(res, root_right, pool);
            }
        }
        // END helper fn

        let mut res = Vec::<T>::new();
        if self.root != AVL_NULL {
            walk(&mut res, self.root, &self.pool);
        }
        res
    }

    pub fn iter<'t, 's>(&'t self, storage: &'s mut Vec<u32>) -> AVLTreeIter<'t, 's, T> {
        storage.clear();
        AVLTreeIter::new(self, storage)
    }

    // Show the tree.  (For debugging only.)
    //pub fn show(&self, depth: i32, node: u32) {
    //  if node != AVL_NULL {
    //    self.show(depth + 1, self.pool[node as usize].left);
    //    for _ in 0..depth {
    //      print!("   ");
    //    }
    //    println!("{}", ToFromU32::to_u32(self.pool[node as usize].item));
    //    self.show(depth + 1, self.pool[node as usize].right);
    //  }
    //}
}

//=============================================================================
// Testing machinery for AVLTree

#[cfg(test)]
mod avl_tree_test_utils {
    use crate::data_structures::Set;
    use std::cmp::Ordering;

    // Perform various checks on the tree, and assert if it is not OK.
    pub fn check_tree(
        tree: &super::AVLTree<u32>,
        should_be_in_tree: &Set<u32>,
        univ_min: u32,
        univ_max: u32,
    ) {
        // Same cardinality
        let n_in_set = should_be_in_tree.card();
        let n_in_tree = tree.count();
        assert!(n_in_set == n_in_tree);

        // Tree is not wildly out of balance.  Depth should not exceed 1.44 *
        // log2(size).
        let tree_depth = tree.depth();
        let mut log2_size = 0;
        {
            let mut n: usize = n_in_tree;
            while n > 0 {
                n = n >> 1;
                log2_size += 1;
            }
        }
        // Actually a tighter limit than stated above.  For these test cases, the
        // tree is either perfectly balanced or within one level of being so
        // (hence the +1).
        assert!(tree_depth <= log2_size + 1);

        // Check that everything that should be in the tree is in it, and vice
        // versa.
        for i in univ_min..univ_max {
            let should_be_in = should_be_in_tree.contains(i);

            // Look it up with a null comparator (so `contains` compares
            // directly)
            let is_in = tree.contains::<fn(u32, u32) -> Option<Ordering>>(i, None);
            assert!(is_in == should_be_in);

            // We should get the same result with a custom comparator that does the
            // same as the null comparator.
            let is_in_w_cmp = tree.contains(
                i,
                Some(&(|x_left: u32, x_right: u32| x_left.partial_cmp(&x_right))),
            );
            assert!(is_in_w_cmp == is_in);

            // And even when the comparator is actually a closure
            let forty_two: u32 = 52;
            let is_in_w_cmp_closure = tree.contains(
                i,
                Some(
                    &(|x_left: u32, x_right: u32| {
                        (x_left + forty_two).partial_cmp(&(x_right + forty_two))
                    }),
                ),
            );
            assert!(is_in_w_cmp_closure == is_in);
        }

        // We could even test that the tree items are in-order, but it hardly
        // seems worth the hassle, since the previous test would surely have
        // failed if that wasn't the case.
    }
}

#[test]
fn test_avl_tree1() {
    use crate::data_structures::Set;

    // Perform tests on an AVL tree.  Use as values, every third number between
    // 5000 and 5999 inclusive.  This is to ensure that there's no confusion
    // between element values and internal tree indices (although I think the
    // typechecker guarantees that anyway).
    //
    // Also carry along a Set<u32>, which keeps track of which values should be
    // in the tree at the current point.
    const UNIV_MIN: u32 = 5000;
    const UNIV_MAX: u32 = 5999;
    const UNIV_SIZE: u32 = UNIV_MAX - UNIV_MIN + 1;

    let mut tree = AVLTree::<u32>::new(0);
    let mut should_be_in_tree = Set::<u32>::empty();

    // Add numbers to the tree, checking as we go.
    for i in UNIV_MIN..UNIV_MAX {
        // Idiotic but simple
        if i % 3 != 0 {
            continue;
        }
        let was_added = tree.insert::<fn(u32, u32) -> Option<Ordering>>(i, None);
        should_be_in_tree.insert(i);
        assert!(was_added == true);
        avl_tree_test_utils::check_tree(&tree, &should_be_in_tree, UNIV_MIN, UNIV_MAX);
    }

    // Then remove the middle half of the tree, also checking.
    for i in UNIV_MIN + UNIV_SIZE / 4..UNIV_MIN + 3 * (UNIV_SIZE / 4) {
        // Note that here, we're asking to delete a bunch of numbers that aren't
        // in the tree.  It should remain valid throughout.
        let was_removed = tree.delete::<fn(u32, u32) -> Option<Ordering>>(i, None);
        let should_have_been_removed = should_be_in_tree.contains(i);
        assert!(was_removed == should_have_been_removed);
        should_be_in_tree.delete(i);
        avl_tree_test_utils::check_tree(&tree, &should_be_in_tree, UNIV_MIN, UNIV_MAX);
    }

    // Now add some numbers which are already in the tree.
    for i in UNIV_MIN..UNIV_MIN + UNIV_SIZE / 4 {
        if i % 3 != 0 {
            continue;
        }
        let was_added = tree.insert::<fn(u32, u32) -> Option<Ordering>>(i, None);
        let should_have_been_added = !should_be_in_tree.contains(i);
        assert!(was_added == should_have_been_added);
        should_be_in_tree.insert(i);
        avl_tree_test_utils::check_tree(&tree, &should_be_in_tree, UNIV_MIN, UNIV_MAX);
    }

    // Then remove all numbers from the tree, in reverse order.
    for ir in UNIV_MIN..UNIV_MAX {
        let i = UNIV_MIN + (UNIV_MAX - ir);
        let was_removed = tree.delete::<fn(u32, u32) -> Option<Ordering>>(i, None);
        let should_have_been_removed = should_be_in_tree.contains(i);
        assert!(was_removed == should_have_been_removed);
        should_be_in_tree.delete(i);
        avl_tree_test_utils::check_tree(&tree, &should_be_in_tree, UNIV_MIN, UNIV_MAX);
    }

    // Now the tree should be empty.
    assert!(should_be_in_tree.is_empty());
    assert!(tree.count() == 0);

    // Now delete some more stuff.  Tree should still be empty :-)
    for i in UNIV_MIN + 10..UNIV_MIN + 100 {
        assert!(should_be_in_tree.is_empty());
        assert!(tree.count() == 0);
        tree.delete::<fn(u32, u32) -> Option<Ordering>>(i, None);
        avl_tree_test_utils::check_tree(&tree, &should_be_in_tree, UNIV_MIN, UNIV_MAX);
    }

    // The tree root should be NULL.
    assert!(tree.root == AVL_NULL);
    assert!(tree.freelist != AVL_NULL);

    // Check the freelist: all entries are of the expected form.
    for e in &tree.pool {
        assert!(e.tag == AVLTag::Free);
        assert!(e.left == AVL_NULL || (e.left as usize) < tree.pool.len());
        assert!(e.right == AVL_NULL);
        assert!(e.item == 0);
    }

    // Check the freelist: it's non-circular, and contains the expected number
    // of elements.
    let mut n_in_freelist = 0;
    let mut cursor: u32 = tree.freelist;
    while cursor != AVL_NULL {
        assert!((cursor as usize) < tree.pool.len());
        n_in_freelist += 1;
        assert!(n_in_freelist < 100000 /*arbitrary*/); // else it has a cycle
        cursor = tree.pool[cursor as usize].left;
    }
    // If we get here, the freelist at least doesn't have a cycle.

    // All elements in the pool are on the freelist.
    assert!(n_in_freelist == tree.pool.len());
}

#[test]
fn test_avl_tree2() {
    use std::cmp::Ordering;

    // Do some simple testing using a custom comparator, which inverts the order
    // of items in the tree, so as to check custom comparators work right.
    let mut tree = AVLTree::<u32>::new(0);

    let nums = [31, 41, 59, 27, 14, 35, 62, 25, 18, 28, 45, 90, 61];

    fn reverse_cmp(x: u32, y: u32) -> Option<Ordering> {
        y.partial_cmp(&x)
    }

    // Insert
    for n in &nums {
        let insert_happened = tree.insert(*n, Some(&reverse_cmp));
        assert!(insert_happened == true);
    }

    // Check membership
    for n in 0..100 {
        let is_in = tree.contains(n, Some(&reverse_cmp));
        let should_be_in = nums.iter().any(|m| n == *m);
        assert!(is_in == should_be_in);
    }

    // Delete
    for n in 0..100 {
        let remove_happened = tree.delete(n, Some(&reverse_cmp));
        let remove_should_have_happened = nums.iter().any(|m| n == *m);
        assert!(remove_happened == remove_should_have_happened);
    }

    // Final check
    assert!(tree.root == AVL_NULL);
    assert!(tree.count() == 0);
}

#[test]
fn test_avl_tree_iter() {
    let mut storage = Vec::new();
    let tree = AVLTree::<u32>::new(0);
    assert!(tree.iter(&mut storage).next().is_none());

    const FROM: u32 = 0;
    const TO: u32 = 10000;

    let mut tree = AVLTree::<u32>::new(0);
    for i in FROM..TO {
        tree.insert(i, Some(&|a: u32, b: u32| a.partial_cmp(&b)));
    }

    let as_vec = tree.to_vec();
    for (i, val) in tree.iter(&mut storage).enumerate() {
        assert_eq!(as_vec[i], val, "not equal for i={}", i);
    }
}
