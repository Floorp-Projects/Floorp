import argparse
import ast
from itertools import zip_longest

from fluent.migrate import transforms
from fluent.migrate.errors import MigrationError
from fluent.migrate.helpers import transforms_from
from fluent.syntax import ast as FTL
from fluent.syntax.visitor import Visitor
from compare_locales import mozpath


class MigrateNotFoundException(Exception):
    pass


class BadContextAPIException(Exception):
    pass


def process_assign(node, context):
    if isinstance(node.value, ast.Str):
        val = node.value.s
    elif isinstance(node.value, ast.Name):
        val = context.get(node.value.id)
    elif isinstance(node.value, ast.Call):
        val = node.value
    if val is None:
        return
    for target in node.targets:
        if isinstance(target, ast.Name):
            context[target.id] = val


class Validator:
    """Validate a migration recipe

    Extract information from the migration recipe about which files to
    migrate from, and which files to migrate to.
    Also check for errors in the recipe, or bad API usage.
    """

    @classmethod
    def validate(cls, path, code=None):
        if code is None:
            with open(path) as fh:
                code = fh.read()
        validator = cls(code, path)
        return validator.inspect()

    def __init__(self, code, path):
        self.ast = ast.parse(code, path)

    def inspect(self):
        migrate_func = None
        global_assigns = {}
        for top_level in ast.iter_child_nodes(self.ast):
            if (
                isinstance(top_level, ast.FunctionDef)
                and top_level.name == 'migrate'
            ):
                if migrate_func:
                    raise MigrateNotFoundException(
                        'Duplicate definition of migrate'
                    )
                migrate_func = top_level
                details = self.inspect_migrate(migrate_func, global_assigns)
            if isinstance(top_level, ast.Assign):
                process_assign(top_level, global_assigns)
            if isinstance(top_level, (ast.Import, ast.ImportFrom)):
                if 'module' in top_level._fields:
                    module = top_level.module
                else:
                    module = None
                for alias in top_level.names:
                    asname = alias.asname or alias.name
                    dotted = alias.name
                    if module:
                        dotted = f'{module}.{dotted}'
                    global_assigns[asname] = dotted
        if not migrate_func:
            raise MigrateNotFoundException(
                'migrate function not found'
            )
        return details

    def inspect_migrate(self, migrate_func, global_assigns):
        if (
                len(migrate_func.args.args) != 1 or
                any(
                    getattr(migrate_func.args, arg_field)
                    for arg_field in migrate_func.args._fields
                    if arg_field != 'args'
                )
        ):
            raise MigrateNotFoundException(
                'migrate takes only one positional argument'
            )
        arg = migrate_func.args.args[0]
        if isinstance(arg, ast.Name):
            ctx_var = arg.id  # python 2
        else:
            ctx_var = arg.arg  # python 3
        visitor = MigrateAnalyzer(ctx_var, global_assigns)
        visitor.visit(migrate_func)
        return {
            'references': visitor.references,
            'issues': visitor.issues,
        }


def full_name(node, global_assigns):
    leafs = []
    while isinstance(node, ast.Attribute):
        leafs.append(node.attr)
        node = node.value
    if isinstance(node, ast.Name):
        leafs.append(global_assigns.get(node.id, node.id))
    return '.'.join(reversed(leafs))


PATH_TYPES = (str,) + (ast.Call,)


class MigrateAnalyzer(ast.NodeVisitor):
    def __init__(self, ctx_var, global_assigns):
        super().__init__()
        self.ctx_var = ctx_var
        self.global_assigns = global_assigns
        self.depth = 0
        self.issues = []
        self.references = set()

    def generic_visit(self, node):
        self.depth += 1
        super().generic_visit(node)
        self.depth -= 1

    def visit_Assign(self, node):
        if self.depth == 1:
            process_assign(node, self.global_assigns)
        self.generic_visit(node)

    def visit_Attribute(self, node):
        if isinstance(node.value, ast.Name) and node.value.id == self.ctx_var:
            if node.attr not in (
                'add_transforms',
                'locale',
            ):
                raise BadContextAPIException(
                    'Unexpected attribute access on {}.{}'.format(
                        self.ctx_var, node.attr
                    )
                )
        self.generic_visit(node)

    def visit_Call(self, node):
        if (
                isinstance(node.func, ast.Attribute) and
                isinstance(node.func.value, ast.Name) and
                node.func.value.id == self.ctx_var
        ):
            return self.call_ctx(node)
        dotted = full_name(node.func, self.global_assigns)
        if dotted == 'fluent.migrate.helpers.transforms_from':
            return self.call_helpers_transforms_from(node)
        if dotted.startswith('fluent.migrate.'):
            return self.call_transform(node, dotted)
        self.generic_visit(node)

    def call_ctx(self, node):
        if node.func.attr == 'add_transforms':
            return self.call_add_transforms(node)
        raise BadContextAPIException(
            'Unexpected call on {}.{}'.format(
                self.ctx_var, node.func.attr
            )
        )

    def call_add_transforms(self, node):
        args_msg = (
            'Expected arguments to {}.add_transforms: '
            'target_ftl_path, reference_ftl_path, list_of_transforms'
        ).format(self.ctx_var)
        ref_msg = (
            'Expected second argument to {}.add_transforms: '
            'reference should be string or variable with string value'
        ).format(self.ctx_var)
        # Just check call signature here, check actual types below
        if not self.check_arguments(node, (ast.AST, ast.AST, ast.AST)):
            self.issues.append({
                'msg': args_msg,
                'line': node.lineno,
            })
            return
        in_reference = node.args[1]
        if isinstance(in_reference, ast.Name):
            in_reference = self.global_assigns.get(in_reference.id)
        if isinstance(in_reference, ast.Str):
            in_reference = in_reference.s
        if not isinstance(in_reference, str):
            self.issues.append({
                'msg': ref_msg,
                'line': node.args[1].lineno,
            })
            return
        self.references.add(in_reference)
        # Checked node.args[1].
        # There's not a lot we can say about our target path,
        # ignoring that.
        # For our transforms, we want more checks.
        self.generic_visit(node.args[2])

    def call_transform(self, node, dotted):
        module, called = dotted.rsplit('.', 1)
        if module not in ('fluent.migrate', 'fluent.migrate.transforms'):
            return
        transform = getattr(transforms, called)
        if not issubclass(transform, transforms.Source):
            return
        bad_args = f'{called} takes path and key as first two params'
        if not self.check_arguments(
            node, ((ast.Str, ast.Name), (ast.Str, ast.Name),),
            allow_more=True, check_kwargs=False
        ):
            self.issues.append({
                'msg': bad_args,
                'line': node.lineno
            })
            return
        path = node.args[0]
        if isinstance(path, ast.Str):
            path = path.s
        if isinstance(path, ast.Name):
            path = self.global_assigns.get(path.id)
        if not isinstance(path, PATH_TYPES):
            self.issues.append({
                'msg': bad_args,
                'line': node.lineno
            })

    def call_helpers_transforms_from(self, node):
        args_msg = (
            'Expected arguments to transforms_from: '
            'str, **substitions'
        )
        if not self.check_arguments(
            node, (ast.Str,), check_kwargs=False
        ):
            self.issues.append({
                'msg': args_msg,
                'line': node.lineno,
            })
            return
        kwargs = {}
        found_bad_keywords = False
        for keyword in node.keywords:
            v = keyword.value
            if isinstance(v, ast.Str):
                v = v.s
            if isinstance(v, ast.Name):
                v = self.global_assigns.get(v.id)
            if isinstance(v, ast.Call):
                v = 'determined at runtime'
            if not isinstance(v, PATH_TYPES):
                msg = 'Bad keyword arg {} to transforms_from'.format(
                    keyword.arg
                )
                self.issues.append({
                    'msg': msg,
                    'line': node.lineno,
                })
                found_bad_keywords = True
            else:
                kwargs[keyword.arg] = v
        if found_bad_keywords:
            return
        try:
            transforms = transforms_from(node.args[0].s, **kwargs)
        except MigrationError as e:
            self.issues.append({
                'msg': str(e),
                'line': node.lineno,
            })
            return
        ti = TransformsInspector()
        ti.visit(transforms)
        self.issues.extend({
            'msg': issue,
            'line': node.lineno,
        } for issue in set(ti.issues))

    def check_arguments(
        self, node, argspec, check_kwargs=True, allow_more=False
    ):
        if check_kwargs and (
            node.keywords or
            (hasattr(node, 'kwargs') and node.kwargs)
        ):
            return False
        if hasattr(node, 'starargs') and node.starargs:
            return False
        for arg, NODE_TYPE in zip_longest(node.args, argspec):
            if NODE_TYPE is None:
                return True if allow_more else False
            if not (isinstance(arg, NODE_TYPE)):
                return False
        return True


class TransformsInspector(Visitor):
    def __init__(self):
        super().__init__()
        self.issues = []

    def generic_visit(self, node):
        if isinstance(node, transforms.Source):
            src = node.path
            # Source needs paths to be normalized
            # https://bugzilla.mozilla.org/show_bug.cgi?id=1568199
            if src != mozpath.normpath(src):
                self.issues.append(
                    f'Source "{src}" needs to be a normalized path'
                )
        super().generic_visit(node)


def cli():
    parser = argparse.ArgumentParser()
    parser.add_argument('migration')
    args = parser.parse_args()
    issues = Validator.validate(args.migration)['issues']
    for issue in issues:
        print(issue['msg'], 'at line', issue['line'])
    return 1 if issues else 0
