//! Constraint graph.

#![allow(dead_code)]

use graph::{Graph, NodeIndex};
use std::collections::VecDeque;
use std::u32;

#[cfg(test)]
mod test;

pub trait Lattice {
    type Element: Clone + Eq;

    fn lub(&self, elem1: &Self::Element, elem2: &Self::Element) -> Option<Self::Element>;
}

pub struct ConstraintGraph<L: Lattice> {
    graph: Graph<(), ()>,
    values: Vec<L::Element>,
    lattice: L,
}

#[derive(Copy, Clone)]
pub struct Var {
    index: u32,
}

impl Var {
    pub fn index(&self) -> usize {
        self.index as usize
    }

    fn to_node_index(self) -> NodeIndex {
        NodeIndex(self.index as usize)
    }

    fn from_node_index(ni: NodeIndex) -> Var {
        assert!(ni.0 < (u32::MAX as usize));
        Var { index: ni.0 as u32 }
    }
}

impl<L> ConstraintGraph<L>
    where L: Lattice
{
    fn new(lattice: L) -> ConstraintGraph<L> {
        ConstraintGraph {
            graph: Graph::new(),
            values: Vec::new(),
            lattice: lattice,
        }
    }

    fn new_var(&mut self, initial_value: L::Element) -> Var {
        assert_eq!(self.graph.all_nodes().len(), self.values.len());
        let node_index = self.graph.add_node(());
        self.values.push(initial_value);
        Var::from_node_index(node_index)
    }

    pub fn constrain_var(&mut self, var: Var, value: L::Element) -> Vec<PropagationError<L>> {
        let propagation = Propagation::new(&self.lattice, &self.graph, &mut self.values);
        propagation.propagate(value, var)
    }

    pub fn add_edge(&mut self, source: Var, target: Var) -> Vec<PropagationError<L>> {
        let source_node = source.to_node_index();
        let target_node = target.to_node_index();

        if self.graph
               .successor_nodes(source_node)
               .any(|n| n == target_node) {
            return vec![];
        }

        self.graph.add_edge(source_node, target_node, ());
        let value = self.current_value(source);
        self.constrain_var(target, value)
    }

    pub fn current_value(&self, node: Var) -> L::Element {
        self.values[node.index()].clone()
    }
}

/// ////////////////////////////////////////////////////////////////////////

struct Propagation<'p, L>
    where L: Lattice + 'p,
          L::Element: 'p
{
    lattice: &'p L,
    graph: &'p Graph<(), ()>,
    values: &'p mut Vec<L::Element>,
    queue: VecDeque<Var>,
    errors: Vec<PropagationError<L>>,
}

pub struct PropagationError<L>
    where L: Lattice
{
    var: Var,
    old_value: L::Element,
    new_value: L::Element,
}

impl<'p, L> Propagation<'p, L>
    where L: Lattice,
          L::Element: 'p
{
    fn new(lattice: &'p L,
           graph: &'p Graph<(), ()>,
           values: &'p mut Vec<L::Element>)
           -> Propagation<'p, L> {
        Propagation {
            lattice: lattice,
            graph: graph,
            values: values,
            queue: VecDeque::new(),
            errors: Vec::new(),
        }
    }

    fn propagate(mut self, value: L::Element, var: Var) -> Vec<PropagationError<L>> {
        self.update_node(value, var);

        while let Some(dirty) = self.queue.pop_front() {
            let value = self.values[dirty.index()].clone();

            for succ_node_index in self.graph.successor_nodes(dirty.to_node_index()) {
                let succ_var = Var::from_node_index(succ_node_index);
                self.update_node(value.clone(), succ_var);
            }
        }

        self.errors
    }

    fn update_node(&mut self, value: L::Element, var: Var) {
        let cur_value = self.values[var.index()].clone();
        match self.lattice.lub(&cur_value, &value) {
            Some(new_value) => {
                if cur_value != new_value {
                    self.values[var.index()] = value;
                    self.queue.push_back(var);
                }
            }

            None => {
                // Error. Record for later.
                self.errors.push(PropagationError::<L> {
                    var: var,
                    old_value: cur_value,
                    new_value: value,
                });
            }
        }
    }
}
