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

  if (c.isIncomplete)
    throw Error("Can't get stack property for incomplete type.");

  if (!c.hasOwnProperty('isStack'))
    c.isStack = calculate();

  return c.isStack;
}

function isVoidPtr(t)
{
  return t.isPointer && t.type.name == 'void';
}

function xrange(start, end, skip)
{
  for (;
       (skip > 0) ? (start < end) : (start > end);
       start += skip)
    yield start;
}

function process_cp_pre_genericize(fndecl)
{
  function findconstructors(t, stack)
  {
    function getLocation() {
      let loc = location_of(t);
      if (loc !== undefined)
        return loc;

      for (let i = stack.length - 1; i >= 0; --i) {
        loc = location_of(stack[i]);
        if (loc !== undefined)
          return loc;
      }
      return location_of(DECL_SAVED_TREE(fndecl));
    }
    
    try {
      t.tree_check(CALL_EXPR);
      let fncall =
        callable_arg_function_decl(CALL_EXPR_FN(t));
      if (fncall == null)
        return;

      let nameid = DECL_NAME(fncall);
      if (IDENTIFIER_OPNAME_P(nameid)) {
        let name = IDENTIFIER_POINTER(nameid);
        
        if (name == "operator new" || name == "operator new []") {
          let fncallobj = dehydra_convert(TREE_TYPE(fncall));
          if (fncallobj.parameters.length == 2 &&
              isVoidPtr(fncallobj.parameters[1]))
            return;

          let i;
          for (i in xrange(stack.length - 1, -1, -1)) {
            if (TREE_CODE(stack[i]) != NOP_EXPR)
              break;
          }
          let assign = stack[i];
          switch (TREE_CODE(assign)) {
          case VAR_DECL:
            break;
            
          case INIT_EXPR:
          case MODIFY_EXPR:
          case TARGET_EXPR:
            assign = assign.operands()[1];
            break;

          case CALL_EXPR:
            assign = stack[i + 1];
            break;
            
          default:
            error("Unrecognized assignment from operator new: " + TREE_CODE(assign), getLocation());
            return;
          }
          
          let destType = dehydra_convert(TREE_TYPE(assign));
          if (!destType.isPointer && !destType.isReference) {
            error("operator new not assigned to pointer/ref?", getLocation());
            return;
          }
          destType = destType.type;

          let r = isStack(destType);
          if (r)
            warning("constructed object of type '%s' not on the stack: %s".format(destType.name, r), getLocation());
        }
      }
    }
    catch (e if e.TreeCheckError) { }
  }

  if (hasAttribute(dehydra_convert(fndecl), 'NS_suppress_stackcheck'))
    return;
  
  walk_tree(DECL_SAVED_TREE(fndecl), findconstructors);
}
