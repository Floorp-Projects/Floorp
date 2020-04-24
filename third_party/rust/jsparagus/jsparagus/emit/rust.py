"""Emit code and parser tables in Rust."""

import json
import re
import unicodedata
import sys

from ..runtime import (ERROR, ErrorToken, SPECIAL_CASE_TAG)
from ..ordered import OrderedSet

from ..grammar import (CallMethod, Some, is_concrete_element, Nt, InitNt, Optional, End,
                       ErrorSymbol)
from ..actions import (Accept, Action, Reduce, Lookahead, CheckNotOnNewLine, FilterFlag, PushFlag,
                       PopFlag, FunCall, Seq)

from .. import types


TERMINAL_NAMES = {
    '{': 'OpenBrace',
    '}': 'CloseBrace',
    '(': 'OpenParenthesis',
    ')': 'CloseParenthesis',
    '[': 'OpenBracket',
    ']': 'CloseBracket',
    '+': 'Plus',
    '-': 'Minus',
    '~': 'BitwiseNot',
    '!': 'LogicalNot',
    '++': 'Increment',
    '--': 'Decrement',
    ':': 'Colon',
    '=>': 'Arrow',
    '=': 'EqualSign',
    '*=': 'MultiplyAssign',
    '/=': 'DivideAssign',
    '%=': 'RemainderAssign',
    '+=': 'AddAssign',
    '-=': 'SubtractAssign',
    '<<=': 'LeftShiftAssign',
    '>>=': 'SignedRightShiftAssign',
    '>>>=': 'UnsignedRightShiftAssign',
    '&=': 'BitwiseAndAssign',
    '^=': 'BitwiseXorAssign',
    '|=': 'BitwiseOrAssign',
    '**=': 'ExponentiateAssign',
    '.': 'Dot',
    '**': 'Exponentiate',
    '?.': 'OptionalChain',
    '?': 'QuestionMark',
    '??': 'Coalesce',
    '*': 'Star',
    '/': 'Divide',
    '%': 'Remainder',
    '<<': 'LeftShift',
    '>>': 'SignedRightShift',
    '>>>': 'UnsignedRightShift',
    '<': 'LessThan',
    '>': 'GreaterThan',
    '<=': 'LessThanOrEqualTo',
    '>=': 'GreaterThanOrEqualTo',
    '==': 'LaxEqual',
    '!=': 'LaxNotEqual',
    '===': 'StrictEqual',
    '!==': 'StrictNotEqual',
    '&': 'BitwiseAnd',
    '^': 'BitwiseXor',
    '|': 'BitwiseOr',
    '&&': 'LogicalAnd',
    '||': 'LogicalOr',
    ',': 'Comma',
    '...': 'Ellipsis',
}

class RustActionWriter:
    """Write epsilon state transitions for a given action function."""
    ast_builder = types.Type("AstBuilderDelegate", (types.Lifetime("alloc"),))

    def __init__(self, writer, traits, indent):
        self.writer = writer
        self.traits = traits
        self.indent = indent
        self.has_ast_builder = self.ast_builder in traits
        self.used_variables = set()

    def implement_trait(self, funcall):
        "Returns True if this function call should be encoded"
        ty = funcall.trait
        if ty.name == "AstBuilder":
            return "AstBuilderDelegate<'alloc>" in map(str, self.traits)
        if ty in self.traits:
            return True
        if len(ty.args) == 0:
            return ty.name in map(lambda t: t.name, self.traits)
        return False

    def reset(self, act):
        "Traverse all action to collect preliminary information."
        self.used_variables = set(self.collect_uses(act))

    def collect_uses(self, act):
        "Generator which visit all used variables."
        assert isinstance(act, Action)
        if isinstance(act, Reduce):
            yield "value"
        elif isinstance(act, FunCall):
            def map_with_offset(args):
                for a in args:
                    if isinstance(a, int):
                        yield a + act.offset
                    if isinstance(a, str):
                        yield a
                    elif isinstance(a, Some):
                        for offset in map_with_offset([a.inner]):
                            yield offset
            if self.implement_trait(act):
                for var in map_with_offset(act.args):
                    yield var
        elif isinstance(act, Seq):
            for a in act.actions:
                for var in self.collect_uses(a):
                    yield var

    def write(self, string, *format_args):
        "Delegate to the RustParserWriter.write function"
        self.writer.write(self.indent, string, *format_args)

    def write_state_transitions(self, state):
        "Given a state, generate the code corresponding to all outgoing epsilon edges."
        assert not state.is_inconsistent()
        assert len(list(state.shifted_edges())) == 0
        for ctx in self.writer.parse_table.debug_context(state.index, None):
            self.write("// {}", ctx)
        try:
            first, dest = next(state.edges(), (None, None))
            if first is None:
                return
            self.reset(first)
            if first.is_condition():
                self.write_condition(state, first)
            else:
                assert len(list(state.edges())) == 1
                self.write_action(first, dest)
        except Exception as exc:
            print("Error while writing code for {}\n\n".format(state))
            self.writer.parse_table.debug_info = True
            print(self.writer.parse_table.debug_context(state.index, "\n", "# "))
            raise exc

    def write_epsilon_transition(self, dest):
        self.write("// --> {}", dest)
        if dest >= self.writer.shift_count:
            self.write("state = {}", dest)
        else:
            self.write("parser.epsilon({});", dest)
            self.write("return Ok(false)")

    def write_condition(self, state, first_act):
        "Write code to test a conditions, and dispatch to the matching destination"
        # NOTE: we already asserted that this state is consistent, this implies
        # that the first state check the same variables as all remaining
        # states. Thus we use the first action to produce the match statement.
        assert isinstance(first_act, Action)
        assert first_act.is_condition()
        if isinstance(first_act, CheckNotOnNewLine):
            # TODO: At the moment this is Action is implemented as a single
            # operation with a single destination. However, we should implement
            # it in the future as 2 branches, one which is verifying the lack
            # of new lines, and one which is shifting an extra error token.
            # This might help remove the overhead of backtracking in addition
            # to make this backtracking visible through APS.
            assert len(list(state.edges())) == 1
            act, dest = next(state.edges())
            assert -act.offset > 0
            self.write("// {}", str(act))
            self.write("if !parser.check_not_on_new_line({})? {{", -act.offset)
            self.indent += 1
            self.write("return Ok(false);")
            self.indent -= 1
            self.write("}")
            self.write_epsilon_transition(dest)
        else:
            raise ValueError("Unexpected action type")

    def write_action(self, act, dest):
        assert isinstance(act, Action)
        assert not act.is_condition()
        is_packed = {}
        if isinstance(act, Seq):
            # Do not pop any of the stack elements if the reduce action has
            # an accept function call. Ideally we should be returning the
            # result instead of keeping it on the parser stack.
            if act.update_stack() and not act.contains_accept():
                assert not act.contains_accept()
                reducer = act.reduce_with()
                start = 0
                depth = reducer.pop
                if reducer.replay > 0:
                    self.write("parser.rewind({});", reducer.replay)
                    start = reducer.replay
                    depth += start
                for i in range(start, depth):
                    name = 's'
                    if i + 1 not in self.used_variables:
                        name = '_s'
                    self.write("let {}{} = parser.pop();", name, i + 1)

            for a in act.actions:
                self.write_single_action(a, is_packed)
                if a.contains_accept():
                    break
        else:
            self.write_single_action(act, is_packed)

        # If we fallthrough the execution of the action, then generate an
        # epsilon transition.
        if not act.update_stack() and not act.contains_accept():
            assert 0 <= dest < self.writer.shift_count + self.writer.action_count
            self.write_epsilon_transition(dest)

    def write_single_action(self, act, is_packed):
        self.write("// {}", str(act))
        if isinstance(act, Reduce):
            self.write_reduce(act, is_packed)
        elif isinstance(act, Accept):
            self.write_accept()
        elif isinstance(act, PushFlag):
            raise ValueError("NYI: PushFlag action")
        elif isinstance(act, PopFlag):
            raise ValueError("NYI: PopFlag action")
        elif isinstance(act, FunCall):
            self.write_funcall(act, is_packed)
        else:
            raise ValueError("Unexpected action type")

    def write_reduce(self, act, is_packed):
        value = "value"
        if value in is_packed:
            packed = is_packed[value]
        else:
            packed = False
            value = "None"

        if packed:
            # Extract the StackValue from the packed TermValue
            value = "{}.value".format(value)
        elif self.has_ast_builder:
            # Convert into a StackValue
            value = "TryIntoStack::try_into_stack({})?".format(value)
        else:
            # Convert into a StackValue (when no ast-builder)
            value = "value"

        self.write("let term = Term::Nonterminal(NonterminalId::{});",
                   self.writer.nonterminal_to_camel(act.nt))
        if value != "value":
            self.write("let value = {};", value)
        self.write("parser.replay(TermValue { term, value });")
        self.write("return Ok(false)")

    def write_accept(self):
        self.write("return Ok(true);")

    def write_funcall(self, act, is_packed):
        def no_unpack(val):
            return val

        def unpack(val):
            if val in is_packed:
                packed = is_packed[val]
            else:
                packed = True
            if packed:
                return "{}.value.to_ast()?".format(val)
            return val

        def map_with_offset(args, unpack):
            get_value = "s{}"
            for a in args:
                if isinstance(a, int):
                    yield unpack(get_value.format(a + act.offset))
                elif isinstance(a, str):
                    yield unpack(a)
                elif isinstance(a, Some):
                    yield "Some({})".format(next(map_with_offset([a.inner], unpack)))
                elif a is None:
                    yield "None"
                else:
                    raise ValueError(a)

        packed = False
        # If the variable is used, then generate the let binding.
        set_var = ""
        if act.set_to in self.used_variables:
            set_var = "let {} = ".format(act.set_to)

        # If the function cannot be call as the generated action function does
        # not use the trait on which this function is implemented, then replace
        # the value by `()`.
        if not self.implement_trait(act):
            self.write("{}();", set_var)
            return

        # NOTE: Currently "AstBuilder" is implemented through the
        # AstBuilderDelegate which returns a mutable reference to the
        # AstBuilder. This would call the specific special case method to get
        # the actual AstBuilder.
        delegate = ""
        if str(act.trait) == "AstBuilder":
            delegate = "ast_builder_refmut()."

        # NOTE: Currently "AstBuilder" functions are made fallible
        # using the fallible_methods taken from some Rust code
        # which extract this information to produce a JSON file.
        forward_errors = ""
        if act.fallible or act.method in self.writer.fallible_methods:
            forward_errors = "?"

        # By default generate a method call, with the method name. However,
        # there is a special case for the "id" function which is an artifact,
        # which does not have to unpack the content of its argument.
        value = "parser.{}{}({})".format(
            delegate, act.method,
            ", ".join(map_with_offset(act.args, unpack)))
        packed = False
        if act.method == "id":
            assert len(act.args) == 1
            value = next(map_with_offset(act.args, no_unpack))
            if isinstance(act.args[0], str):
                packed = is_packed[act.args[0]]
            else:
                assert isinstance(act.args[0], int)
                packed = True

        self.write("{}{}{};", set_var, value, forward_errors)
        is_packed[act.set_to] = packed


class RustParserWriter:
    def __init__(self, out, pt, fallible_methods):
        self.out = out
        self.fallible_methods = fallible_methods
        assert pt.exec_modes is not None
        self.parse_table = pt
        self.states = pt.states
        self.shift_count = pt.count_shift_states()
        self.action_count = pt.count_action_states()
        self.init_state_map = pt.named_goals
        self.terminals = list(OrderedSet(pt.terminals))
        # This extra terminal is used to represent any ErrorySymbol transition,
        # knowing that we assert that there is only one ErrorSymbol kind per
        # state.
        self.terminals.append("ErrorToken")
        self.nonterminals = list(OrderedSet(pt.nonterminals))

    def emit(self):
        self.header()
        self.terms_id()
        self.shift()
        self.error_codes()
        self.check_camel_case()
        self.parser_trait()
        self.actions()
        self.entry()

    def write(self, indentation, string, *format_args):
        if len(format_args) == 0:
            formatted = string
        else:
            formatted = string.format(*format_args)
        self.out.write("    " * indentation + formatted + "\n")

    def header(self):
        self.write(0, "// WARNING: This file is autogenerated.")
        self.write(0, "")
        self.write(0, "use crate::ast_builder::AstBuilderDelegate;")
        self.write(0, "use crate::stack_value_generated::{StackValue, TryIntoStack};")
        self.write(0, "use crate::error::Result;")
        traits = OrderedSet()
        for mode_traits in self.parse_table.exec_modes.values():
            traits |= mode_traits
        traits = list(traits)
        traits = [ty for ty in traits if ty.name != "AstBuilderDelegate"]
        traits = [ty for ty in traits if ty.name != "ParserTrait"]
        if traits == []:
            pass
        elif len(traits) == 1:
            self.write(0, "use crate::traits::{};", traits[0].name)
        else:
            self.write(0, "use crate::traits::{{{}}};", ", ".join(ty.name for ty in traits))
        self.write(0, "")
        self.write(0, "const ERROR: i64 = {};", hex(ERROR))
        self.write(0, "")

    def terminal_name(self, value):
        if isinstance(value, End) or value is None:
            return "End"
        elif isinstance(value, ErrorSymbol) or value is ErrorToken:
            return "ErrorToken"
        elif value in TERMINAL_NAMES:
            return TERMINAL_NAMES[value]
        elif value.isalpha():
            if value.islower():
                return value.capitalize()
            else:
                return value
        else:
            raw_name = " ".join((unicodedata.name(c) for c in value))
            snake_case = raw_name.replace("-", " ").replace(" ", "_").lower()
            camel_case = self.to_camel_case(snake_case)
            return camel_case

    def terminal_name_camel(self, value):
        return self.to_camel_case(self.terminal_name(value))

    def terms_id(self):
        self.write(0, "#[derive(Copy, Clone, Debug, PartialEq)]")
        self.write(0, "pub enum TerminalId {")
        for i, t in enumerate(self.terminals):
            name = self.terminal_name(t)
            self.write(1, "{} = {}, // {}", name, i, repr(t))
        self.write(0, "}")
        self.write(0, "")
        self.write(0, "#[derive(Clone, Copy, Debug, PartialEq)]")
        self.write(0, "pub enum NonterminalId {")
        offset = len(self.terminals)
        for i, nt in enumerate(self.nonterminals):
            self.write(1, "{} = {},", self.nonterminal_to_camel(nt), i + offset)
        self.write(0, "}")
        self.write(0, "")
        self.write(0, "#[derive(Clone, Copy, Debug, PartialEq)]")
        self.write(0, "pub enum Term {")
        self.write(1, "Terminal(TerminalId),")
        self.write(1, "Nonterminal(NonterminalId),")
        self.write(0, "}")
        self.write(0, "")
        self.write(0, "impl From<Term> for usize {")
        self.write(1, "fn from(term: Term) -> Self {")
        self.write(2, "match term {")
        self.write(3, "Term::Terminal(t) => t as usize,")
        self.write(3, "Term::Nonterminal(nt) => nt as usize,")
        self.write(2, "}")
        self.write(1, "}")
        self.write(0, "}")
        self.write(0, "")
        self.write(0, "impl From<Term> for &'static str {")
        self.write(1, "fn from(term: Term) -> Self {")
        self.write(2, "match term {")
        for t in self.terminals:
            name = self.terminal_name(t)
            self.write(3, "Term::Terminal(TerminalId::{}) => &\"{}\",", name, repr(t))
        for nt in self.nonterminals:
            name = self.nonterminal_to_camel(nt)
            self.write(3, "Term::Nonterminal(NonterminalId::{}) => &\"{}\",", name, str(nt.name))
        self.write(2, "}")
        self.write(1, "}")
        self.write(0, "}")
        self.write(0, "")

    def shift(self):
        self.write(0, "#[rustfmt::skip]")
        width = len(self.terminals) + len(self.nonterminals)
        num_shifted_edges = 0

        def state_get(state, t):
            nonlocal num_shifted_edges
            res = state.get(t, "ERROR")
            if res == "ERROR":
                error_symbol = state.get_error_symbol()
                if t == "ErrorToken" and error_symbol:
                    res = state[error_symbol]
                    num_shifted_edges += 1
            else:
                num_shifted_edges += 1
            return res

        self.write(0, "static SHIFT: [i64; {}] = [", self.shift_count * width)
        assert self.terminals[-1] == "ErrorToken"
        for i, state in enumerate(self.states[:self.shift_count]):
            num_shifted_edges = 0
            self.write(1, "// {}.", i)
            for ctx in self.parse_table.debug_context(state.index, None):
                self.write(1, "// {}", ctx)
            self.write(1, "{}",
                       ' '.join("{},".format(state_get(state, t)) for t in self.terminals))
            self.write(1, "{}",
                       ' '.join("{},".format(state_get(state, t)) for t in self.nonterminals))
            try:
                assert sum(1 for _ in state.shifted_edges()) == num_shifted_edges
            except Exception:
                print("Some edges are not encoded.")
                print("List of terminals: {}".format(', '.join(map(repr, self.terminals))))
                print("List of nonterminals: {}".format(', '.join(map(repr, self.nonterminals))))
                print("State having the issue: {}".format(str(state)))
                raise
        self.write(0, "];")
        self.write(0, "")

    def render_action(self, action):
        if isinstance(action, tuple):
            if action[0] == 'IfSameLine':
                _, a1, a2 = action
                if a1 is None:
                    a1 = 'ERROR'
                if a2 is None:
                    a2 = 'ERROR'
                index = self.add_special_case(
                    "if token.is_on_new_line { %s } else { %s }"
                    % (a2, a1))
            else:
                raise ValueError("unrecognized kind of special case: {!r}".format(action))
            return SPECIAL_CASE_TAG + index
        elif action == 'ERROR':
            return action
        else:
            assert isinstance(action, int)
            return action

    def emit_special_cases(self):
        self.write(0, "static SPECIAL_CASES: [fn(&Token) -> i64; {}] = [",
                   len(self.special_cases))
        for i, code in enumerate(self.special_cases):
            self.write(1, "|token| {{ {} }},", code)
        self.write(0, "];")
        self.write(0, "")

    def error_codes(self):
        self.write(0, "#[derive(Clone, Copy, Debug, PartialEq)]")
        self.write(0, "pub enum ErrorCode {")
        error_symbols = (s.get_error_symbol() for s in self.states[:self.shift_count])
        error_codes = (e.error_code for e in error_symbols if e is not None)
        for error_code in OrderedSet(error_codes):
            self.write(1, "{},", self.to_camel_case(error_code))
        self.write(0, "}")
        self.write(0, "")

        self.write(0, "static STATE_TO_ERROR_CODE: [Option<ErrorCode>; {}] = [",
                   self.shift_count)
        for i, state in enumerate(self.states[:self.shift_count]):
            error_symbol = state.get_error_symbol()
            if error_symbol is None:
                self.write(1, "None,")
            else:
                self.write(1, "// {}.", i)
                for ctx in self.parse_table.debug_context(state.index, None):
                    self.write(1, "// {}", ctx)
                self.write(1, "Some(ErrorCode::{}),",
                           self.to_camel_case(error_symbol.error_code))
        self.write(0, "];")
        self.write(0, "")

    def nonterminal_to_snake(self, ident):
        if isinstance(ident, Nt):
            if isinstance(ident.name, InitNt):
                name = "Start" + ident.name.goal.name
            else:
                name = ident.name
            base_name = self.to_snek_case(name)
            args = ''.join((("_" + self.to_snek_case(name))
                            for name, value in ident.args if value))
            return base_name + args
        else:
            assert isinstance(ident, str)
            return self.to_snek_case(ident)

    def nonterminal_to_camel(self, nt):
        return self.to_camel_case(self.nonterminal_to_snake(nt))

    def to_camel_case(self, ident):
        if '_' in ident:
            return ''.join(word.capitalize() for word in ident.split('_'))
        elif ident.islower():
            return ident.capitalize()
        else:
            return ident

    def check_camel_case(self):
        seen = {}
        for nt in self.nonterminals:
            cc = self.nonterminal_to_camel(nt)
            if cc in seen:
                raise ValueError("{} and {} have the same camel-case spelling ({})".format(
                    seen[cc], nt, cc))
            seen[cc] = nt

    def to_snek_case(self, ident):
        # https://stackoverflow.com/questions/1175208
        s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', ident)
        return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()

    def type_to_rust(self, ty, namespace="", boxed=False):
        """
        Convert a jsparagus type (see types.py) to Rust.

        Pass boxed=True if the type needs to be boxed.
        """
        if isinstance(ty, types.Lifetime):
            assert not boxed
            rty = "'" + ty.name
        elif ty == types.UnitType:
            assert not boxed
            rty = '()'
        elif ty == types.TokenType:
            rty = "Token"
        elif ty.name == 'Option' and len(ty.args) == 1:
            # We auto-translate `Box<Option<T>>` to `Option<Box<T>>` since
            # that's basically the same thing but more efficient.
            [arg] = ty.args
            return 'Option<{}>'.format(self.type_to_rust(arg, namespace, boxed))
        elif ty.name == 'Vec' and len(ty.args) == 1:
            [arg] = ty.args
            rty = "Vec<'alloc, {}>".format(self.type_to_rust(arg, namespace, boxed=False))
        else:
            if namespace == "":
                rty = ty.name
            else:
                rty = namespace + '::' + ty.name
            if ty.args:
                rty += '<{}>'.format(', '.join(self.type_to_rust(arg, namespace, boxed)
                                               for arg in ty.args))
        if boxed:
            return "Box<'alloc, {}>".format(rty)
        else:
            return rty

    def parser_trait(self):
        self.write(0, "#[derive(Debug)]")
        self.write(0, "pub struct TermValue<Value> {")
        self.write(1, "pub term: Term,")
        self.write(1, "pub value: Value,")
        self.write(0, "}")
        self.write(0, "")
        self.write(0, "pub trait ParserTrait<'alloc, Value> {")
        self.write(1, "fn shift(&mut self, tv: TermValue<Value>) -> Result<'alloc, bool>;")
        self.write(1, "fn unshift(&mut self);")
        self.write(1, "fn rewind(&mut self, n: usize) {")
        self.write(2, "for _ in 0..n {")
        self.write(3, "self.unshift();")
        self.write(2, "}")
        self.write(1, "}")
        self.write(1, "fn pop(&mut self) -> TermValue<Value>;")
        self.write(1, "fn replay(&mut self, tv: TermValue<Value>);")
        self.write(1, "fn epsilon(&mut self, state: usize);")
        self.write(1, "fn check_not_on_new_line(&mut self, peek: usize) -> Result<'alloc, bool>;")
        self.write(0, "}")
        self.write(0, "")

    def actions(self):
        # For each execution mode, add a corresponding function which
        # implements various traits. The trait list is used for filtering which
        # function is added in the generated code.
        for mode, traits in self.parse_table.exec_modes.items():
            action_writer = RustActionWriter(self, traits, 4)
            self.write(0,
                       "pub fn {}<'alloc, Handler>(parser: &mut Handler, state: usize) "
                       "-> Result<'alloc, bool>",
                       mode)
            self.write(0, "where")
            self.write(1, "Handler: {}", ' + '.join(map(self.type_to_rust, traits)))
            self.write(0, "{")
            self.write(1, "let mut state = state;")
            self.write(1, "loop {")
            self.write(2, "match state {")
            assert len(self.states[self.shift_count:]) == self.action_count
            for state in self.states[self.shift_count:]:
                self.write(3, "{} => {{", state.index)
                action_writer.write_state_transitions(state)
                self.write(3, "}")
            self.write(3, '_ => panic!("no such state: {}", state),')
            self.write(2, "}")
            self.write(1, "}")
            self.write(0, "}")
            self.write(0, "")

    def entry(self):
        self.write(0, "#[derive(Clone, Copy)]")
        self.write(0, "pub struct ParseTable<'a> {")
        self.write(1, "pub shift_count: usize,")
        self.write(1, "pub action_count: usize,")
        self.write(1, "pub shift_table: &'a [i64],")
        self.write(1, "pub shift_width: usize,")
        self.write(1, "pub error_codes: &'a [Option<ErrorCode>],")
        self.write(0, "}")
        self.write(0, "")

        self.write(0, "impl<'a> ParseTable<'a> {")
        self.write(1, "pub fn check(&self) {")
        self.write(2, "assert_eq!(")
        self.write(3, "self.shift_table.len(),")
        self.write(3, "(self.shift_count * self.shift_width) as usize")
        self.write(2, ");")
        self.write(1, "}")
        self.write(0, "}")
        self.write(0, "")

        self.write(0, "pub static TABLES: ParseTable<'static> = ParseTable {")
        self.write(1, "shift_count: {},", self.shift_count)
        self.write(1, "action_count: {},", self.action_count)
        self.write(1, "shift_table: &SHIFT,")
        self.write(1, "shift_width: {},", len(self.terminals) + len(self.nonterminals))
        self.write(1, "error_codes: &STATE_TO_ERROR_CODE,")
        self.write(0, "};")
        self.write(0, "")

        for init_nt, index in self.init_state_map:
            assert init_nt.args == ()
            self.write(0, "pub static START_STATE_{}: usize = {};",
                       self.nonterminal_to_snake(init_nt).upper(), index)
            self.write(0, "")


def write_rust_parse_table(out, parse_table, handler_info):
    if not handler_info:
        print("WARNING: info.json is not provided", file=sys.stderr)
        fallible_methods = []
    else:
        with open(handler_info, "r") as json_file:
            handler_info_json = json.load(json_file)
        fallible_methods = handler_info_json["fallible-methods"]

    RustParserWriter(out, parse_table, fallible_methods).emit()
