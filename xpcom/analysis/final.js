function process_type(c)
{
  if ((c.kind == 'class' || c.kind == 'struct') && !c.isIncomplete) {
    for each (let base in c.bases)
      if (isFinal(base.type))
        error("Class '" + c.name + "' derives from final class '" + base.name + "'.", c.loc);
  }
}

function isFinal(c)
{
  if (c.isIncomplete)
    throw Error("Can't get final property for incomplete type.");

  return hasAttribute(c, 'NS_final');
}
