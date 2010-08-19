function process_type(c)
{
  if ((c.kind == 'class' || c.kind == 'struct') && !c.isIncomplete) {
    for each (let base in c.bases)
      if (isFinal(base.type))
        error("Class '" + c.name + "' derives from final class '" + base.type.name + "'.", c.loc);
  }
}

function process_function(decl, body)
{
  if (!decl.memberOf)
    return;

  let c = decl.memberOf;
  if ((c.kind == 'class' || c.kind == 'struct') && !c.isIncomplete) {
    for each (let base in ancestorTypes(c))
      for each (let member in base.members)
        if (member.isFunction && isFinal(member) && member.shortName == decl.shortName)
          error("Function '" + decl.name + "' overrides final ancestor in '" +
                base.name + "'.", c.loc);
  }
}

function ancestorTypes(c)
{
  for each (let base in c.bases) {
    yield base.type;
    for (let bb in ancestorTypes(base.type))
      yield bb;
  }
}

function isFinal(c)
{
  return hasAttribute(c, 'NS_final');
}
