function process_function(f, stmts)
{
  var stmt;
  function getLocation()
  {
    if (stmt.loc)
      return stmt.loc;

    return f.loc;
  }

  function processVar(v)
  {
    if (v.isConstructor &&
        v.fieldOf &&
        get_class(v.memberOf, false).stack &&
        v.fieldOf.type.isPointer) {
      error(getLocation() + ": constructed object of type '" +
            v.memberOf.name + "' not on the stack.");
    }
  }

  for each (stmt in stmts) {
    iter(processVar, stmt.statements);
  }
}
