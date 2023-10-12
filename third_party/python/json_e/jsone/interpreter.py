from .shared import InterpreterError, string
import operator


def infixExpectationError(operator, expected):
    return InterpreterError('infix: {} expects {} {} {}'.
                            format(operator, expected, operator, expected))


class Interpreter:
    def __init__(self, context):
        self.context = context

    def visit(self, node):
        method_name = 'visit_' + type(node).__name__
        visitor = getattr(self, method_name)
        return visitor(node)

    def visit_ASTNode(self, node):
        if node.token.kind == "number":
            v = node.token.value
            return float(v) if '.' in v else int(v)
        elif node.token.kind == "null":
            return None
        elif node.token.kind == "string":
            return node.token.value[1:-1]
        elif node.token.kind == "true":
            return True
        elif node.token.kind == "false":
            return False
        elif node.token.kind == "identifier":
            return node.token.value

    def visit_UnaryOp(self, node):
        value = self.visit(node.expr)
        if node.token.kind == "+":
            if not is_number(value):
                raise InterpreterError('{} expects {}'.format('unary +', 'number'))
            return value
        elif node.token.kind == "-":
            if not is_number(value):
                raise InterpreterError('{} expects {}'.format('unary -', 'number'))
            return -value
        elif node.token.kind == "!":
            return not self.visit(node.expr)

    def visit_BinOp(self, node):
        left = self.visit(node.left)
        if node.token.kind == "||":
            return bool(left or self.visit(node.right))
        elif node.token.kind == "&&":
            return bool(left and self.visit(node.right))
        else:
            right = self.visit(node.right)

        if node.token.kind == "+":
            if not isinstance(left, (string, int, float)) or isinstance(left, bool):
                raise infixExpectationError('+', 'numbers/strings')
            if not isinstance(right, (string, int, float)) or isinstance(right, bool):
                raise infixExpectationError('+', 'numbers/strings')
            if type(right) != type(left) and \
                    (isinstance(left, string) or isinstance(right, string)):
                raise infixExpectationError('+', 'numbers/strings')
            return left + right
        elif node.token.kind == "-":
            test_math_operands("-", left, right)
            return left - right
        elif node.token.kind == "/":
            test_math_operands("/", left, right)
            return operator.truediv(left, right)
        elif node.token.kind == "*":
            test_math_operands("*", left, right)
            return left * right
        elif node.token.kind == ">":
            test_comparison_operands(">", left, right)
            return left > right
        elif node.token.kind == "<":
            test_comparison_operands("<", left, right)
            return left < right
        elif node.token.kind == ">=":
            test_comparison_operands(">=", left, right)
            return left >= right
        elif node.token.kind == "<=":
            test_comparison_operands("<=", left, right)
            return left <= right
        elif node.token.kind == "!=":
            return left != right
        elif node.token.kind == "==":
            return left == right
        elif node.token.kind == "**":
            test_math_operands("**", left, right)
            return right ** left
        elif node.token.value == "in":
            if isinstance(right, dict):
                if not isinstance(left, string):
                    raise infixExpectationError('in-object', 'string on left side')
            elif isinstance(right, string):
                if not isinstance(left, string):
                    raise infixExpectationError('in-string', 'string on left side')
            elif not isinstance(right, list):
                raise infixExpectationError(
                    'in', 'Array, string, or object on right side')
            try:
                return left in right
            except TypeError:
                raise infixExpectationError('in', 'scalar value, collection')

        elif node.token.kind == ".":
            if not isinstance(left, dict):
                raise InterpreterError('infix: {} expects {}'.format(".", 'objects'))
            try:
                return left[right]
            except KeyError:
                raise InterpreterError(
                    'object has no property "{}"'.format(right))

    def visit_List(self, node):
        list = []

        if node.list[0] is not None:
            for item in node.list:
                list.append(self.visit(item))

        return list

    def visit_ValueAccess(self, node):
        value = self.visit(node.arr)
        left = 0
        right = None

        if node.left:
            left = self.visit(node.left)
        if node.right:
            right = self.visit(node.right)

        if isinstance(value, (list, string)):
            if node.isInterval:
                if right is None:
                    right = len(value)
                try:
                    return value[left:right]
                except TypeError:
                    raise InterpreterError('cannot perform interval access with non-integers')
            else:
                try:
                    return value[left]
                except IndexError:
                    raise InterpreterError('index out of bounds')
                except TypeError:
                    raise InterpreterError('should only use integers to access arrays or strings')

        if not isinstance(value, dict):
            raise InterpreterError('infix: {} expects {}'.format('"[..]"', 'object, array, or string'))
        if not isinstance(left, string):
            raise InterpreterError('object keys must be strings')

        try:
            return value[left]
        except KeyError:
            return None

    def visit_ContextValue(self, node):
        try:
            contextValue = self.context[node.token.value]
        except KeyError:
            raise InterpreterError(
                'unknown context value {}'.format(node.token.value))
        return contextValue

    def visit_FunctionCall(self, node):
        args = []
        func_name = self.visit(node.name)
        if callable(func_name):
            if node.args is not None:
                for item in node.args:
                    args.append(self.visit(item))
                if hasattr(func_name, "_jsone_builtin"):
                    return func_name(self.context, *args)
                else:
                    return func_name(*args)
        else:
            raise InterpreterError(
                '{} is not callable'.format(func_name))

    def visit_Object(self, node):
        obj = {}
        for key in node.obj:
            obj[key] = self.visit(node.obj[key])
        return obj

    def interpret(self, tree):
        return self.visit(tree)


def test_math_operands(op, left, right):
    if not is_number(left):
        raise infixExpectationError(op, 'number')
    if not is_number(right):
        raise infixExpectationError(op, 'number')
    return


def test_comparison_operands(op, left, right):
    if type(left) != type(right) or \
            not (isinstance(left, (int, float, string)) and not isinstance(left, bool)):
        raise infixExpectationError(op, 'numbers/strings')
    return


def is_number(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)
