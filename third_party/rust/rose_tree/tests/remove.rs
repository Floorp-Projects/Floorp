
extern crate rose_tree;

use rose_tree::RoseTree;


#[test]
fn remove_node() {

    let (mut tree, root) = RoseTree::<u32, u32>::new(0);
    let a = tree.add_child(root, 1);
    let _b = tree.add_child(a, 2);
    let _c = tree.add_child(a, 3);
    let _d = tree.add_child(a, 4);

    assert_eq!(Some(1), tree.remove_node(a));

    let mut children = tree.children(root);
    assert_eq!(Some(2), children.next().map(|idx| tree[idx]));
    assert_eq!(Some(3), children.next().map(|idx| tree[idx]));
    assert_eq!(Some(4), children.next().map(|idx| tree[idx]));
    assert_eq!(None, children.next());
}


