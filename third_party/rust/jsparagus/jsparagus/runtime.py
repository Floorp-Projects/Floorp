"""Runtime support for jsparagus-generated parsers."""

# Nt is unused here, but we re-export it.
from .grammar import Nt, InitNt, End
from .lexer import UnexpectedEndError
import collections
from dataclasses import dataclass


__all__ = ['ACCEPT', 'ERROR', 'Nt', 'InitNt', 'End', 'Parser', 'ErrorToken']

# Actions are encoded as 64-bit signed integers, with the following meanings:
# - n in range(0, 0x8000_0000_0000_0000) - shift to state n
# - n in range(0x8000_0000_0000_0000, 0xc000_0000_0000_0000) - call special_case(n & SPECIAL_CASE_MASK)
# - n == ERROR (0xbfff_ffff_ffff_fffe)
# - n == ACCEPT (0xbfff_ffff_ffff_ffff)
# - n in range(0xc000_0000_0000_0000, 0x1_0000_0000_0000_0000) - reduce by production -n - 1

SPECIAL_CASE_MASK = 0x3fff_ffff_ffff_ffff
SPECIAL_CASE_TAG = -0x8000_0000_0000_0000
ACCEPT = 0x_bfff_ffff_ffff_ffff - (1 << 64)
ERROR = ACCEPT - 1


@dataclass(frozen=True)
class ErrorTokenClass:
    def __repr__(self):
        return 'ErrorToken'


ErrorToken = ErrorTokenClass()


def throw_syntax_error(actions, state, t, tokens):
    assert t is not None
    if isinstance(state, StateTermValue):
        state = state.state
    expected = set(actions[state].keys())
    expected = set(e for e in expected if not isinstance(e, Nt))

    # Tidy up the `expected` set a bit.
    if End() in expected:
        expected.remove(End())
        expected.add("end of input")
    if ErrorToken in expected:
        # This is possible because we restore the stack in _try_error_handling
        # after reducing and then failing to find a recovery rule after all.
        # But don't tell people in error messages that an error is one of the
        # things we expect. It makes no sense.
        expected.remove(ErrorToken)

    if len(expected) < 2:
        tokens.throw("expected {!r}, got {!r}".format(list(expected)[0], t))
    else:
        tokens.throw("expected one of {!r}, got {!r}"
                     .format(sorted(expected), t))


StateTermValue = collections.namedtuple("StateTermValue", "state term value new_line")


class ShiftError(Exception):
    pass


class ShiftAccept(Exception):
    pass


class Parser:
    """Parser using jsparagus-generated tables.

    The usual design is, a parser object consumes a token iterator.
    This Parser is not like that. Instead, the lexer feeds tokens to it
    by calling `parser.write_terminal(lexer, token)` repeatedly, then
    `parser.close(lexer)`.

    The parser uses these methods of the lexer object:

    *   lexer.take() - Return data associated with a token, like the
        numeric value of an int literal token.

    *   lexer.throw(message) - Throw a syntax error. (This is on the lexer
        because the lexer has the current position.)

    *   lexer.throw_unexpected_end() - Throw a syntax error after we
        successfully parsed the whole file except more tokens were expected at
        the end.

    """

    def __init__(self, actions, error_codes, entry_state, methods):
        self.actions = actions
        self.stack = [StateTermValue(entry_state, None, None, False)]
        self.replay = []
        self.flags = collections.defaultdict(lambda: [])
        self.error_codes = error_codes
        self.methods = methods
        self.closed = False
        self.debug = False
        self.is_simulator = False
        self.last_shift = None

    def clone(self):
        return Parser(self.actions, self.error_codes, 0, self.methods)

    def simulator_clone(self):
        """Make a copy of this parser for simulation.

        The copy has a version of the self.reductions table that never actually
        does anything.

        This is absurdly expensive and is for very odd and special use cases.
        """
        p = self.clone()
        p.stack = self.stack[:]
        p.replay = self.replay[:]
        p.debug = self.debug
        p.is_simulator = True
        return p

    def _str_stv(self, stv):
        # NOTE: replace this function by repr(), to inspect wrong computations.
        val = ''
        if stv.value:
            val = '*'
        return "-- {} {}--> {}".format(stv.term, val, stv.state)

    def _dbg_where(self, t=""):
        name = "stack"
        if self.is_simulator:
            name = "simulator"
        print("{}: {}; {}\nexpect one of: {}".format(
            name,
            " ".join(self._str_stv(s) for s in self.stack), t,
            repr(self.actions[self.stack[-1].state])
        ))

    def _shift(self, stv, lexer):
        state = self.stack[-1].state
        if self.debug:
            self._dbg_where("shift: {}".format(str(stv.term)))
        if not isinstance(self.actions[state], dict):
            # This happens after raising a ShiftAccept error.
            if stv.term == End():
                raise ShiftAccept()
            raise ShiftError()
        self.last_shift = (state, stv)
        while True:
            goto = self.actions[state].get(stv.term, ERROR)
            if goto == ERROR:
                if self.debug:
                    self._dbg_where("(error)")
                self._try_error_handling(lexer, stv)
                stv = self.replay.pop()
                if self.debug:
                    self._dbg_where("error: {}".format(str(stv.term)))
                continue
            state = goto
            self.stack.append(StateTermValue(state, stv.term, stv.value, stv.new_line))
            action = self.actions[state]
            if not isinstance(action, dict):  # Action
                if self.debug:
                    self._dbg_where("(action {})".format(state))
                action(self, lexer)
                state = self.stack[-1].state
                action = self.actions[state]
                # Actions should always unwind or do an epsilon transition to a
                # shift state.
                assert isinstance(action, dict)
            if self.replay != []:
                stv = self.replay.pop()
                if self.debug:
                    self._dbg_where("replay: {}".format(repr(stv.term)))
            else:
                break

    def shift_list(self, stv_list, lexer):
        self.replay.extend(reversed(stv_list))

    def write_terminal(self, lexer, t):
        assert not self.closed
        try:
            stv = StateTermValue(0, t, lexer.take(), lexer.saw_line_terminator())
            self._shift(stv, lexer)
        except ShiftAccept:
            if self.debug:
                self._dbg_where("(write_terminal accept)")
            if self.replay != []:
                state, stv = self.last_shift
                throw_syntax_error(self.actions, state, lexer.take(), lexer)
        except ShiftError:
            state, stv = self.last_shift
            throw_syntax_error(self.actions, state, lexer.take(), lexer)

    def close(self, lexer):
        assert not self.closed
        self.closed = True
        try:
            self._shift(StateTermValue(0, End(), End(), False), lexer)
        except ShiftAccept:
            if self.debug:
                self._dbg_where("(close accept)")
                print(repr(self.stack))
            while self.stack[-1].term == End():
                self.stack.pop()
            assert len(self.stack) == 2
            assert self.stack[0].term is None
            assert isinstance(self.stack[1].term, Nt)
            return self.stack[1].value

    def check_not_on_new_line(self, lexer, peek):
        if peek <= 0:
            raise ValueError("check_not_on_new_line got an impossible peek offset")
        if not self.stack[-peek].new_line:
            return True
        for _ in range(peek - 1):
            self.replay.append(self.stack.pop())
        stv = self.stack.pop()
        self._try_error_handling(lexer, stv)
        return False

    def _try_error_handling(self, lexer, stv):
        # Error recovery version of the code in write_terminal. Three differences
        # between this and write_terminal are commented below.
        if stv.term is ErrorToken:
            if stv.value == End():
                lexer.throw_unexpected_end()
                raise
            throw_syntax_error(self.actions, self.stack[-1], stv.value, lexer)
            raise

        state = self.stack[-1].state
        error_code = self.error_codes[state]
        if error_code is not None:
            self.on_recover(error_code, lexer, stv)
            self.replay.append(stv)
            self.replay.append(StateTermValue(0, ErrorToken, stv.value, stv.new_line))
        elif stv.term == End():
            lexer.throw_unexpected_end()
            raise
        else:
            throw_syntax_error(self.actions, self.stack[-1], stv.value, lexer)
            raise

    def on_recover(self, error_code, lexer, stv):
        """Called when the grammar says to recover from a parse error.

        Subclasses can override this to add custom code when an ErrorSymbol in
        a production is matched. This base-class implementation does nothing,
        allowing the parser to recover from the error silently.
        """
        pass

    def can_accept_terminal(self, lexer, t):
        """Return True if the terminal `t` is OK next.

        False if it's an error. `t` can be None, querying if we can accept
        end-of-input.
        """
        class BogusLexer:
            def throw_unexpected_end(self):
                raise UnexpectedEndError("")

            def throw(self, message):
                raise SyntaxError(message)

            def take(self):
                return str(t)

            def saw_line_terminator(self):
                return lexer.saw_line_terminator()

        sim = self.simulator_clone()
        try:
            sim.write_terminal(BogusLexer(), t)
        except Exception:
            return False
        return True

    def can_close(self):
        """Return True if self.close() would succeed."""

        # The easy case: no error, parsing just succeeds.
        # The hard case: maybe error-handling would succeed?
        # The easiest thing is simply to run the method.
        class BogusLexer:
            def throw_unexpected_end(self):
                raise UnexpectedEndError("")

            def throw(self, message):
                raise SyntaxError(message)

        sim = self.simulator_clone()
        try:
            sim.close(BogusLexer())
        except SyntaxError:
            return False
        return True

    def simulate(self, t):
        """Simulate receiving the terminal `t` without modifying parser state.

        Walk the current stack to simulate the reduce actions that would occur
        if the next token from the lexer was `t`. Return the state reached when
        we're done reducing.
        """
        sim = self.simulator_clone()
        stack = self.stack
        sp = len(stack) - 1
        state = stack[sp]
        while True:
            action = self.actions[state].get(t, ERROR)
            if ACCEPT < action < 0:  # reduce
                tag_name, n, _reducer = self.reductions[-action - 1]
                sp -= 2 * n
                state = stack[sp]
                sp += 2
                state = self.ctns[state][tag_name]
            else:
                return state
