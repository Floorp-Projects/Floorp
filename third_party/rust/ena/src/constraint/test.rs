use super::*;

use std::cmp;

struct MaxLattice;

impl Lattice for MaxLattice {
    type Element = u32;

    fn lub(&self, elem1: &u32, elem2: &u32) -> Option<u32> {
        Some(cmp::max(*elem1, *elem2))
    }
}

#[test]
fn basic() {
    // v1 --+--> v2
    //      |
    // v3 --+

    let mut graph = ConstraintGraph::new(MaxLattice);
    let v1 = graph.new_var(3);
    let v2 = graph.new_var(0);
    graph.add_edge(v1, v2);
    assert_eq!(graph.current_value(v1), 3);
    assert_eq!(graph.current_value(v2), 3);

    let v3 = graph.new_var(5);
    graph.add_edge(v3, v2);
    assert_eq!(graph.current_value(v1), 3);
    assert_eq!(graph.current_value(v2), 5);
    assert_eq!(graph.current_value(v3), 5);

    graph.constrain_var(v1, 10);
    assert_eq!(graph.current_value(v1), 10);
    assert_eq!(graph.current_value(v2), 10);
    assert_eq!(graph.current_value(v3), 5);
}


#[test]
fn cycle() {
    // v1 ----> v2
    // ^        |
    // |        v
    // v3 <---- v3

    let mut graph = ConstraintGraph::new(MaxLattice);
    let vars = [graph.new_var(0), graph.new_var(0), graph.new_var(0), graph.new_var(0)];

    for i in 0..4 {
        graph.add_edge(vars[i], vars[(i + 1) % vars.len()]);
    }

    graph.constrain_var(vars[1], 3);
    assert!(vars.iter().all(|&var| graph.current_value(var) == 3));

    graph.constrain_var(vars[2], 5);
    assert!(vars.iter().all(|&var| graph.current_value(var) == 5));

    graph.constrain_var(vars[3], 2);
    assert!(vars.iter().all(|&var| graph.current_value(var) == 5));

    graph.constrain_var(vars[3], 6);
    assert!(vars.iter().all(|&var| graph.current_value(var) == 6));

    graph.constrain_var(vars[0], 10);
    assert!(vars.iter().all(|&var| graph.current_value(var) == 10));
}
