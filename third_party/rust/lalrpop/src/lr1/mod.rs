//! Naive LR(1) generation algorithm.

use grammar::repr::*;

mod build;
mod build_lalr;
pub mod codegen;
mod core;
mod error;
mod example;
mod first;
mod lane_table;
mod lookahead;
mod report;
mod state_graph;
mod tls;
mod trace;
use std::io::{self, Write};

#[cfg(test)]
mod interpret;

pub use self::core::{LR1Result, LR1TableConstructionError};
pub use self::error::report_error;
pub use self::tls::Lr1Tls;

pub fn build_states<'grammar>(
    grammar: &'grammar Grammar,
    start: NonterminalString,
) -> LR1Result<'grammar> {
    if !grammar.algorithm.lalr {
        build::build_lr1_states(grammar, start)
    } else {
        build_lalr::build_lalr_states(grammar, start)
    }
}

pub fn generate_report<'grammar, W: Write + 'grammar>(
    out: &'grammar mut W,
    lr1result: &LR1Result<'grammar>,
) -> io::Result<()> {
    report::generate_report(out, lr1result)
}
