"""Data structure extracted from parsing the EDSL which are added within the
Rust code."""

from dataclasses import dataclass
from .utils import keep_until
from . import grammar


@dataclass(frozen=True)
class ImplFor:
    __slots__ = ['param', 'trait', 'for_type']
    param: str
    trait: str
    for_type: str


def eq_productions(grammar, prod1, prod2):
    s1 = tuple(e for e in prod1.body if grammar.is_shifted_element(e))
    s2 = tuple(e for e in prod2.body if grammar.is_shifted_element(e))
    return s1 == s2


def merge_productions(grammar, prod1, prod2):
    # Consider all shifted elements as non-moveable elements, and insert other
    # around these.
    assert eq_productions(grammar, prod1, prod2)
    l1 = list(prod1.body)
    l2 = list(prod2.body)
    body = []
    while l1 != [] and l2 != []:
        front1 = list(keep_until(l1, grammar.is_shifted_element))
        front2 = list(keep_until(l2, grammar.is_shifted_element))
        assert front1[-1] == front2[-1]
        l1 = l1[len(front1):]
        l2 = l2[len(front2):]
        if len(front1) == 1:
            body = body + front2
        elif len(front2) == 1:
            body = body + front1
        else:
            raise ValueError("We do not know how to sort operations yet.")
    return prod1.copy_with(body=body)


@dataclass(frozen=True)
class ExtPatch:
    "Patch an existing grammar rule by adding Code"

    prod: grammar.NtDef

    def apply_patch(self, filename, grammar, nonterminals):
        # - name: non-terminal.
        # - namespace: ":" for syntactic or "::" for lexical. Always ":" as
        #     defined by rust_nt_def.
        # - nt_def: A single non-terminal definition with a single production.
        (name, namespace, nt_def) = self.prod
        gnt_def = nonterminals[name]
        # Find a matching production in the grammar.
        assert nt_def.params == gnt_def.params
        new_rhs_list = []
        assert len(nt_def.rhs_list) == 1
        patch_prod = nt_def.rhs_list[0]
        applied = False
        for grammar_prod in gnt_def.rhs_list:
            if eq_productions(grammar, grammar_prod, patch_prod):
                grammar_prod = merge_productions(grammar, grammar_prod, patch_prod)
                applied = True
            new_rhs_list.append(grammar_prod)
        if not applied:
            raise ValueError("{}: Unable to find a matching production for {} in the grammar:\n {}"
                             .format(filename, name, grammar.production_to_str(name, patch_prod)))
        result = gnt_def.with_rhs_list(new_rhs_list)
        nonterminals[name] = result


class GrammarExtension:
    """A collection of grammar extensions, with added code, added traits for the
    action functions.

    """

    def __init__(self, target, grammar, filename):
        self.target = target
        self.grammar = grammar
        self.filename = filename

    def __repr__(self):
        return "GrammarExtension({}, {})".format(repr(self.target), repr(self.grammar))

    def apply_patch(self, grammar, nonterminals):
        # A grammar extension is composed of multiple production patches.
        for ext in self.grammar:
            if isinstance(ext, ExtPatch):
                ext.apply_patch(self.filename, grammar, nonterminals)
            else:
                raise ValueError("Extension of type {} not yet supported.".format(ext.__class__))
