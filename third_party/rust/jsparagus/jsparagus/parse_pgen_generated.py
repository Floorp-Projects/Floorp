# type: ignore

from jsparagus import runtime
from jsparagus.runtime import (Nt, InitNt, End, ErrorToken, StateTermValue,
                               ShiftError, ShiftAccept)

def state_43_actions(parser, lexer):
    # { value = AstBuilder::id(1) [off: 0]; Unwind(Nt(InitNt(goal=Nt('grammar'))), 1, 0) }

    value = None
    value = parser.stack[-1].value
    replay = []
    replay.append(StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_84_actions(parser, lexer, r0)
    return

def state_44_actions(parser, lexer):
    # { value = AstBuilder::nt_defs_single(1) [off: 0]; Unwind(Nt('nt_defs'), 1, 0) }

    value = None
    value = parser.methods.nt_defs_single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_defs'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_82_actions(parser, lexer, r0)
    return

def state_45_actions(parser, lexer):
    # { value = AstBuilder::single(1) [off: 0]; Unwind(Nt('token_defs'), 1, 0) }

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('token_defs'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_83_actions(parser, lexer, r0)
    return

def state_46_actions(parser, lexer):
    # { value = AstBuilder::nt_defs_append(2, 1) [off: 0]; Unwind(Nt('nt_defs'), 2, 0) }

    value = None
    value = parser.methods.nt_defs_append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_defs'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_82_actions(parser, lexer, r0)
    return

def state_47_actions(parser, lexer):
    # { value = AstBuilder::append(2, 1) [off: 0]; Unwind(Nt('token_defs'), 2, 0) }

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('token_defs'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_83_actions(parser, lexer, r0)
    return

def state_48_actions(parser, lexer):
    # { Accept(); Unwind(Nt(InitNt(goal=Nt('grammar'))), 2, 0) }

    value = None
    raise ShiftAccept()
    replay = []
    replay.append(StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_84_actions(parser, lexer, r0)
    return

def state_49_actions(parser, lexer):
    # { value = AstBuilder::nt_def(None, None, 3, None) [off: 0]; Unwind(Nt('nt_def'), 4, 0) }

    value = None
    value = parser.methods.nt_def(None, None, parser.stack[-3].value, None)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-4:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_50_actions(parser, lexer):
    # { value = AstBuilder::single(1) [off: 0]; Unwind(Nt('prods'), 1, 0) }

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('prods'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_94_actions(parser, lexer, r0)
    return

def state_51_actions(parser, lexer):
    # { value = AstBuilder::single(1) [off: 0]; Unwind(Nt('terms'), 1, 0) }

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('terms'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_97_actions(parser, lexer, r0)
    return

def state_52_actions(parser, lexer):
    # { value = AstBuilder::ident(1) [off: 0]; Unwind(Nt('symbol'), 1, 0) }

    value = None
    value = parser.methods.ident(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('symbol'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_91_actions(parser, lexer, r0)
    return

def state_53_actions(parser, lexer):
    # { value = AstBuilder::str(1) [off: 0]; Unwind(Nt('symbol'), 1, 0) }

    value = None
    value = parser.methods.str(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('symbol'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_91_actions(parser, lexer, r0)
    return

def state_54_actions(parser, lexer):
    # { value = AstBuilder::empty(1) [off: 0]; Unwind(Nt('prods'), 1, 0) }

    value = None
    value = parser.methods.empty(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('prods'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_94_actions(parser, lexer, r0)
    return

def state_55_actions(parser, lexer):
    # { value = AstBuilder::var_token(2) [off: 0]; Unwind(Nt('token_def'), 4, 0) }

    value = None
    value = parser.methods.var_token(parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('token_def'), value, False))
    del parser.stack[-4:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_100_actions(parser, lexer, r0)
    return

def state_56_actions(parser, lexer):
    # { value = AstBuilder::nt_def(None, None, 4, Some(inner=2)) [off: 0]; Unwind(Nt('nt_def'), 5, 0) }

    value = None
    value = parser.methods.nt_def(None, None, parser.stack[-4].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-5:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_57_actions(parser, lexer):
    # { value = AstBuilder::append(2, 1) [off: 0]; Unwind(Nt('prods'), 2, 0) }

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('prods'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_94_actions(parser, lexer, r0)
    return

def state_58_actions(parser, lexer):
    # { value = AstBuilder::prod(2, None) [off: 0]; Unwind(Nt('prod'), 2, 0) }

    value = None
    value = parser.methods.prod(parser.stack[-2].value, None)
    replay = []
    replay.append(StateTermValue(0, Nt('prod'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_101_actions(parser, lexer, r0)
    return

def state_59_actions(parser, lexer):
    # { value = AstBuilder::append(2, 1) [off: 0]; Unwind(Nt('terms'), 2, 0) }

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('terms'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_97_actions(parser, lexer, r0)
    return

def state_60_actions(parser, lexer):
    # { value = AstBuilder::optional(2) [off: 0]; Unwind(Nt('term'), 2, 0) }

    value = None
    value = parser.methods.optional(parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('term'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_111_actions(parser, lexer, r0)
    return

def state_61_actions(parser, lexer):
    # { value = AstBuilder::nt_def(Some(inner=5), None, 3, None) [off: 0]; Unwind(Nt('nt_def'), 5, 0) }

    value = None
    value = parser.methods.nt_def(parser.stack[-5].value, None, parser.stack[-3].value, None)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-5:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_62_actions(parser, lexer):
    # { value = AstBuilder::nt_def(None, Some(inner=5), 3, None) [off: 0]; Unwind(Nt('nt_def'), 5, 0) }

    value = None
    value = parser.methods.nt_def(None, parser.stack[-5].value, parser.stack[-3].value, None)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-5:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_63_actions(parser, lexer):
    # { value = AstBuilder::const_token(4, 2) [off: 0]; Unwind(Nt('token_def'), 5, 0) }

    value = None
    value = parser.methods.const_token(parser.stack[-4].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('token_def'), value, False))
    del parser.stack[-5:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_100_actions(parser, lexer, r0)
    return

def state_64_actions(parser, lexer):
    # { value = AstBuilder::prod(3, Some(inner=2)) [off: 0]; Unwind(Nt('prod'), 3, 0) }

    value = None
    value = parser.methods.prod(parser.stack[-3].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('prod'), value, False))
    del parser.stack[-3:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_101_actions(parser, lexer, r0)
    return

def state_65_actions(parser, lexer):
    # { value = AstBuilder::id(1) [off: 0]; Unwind(Nt('reducer'), 2, 0) }

    value = None
    value = parser.stack[-1].value
    replay = []
    replay.append(StateTermValue(0, Nt('reducer'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_102_actions(parser, lexer, r0)
    return

def state_66_actions(parser, lexer):
    # { value = AstBuilder::expr_match(1) [off: 0]; Unwind(Nt('expr'), 1, 0) }

    value = None
    value = parser.methods.expr_match(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_108_actions(parser, lexer, r0)
    return

def state_67_actions(parser, lexer):
    # { value = AstBuilder::expr_none() [off: 0]; Unwind(Nt('expr'), 1, 0) }

    value = None
    value = parser.methods.expr_none()
    replay = []
    replay.append(StateTermValue(0, Nt('expr'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_108_actions(parser, lexer, r0)
    return

def state_68_actions(parser, lexer):
    # { value = AstBuilder::nt_def(Some(inner=6), None, 4, Some(inner=2)) [off: 0]; Unwind(Nt('nt_def'), 6, 0) }

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, None, parser.stack[-4].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-6:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_69_actions(parser, lexer):
    # { value = AstBuilder::nt_def(Some(inner=6), Some(inner=5), 3, None) [off: 0]; Unwind(Nt('nt_def'), 6, 0) }

    value = None
    value = parser.methods.nt_def(parser.stack[-6].value, parser.stack[-5].value, parser.stack[-3].value, None)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-6:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_70_actions(parser, lexer):
    # { value = AstBuilder::nt_def(None, Some(inner=6), 4, Some(inner=2)) [off: 0]; Unwind(Nt('nt_def'), 6, 0) }

    value = None
    value = parser.methods.nt_def(None, parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-6:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_71_actions(parser, lexer):
    # { value = AstBuilder::nt_def(Some(inner=7), Some(inner=6), 4, Some(inner=2)) [off: 0]; Unwind(Nt('nt_def'), 7, 0) }

    value = None
    value = parser.methods.nt_def(parser.stack[-7].value, parser.stack[-6].value, parser.stack[-4].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_def'), value, False))
    del parser.stack[-7:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_107_actions(parser, lexer, r0)
    return

def state_72_actions(parser, lexer):
    # { value = AstBuilder::expr_call(3, None) [off: 0]; Unwind(Nt('expr'), 3, 0) }

    value = None
    value = parser.methods.expr_call(parser.stack[-3].value, None)
    replay = []
    replay.append(StateTermValue(0, Nt('expr'), value, False))
    del parser.stack[-3:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_108_actions(parser, lexer, r0)
    return

def state_73_actions(parser, lexer):
    # { value = AstBuilder::args_single(1) [off: 0]; Unwind(Nt('expr_args'), 1, 0) }

    value = None
    value = parser.methods.args_single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr_args'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_109_actions(parser, lexer, r0)
    return

def state_74_actions(parser, lexer):
    # { value = AstBuilder::expr_call(4, Some(inner=2)) [off: 0]; Unwind(Nt('expr'), 4, 0) }

    value = None
    value = parser.methods.expr_call(parser.stack[-4].value, parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr'), value, False))
    del parser.stack[-4:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_108_actions(parser, lexer, r0)
    return

def state_75_actions(parser, lexer):
    # { value = AstBuilder::expr_some(2) [off: 0]; Unwind(Nt('expr'), 4, 0) }

    value = None
    value = parser.methods.expr_some(parser.stack[-2].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr'), value, False))
    del parser.stack[-4:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_108_actions(parser, lexer, r0)
    return

def state_76_actions(parser, lexer):
    # { value = AstBuilder::args_append(3, 1) [off: 0]; Unwind(Nt('expr_args'), 3, 0) }

    value = None
    value = parser.methods.args_append(parser.stack[-3].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr_args'), value, False))
    del parser.stack[-3:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_109_actions(parser, lexer, r0)
    return

def state_77_actions(parser, lexer):
    # { value = AstBuilder::grammar(None, 1) [off: 1]; Unwind(Nt('grammar'), 1, 1) }

    value = None
    value = parser.methods.grammar(None, parser.stack[-2].value)
    replay = []
    replay.append(parser.stack.pop())
    replay.append(StateTermValue(0, Nt('grammar'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_110_actions(parser, lexer, r0)
    return

def state_78_actions(parser, lexer):
    # { value = AstBuilder::grammar(Some(inner=2), 1) [off: 1]; Unwind(Nt('grammar'), 2, 1) }

    value = None
    value = parser.methods.grammar(parser.stack[-3].value, parser.stack[-2].value)
    replay = []
    replay.append(parser.stack.pop())
    replay.append(StateTermValue(0, Nt('grammar'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_110_actions(parser, lexer, r0)
    return

def state_79_actions(parser, lexer):
    # { value = AstBuilder::id(1) [off: 1]; Unwind(Nt('term'), 1, 1) }

    value = None
    value = parser.stack[-2].value
    replay = []
    replay.append(parser.stack.pop())
    replay.append(StateTermValue(0, Nt('term'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_111_actions(parser, lexer, r0)
    return

def state_80_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((12,))

    value = None
    parser.replay_action(12)
    top = parser.stack.pop()
    top = StateTermValue(12, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_81_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((13,))

    value = None
    parser.replay_action(13)
    top = parser.stack.pop()
    top = StateTermValue(13, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_82_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [10]:
        r0 = parser.replay.pop()
        state_80_actions(parser, lexer, r0)
        return
    if parser.top_state() in [11]:
        r0 = parser.replay.pop()
        state_81_actions(parser, lexer, r0)
        return

def state_83_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((11,))

    value = None
    parser.replay_action(11)
    top = parser.stack.pop()
    top = StateTermValue(11, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_84_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((23,))

    value = None
    parser.replay_action(23)
    top = parser.stack.pop()
    top = StateTermValue(23, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_85_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::nt_defs_single(1) [off: -1]; Unwind(Nt('nt_defs'), 1, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.nt_defs_single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_defs'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_82_actions(parser, lexer, r0)
    return

def state_86_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::nt_defs_append(2, 1) [off: -1]; Unwind(Nt('nt_defs'), 2, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.nt_defs_append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('nt_defs'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_82_actions(parser, lexer, r0)
    return

def state_87_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((4,))

    value = None
    parser.replay_action(4)
    top = parser.stack.pop()
    top = StateTermValue(4, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_88_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((5,))

    value = None
    parser.replay_action(5)
    top = parser.stack.pop()
    top = StateTermValue(5, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_89_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((6,))

    value = None
    parser.replay_action(6)
    top = parser.stack.pop()
    top = StateTermValue(6, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_90_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((7,))

    value = None
    parser.replay_action(7)
    top = parser.stack.pop()
    top = StateTermValue(7, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_91_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((9,))

    value = None
    parser.replay_action(9)
    top = parser.stack.pop()
    top = StateTermValue(9, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_92_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::single(1) [off: -1]; Unwind(Nt('token_defs'), 1, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('token_defs'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_83_actions(parser, lexer, r0)
    return

def state_93_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::append(2, 1) [off: -1]; Unwind(Nt('token_defs'), 2, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('token_defs'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_83_actions(parser, lexer, r0)
    return

def state_94_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [0]:
        r0 = parser.replay.pop()
        state_87_actions(parser, lexer, r0)
        return
    if parser.top_state() in [1]:
        r0 = parser.replay.pop()
        state_88_actions(parser, lexer, r0)
        return
    if parser.top_state() in [2]:
        r0 = parser.replay.pop()
        state_89_actions(parser, lexer, r0)
        return
    if parser.top_state() in [3]:
        r0 = parser.replay.pop()
        state_90_actions(parser, lexer, r0)
        return

def state_95_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::single(1) [off: -1]; Unwind(Nt('prods'), 1, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('prods'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_94_actions(parser, lexer, r0)
    return

def state_96_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::append(2, 1) [off: -1]; Unwind(Nt('prods'), 2, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('prods'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_94_actions(parser, lexer, r0)
    return

def state_97_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((8,))

    value = None
    parser.replay_action(8)
    top = parser.stack.pop()
    top = StateTermValue(8, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_98_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::single(1) [off: -1]; Unwind(Nt('terms'), 1, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('terms'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_97_actions(parser, lexer, r0)
    return

def state_99_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::append(2, 1) [off: -1]; Unwind(Nt('terms'), 2, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.append(parser.stack[-2].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('terms'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_97_actions(parser, lexer, r0)
    return

def state_100_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [10]:
        r0 = parser.replay.pop()
        state_92_actions(parser, lexer, r0)
        return
    if parser.top_state() in [11]:
        r0 = parser.replay.pop()
        state_93_actions(parser, lexer, r0)
        return

def state_101_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [0, 1, 2, 3]:
        r0 = parser.replay.pop()
        state_95_actions(parser, lexer, r0)
        return
    if parser.top_state() in [4, 5, 6, 7]:
        r0 = parser.replay.pop()
        state_96_actions(parser, lexer, r0)
        return

def state_102_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((38,))

    value = None
    parser.replay_action(38)
    top = parser.stack.pop()
    top = StateTermValue(38, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_103_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::id(1) [off: -1]; Unwind(Nt('reducer'), 2, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.stack[-1].value
    replay = []
    replay.append(StateTermValue(0, Nt('reducer'), value, False))
    del parser.stack[-2:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_102_actions(parser, lexer, r0)
    return

def state_104_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::args_single(1) [off: -1]; Unwind(Nt('expr_args'), 1, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.args_single(parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr_args'), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_109_actions(parser, lexer, r0)
    return

def state_105_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((42,))

    value = None
    parser.replay_action(42)
    top = parser.stack.pop()
    top = StateTermValue(42, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_106_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::args_append(3, 1) [off: -1]; Unwind(Nt('expr_args'), 3, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.methods.args_append(parser.stack[-3].value, parser.stack[-1].value)
    replay = []
    replay.append(StateTermValue(0, Nt('expr_args'), value, False))
    del parser.stack[-3:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_109_actions(parser, lexer, r0)
    return

def state_107_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [10, 11]:
        r0 = parser.replay.pop()
        state_85_actions(parser, lexer, r0)
        return
    if parser.top_state() in [12, 13]:
        r0 = parser.replay.pop()
        state_86_actions(parser, lexer, r0)
        return

def state_108_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [15]:
        r0 = parser.replay.pop()
        state_103_actions(parser, lexer, r0)
        return
    if parser.top_state() in [14]:
        r0 = parser.replay.pop()
        state_104_actions(parser, lexer, r0)
        return
    if parser.top_state() in [16]:
        r0 = parser.replay.pop()
        state_105_actions(parser, lexer, r0)
        return
    if parser.top_state() in [17]:
        r0 = parser.replay.pop()
        state_106_actions(parser, lexer, r0)
        return

def state_109_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # Replay((41,))

    value = None
    parser.replay_action(41)
    top = parser.stack.pop()
    top = StateTermValue(41, top.term, top.value, top.new_line)
    parser.stack.append(top)
    return

def state_110_actions(parser, lexer, a0):
    parser.replay.extend([a0])
    # { value = AstBuilder::id(1) [off: -1]; Unwind(Nt(InitNt(goal=Nt('grammar'))), 1, -1) }
    parser.stack.append(parser.replay.pop())

    value = None
    value = parser.stack[-1].value
    replay = []
    replay.append(StateTermValue(0, Nt(InitNt(goal=Nt('grammar'))), value, False))
    del parser.stack[-1:]
    parser.replay.extend(replay)
    r0 = parser.replay.pop()
    state_84_actions(parser, lexer, r0)
    return

def state_111_actions(parser, lexer, a0):
    parser.replay.extend([a0])

    value = None
    if parser.top_state() in [0, 1, 2, 3, 4, 5, 6, 7]:
        r0 = parser.replay.pop()
        state_98_actions(parser, lexer, r0)
        return
    if parser.top_state() in [8]:
        r0 = parser.replay.pop()
        state_99_actions(parser, lexer, r0)
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

]

error_codes = [
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
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
    def prod(self, x0, x1):
        return ('prod', x0, x1)
    def optional(self, x0):
        return ('optional', x0)
    def const_token(self, x0, x1):
        return ('const_token', x0, x1)
    def expr_match(self, x0):
        return ('expr_match', x0)
    def expr_none(self, ):
        return ('expr_none', )
    def expr_call(self, x0, x1):
        return ('expr_call', x0, x1)
    def args_single(self, x0):
        return ('args_single', x0)
    def expr_some(self, x0):
        return ('expr_some', x0)
    def args_append(self, x0, x1):
        return ('args_append', x0, x1)
    def grammar(self, x0, x1):
        return ('grammar', x0, x1)

class Parser(runtime.Parser):
    def __init__(self, goal='grammar', builder=None):
        if builder is None:
            builder = DefaultMethods()
        super().__init__(actions, error_codes, goal_nt_to_init_state[goal], builder)

