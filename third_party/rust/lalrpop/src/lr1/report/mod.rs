use collections::*;
use grammar::repr::*;
use lr1::core::*;
use std::cmp::max;
use std::io::{self, Write};

use super::lookahead::*;

pub fn generate_report<'grammar, W: Write + 'grammar>(
    out: &'grammar mut W,
    lr1result: &LR1Result<'grammar>,
) -> io::Result<()> {
    let mut generator = ReportGenerator::new(out);
    generator.report_lr_table_construction(lr1result)
}

static INDENT_STRING: &'static str = "    ";

struct ReportGenerator<'report, W>
where
    W: Write + 'report,
{
    pub out: &'report mut W,
}

type ConflictStateMap<'report, 'grammar, L> = Map<StateIndex, Vec<&'report Conflict<'grammar, L>>>;

impl<'report, W> ReportGenerator<'report, W>
where
    W: Write + 'report,
{
    pub fn new(out: &'report mut W) -> Self {
        ReportGenerator { out: out }
    }

    pub fn report_lr_table_construction<'grammar: 'report, L>(
        &mut self,
        lr1result: &'report LRResult<'grammar, L>,
    ) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        try!(self.write_header());
        try!(self.write_section_header("Summary"));
        try!(writeln!(self.out, ""));
        match lr1result {
            &Ok(ref states) => {
                try!(writeln!(self.out, "Constructed {} states", states.len()));
                try!(self.report_states(&states, &Map::new()));
            }
            &Err(ref table_construction_error) => {
                try!(writeln!(self.out, "Failure"));
                try!(writeln!(
                    self.out,
                    "Constructed {} states",
                    table_construction_error.states.len()
                ));
                try!(writeln!(
                    self.out,
                    "Has {} conflicts",
                    table_construction_error.conflicts.len()
                ));
                let (sr, rr, conflict_map) =
                    self.process_conflicts(&table_construction_error.conflicts);
                if (sr > 0) {
                    try!(writeln!(self.out, "{}shift/reduce:  {}", INDENT_STRING, sr));
                }
                if (rr > 0) {
                    try!(writeln!(self.out, "{}reduce/reduce: {}", INDENT_STRING, rr));
                }
                try!(write!(self.out, "States with conflicts: "));
                for state in conflict_map.keys() {
                    try!(write!(self.out, " {}", state));
                }
                try!(writeln!(self.out, ""));
                try!(self.report_states(&table_construction_error.states, &conflict_map));
            }
        };
        Ok(())
    }

    fn process_conflicts<'grammar, L>(
        &mut self,
        conflicts: &'report Vec<Conflict<'grammar, L>>,
    ) -> (usize, usize, ConflictStateMap<'report, 'grammar, L>)
    where
        L: Lookahead,
    {
        let mut sr: usize = 0;
        let mut rr: usize = 0;
        let mut conflict_map = Map::new();
        for conflict in conflicts.iter() {
            match conflict.action {
                Action::Shift(_, _) => sr = sr + 1,
                Action::Reduce(_) => rr = rr + 1,
            }
            conflict_map
                .entry(conflict.state)
                .or_insert(vec![])
                .push(conflict);
        }
        (sr, rr, conflict_map)
    }

    fn report_states<'grammar, L>(
        &mut self,
        states: &Vec<State<'grammar, L>>,
        conflict_map: &ConflictStateMap<'report, 'grammar, L>,
    ) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        try!(self.write_section_header("State Table"));
        for ref state in states {
            try!(writeln!(self.out, ""));
            try!(self.report_state(&state, conflict_map.get(&state.index)));
        }
        Ok(())
    }

    fn report_state<'grammar, L>(
        &mut self,
        state: &State<'grammar, L>,
        conflicts_opt: Option<&Vec<&'report Conflict<'grammar, L>>>,
    ) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        try!(writeln!(self.out, "State {} {{", state.index));
        try!(self.write_items(&state.items));
        if (state.reductions.len() > 0) {
            try!(writeln!(self.out, ""));
            try!(self.write_reductions(&state.reductions));
        }

        let max_width = get_width_for_gotos(state);

        if (!state.shifts.len() > 0) {
            try!(writeln!(self.out, ""));
            try!(self.write_shifts(&state.shifts, max_width));
        }

        if (!state.gotos.len() > 0) {
            try!(writeln!(self.out, ""));
            try!(self.write_gotos(&state.gotos, max_width));
        }

        if let Some(conflicts) = conflicts_opt {
            for conflict in conflicts.iter() {
                try!(self.write_conflict(conflict));
            }
        }

        try!(writeln!(self.out, "}}"));
        Ok(())
    }

    fn write_conflict<'grammar, L>(&mut self, conflict: &Conflict<'grammar, L>) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        try!(writeln!(self.out, ""));
        match conflict.action {
            Action::Shift(ref terminal, state) => {
                let max_width = max(
                    terminal.display_len(),
                    conflict.production.nonterminal.len(),
                );
                try!(writeln!(self.out, "{}shift/reduce conflict", INDENT_STRING));
                try!(write!(
                    self.out,
                    "{}{}reduction ",
                    INDENT_STRING, INDENT_STRING
                ));
                try!(self.write_production(conflict.production, max_width));
                let sterminal = format!("{}", terminal);
                try!(writeln!(
                    self.out,
                    "{}{}shift     {:width$}    shift and goto {}",
                    INDENT_STRING,
                    INDENT_STRING,
                    sterminal,
                    state,
                    width = max_width
                ));
            }
            Action::Reduce(other_production) => {
                let max_width = max(
                    other_production.nonterminal.len(),
                    conflict.production.nonterminal.len(),
                );
                try!(writeln!(
                    self.out,
                    "{}reduce/reduce conflict",
                    INDENT_STRING
                ));
                try!(write!(
                    self.out,
                    "{}{}reduction ",
                    INDENT_STRING, INDENT_STRING
                ));
                try!(self.write_production(conflict.production, max_width));
                try!(write!(
                    self.out,
                    "{}{}reduction ",
                    INDENT_STRING, INDENT_STRING
                ));
                try!(self.write_production(other_production, max_width));
            }
        }
        try!(self.write_lookahead(&conflict.lookahead));
        Ok(())
    }

    fn write_items<'grammar, L>(&mut self, items: &Items<'grammar, L>) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        let max_width = get_max_length(items.vec.iter().map(|item| &item.production.nonterminal));

        for item in items.vec.iter() {
            try!(writeln!(self.out, ""));
            try!(self.write_item(item, max_width));
        }
        Ok(())
    }

    fn write_item<'grammar, L>(
        &mut self,
        item: &Item<'grammar, L>,
        max_width: usize,
    ) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        try!(write!(self.out, "{}", INDENT_STRING));
        // stringize it first to allow handle :width by Display for string
        let s = format!("{}", item.production.nonterminal);
        try!(write!(self.out, "{:width$} ->", s, width = max_width));
        for i in 0..item.index {
            try!(write!(self.out, " {}", item.production.symbols[i]));
        }
        try!(write!(self.out, " ."));
        for i in item.index..item.production.symbols.len() {
            try!(write!(self.out, " {}", item.production.symbols[i]));
        }
        try!(writeln!(self.out, ""));
        try!(self.write_lookahead(&item.lookahead));
        Ok(())
    }

    fn write_shifts(
        &mut self,
        shifts: &Map<TerminalString, StateIndex>,
        max_width: usize,
    ) -> io::Result<()> {
        for entry in shifts {
            try!(write!(self.out, "{}", INDENT_STRING));
            // stringize it first to allow handle :width by Display for string
            let s = format!("{}", entry.0);
            try!(writeln!(
                self.out,
                "{:width$} shift and goto {}",
                s,
                entry.1,
                width = max_width
            ));
        }
        Ok(())
    }

    fn write_reductions<'grammar, L>(
        &mut self,
        reductions: &Vec<(L, &'grammar Production)>,
    ) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        let max_width = get_max_length(reductions.into_iter().map(|p| &p.1.nonterminal));
        for reduction in reductions.iter() {
            try!(writeln!(self.out, ""));
            try!(self.write_reduction(reduction, max_width));
        }
        Ok(())
    }

    fn write_production<'grammar>(
        &mut self,
        production: &'grammar Production,
        max_width: usize,
    ) -> io::Result<()> {
        try!(write!(
            self.out,
            "{:width$} ->",
            production.nonterminal,
            width = max_width
        ));
        for symbol in production.symbols.iter() {
            try!(write!(self.out, " {}", symbol));
        }
        try!(writeln!(self.out, ""));
        Ok(())
    }

    fn write_reduction<'grammar, L>(
        &mut self,
        reduction: &(L, &'grammar Production),
        max_width: usize,
    ) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        let ref production = reduction.1;
        try!(write!(self.out, "{}reduction ", INDENT_STRING));
        try!(self.write_production(production, max_width));
        try!(self.write_lookahead(&reduction.0));
        Ok(())
    }

    fn write_lookahead<L>(&mut self, lookahead: &L) -> io::Result<()>
    where
        L: Lookahead + LookaheadPrinter<W>,
    {
        if (lookahead.has_anything_to_print()) {
            try!(write!(
                self.out,
                "{}{}lookahead",
                INDENT_STRING, INDENT_STRING
            ));
            try!(lookahead.print(self.out));
            try!(writeln!(self.out, ""));
        }
        Ok(())
    }

    fn write_gotos(
        &mut self,
        gotos: &Map<NonterminalString, StateIndex>,
        max_width: usize,
    ) -> io::Result<()> {
        for entry in gotos {
            try!(write!(self.out, "{}", INDENT_STRING));
            // stringize it first to allow handle :width by Display for string
            let s = format!("{}", entry.0);
            try!(writeln!(
                self.out,
                "{:width$} goto {}",
                s,
                entry.1,
                width = max_width
            ));
        }
        Ok(())
    }

    fn write_section_header(&mut self, title: &str) -> io::Result<()> {
        try!(writeln!(self.out, "\n{}", title));
        try!(writeln!(
            self.out,
            "----------------------------------------"
        ));
        Ok(())
    }

    fn write_header(&mut self) -> io::Result<()> {
        try!(writeln!(self.out, "Lalrpop Report File"));
        try!(writeln!(
            self.out,
            "========================================"
        ));
        Ok(())
    }
}

// helpers

trait LookaheadPrinter<W>
where
    W: Write,
{
    fn print<'report, 'grammar>(self: &Self, out: &'report mut W) -> io::Result<()>;

    fn has_anything_to_print<'report>(self: &Self) -> bool;
}

impl<W> LookaheadPrinter<W> for Nil
where
    W: Write,
{
    fn print<'report, 'grammar>(self: &Self, _: &'report mut W) -> io::Result<()> {
        Ok(())
    }

    fn has_anything_to_print<'report>(self: &Self) -> bool {
        false
    }
}

impl<W> LookaheadPrinter<W> for TokenSet
where
    W: Write,
{
    fn print<'report, 'grammar>(self: &Self, out: &'report mut W) -> io::Result<()> {
        for i in self.iter() {
            try!(write!(out, " {}", i))
        }
        Ok(())
    }

    fn has_anything_to_print<'report>(self: &Self) -> bool {
        self.len() > 0
    }
}

trait HasDisplayLen {
    fn display_len(&self) -> usize;
}

impl<'a> HasDisplayLen for &'a TerminalString {
    fn display_len(&self) -> usize {
        TerminalString::display_len(self)
    }
}

impl<'a> HasDisplayLen for &'a NonterminalString {
    fn display_len(&self) -> usize {
        self.len()
    }
}

fn get_max_length<I>(m: I) -> usize
where
    I: Iterator,
    I::Item: HasDisplayLen,
{
    m.map(|k| k.display_len()).fold(0, |acc, x| max(acc, x))
}

fn get_width_for_gotos<'grammar, L>(state: &State<'grammar, L>) -> usize
where
    L: Lookahead,
{
    let shifts_max_width = get_max_length(state.shifts.keys());
    let gotos_max_width = get_max_length(state.gotos.keys());
    max(shifts_max_width, gotos_max_width)
}
