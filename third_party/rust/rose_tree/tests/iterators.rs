
extern crate rose_tree;

use rose_tree::RoseTree;

struct Weight;


#[test]
fn children() {

    let (mut tree, root) = RoseTree::<Weight, u32>::new(Weight);
    let a = tree.add_child(root, Weight);
    let b = tree.add_child(root, Weight);
    let c = tree.add_child(root, Weight);

    {
        let mut children = tree.children(root);
        assert_eq!(Some(c), children.next());
        assert_eq!(Some(b), children.next());
        assert_eq!(Some(a), children.next());
        assert_eq!(None, children.next());
    }

    {
        let d = tree.add_child(b, Weight);
        let e = tree.add_child(b, Weight);
        let f = tree.add_child(b, Weight);
        let mut children = tree.children(b);
        assert_eq!(Some(f), children.next());
        assert_eq!(Some(e), children.next());
        assert_eq!(Some(d), children.next());
        assert_eq!(None, children.next());
    }
}


#[test]
fn parent_recursion() {

    let (mut tree, root) = RoseTree::<Weight, u32>::new(Weight);
    let a = tree.add_child(root, Weight);
    let a_1 = tree.add_child(root, Weight);
    let a_2 = tree.add_child(root, Weight);
    let b = tree.add_child(a, Weight);
    let c = tree.add_child(b, Weight);

    {
        let mut parent_recursion = tree.parent_recursion(c);
        assert_eq!(Some(b), parent_recursion.next());
        assert_eq!(Some(a), parent_recursion.next());
        assert_eq!(Some(root), parent_recursion.next());
        assert_eq!(None, parent_recursion.next());
    }

    let b_1 = tree.add_child(a_1, Weight);
    let c_1 = tree.add_child(b_1, Weight);
    let d_1 = tree.add_child(c_1, Weight);

    {
        let mut parent_recursion = tree.parent_recursion(d_1);
        assert_eq!(Some(c_1), parent_recursion.next());
        assert_eq!(Some(b_1), parent_recursion.next());
        assert_eq!(Some(a_1), parent_recursion.next());
        assert_eq!(Some(root), parent_recursion.next());
        assert_eq!(None, parent_recursion.next());
    }

    let b_2 = tree.add_child(a_2, Weight);
    let c_2 = tree.add_child(b_2, Weight);

    {
        let mut parent_recursion = tree.parent_recursion(c_2);
        assert_eq!(Some(b_2), parent_recursion.next());
        assert_eq!(Some(a_2), parent_recursion.next());
        assert_eq!(Some(root), parent_recursion.next());
        assert_eq!(None, parent_recursion.next());
    }
}


#[test]
fn siblings() {

    let (mut tree, root) = RoseTree::<Weight, u32>::new(Weight);
    let a = tree.add_child(root, Weight);
    let b = tree.add_child(root, Weight);
    let c = tree.add_child(root, Weight);
    let d = tree.add_child(root, Weight);

    {
        let mut siblings = tree.siblings(a);
        assert_eq!(Some(d), siblings.next());
        assert_eq!(Some(c), siblings.next());
        assert_eq!(Some(b), siblings.next());
        assert_eq!(None, siblings.next());
    }
}
