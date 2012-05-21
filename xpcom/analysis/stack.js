/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

require({ after_gcc_pass: "cfg" });

include("gcc_util.js");
include("unstable/lazy_types.js");

function process_type(c)
{
  if ((c.kind == 'class' || c.kind == 'struct') &&
      !c.isIncomplete)
    isStack(c);
}

/**
 * A BlameChain records a chain of one or more location/message pairs. It
 * can be used to issue a complex error message such as:
 * location: error: Allocated class Foo on the heap
 * locationofFoo:   class Foo inherits from class Bar
 * locationofBar:   in class Bar
 * locationofBarMem:   Member Bar::mFoo
 * locationofBaz:   class Baz is annotated NS_STACK
 */
function BlameChain(loc, message, prev)
{
  this.loc = loc;
  this.message = message;
  this.prev = prev;
}

BlameChain.prototype.toString = function()
{
  let loc = this.loc;
  if (loc === undefined)
    loc = "<unknown location>";

  let str = '%s:   %s'.format(loc.toString(), this.message);
  if (this.prev)
    str += "\n%s".format(this.prev);
  return str;
};

function isStack(c)
{
  function calculate()
  {
    if (hasAttribute(c, 'NS_stack'))
      return new BlameChain(c.loc, '%s %s is annotated NS_STACK_CLASS'.format(c.kind, c.name));

    for each (let base in c.bases) {
      let r = isStack(base.type);
      if (r != null)
        return new BlameChain(c.loc, '%s %s is a base of %s %s'.format(base.type.kind, base.type.name, c.kind, c.name), r);
    }

    for each (let member in c.members) {
      if (member.isFunction)
        continue;

      if (hasAttribute(member, 'NS_okonheap'))
        continue;

      let type = member.type;
      while (true) {
        if (type === undefined)
          break;

        if (type.isArray) {
          type = type.type;
          continue;
        }

        if (type.typedef) {
          if (hasAttribute(type, 'NS_stack'))
            return true;

          type = type.typedef;
          continue;
        }
        break;
      }

      if (type === undefined) {
        warning("incomplete type for member " + member + ".", member.loc);
        continue;
      }

      if (type.isPointer || type.isReference)
        continue;

      if (!type.kind || (type.kind != 'class' && type.kind != 'struct'))
        continue;

      let r = isStack(type);
      if (r != null)
        return new BlameChain(c.loc, 'In class %s'.format(c.name),
                                 new BlameChain(member.loc, 'Member %s'.format(member.name), r));
    }
    return null;
  }

  if (!c.hasOwnProperty('isStack'))
    c.isStack = calculate();

  return c.isStack;
}

function process_tree(fn)
{
  if (hasAttribute(dehydra_convert(fn), 'NS_suppress_stackcheck'))
    return;

  let cfg = function_decl_cfg(fn);

  for (let bb in cfg_bb_iterator(cfg)) {
    let it = bb_isn_iterator(bb);
    for (let isn in it) {
      if (isn.tree_code() != GIMPLE_CALL)
        continue;

      let name = gimple_call_function_name(isn);
      if (name != "operator new" && name != "operator new []")
        continue;

      // ignore placement new
      // TODO? ensure 2nd arg is local stack variable
      if (gimple_call_num_args(isn) == 2 &&
          TREE_TYPE(gimple_call_arg(isn, 1)).tree_code() == POINTER_TYPE)
        continue;

      let newLhs = gimple_call_lhs(isn);
      if (!newLhs)
        error("Non assigning call to operator new", location_of(isn));

      // if isn is the last of its block there are other problems...
      assign = it.next();

      // Calls to |new| are always followed by an assignment, casting the void ptr to which
      // |new| was assigned, to a ptr variable of the same type as the allocated object.
      // Exception: explicit calls to |::operator new (size_t)|, which can be ignored.
      if (assign.tree_code() != GIMPLE_ASSIGN)
        continue;

      let assignRhs = gimple_op(assign, 1);
      if (newLhs != assignRhs)
        continue;

      let assignLhs = gimple_op(assign, 0);
      let type = TREE_TYPE(TREE_TYPE(assignLhs));
      let dehydraType = dehydra_convert(type);

      let r = isStack(dehydraType);
      if (r)
        warning("constructed object of type '%s' not on the stack: %s".format(dehydraType.name, r), location_of(isn));
    }
  }
}
