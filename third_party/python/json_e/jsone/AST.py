class ASTNode(object):
    def __init__(self, token):
        self.token = token


Primitive = ASTNode


class BinOp(ASTNode):
    def __init__(self, token, left, right):
        ASTNode.__init__(self, token)
        self.left = left
        self.right = right


class UnaryOp(ASTNode):
    def __init__(self, token, expr):
        ASTNode.__init__(self, token)
        self.expr = expr


class FunctionCall(ASTNode):
    def __init__(self, token, name, args):
        ASTNode.__init__(self, token)
        self.name = name
        self.args = args


class ContextValue(ASTNode):
    def __init__(self, token):
        ASTNode.__init__(self, token)


class List(ASTNode):
    def __init__(self, token, list):
        ASTNode.__init__(self, token)
        self.list = list


class ValueAccess(ASTNode):
    def __init__(self, token, arr, isInterval, left, right):
        ASTNode.__init__(self, token)
        self.arr = arr
        self.isInterval = isInterval
        self.left = left
        self.right = right


class Object(ASTNode):
    def __init__(self, token, obj):
        ASTNode.__init__(self, token)
        self.obj = obj
