"""Parse a grammar written in ECMArkup."""

import os
import collections
from jsparagus import parse_pgen, gen, grammar, extension, types
from jsparagus.lexer import LexicalGrammar
from jsparagus.ordered import OrderedSet, OrderedFrozenSet


ESGrammarLexer = LexicalGrammar(
    # the operators and keywords:
    "[ ] { } , ~ + ? <! = == != => ( ) @ < > ' ; "
    "but empty here lookahead no not of one or returns through Some None impl for let",

    NL="\n",

    # any number of colons together
    EQ=r':+',

    # terminals of the ES grammar, quoted with backticks
    T=r'`[^` \n]+`|```',

    # also terminals, denoting control characters
    CHR=r'<[A-Z ]+>|U\+[0-9A-f]{4}',

    # nonterminals/types that will be followed by parameters
    NTCALL=r'[A-Za-z]\w*(?=[\[<])',

    # nonterminals (also, boolean parameters and type names)
    NT=r'[A-Za-z]\w*',

    # nonterminals wrapped in vertical bars for no apparent reason
    NTALT=r'\|[A-Z]\w+\|',

    # the spec also gives a few productions names
    PRODID=r'#[A-Za-z]\w*',

    # prose not wrapped in square brackets
    # To avoid conflict with the `>` token, this is recognized only after a space.
    PROSE=r'(?<= )>[^\n]*',

    # prose wrapped in square brackets
    WPROSE=r'\[>[^]]*\]',

    # expression denoting a matched terminal or nonterminal
    MATCH_REF=r'\$(?:0|[1-9][0-9]*)',

    # the spec also gives a few productions names
    RUSTCOMMENT=r'//.*\n',
)


ESGrammarParser = gen.compile(
    parse_pgen.load_grammar(
        os.path.join(os.path.dirname(__file__), "esgrammar.pgen")))


SIGIL_FALSE = '~'
SIGIL_TRUE = '+'

# Abbreviations for single-character terminals, used in the lexical grammar.
ECMASCRIPT_CODE_POINTS = {
    # From <https://tc39.es/ecma262/#table-31>
    '<ZWNJ>': grammar.Literal('\u200c'),
    '<ZWJ>': grammar.Literal('\u200d'),
    '<ZWNBSP>': grammar.Literal('\ufeff'),

    # From <https://tc39.es/ecma262/#table-32>
    '<TAB>': grammar.Literal('\t'),
    '<VT>': grammar.Literal('\u000b'),
    '<FF>': grammar.Literal('\u000c'),
    '<SP>': grammar.Literal(' '),
    '<NBSP>': grammar.Literal('\u00a0'),
    # <ZWNBSP> already defined above
    '<USP>': grammar.UnicodeCategory('Zs'),

    # From <https://tc39.es/ecma262/#table-33>
    '<LF>': grammar.Literal('\u000a'),
    '<CR>': grammar.Literal('\u000d'),
    '<LS>': grammar.Literal('\u2028'),
    '<PS>': grammar.Literal('\u2028'),
}


class ESGrammarBuilder:
    def __init__(self, terminal_names):
        # Names of terminals that are written as nonterminals in the grammar.
        # For example, "BooleanLiteral" is a terminal name when parsing the
        # syntactic grammar.
        if terminal_names is None:
            terminal_names = frozenset()
        self.terminal_names = terminal_names
        self.reset()

    def reset(self):
        self.lexer = None
        # This is how full-parsing and lazy-parsing are implemented, using
        # different traits.
        #
        # This field contains the Rust's trait used for calling the method.
        # When a CallMethod is generated, it is assumed to be a function of
        # this trait. The trait is used by the Rust backend to generate
        # multiple backends which are implementing different set of traits.
        # Having the trait on the function call is useful as a way to filter
        # functions calls at code-generation time.
        #
        # This field is updated by the `rust_param_impl`, which is used in
        # grammar extensions, and visited before producing any CallMethod.
        self.method_trait = "AstBuilder"

    def rust_edsl(self, impl, grammar):
        return extension.GrammarExtension(impl, grammar, self.lexer.filename)

    def rust_param_impl(self, trait, for_type, param):
        self.method_trait = trait
        return extension.ImplFor(param, trait, for_type)

    def rust_impl(self, trait, impl_type):
        return self.rust_param_impl(trait, impl_type, [])

    def rust_nt_def(self, lhs, rhs_line):
        # Right now, only focus on the syntactic grammar, and assume that all
        # rules are patching existing grammar production by adding code.
        return extension.ExtPatch(self.nt_def(None, lhs, ':', [rhs_line]))

    def rust_rhs_line(self, symbols):
        return self.rhs_line(None, symbols, None, None)

    def rust_expr(self, expr):
        assert isinstance(expr, grammar.CallMethod)
        return expr

    def empty(self):
        return []

    def single(self, x):
        return [x]

    def append(self, x, y):
        return x + [y]

    def concat(self, x, y):
        return x + y

    def blank_line(self):
        return []

    def nt_def_to_list(self, nt_def):
        return [nt_def]

    def to_production(self, lhs, i, rhs, is_sole_production):
        """Wrap a list of grammar symbols `rhs` in a Production object."""
        body, reducer, condition = rhs
        if reducer is None:
            reducer = self.default_reducer(lhs, i, body, is_sole_production)
        return grammar.Production(body, reducer, condition=condition)

    def default_reducer(self, lhs, i, body, is_sole_production):
        assert isinstance(lhs, grammar.Nt)
        nt_name = lhs.name

        nargs = sum(1 for e in body if grammar.is_concrete_element(e))
        if is_sole_production:
            method_name = nt_name
        else:
            method_name = '{} {}'.format(nt_name, i)
        return self.expr_call(method_name, tuple(range(nargs)), None)

    def needs_asi(self, lhs, p):
        """True if p is a production in which ASI can happen."""
        # The purpose of the fake ForLexicalDeclaration production is to have a
        # copy of LexicalDeclaration that does not trigger ASI.
        #
        # Two productions have body == [";"] -- one for EmptyStatement and one
        # for ClassMember. Neither should trigger ASI.
        #
        # The only other semicolons that should not trigger ASI are the ones in
        # `for` statement productions, which happen to be exactly those
        # semicolons that are not at the end of a production.
        return (not (isinstance(lhs, grammar.Nt)
                     and lhs.name == 'ForLexicalDeclaration')
                and len(p.body) > 1
                and p.body[-1] == ';')

    def apply_asi(self, p, reducer_was_autogenerated):
        """Return two rules based on p, so that ASI can be applied."""
        assert isinstance(p.reducer, grammar.CallMethod)

        if reducer_was_autogenerated:
            # Don't pass the semicolon to the method.
            reducer = self.expr_call(p.reducer.method,
                                     p.reducer.args[:-1],
                                     None)
        else:
            reducer = p.reducer

        # Except for do-while loops, check at runtime that ASI occurs only at
        # the end of a line.
        if (len(p.body) == 7
                and p.body[0] == 'do'
                and p.body[2] == 'while'
                and p.body[3] == '('
                and p.body[5] == ')'
                and p.body[6] == ';'):
            code = "do_while_asi"
        else:
            code = "asi"

        return [
            # The preferred production, with the semicolon in.
            p.copy_with(body=p.body[:],
                        reducer=reducer),
            # The fallback production, performing ASI.
            p.copy_with(body=p.body[:-1] + [grammar.ErrorSymbol(code)],
                        reducer=reducer),
        ]

    def expand_lexical_rhs(self, rhs):
        body, reducer, condition = rhs
        out = []
        for e in body:
            if isinstance(e, str):
                # The terminal symbols of the lexical grammar are characters, so
                # add each character of this string as a separate element.
                out += [grammar.Literal(ch) for ch in e]
            else:
                out.append(e)
        return [out, reducer, condition]

    def nt_def(self, nt_type, lhs, eq, rhs_list):
        has_sole_production = (len(rhs_list) == 1)
        production_list = []
        for i, rhs in enumerate(rhs_list):
            if eq == ':':
                # Syntactic grammar. A hack is needed for ASI.
                reducer_was_autogenerated = rhs[1] is None
                p = self.to_production(lhs, i, rhs, has_sole_production)
                if self.needs_asi(lhs, p):
                    production_list += self.apply_asi(p, reducer_was_autogenerated)
                else:
                    production_list.append(p)
            elif eq == '::':
                # Lexical grammar. A hack is needed to replace multicharacter
                # terminals like `!==` into sequences of character terminals.
                rhs = self.expand_lexical_rhs(rhs)
                p = self.to_production(lhs, i, rhs, has_sole_production)
                production_list.append(p)
        return (lhs.name, eq, grammar.NtDef(lhs.args, production_list, nt_type))

    def nt_def_one_of(self, nt_type, nt_lhs, eq, terminals):
        return self.nt_def(nt_type, nt_lhs, eq, [([t], None, None) for t in terminals])

    def nt_lhs_no_params(self, name):
        return grammar.Nt(name, ())

    def nt_lhs_with_params(self, name, params):
        return grammar.Nt(name, tuple(params))

    def simple_type(self, name):
        return types.Type(name)

    def lifetime_type(self, name):
        return types.Lifetime(name)

    def parameterized_type(self, name, args):
        return types.Type(name, tuple(args))

    def t_list_line(self, terminals):
        return terminals

    def terminal(self, t):
        assert t[0] == "`"
        assert t[-1] == "`"
        return t[1:-1]

    def terminal_chr(self, chr):
        raise ValueError("FAILED: %r" % chr)

    def rhs_line(self, ifdef, rhs, reducer, _prodid):
        return (rhs, reducer, ifdef)

    def rhs_line_prose(self, prose):
        return ([prose], None, None)

    def empty_rhs(self):
        return []

    def expr_match_ref(self, token):
        assert token.startswith('$')
        return int(token[1:])

    def expr_call(self, method, args, fallible):
        # NOTE: Currently "AstBuilder" functions are made fallible using the
        # fallible_methods taken from some Rust code which extract this
        # information to produce a JSON file.
        if self.method_trait == "AstBuilder":
            fallible = None
        return grammar.CallMethod(method, args or (), types.Type(self.method_trait),
                                  fallible is not None)

    def expr_some(self, expr):
        return grammar.Some(expr)

    def expr_none(self):
        return None

    def ifdef(self, value, nt):
        return nt, value

    def optional(self, nt):
        return grammar.Optional(nt)

    def but_not(self, nt, exclusion):
        _, exclusion = exclusion
        return grammar.Exclude(nt, [exclusion])
        # return ('-', nt, exclusion)

    def but_not_one_of(self, nt, exclusion_list):
        exclusion_list = [exclusion for _, exclusion in exclusion_list]
        return grammar.Exclude(nt, exclusion_list)
        # return ('-', nt, exclusion_list)

    def no_line_terminator_here(self, lt):
        if lt not in ('LineTerminator', '|LineTerminator|'):
            raise ValueError("unrecognized directive " + repr("[no " + lt + " here]"))
        return grammar.NoLineTerminatorHere

    def nonterminal(self, name):
        if name in self.terminal_names:
            return name
        return grammar.Nt(name, ())

    def nonterminal_apply(self, name, args):
        if name in self.terminal_names:
            raise ValueError("parameters applied to terminal {!r}".format(name))
        if len(set(k for k, expr in args)) != len(args):
            raise ValueError("parameter passed multiple times")
        return grammar.Nt(name, tuple(args))

    def arg_expr(self, sigil, argname):
        if sigil == '?':
            return (argname, grammar.Var(argname))
        else:
            return (argname, sigil)

    def sigil_false(self):
        return False

    def sigil_true(self):
        return True

    def exclusion_terminal(self, t):
        return ("t", t)

    def exclusion_nonterminal(self, nt):
        return ("nt", nt)

    def exclusion_chr_range(self, c1, c2):
        return ("range", c1, c2)

    def la_eq(self, t):
        return grammar.LookaheadRule(OrderedFrozenSet([t]), True)

    def la_ne(self, t):
        return grammar.LookaheadRule(OrderedFrozenSet([t]), False)

    def la_not_in_nonterminal(self, nt):
        return grammar.LookaheadRule(OrderedFrozenSet([nt]), False)

    def la_not_in_set(self, lookahead_exclusions):
        if all(len(excl) == 1 for excl in lookahead_exclusions):
            return grammar.LookaheadRule(
                OrderedFrozenSet(excl[0] for excl in lookahead_exclusions),
                False)
        raise ValueError("unsupported: lookahead > 1 token, {!r}"
                         .format(lookahead_exclusions))

    def chr(self, t):
        assert t[0] == "<" or t[0] == 'U'
        if t[0] == "<":
            assert t[-1] == ">"
            if t not in ECMASCRIPT_CODE_POINTS:
                raise ValueError("unrecognized character abbreviation {!r}".format(t))
            return ECMASCRIPT_CODE_POINTS[t]
        else:
            assert t[1] == "+"
            return grammar.Literal(chr(int(t[2:], base=16)))


def finish_grammar(nt_defs, goals, variable_terminals, synthetic_terminals,
                   single_grammar=True, extensions=[]):
    nt_grammars = {}
    for nt_name, eq, _ in nt_defs:
        if nt_name in nt_grammars:
            raise ValueError(
                "duplicate definitions for nonterminal {!r}"
                .format(nt_name))
        nt_grammars[nt_name] = eq

    # Figure out which grammar we were trying to get (":" for syntactic,
    # "::" for lexical) based on the goal symbols.
    goals = list(goals)
    if len(goals) == 0:
        raise ValueError("no goal nonterminals specified")
    if single_grammar:
        selected_grammars = set(nt_grammars[goal] for goal in goals)
        assert len(selected_grammars) != 0
        if len(selected_grammars) > 1:
            raise ValueError(
                "all goal nonterminals must be part of the same grammar; "
                "got {!r} (matching these grammars: {!r})"
                .format(set(goals), set(selected_grammars)))
        [selected_grammar] = selected_grammars

    terminal_set = set()

    def hack_production(p):
        for i, e in enumerate(p.body):
            if isinstance(e, str) and e[:1] == "`":
                if len(e) < 3 or e[-1:] != "`":
                    raise ValueError(
                        "Unrecognized grammar symbol: {!r} (in {!r})"
                        .format(e, p))
                p[i] = token = e[1:-1]
                terminal_set.add(token)

    nonterminals = {}
    for nt_name, eq, rhs_list_or_lambda in nt_defs:
        if single_grammar and eq != selected_grammar:
            continue

        if isinstance(rhs_list_or_lambda, grammar.NtDef):
            nonterminals[nt_name] = rhs_list_or_lambda
        else:
            rhs_list = rhs_list_or_lambda
            for p in rhs_list:
                if not isinstance(p, grammar.Production):
                    raise ValueError(
                        "invalid grammar: ifdef in non-function-call context")
                hack_production(p)
            if nt_name in nonterminals:
                raise ValueError(
                    "unsupported: multiple definitions for nt " + nt_name)
            nonterminals[nt_name] = rhs_list

    for t in terminal_set:
        if t in nonterminals:
            raise ValueError(
                "grammar contains both a terminal `{}` and nonterminal {}"
                .format(t, t))

    # Add execution modes to generate the various functions needed to handle
    # syntax parsing and full parsing execution modes.
    exec_modes = collections.defaultdict(OrderedSet)
    noop_parser = types.Type("ParserTrait", (types.Lifetime("alloc"), types.UnitType))
    token_parser = types.Type("ParserTrait", (
        types.Lifetime("alloc"), types.Type("StackValue", (types.Lifetime("alloc"),))))
    ast_builder = types.Type("AstBuilderDelegate", (types.Lifetime("alloc"),))

    # Full parsing takes token as input and build an AST.
    exec_modes["full_actions"].extend([token_parser, ast_builder])

    # Syntax parsing takes token as input but skip building the AST.
    # TODO: The syntax parser is commented out for now, as we need something to
    # be produced when we cannot call the AstBuilder for producing the values.

    # No-op parsing is used for the simulator, which is so far used for
    # querying whether we can end the incremental input and lookup if a state
    # can accept some kind of tokens.
    exec_modes["noop_actions"].add(noop_parser)

    # Extensions are using an equivalent of Rust types to define the kind of
    # parsers to be used, this map is used to convert these type names to the
    # various execution modes.
    full_parser = types.Type("FullParser")
    syntax_parser = types.Type("SyntaxParser")
    noop_parser = types.Type("NoopParser")
    type_to_modes = {
        noop_parser: ["noop_actions", "full_actions"],
        syntax_parser: ["full_actions"],
        full_parser: ["full_actions"],
    }

    result = grammar.Grammar(
        nonterminals,
        goal_nts=goals,
        variable_terminals=variable_terminals,
        synthetic_terminals=synthetic_terminals,
        exec_modes=exec_modes,
        type_to_modes=type_to_modes)
    result.patch(extensions)
    return result


def parse_esgrammar(
        text,
        *,
        filename=None,
        extensions=[],
        goals=None,
        terminal_names=None,
        synthetic_terminals=None,
        single_grammar=True):
    builder = ESGrammarBuilder(terminal_names)
    parser = ESGrammarParser(builder=builder, goal="grammar")
    lexer = ESGrammarLexer(parser, filename=filename)
    lexer.write(text)
    nt_defs = lexer.close()
    grammar_extensions = []
    for ext_filename, start_lineno, content in extensions:
        builder.reset()
        parser = ESGrammarParser(builder=builder, goal="rust_edsl")
        lexer = ESGrammarLexer(parser, filename=ext_filename)
        builder.lexer = lexer
        lexer.start_lineno = start_lineno
        lexer.write(content)
        result = lexer.close()
        grammar_extensions.append(result)

    return finish_grammar(
        nt_defs,
        goals=goals,
        variable_terminals=set(terminal_names) - set(synthetic_terminals),
        synthetic_terminals=synthetic_terminals,
        single_grammar=single_grammar,
        extensions=grammar_extensions)
