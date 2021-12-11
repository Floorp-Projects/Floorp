from fluent.syntax import ast as FTL
from fluent.syntax.visitor import Transformer

from .transforms import Transform


class Evaluator(Transformer):
    """An AST transformer for evaluating migration Transforms.

    An AST transformer (i.e. a visitor capable of modifying the AST) which
    walks an AST hierarchy and evaluates nodes which are migration Transforms.
    """

    def __init__(self, ctx):
        self.ctx = ctx

    def visit(self, node):
        if not isinstance(node, FTL.BaseNode):
            return node

        if isinstance(node, Transform):
            # Some transforms don't expect other transforms as children.
            # Evaluate the children first.
            transform = self.generic_visit(node)
            # Then, evaluate this transform.
            return transform(self.ctx)

        return self.generic_visit(node)
