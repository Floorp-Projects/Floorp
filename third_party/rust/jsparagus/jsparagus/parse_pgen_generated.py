from jsparagus import runtime
from jsparagus.runtime import Nt, ErrorToken

actions = [
    # 0. <empty>
    {'nt': 1, 'goal': 2, 'token': 3, 'var': 4},

    # 1. "nt"
    {'IDENT': 10},

    # 2. "goal"
    {'nt': 11},

    # 3. "token"
    {'IDENT': 12},

    # 4. "var"
    {'token': 13},

    # 5. grammar
    {None: -4611686018427387905},

    # 6. nt_defs
    {'nt': 1, 'goal': 2, None: -1},

    # 7. token_defs
    {'token': 3, 'var': 4, 'nt': 1, 'goal': 2},

    # 8. nt_def
    {None: -5, 'nt': -5, 'goal': -5},

    # 9. token_def
    {'nt': -3, 'goal': -3, 'token': -3, 'var': -3},

    # 10. "nt" IDENT
    {'{': 17},

    # 11. "goal" "nt"
    {'IDENT': 18},

    # 12. "token" IDENT
    {'=': 19},

    # 13. "var" "token"
    {'IDENT': 20},

    # 14. nt_defs nt_def
    {None: -6, 'nt': -6, 'goal': -6},

    # 15. token_defs nt_defs
    {'nt': 1, 'goal': 2, None: -2},

    # 16. token_defs token_def
    {'nt': -4, 'goal': -4, 'token': -4, 'var': -4},

    # 17. "nt" IDENT "{"
    {'}': 21, 'IDENT': 22, 'STR': 23},

    # 18. "goal" "nt" IDENT
    {'{': 29},

    # 19. "token" IDENT "="
    {'STR': 30},

    # 20. "var" "token" IDENT
    {';': 31},

    # 21. "nt" IDENT "{" "}"
    {None: -9, 'nt': -9, 'goal': -9},

    # 22. "nt" IDENT "{" IDENT
    {';': -27, '?': -27, '=>': -27, 'IDENT': -27, 'STR': -27},

    # 23. "nt" IDENT "{" STR
    {';': -28, '?': -28, '=>': -28, 'IDENT': -28, 'STR': -28},

    # 24. "nt" IDENT "{" prods
    {'}': 32, 'IDENT': 22, 'STR': 23},

    # 25. "nt" IDENT "{" prod
    {'}': -13, 'IDENT': -13, 'STR': -13},

    # 26. "nt" IDENT "{" terms
    {';': 34, '=>': 35, 'IDENT': 22, 'STR': 23},

    # 27. "nt" IDENT "{" term
    {';': -17, '=>': -17, 'IDENT': -17, 'STR': -17},

    # 28. "nt" IDENT "{" symbol
    {'?': 38, ';': -20, '=>': -20, 'IDENT': -20, 'STR': -20},

    # 29. "goal" "nt" IDENT "{"
    {'}': 39, 'IDENT': 22, 'STR': 23},

    # 30. "token" IDENT "=" STR
    {';': 41},

    # 31. "var" "token" IDENT ";"
    {'nt': -8, 'goal': -8, 'token': -8, 'var': -8},

    # 32. "nt" IDENT "{" prods "}"
    {None: -11, 'nt': -11, 'goal': -11},

    # 33. "nt" IDENT "{" prods prod
    {'}': -14, 'IDENT': -14, 'STR': -14},

    # 34. "nt" IDENT "{" terms ";"
    {'}': -15, 'IDENT': -15, 'STR': -15},

    # 35. "nt" IDENT "{" terms "=>"
    {'MATCH': 42, 'IDENT': 43, 'Some': 44, 'None': 45},

    # 36. "nt" IDENT "{" terms reducer
    {';': 47},

    # 37. "nt" IDENT "{" terms term
    {';': -18, '=>': -18, 'IDENT': -18, 'STR': -18},

    # 38. "nt" IDENT "{" symbol "?"
    {';': -21, '=>': -21, 'IDENT': -21, 'STR': -21},

    # 39. "goal" "nt" IDENT "{" "}"
    {None: -10, 'nt': -10, 'goal': -10},

    # 40. "goal" "nt" IDENT "{" prods
    {'}': 48, 'IDENT': 22, 'STR': 23},

    # 41. "token" IDENT "=" STR ";"
    {'nt': -7, 'goal': -7, 'token': -7, 'var': -7},

    # 42. "nt" IDENT "{" terms "=>" MATCH
    {';': -22, ')': -22, ',': -22},

    # 43. "nt" IDENT "{" terms "=>" IDENT
    {'(': 49},

    # 44. "nt" IDENT "{" terms "=>" "Some"
    {'(': 50},

    # 45. "nt" IDENT "{" terms "=>" "None"
    {';': -26, ')': -26, ',': -26},

    # 46. "nt" IDENT "{" terms "=>" expr
    {';': -19},

    # 47. "nt" IDENT "{" terms reducer ";"
    {'}': -16, 'IDENT': -16, 'STR': -16},

    # 48. "goal" "nt" IDENT "{" prods "}"
    {None: -12, 'nt': -12, 'goal': -12},

    # 49. "nt" IDENT "{" terms "=>" IDENT "("
    {')': 51, 'MATCH': 42, 'IDENT': 43, 'Some': 44, 'None': 45},

    # 50. "nt" IDENT "{" terms "=>" "Some" "("
    {'MATCH': 42, 'IDENT': 43, 'Some': 44, 'None': 45},

    # 51. "nt" IDENT "{" terms "=>" IDENT "(" ")"
    {';': -23, ')': -23, ',': -23},

    # 52. "nt" IDENT "{" terms "=>" IDENT "(" expr_args
    {')': 55, ',': 56},

    # 53. "nt" IDENT "{" terms "=>" IDENT "(" expr
    {')': -29, ',': -29},

    # 54. "nt" IDENT "{" terms "=>" "Some" "(" expr
    {')': 57},

    # 55. "nt" IDENT "{" terms "=>" IDENT "(" expr_args ")"
    {';': -24, ')': -24, ',': -24},

    # 56. "nt" IDENT "{" terms "=>" IDENT "(" expr_args ","
    {'MATCH': 42, 'IDENT': 43, 'Some': 44, 'None': 45},

    # 57. "nt" IDENT "{" terms "=>" "Some" "(" expr ")"
    {';': -25, ')': -25, ',': -25},

    # 58. "nt" IDENT "{" terms "=>" IDENT "(" expr_args "," expr
    {')': -30, ',': -30},

]

ctns = [
    {'grammar': 5, 'nt_defs': 6, 'token_defs': 7, 'nt_def': 8, 'token_def': 9},
    {},
    {},
    {},
    {},
    {},
    {'nt_def': 14},
    {'nt_defs': 15, 'token_def': 16, 'nt_def': 8},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {'nt_def': 14},
    {},
    {'prods': 24, 'prod': 25, 'terms': 26, 'term': 27, 'symbol': 28},
    {},
    {},
    {},
    {},
    {},
    {},
    {'prod': 33, 'terms': 26, 'term': 27, 'symbol': 28},
    {},
    {'reducer': 36, 'term': 37, 'symbol': 28},
    {},
    {},
    {'prods': 40, 'prod': 25, 'terms': 26, 'term': 27, 'symbol': 28},
    {},
    {},
    {},
    {},
    {},
    {'expr': 46},
    {},
    {},
    {},
    {},
    {'prod': 33, 'terms': 26, 'term': 27, 'symbol': 28},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {'expr_args': 52, 'expr': 53},
    {'expr': 54},
    {},
    {},
    {},
    {},
    {},
    {'expr': 58},
    {},
    {},
]

special_cases = [
]

error_codes = [
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None,
    None, None, None, None, None, None, None, None, None, None, None,
]

reductions = [
    # 0. grammar ::= nt_defs => grammar(None, $0)
    ('grammar', 1, lambda builder, x0: builder.grammar(None, x0)),
    # 1. grammar ::= token_defs nt_defs => grammar(Some($0), $1)
    ('grammar', 2, lambda builder, x0, x1: builder.grammar(x0, x1)),
    # 2. token_defs ::= token_def => single($0)
    ('token_defs', 1, lambda builder, x0: builder.single(x0)),
    # 3. token_defs ::= token_defs token_def => append($0, $1)
    ('token_defs', 2, lambda builder, x0, x1: builder.append(x0, x1)),
    # 4. nt_defs ::= nt_def => nt_defs_single($0)
    ('nt_defs', 1, lambda builder, x0: builder.nt_defs_single(x0)),
    # 5. nt_defs ::= nt_defs nt_def => nt_defs_append($0, $1)
    ('nt_defs', 2, lambda builder, x0, x1: builder.nt_defs_append(x0, x1)),
    # 6. token_def ::= "token" IDENT "=" STR ";" => const_token($1, $3)
    ('token_def', 5, lambda builder, x0, x1, x2, x3, x4: builder.const_token(x1, x3)),
    # 7. token_def ::= "var" "token" IDENT ";" => var_token($2)
    ('token_def', 4, lambda builder, x0, x1, x2, x3: builder.var_token(x2)),
    # 8. nt_def ::= "nt" IDENT "{" "}" => nt_def(None, $1, None)
    ('nt_def', 4, lambda builder, x0, x1, x2, x3: builder.nt_def(None, x1, None)),
    # 9. nt_def ::= "goal" "nt" IDENT "{" "}" => nt_def(Some($0), $2, None)
    ('nt_def', 5, lambda builder, x0, x1, x2, x3, x4: builder.nt_def(x0, x2, None)),
    # 10. nt_def ::= "nt" IDENT "{" prods "}" => nt_def(None, $1, Some($3))
    ('nt_def', 5, lambda builder, x0, x1, x2, x3, x4: builder.nt_def(None, x1, x3)),
    # 11. nt_def ::= "goal" "nt" IDENT "{" prods "}" => nt_def(Some($0), $2, Some($4))
    ('nt_def', 6, lambda builder, x0, x1, x2, x3, x4, x5: builder.nt_def(x0, x2, x4)),
    # 12. prods ::= prod => single($0)
    ('prods', 1, lambda builder, x0: builder.single(x0)),
    # 13. prods ::= prods prod => append($0, $1)
    ('prods', 2, lambda builder, x0, x1: builder.append(x0, x1)),
    # 14. prod ::= terms ";" => prod($0, None)
    ('prod', 2, lambda builder, x0, x1: builder.prod(x0, None)),
    # 15. prod ::= terms reducer ";" => prod($0, Some($1))
    ('prod', 3, lambda builder, x0, x1, x2: builder.prod(x0, x1)),
    # 16. terms ::= term => single($0)
    ('terms', 1, lambda builder, x0: builder.single(x0)),
    # 17. terms ::= terms term => append($0, $1)
    ('terms', 2, lambda builder, x0, x1: builder.append(x0, x1)),
    # 18. reducer ::= "=>" expr => $1
    ('reducer', 2, lambda builder, x0, x1: x1),
    # 19. term ::= symbol => $0
    ('term', 1, lambda builder, x0: x0),
    # 20. term ::= symbol "?" => optional($0)
    ('term', 2, lambda builder, x0, x1: builder.optional(x0)),
    # 21. expr ::= MATCH => expr_match($0)
    ('expr', 1, lambda builder, x0: builder.expr_match(x0)),
    # 22. expr ::= IDENT "(" ")" => expr_call($0, None)
    ('expr', 3, lambda builder, x0, x1, x2: builder.expr_call(x0, None)),
    # 23. expr ::= IDENT "(" expr_args ")" => expr_call($0, Some($2))
    ('expr', 4, lambda builder, x0, x1, x2, x3: builder.expr_call(x0, x2)),
    # 24. expr ::= "Some" "(" expr ")" => expr_some($2)
    ('expr', 4, lambda builder, x0, x1, x2, x3: builder.expr_some(x2)),
    # 25. expr ::= "None" => expr_none()
    ('expr', 1, lambda builder, x0: builder.expr_none()),
    # 26. symbol ::= IDENT => ident($0)
    ('symbol', 1, lambda builder, x0: builder.ident(x0)),
    # 27. symbol ::= STR => str($0)
    ('symbol', 1, lambda builder, x0: builder.str(x0)),
    # 28. expr_args ::= expr => args_single($0)
    ('expr_args', 1, lambda builder, x0: builder.args_single(x0)),
    # 29. expr_args ::= expr_args "," expr => args_append($0, $2)
    ('expr_args', 3, lambda builder, x0, x1, x2: builder.args_append(x0, x2)),
]


class DefaultBuilder:
    def grammar(self, x0, x1): return ('grammar', x0, x1)
    def single(self, x0): return ('single', x0)
    def append(self, x0, x1): return ('append', x0, x1)
    def const_token(self, x0, x1): return ('const_token', x0, x1)
    def var_token(self, x0): return ('var_token', x0)
    def nt_defs_single(self, x0): return ('nt_defs_single', x0)
    def nt_defs_append(self, x0, x1): return ('nt_defs_append', x0, x1)
    def nt_def(self, x0, x1, x2): return ('nt_def', x0, x1, x2)
    def prod(self, x0, x1): return ('prod', x0, x1)
    def optional(self, x0): return ('optional', x0)
    def ident(self, x0): return ('ident', x0)
    def str(self, x0): return ('str', x0)
    def expr_match(self, x0): return ('expr_match', x0)
    def expr_call(self, x0, x1): return ('expr_call', x0, x1)
    def expr_some(self, x0): return ('expr_some', x0)
    def expr_none(self, ): return ('expr_none', )
    def args_single(self, x0): return ('args_single', x0)
    def args_append(self, x0, x1): return ('args_append', x0, x1)


goal_nt_to_init_state = {
    'grammar': 0,
}

class Parser(runtime.Parser):
    def __init__(self, goal='grammar', builder=None):
        if builder is None:
            builder = DefaultBuilder()
        super().__init__(actions, ctns, reductions, special_cases, error_codes,
                         goal_nt_to_init_state[goal], builder)

