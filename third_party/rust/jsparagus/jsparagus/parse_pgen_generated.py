# type: ignore

from jsparagus import runtime
from jsparagus.runtime import (Nt, InitNt, End, ErrorToken, StateTermValue,
                               ShiftError, ShiftAccept)

def state_43_actions(parser, lexer):

    value = None
    value = parser.stack[-1].value
    replay = [StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_44_actions(parser, lexer):

    value = None
    value = parser.methods.nt_defs_single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('nt_defs'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_45_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('token_defs'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_46_actions(parser, lexer):

    value = None
    value = parser.methods.nt_defs_append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('nt_defs'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_47_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('token_defs'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_48_actions(parser, lexer):

    value = None
    raise ShiftAccept()
    replay = [StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_49_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, None, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_50_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_51_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('terms'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_52_actions(parser, lexer):

    value = None
    value = parser.methods.ident(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('symbol'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_53_actions(parser, lexer):

    value = None
    value = parser.methods.str(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('symbol'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_54_actions(parser, lexer):

    value = None
    value = parser.methods.empty(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_55_actions(parser, lexer):

    value = None
    value = parser.methods.var_token(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('token_def'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_56_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, None, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_57_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_58_actions(parser, lexer):

    value = None
    value = parser.methods.prod(parser.stack[-2].value, None)
    replay = [StateTermValue(0, Nt('prod'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_59_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('terms'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_60_actions(parser, lexer):

    value = None
    value = parser.methods.optional(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('term'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_61_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-5].value, None, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_62_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, parser.stack[-5].value, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_63_actions(parser, lexer):

    value = None
    value = parser.methods.const_token(parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('token_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    return

def state_64_actions(parser, lexer):

    value = None
    value = parser.methods.prod(parser.stack[-3].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('prod'), value, False)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_65_actions(parser, lexer):

    value = None
    value = parser.stack[-1].value
    replay = [StateTermValue(0, Nt('reducer'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_66_actions(parser, lexer):

    value = None
    value = parser.methods.expr_match(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_67_actions(parser, lexer):

    value = None
    value = parser.methods.expr_none()
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_68_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, None, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    return

def state_69_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, parser.stack[-5].value, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    return

def state_70_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    return

def state_71_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-7].value, parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-7:]
    parser.shift_list(replay, lexer)
    return

def state_72_actions(parser, lexer):

    value = None
    value = parser.methods.expr_call(parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_73_actions(parser, lexer):

    value = None
    value = parser.methods.args_single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr_args'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    return

def state_74_actions(parser, lexer):

    value = None
    value = parser.methods.expr_call(parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_75_actions(parser, lexer):

    value = None
    value = parser.methods.expr_some(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    return

def state_76_actions(parser, lexer):

    value = None
    value = parser.methods.args_append(parser.stack[-3].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr_args'), value, False)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_77_actions(parser, lexer):

    value = None
    value = parser.methods.grammar(None, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('grammar'), value, False)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

def state_78_actions(parser, lexer):

    value = None
    value = parser.methods.grammar(parser.stack[-3].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('grammar'), value, False)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    return

def state_79_actions(parser, lexer):

    value = None
    value = parser.stack[-2].value
    replay = [StateTermValue(0, Nt('term'), value, False)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    return

actions = [
    # 0.

    {'nt': 2, 'COMMENT': 3, 'goal': 4, 'token': 6, 'var': 7, Nt('grammar'): 43, Nt('nt_defs'): 1, Nt('nt_def'): 44, Nt('token_defs'): 5, Nt('token_def'): 45, Nt(InitNt(goal=Nt('grammar'))): 8},

    # 1.

    {End(): 77, 'goal': 4, 'COMMENT': 3, 'nt': 2, Nt('nt_def'): 46},

    # 2.

    {'IDENT': 10},

    # 3.

    {'nt': 11, 'goal': 12},

    # 4.

    {'nt': 13},

    # 5.

    {'nt': 2, 'COMMENT': 3, 'goal': 4, 'token': 6, 'var': 7, Nt('nt_defs'): 14, Nt('nt_def'): 44, Nt('token_def'): 47},

    # 6.

    {'IDENT': 15},

    # 7.

    {'token': 16},

    # 8.

    {End(): 48},

    # 9.

    {},

    # 10.

    {'{': 17},

    # 11.

    {'IDENT': 18},

    # 12.

    {'nt': 19},

    # 13.

    {'IDENT': 20},

    # 14.

    {End(): 78, 'goal': 4, 'COMMENT': 3, 'nt': 2, Nt('nt_def'): 46},

    # 15.

    {'=': 21},

    # 16.

    {'IDENT': 22},

    # 17.

    {'}': 49, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 23, Nt('prod'): 50, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 18.

    {'{': 26},

    # 19.

    {'IDENT': 27},

    # 20.

    {'{': 28},

    # 21.

    {'STR': 29},

    # 22.

    {';': 55},

    # 23.

    {'}': 56, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 24.

    {';': 58, 'IDENT': 52, 'STR': 53, '=>': 31, Nt('term'): 59, Nt('symbol'): 25, Nt('reducer'): 30},

    # 25.

    {'=>': 79, 'STR': 79, 'IDENT': 79, ';': 79, '?': 60, Nt('reducer'): 79, Nt('symbol'): 79, Nt('term'): 79},

    # 26.

    {'}': 61, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 32, Nt('prod'): 50, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 27.

    {'{': 33},

    # 28.

    {'}': 62, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 34, Nt('prod'): 50, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 29.

    {';': 63},

    # 30.

    {';': 64},

    # 31.

    {'MATCH': 66, 'IDENT': 35, 'Some': 36, 'None': 67, Nt('expr'): 65},

    # 32.

    {'}': 68, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 33.

    {'}': 69, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 37, Nt('prod'): 50, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 34.

    {'}': 70, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 35.

    {'(': 38},

    # 36.

    {'(': 39},

    # 37.

    {'}': 71, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 24, Nt('term'): 51, Nt('symbol'): 25},

    # 38.

    {')': 72, 'MATCH': 66, 'IDENT': 35, 'Some': 36, 'None': 67, Nt('expr_args'): 40, Nt('expr'): 73},

    # 39.

    {'MATCH': 66, 'IDENT': 35, 'Some': 36, 'None': 67, Nt('expr'): 41},

    # 40.

    {')': 74, ',': 42},

    # 41.

    {')': 75},

    # 42.

    {'MATCH': 66, 'IDENT': 35, 'Some': 36, 'None': 67, Nt('expr'): 76},

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

    # 65.

    state_65_actions,

    # 66.

    state_66_actions,

    # 67.

    state_67_actions,

    # 68.

    state_68_actions,

    # 69.

    state_69_actions,

    # 70.

    state_70_actions,

    # 71.

    state_71_actions,

    # 72.

    state_72_actions,

    # 73.

    state_73_actions,

    # 74.

    state_74_actions,

    # 75.

    state_75_actions,

    # 76.

    state_76_actions,

    # 77.

    state_77_actions,

    # 78.

    state_78_actions,

    # 79.

    state_79_actions,

]

error_codes = [
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
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
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def ident(self, x0):
        return ('ident', x0)
    def str(self, x0):
        return ('str', x0)
    def empty(self, x0):
        return ('empty', x0)
    def var_token(self, x0):
        return ('var_token', x0)
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def prod(self, x0, x1):
        return ('prod', x0, x1)
    def optional(self, x0):
        return ('optional', x0)
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def const_token(self, x0, x1):
        return ('const_token', x0, x1)
    def prod(self, x0, x1):
        return ('prod', x0, x1)
    def expr_match(self, x0):
        return ('expr_match', x0)
    def expr_none(self, ):
        return ('expr_none', )
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
    def nt_def(self, x0, x1, x2, x3):
        return ('nt_def', x0, x1, x2, x3)
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

