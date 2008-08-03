include("gcc_util.js");
include("unstable/lazy_types.js");

function process_type(c)
{
  if ((c.kind == 'class' || c.kind == 'struct') &&
      !c.isIncomplete)
    isStack(c);
}

function isStack(c)
{
  function calculate()
  {
    if (hasAttribute(c, 'NS_stack'))
      return true;

    for each (let base in c.bases)
      if (isStack(base))
        return true;

    for each (let member in c.members) {
      if (member.isFunction)
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

      if (isStack(type))
        return true;
    }
    return false;
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

/**
 * Detect a call to operator new. If this is operator new, and not
 * placement-new, return the VAR_DECL of the temporary variable that operator
 * new is always assigned to.
 */
function operator_new_assign(stmt)
{
  try {
    stmt.tree_check(GIMPLE_MODIFY_STMT);
    let [varDecl, callExpr] = stmt.operands();
    varDecl.tree_check(VAR_DECL);
    callExpr.tree_check(CALL_EXPR);
    
    let fncall = callable_arg_function_decl(CALL_EXPR_FN(callExpr)).
                   tree_check(FUNCTION_DECL);

    let nameid = DECL_NAME(fncall);
    if (IDENTIFIER_OPNAME_P(nameid)) {
      let name = IDENTIFIER_POINTER(nameid);
      if (name == "operator new" || name == "operator new []") {
        // if this is placement-new, ignore it (should we issue a warning?)
        let fncallobj = dehydra_convert(TREE_TYPE(fncall));
        if (fncallobj.parameters.length == 2 &&
            isVoidPtr(fncallobj.parameters[1]))
          return null;

        return varDecl;
      }
    }
  }
  catch (e if e.TreeCheckError) { }

  return null;
}

function process_tree(fndecl)
{
  function findconstructors(t, stack)
  {
    function getLocation(t) {
      let loc;
      if (t) {
        loc = location_of(t);
        if (loc !== undefined)
          return loc;
      }

      for (let i = stack.length - 1; i >= 0; --i) {
        let loc = location_of(stack[i]);
        if (loc !== undefined)
          return loc;
      }
      return location_of(DECL_SAVED_TREE(fndecl));
    }
    
    function check_opnew_assignment(varDecl, stmt)
    {
      if (TREE_CODE(stmt) != GIMPLE_MODIFY_STMT) {
        warning("operator new not followed by a GIMPLE_MODIFY_STMT: " + TREE_CODE(stmt), getLocation(stmt));
        return;
      }

      let [destVar, assign] = stmt.operands();
      if (TREE_CODE(assign) == NOP_EXPR)
        assign = assign.operands()[0];

      if (assign != varDecl) {
        warning("operator new not followed by an known assignment pattern", getLocation(stmt));
        return;
      }

      let destType = dehydra_convert(TREE_TYPE(destVar));
      if (!destType.isPointer && !destType.isReference) {
        error("operator new not assigned to pointer/ref?", getLocation(stmt));
        return;
      }
      destType = destType.type;

      if (isStack(destType))
        error("constructed object of type '" + destType.name + "' not on the stack.", getLocation(stmt));
    }

    if (TREE_CODE(t) != STATEMENT_LIST)
      return;

    // if we find a tuple of "operator new" / GIMPLE_MODIFY_STMT casting
    // the result of operator new to a pointer type
    let opnew = null;
    for (let stmt in iter_statement_list(t)) {
      if (opnew != null)
        check_opnew_assignment(opnew, stmt);

      opnew = operator_new_assign(stmt);
    }

    if (opnew != null)
      warning("operator new not followed by an assignment", getLocation());
  }
  
  let tmap = new Map();
  walk_tree(DECL_SAVED_TREE(fndecl), findconstructors, tmap);
}
