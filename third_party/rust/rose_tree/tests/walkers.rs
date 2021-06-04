
extern crate rose_tree;

use rose_tree::RoseTree;

struct Weight;


#[test]
fn walk_children() {

    let (mut tree, root) = RoseTree::<Weight, u32>::new(Weight);
    let a = tree.add_child(root, Weight);
    let b = tree.add_child(root, Weight);
    let c = tree.add_child(root, Weight);

    let mut child_walker = tree.walk_children(root);
    assert_eq!(Some(c), child_walker.next(&tree));
    assert_eq!(Some(b), child_walker.next(&tree));
    assert_eq!(Some(a), child_walker.next(&tree));
    assert_eq!(None, child_walker.next(&tree));

    let d = tree.add_child(b, Weight);
    let e = tree.add_child(b, Weight);
    let f = tree.add_child(b, Weight);

    child_walker = tree.walk_children(b);
    assert_eq!(Some(f), child_walker.next(&tree));
    assert_eq!(Some(e), child_walker.next(&tree));
    assert_eq!(Some(d), child_walker.next(&tree));
    assert_eq!(None, child_walker.next(&tree));
}


#[test]
fn walk_siblings() {

    let (mut tree, root) = RoseTree::<Weight, u32>::new(Weight);
    let a = tree.add_child(root, Weight);
    let b = tree.add_child(root, Weight);
    let c = tree.add_child(root, Weight);
    let d = tree.add_child(root, Weight);

    let mut sibling_walker = tree.walk_siblings(a);
    assert_eq!(Some(d), sibling_walker.next(&tree));
    assert_eq!(Some(c), sibling_walker.next(&tree));
    assert_eq!(Some(b), sibling_walker.next(&tree));
    assert_eq!(None, sibling_walker.next(&tree));
}

