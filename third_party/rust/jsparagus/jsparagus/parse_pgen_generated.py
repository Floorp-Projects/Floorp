from jsparagus import runtime
from jsparagus.runtime import Nt, InitNt, End, ErrorToken, StateTermValue, ShiftError, ShiftAccept

def state_33_actions(parser, lexer):

    value = None
    value = parser.stack[-1].value
    replay = [StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_34_actions(parser, lexer):

    value = None
    value = parser.methods.nt_defs_single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('nt_defs'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_35_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('token_defs'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_36_actions(parser, lexer):

    value = None
    value = parser.methods.nt_defs_append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('nt_defs'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_37_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('token_defs'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_38_actions(parser, lexer):

    value = None
    raise ShiftAccept()
    replay = [StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_39_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_40_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_41_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('terms'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_42_actions(parser, lexer):

    value = None
    value = parser.methods.ident(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('symbol'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_43_actions(parser, lexer):

    value = None
    value = parser.methods.str(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('symbol'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_44_actions(parser, lexer):

    value = None
    value = parser.methods.var_token(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('token_def'), value)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_45_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_46_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_47_actions(parser, lexer):

    value = None
    value = parser.methods.prod(parser.stack[-2].value, None)
    replay = [StateTermValue(0, Nt('prod'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_48_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('terms'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_49_actions(parser, lexer):

    value = None
    value = parser.methods.optional(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('term'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_50_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-5].value, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_51_actions(parser, lexer):

    value = None
    value = parser.methods.const_token(parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('token_def'), value)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_52_actions(parser, lexer):

    value = None
    value = parser.methods.prod(parser.stack[-3].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('prod'), value)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_53_actions(parser, lexer):

    value = None
    value = parser.stack[-1].value
    replay = [StateTermValue(0, Nt('reducer'), value)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_54_actions(parser, lexer):

    value = None
    value = parser.methods.expr_match(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_55_actions(parser, lexer):

    value = None
    value = parser.methods.expr_none()
    replay = [StateTermValue(0, Nt('expr'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_56_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    return

def state_57_actions(parser, lexer):

    value = None
    value = parser.methods.expr_call(parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('expr'), value)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_58_actions(parser, lexer):

    value = None
    value = parser.methods.args_single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr_args'), value)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_59_actions(parser, lexer):

    value = None
    value = parser.methods.expr_call(parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('expr'), value)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_60_actions(parser, lexer):

    value = None
    value = parser.methods.expr_some(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('expr'), value)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_61_actions(parser, lexer):

    value = None
    value = parser.methods.args_append(parser.stack[-3].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr_args'), value)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_62_actions(parser, lexer):

    value = None
    value = parser.methods.grammar(None, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('grammar'), value)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_63_actions(parser, lexer):

    value = None
    value = parser.methods.grammar(parser.stack[-3].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('grammar'), value)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_64_actions(parser, lexer):

    value = None
    value = parser.stack[-2].value
    replay = [StateTermValue(0, Nt('term'), value)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

actions = [
    # 0.

    {'nt': 2, 'goal': 3, 'token': 5, 'var': 6, Nt('grammar'): 33, Nt('nt_defs'): 1, Nt('nt_def'): 34, Nt('token_defs'): 4, Nt('token_def'): 35, Nt(InitNt(goal=Nt('grammar'))): 7},

    # 1.

    {End(): 62, 'goal': 3, 'nt': 2, Nt('nt_def'): 36},

    # 2.

    {'IDENT': 9},

    # 3.

    {'nt': 10},

    # 4.

    {'nt': 2, 'goal': 3, 'token': 5, 'var': 6, Nt('nt_defs'): 11, Nt('nt_def'): 34, Nt('token_def'): 37},

    # 5.

    {'IDENT': 12},

    # 6.

    {'token': 13},

    # 7.

    {End(): 38},

    # 8.

    {},

    # 9.

    {'{': 14},

    # 10.

    {'IDENT': 15},

    # 11.

    {End(): 63, 'goal': 3, 'nt': 2, Nt('nt_def'): 36},

    # 12.

    {'=': 16},

    # 13.

    {'IDENT': 17},

    # 14.

    {'}': 39, 'IDENT': 42, 'STR': 43, Nt('prods'): 18, Nt('prod'): 40, Nt('terms'): 19, Nt('term'): 41, Nt('symbol'): 20},

    # 15.

    {'{': 21},

    # 16.

    {'STR': 22},

    # 17.

    {';': 44},

    # 18.

    {'}': 45, 'IDENT': 42, 'STR': 43, Nt('prod'): 46, Nt('terms'): 19, Nt('term'): 41, Nt('symbol'): 20},

    # 19.

    {';': 47, 'IDENT': 42, 'STR': 43, '=>': 24, Nt('term'): 48, Nt('symbol'): 20, Nt('reducer'): 23},

    # 20.

    {'=>': 64, 'STR': 64, 'IDENT': 64, ';': 64, '?': 49, Nt('reducer'): 64, Nt('symbol'): 64, Nt('term'): 64},

    # 21.

    {'}': 50, 'IDENT': 42, 'STR': 43, Nt('prods'): 25, Nt('prod'): 40, Nt('terms'): 19, Nt('term'): 41, Nt('symbol'): 20},

    # 22.

    {';': 51},

    # 23.

    {';': 52},

    # 24.

    {'MATCH': 54, 'IDENT': 26, 'Some': 27, 'None': 55, Nt('expr'): 53},

    # 25.

    {'}': 56, 'IDENT': 42, 'STR': 43, Nt('prod'): 46, Nt('terms'): 19, Nt('term'): 41, Nt('symbol'): 20},

    # 26.

    {'(': 28},

    # 27.

    {'(': 29},

    # 28.

    {')': 57, 'MATCH': 54, 'IDENT': 26, 'Some': 27, 'None': 55, Nt('expr_args'): 30, Nt('expr'): 58},

    # 29.

    {'MATCH': 54, 'IDENT': 26, 'Some': 27, 'None': 55, Nt('expr'): 31},

    # 30.

    {')': 59, ',': 32},

    # 31.

    {')': 60},

    # 32.

    {'MATCH': 54, 'IDENT': 26, 'Some': 27, 'None': 55, Nt('expr'): 61},

    # 33.

    state_33_actions,

    # 34.

    state_34_actions,

    # 35.

    state_35_actions,

    # 36.

    state_36_actions,

    # 37.

    state_37_actions,

    # 38.

    state_38_actions,

    # 39.

    state_39_actions,

    # 40.

    state_40_actions,

    # 41.

    state_41_actions,

    # 42.

    state_42_actions,

    # 43.

    state_43_actions,

    # 44.

    state_44_actions,

    # 45.

    state_45_actions,

    # 46.

    state_46_actions,

    # 47.

    state_47_actions,

    # 48.

    state_48_actions,

    # 49.

    state_49_actions,

    # 50.

    state_50_actions,

    # 51.

    state_51_actions,

    # 52.

    state_52_actions,

    # 53.

    state_53_actions,

    # 54.

    state_54_actions,

    # 55.

    state_55_actions,

    # 56.

    state_56_actions,

    # 57.

    state_57_actions,

    # 58.

    state_58_actions,

    # 59.

    state_59_actions,

    # 60.

    state_60_actions,

    # 61.

    state_61_actions,

    # 62.

    state_62_actions,

    # 63.

    state_63_actions,

    # 64.

    state_64_actions,

]

error_codes = [
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None,
]

goal_nt_to_init_state = {'grammar': 0}

class DefaultMethods:
    def nt_defs_single(self, x0):
        return ('nt_defs_single', x0)
    def single(self, x0):
        return ('single', x0)
    def nt_defs_append(self, x0, x1):
        return ('nt_defs_append', x0, x1)
    def append(self, x0, x1):
        return ('append', x0, x1)
    def nt_def(self, x0, x1, x2):
        return ('nt_def', x0, x1, x2)
    def ident(self, x0):
        return ('ident', x0)
    def str(self, x0):
        return ('str', x0)
    def var_token(self, x0):
        return ('var_token', x0)
    def nt_def(self, x0, x1, x2):
        return ('nt_def', x0, x1, x2)
    def prod(self, x0, x1):
        return ('prod', x0, x1)
    def optional(self, x0):
        return ('optional', x0)
    def nt_def(self, x0, x1, x2):
        return ('nt_def', x0, x1, x2)
    def const_token(self, x0, x1):
        return ('const_token', x0, x1)
    def prod(self, x0, x1):
        return ('prod', x0, x1)
    def expr_match(self, x0):
        return ('expr_match', x0)
    def expr_none(self, ):
        return ('expr_none', )
    def nt_def(self, x0, x1, x2):
        return ('nt_def', x0, x1, x2)
    def expr_call(self, x0, x1):
        return ('expr_call', x0, x1)
    def args_single(self, x0):
        return ('args_single', x0)
    def expr_call(self, x0, x1):
        return ('expr_call', x0, x1)
    def expr_some(self, x0):
        return ('expr_some', x0)
    def args_append(self, x0, x1):
        return ('args_append', x0, x1)
    def grammar(self, x0, x1):
        return ('grammar', x0, x1)
    def grammar(self, x0, x1):
        return ('grammar', x0, x1)

class Parser(runtime.Parser):
    def __init__(self, goal='grammar', builder=None):
        if builder is None:
            builder = DefaultMethods()
        super().__init__(actions, error_codes, goal_nt_to_init_state[goal], builder)

