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
    state_86_actions(parser, lexer)
    return

def state_44_actions(parser, lexer):

    value = None
    value = parser.methods.nt_defs_single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('nt_defs'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_84_actions(parser, lexer)
    return

def state_45_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('token_defs'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_85_actions(parser, lexer)
    return

def state_46_actions(parser, lexer):

    value = None
    value = parser.methods.nt_defs_append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('nt_defs'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_84_actions(parser, lexer)
    return

def state_47_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('token_defs'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_85_actions(parser, lexer)
    return

def state_48_actions(parser, lexer):

    value = None
    raise ShiftAccept()
    replay = [StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_86_actions(parser, lexer)
    return

def state_49_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, None, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_50_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_98_actions(parser, lexer)
    return

def state_51_actions(parser, lexer):

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('terms'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_101_actions(parser, lexer)
    return

def state_52_actions(parser, lexer):

    value = None
    value = parser.methods.ident(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('symbol'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_95_actions(parser, lexer)
    return

def state_53_actions(parser, lexer):

    value = None
    value = parser.methods.str(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('symbol'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_95_actions(parser, lexer)
    return

def state_54_actions(parser, lexer):

    value = None
    value = parser.methods.empty(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_98_actions(parser, lexer)
    return

def state_55_actions(parser, lexer):

    value = None
    value = parser.methods.var_token(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('token_def'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    state_104_actions(parser, lexer)
    return

def state_56_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, None, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_57_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('prods'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_98_actions(parser, lexer)
    return

def state_58_actions(parser, lexer):

    value = None
    value = parser.methods.prod(parser.stack[-2].value, None)
    replay = [StateTermValue(0, Nt('prod'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_105_actions(parser, lexer)
    return

def state_59_actions(parser, lexer):

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('terms'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_101_actions(parser, lexer)
    return

def state_60_actions(parser, lexer):

    value = None
    value = parser.methods.optional(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('term'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_118_actions(parser, lexer)
    return

def state_61_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-5].value, None, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_62_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, parser.stack[-5].value, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_63_actions(parser, lexer):

    value = None
    value = parser.methods.const_token(parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('token_def'), value, False)]
    del parser.stack[-5:]
    parser.shift_list(replay, lexer)
    state_104_actions(parser, lexer)
    return

def state_64_actions(parser, lexer):

    value = None
    value = parser.methods.prod(parser.stack[-3].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('prod'), value, False)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    state_105_actions(parser, lexer)
    return

def state_65_actions(parser, lexer):

    value = None
    value = parser.stack[-1].value
    replay = [StateTermValue(0, Nt('reducer'), value, False)]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_106_actions(parser, lexer)
    return

def state_66_actions(parser, lexer):

    value = None
    value = parser.methods.expr_match(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_114_actions(parser, lexer)
    return

def state_67_actions(parser, lexer):

    value = None
    value = parser.methods.expr_none()
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_114_actions(parser, lexer)
    return

def state_68_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, None, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_69_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, parser.stack[-5].value, parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_70_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(None, parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-6:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_71_actions(parser, lexer):

    value = None
    value = parser.methods.nt_def(parser.stack[-7].value, parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('nt_def'), value, False)]
    del parser.stack[-7:]
    parser.shift_list(replay, lexer)
    state_112_actions(parser, lexer)
    return

def state_72_actions(parser, lexer):

    value = None
    value = parser.methods.expr_call(parser.stack[-3].value, None)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    state_114_actions(parser, lexer)
    return

def state_73_actions(parser, lexer):

    value = None
    value = parser.methods.args_single(parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr_args'), value, False)]
    del parser.stack[-1:]
    parser.shift_list(replay, lexer)
    state_115_actions(parser, lexer)
    return

def state_74_actions(parser, lexer):

    value = None
    value = parser.methods.expr_call(parser.stack[-4].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    state_114_actions(parser, lexer)
    return

def state_75_actions(parser, lexer):

    value = None
    value = parser.methods.expr_some(parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('expr'), value, False)]
    del parser.stack[-4:]
    parser.shift_list(replay, lexer)
    state_114_actions(parser, lexer)
    return

def state_76_actions(parser, lexer):

    value = None
    value = parser.methods.args_append(parser.stack[-3].value, parser.stack[-1].value)
    replay = [StateTermValue(0, Nt('expr_args'), value, False)]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    state_115_actions(parser, lexer)
    return

def state_77_actions(parser, lexer):

    value = None
    value = parser.methods.grammar(None, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('grammar'), value, False)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_116_actions(parser, lexer)
    return

def state_78_actions(parser, lexer):

    value = None
    value = parser.methods.grammar(parser.stack[-3].value, parser.stack[-2].value)
    replay = [StateTermValue(0, Nt('grammar'), value, False)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-3:]
    parser.shift_list(replay, lexer)
    state_116_actions(parser, lexer)
    return

def state_79_actions(parser, lexer):

    value = None
    value = parser.stack[-2].value
    replay = [StateTermValue(0, Nt('term'), value, False)]
    replay = replay + parser.stack[-1:]
    del parser.stack[-2:]
    parser.shift_list(replay, lexer)
    state_118_actions(parser, lexer)
    return

def state_80_actions(parser, lexer):

    value = None
    parser.replay_action(23)
    top = parser.stack.pop()
    top = StateTermValue(23, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_81_actions(parser, lexer):

    value = None
    parser.replay_action(12)
    top = parser.stack.pop()
    top = StateTermValue(12, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_82_actions(parser, lexer):

    value = None
    parser.replay_action(13)
    top = parser.stack.pop()
    top = StateTermValue(13, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_83_actions(parser, lexer):

    value = None
    parser.replay_action(11)
    top = parser.stack.pop()
    top = StateTermValue(11, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_84_actions(parser, lexer):

    value = None
    if parser.top_state() in [10]:
        state_81_actions(parser, lexer)
        return
    if parser.top_state() in [11]:
        state_82_actions(parser, lexer)
        return

def state_85_actions(parser, lexer):

    value = None
    if parser.top_state() in [10]:
        state_83_actions(parser, lexer)
        return

def state_86_actions(parser, lexer):

    value = None
    if parser.top_state() in [10]:
        state_80_actions(parser, lexer)
        return

def state_87_actions(parser, lexer):

    value = None
    parser.replay_action(44)
    state_44_actions(parser, lexer)
    return

def state_88_actions(parser, lexer):

    value = None
    parser.replay_action(46)
    state_46_actions(parser, lexer)
    return

def state_89_actions(parser, lexer):

    value = None
    parser.replay_action(4)
    top = parser.stack.pop()
    top = StateTermValue(4, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_90_actions(parser, lexer):

    value = None
    parser.replay_action(5)
    top = parser.stack.pop()
    top = StateTermValue(5, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_91_actions(parser, lexer):

    value = None
    parser.replay_action(6)
    top = parser.stack.pop()
    top = StateTermValue(6, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_92_actions(parser, lexer):

    value = None
    parser.replay_action(7)
    top = parser.stack.pop()
    top = StateTermValue(7, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_93_actions(parser, lexer):

    value = None
    parser.replay_action(8)
    top = parser.stack.pop()
    top = StateTermValue(8, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_94_actions(parser, lexer):

    value = None
    parser.replay_action(9)
    top = parser.stack.pop()
    top = StateTermValue(9, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_95_actions(parser, lexer):

    value = None
    if parser.top_state() in [0, 1, 2, 3, 4, 5, 6, 7, 8]:
        state_94_actions(parser, lexer)
        return

def state_96_actions(parser, lexer):

    value = None
    parser.replay_action(45)
    state_45_actions(parser, lexer)
    return

def state_97_actions(parser, lexer):

    value = None
    parser.replay_action(47)
    state_47_actions(parser, lexer)
    return

def state_98_actions(parser, lexer):

    value = None
    if parser.top_state() in [0]:
        state_89_actions(parser, lexer)
        return
    if parser.top_state() in [1]:
        state_90_actions(parser, lexer)
        return
    if parser.top_state() in [2]:
        state_91_actions(parser, lexer)
        return
    if parser.top_state() in [3]:
        state_92_actions(parser, lexer)
        return

def state_99_actions(parser, lexer):

    value = None
    parser.replay_action(50)
    state_50_actions(parser, lexer)
    return

def state_100_actions(parser, lexer):

    value = None
    parser.replay_action(57)
    state_57_actions(parser, lexer)
    return

def state_101_actions(parser, lexer):

    value = None
    if parser.top_state() in [0, 1, 2, 3, 4, 5, 6, 7]:
        state_93_actions(parser, lexer)
        return

def state_102_actions(parser, lexer):

    value = None
    parser.replay_action(51)
    state_51_actions(parser, lexer)
    return

def state_103_actions(parser, lexer):

    value = None
    parser.replay_action(59)
    state_59_actions(parser, lexer)
    return

def state_104_actions(parser, lexer):

    value = None
    if parser.top_state() in [10]:
        state_96_actions(parser, lexer)
        return
    if parser.top_state() in [11]:
        state_97_actions(parser, lexer)
        return

def state_105_actions(parser, lexer):

    value = None
    if parser.top_state() in [0, 1, 2, 3]:
        state_99_actions(parser, lexer)
        return
    if parser.top_state() in [4, 5, 6, 7]:
        state_100_actions(parser, lexer)
        return

def state_106_actions(parser, lexer):

    value = None
    if parser.top_state() in [8]:
        state_107_actions(parser, lexer)
        return

def state_107_actions(parser, lexer):

    value = None
    parser.replay_action(38)
    top = parser.stack.pop()
    top = StateTermValue(38, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_108_actions(parser, lexer):

    value = None
    parser.replay_action(65)
    state_65_actions(parser, lexer)
    return

def state_109_actions(parser, lexer):

    value = None
    parser.replay_action(73)
    state_73_actions(parser, lexer)
    return

def state_110_actions(parser, lexer):

    value = None
    parser.replay_action(42)
    top = parser.stack.pop()
    top = StateTermValue(42, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_111_actions(parser, lexer):

    value = None
    parser.replay_action(76)
    state_76_actions(parser, lexer)
    return

def state_112_actions(parser, lexer):

    value = None
    if parser.top_state() in [10, 11]:
        state_87_actions(parser, lexer)
        return
    if parser.top_state() in [12, 13]:
        state_88_actions(parser, lexer)
        return

def state_113_actions(parser, lexer):

    value = None
    parser.replay_action(41)
    top = parser.stack.pop()
    top = StateTermValue(41, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_114_actions(parser, lexer):

    value = None
    if parser.top_state() in [15]:
        state_108_actions(parser, lexer)
        return
    if parser.top_state() in [14]:
        state_109_actions(parser, lexer)
        return
    if parser.top_state() in [16]:
        state_110_actions(parser, lexer)
        return
    if parser.top_state() in [17]:
        state_111_actions(parser, lexer)
        return

def state_115_actions(parser, lexer):

    value = None
    if parser.top_state() in [14]:
        state_113_actions(parser, lexer)
        return

def state_116_actions(parser, lexer):

    value = None
    if parser.top_state() in [10]:
        state_117_actions(parser, lexer)
        return

def state_117_actions(parser, lexer):

    value = None
    parser.replay_action(43)
    state_43_actions(parser, lexer)
    return

def state_118_actions(parser, lexer):

    value = None
    if parser.top_state() in [0, 1, 2, 3, 4, 5, 6, 7]:
        state_102_actions(parser, lexer)
        return
    if parser.top_state() in [8]:
        state_103_actions(parser, lexer)
        return

actions = [
    # 0.

    {'}': 49, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 4, Nt('prod'): 50, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 1.

    {'}': 61, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 5, Nt('prod'): 50, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 2.

    {'}': 62, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 6, Nt('prod'): 50, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 3.

    {'}': 69, 'IDENT': 52, 'STR': 53, 'COMMENT': 54, Nt('prods'): 7, Nt('prod'): 50, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 4.

    {'}': 56, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 5.

    {'}': 68, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 6.

    {'}': 70, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 7.

    {'}': 71, 'IDENT': 52, 'STR': 53, Nt('prod'): 57, Nt('terms'): 8, Nt('term'): 51, Nt('symbol'): 9},

    # 8.

    {';': 58, 'IDENT': 52, 'STR': 53, '=>': 15, Nt('term'): 59, Nt('symbol'): 9, Nt('reducer'): 38},

    # 9.

    {'=>': 79, 'STR': 79, 'IDENT': 79, ';': 79, '?': 60, Nt('reducer'): 79, Nt('symbol'): 79, Nt('term'): 79},

    # 10.

    {'nt': 18, 'COMMENT': 19, 'goal': 20, 'token': 21, 'var': 22, Nt('grammar'): 43, Nt('nt_defs'): 12, Nt('nt_def'): 44, Nt('token_defs'): 11, Nt('token_def'): 45, Nt(InitNt(goal=Nt('grammar'))): 23},

    # 11.

    {'nt': 18, 'COMMENT': 19, 'goal': 20, 'token': 21, 'var': 22, Nt('nt_defs'): 13, Nt('nt_def'): 44, Nt('token_def'): 47},

    # 12.

    {End(): 77, 'goal': 20, 'COMMENT': 19, 'nt': 18, Nt('nt_def'): 46},

    # 13.

    {End(): 78, 'goal': 20, 'COMMENT': 19, 'nt': 18, Nt('nt_def'): 46},

    # 14.

    {')': 72, 'MATCH': 66, 'IDENT': 39, 'Some': 40, 'None': 67, Nt('expr_args'): 41, Nt('expr'): 73},

    # 15.

    {'MATCH': 66, 'IDENT': 39, 'Some': 40, 'None': 67, Nt('expr'): 65},

    # 16.

    {'MATCH': 66, 'IDENT': 39, 'Some': 40, 'None': 67, Nt('expr'): 42},

    # 17.

    {'MATCH': 66, 'IDENT': 39, 'Some': 40, 'None': 67, Nt('expr'): 76},

    # 18.

    {'IDENT': 25},

    # 19.

    {'nt': 26, 'goal': 27},

    # 20.

    {'nt': 28},

    # 21.

    {'IDENT': 29},

    # 22.

    {'token': 30},

    # 23.

    {End(): 48},

    # 24.

    {},

    # 25.

    {'{': 0},

    # 26.

    {'IDENT': 31},

    # 27.

    {'nt': 32},

    # 28.

    {'IDENT': 33},

    # 29.

    {'=': 34},

    # 30.

    {'IDENT': 35},

    # 31.

    {'{': 1},

    # 32.

    {'IDENT': 36},

    # 33.

    {'{': 2},

    # 34.

    {'STR': 37},

    # 35.

    {';': 55},

    # 36.

    {'{': 3},

    # 37.

    {';': 63},

    # 38.

    {';': 64},

    # 39.

    {'(': 14},

    # 40.

    {'(': 16},

    # 41.

    {')': 74, ',': 17},

    # 42.

    {')': 75},

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

    # 80.

    state_80_actions,

    # 81.

    state_81_actions,

    # 82.

    state_82_actions,

    # 83.

    state_83_actions,

    # 84.

    state_84_actions,

    # 85.

    state_85_actions,

    # 86.

    state_86_actions,

    # 87.

    state_87_actions,

    # 88.

    state_88_actions,

    # 89.

    state_89_actions,

    # 90.

    state_90_actions,

    # 91.

    state_91_actions,

    # 92.

    state_92_actions,

    # 93.

    state_93_actions,

    # 94.

    state_94_actions,

    # 95.

    state_95_actions,

    # 96.

    state_96_actions,

    # 97.

    state_97_actions,

    # 98.

    state_98_actions,

    # 99.

    state_99_actions,

    # 100.

    state_100_actions,

    # 101.

    state_101_actions,

    # 102.

    state_102_actions,

    # 103.

    state_103_actions,

    # 104.

    state_104_actions,

    # 105.

    state_105_actions,

    # 106.

    state_106_actions,

    # 107.

    state_107_actions,

    # 108.

    state_108_actions,

    # 109.

    state_109_actions,

    # 110.

    state_110_actions,

    # 111.

    state_111_actions,

    # 112.

    state_112_actions,

    # 113.

    state_113_actions,

    # 114.

    state_114_actions,

    # 115.

    state_115_actions,

    # 116.

    state_116_actions,

    # 117.

    state_117_actions,

    # 118.

    state_118_actions,

]

error_codes = [
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None,
]

goal_nt_to_init_state = {'grammar': 10}

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

