/**
 * A script for GCC-dehydra to analyze the Mozilla codebase and catch
 * patterns that are incorrect, but which cannot be detected by a compiler. */

/**
 * Activate Treehydra outparams analysis if running in Treehydra.
 */

function treehydra_enabled() {
  return this.hasOwnProperty('TREE_CODE');
}

include('unstable/getopt.js');
[options, args] = getopt();

// XXXbugfix: when you pass arguments to -fplugin-arg, include_path[0] is bad
sys.include_path[0] = options.topsrcdir + "/xpcom/analysis";
sys.include_path.push(options.topsrcdir);

let modules = [];

function LoadModules(modulelist)
{
  if (modulelist == "")
    return;

  let modulenames = modulelist.split(',');
  for each (let modulename in modulenames) {
    let module = { __proto__: this };
    include(modulename, module);
    modules.push(module);
  }
}

LoadModules(options['dehydra-modules']);
if (treehydra_enabled())
  LoadModules(options['treehydra-modules']);

/**
 * gClassMap maps class names to an object with the following properties:
 *
 * .final = true   if the class has been annotated as final, and may not be
 *                 subclassed
 * .stack = true   if the class has been annotated as a class which may only
 *                 be instantiated on the stack
 */
var gClassMap = {};

function ClassType(name)
{
  this.name = name;
}

ClassType.prototype = {
  final: false,
  stack: false,
};

function process_type(c)
{
  if (c.kind == 'class' || c.kind == 'struct')
    get_class(c, true);

  for each (let module in modules)
    if (module.hasOwnProperty('process_type'))
      module.process_type(c);
}

/**
 * Get the ClassType for a type 'c'
 *
 * If allowIncomplete is true and the type is incomplete, this function
 * will return null.
 *
 * If allowIncomplete is false and the type is incomplete, this function will
 * throw.
 */
function get_class(c, allowIncomplete)
{
  var classattr, base, member, type, realtype, foundConstructor;
  var bases = [];

  if (c.isIncomplete) {
    if (allowIncomplete)
      return null;

    throw Error("Can't process incomplete type '" + c + "'.");
  }

  if (gClassMap.hasOwnProperty(c.name)) {
    return gClassMap[c.name];
  }

  for each (base in c.bases) {
      realtype = get_class(base, allowIncomplete);
      if (realtype == null) {
        error("Complete type " + c + " has incomplete base " + base);
        return null;
      }

      bases.push(realtype);
  }

  function hasAttribute(attrname)
  {
    var attr;

    if (c.attributes === undefined)
      return false;

    for each (attr in c.attributes) {
      if (attr.name == 'user' && attr.value[0] == attrname) {
        return true;
      }
    }

    return false;
  }

  classattr = new ClassType(c.name);
  gClassMap[c.name] = classattr;

  // check for .final

  if (hasAttribute('NS_final')) {
    classattr.final = true;
  }

  // check for .stack

  if (hasAttribute('NS_stack')) {
    classattr.stack = true;
  }
  else {
    for each (base in bases) {
      if (base.stack) {
        classattr.stack = true;
        break;
      }
    }
  }

  if (!classattr.stack) {
    // Check members
    for each (member in c.members) {
      if (member.isFunction)
        continue;

      type = member.type;

      /* recurse through arrays and typedefs */
      while (true) {
          if (type === undefined) {
              break;
          }
              
          if (type.isArray) {
              type = type.type;
              continue;
          }
          if (type.typedef) {
              type = type.typedef;
              continue;
          }
          break;
      }

      if (type === undefined) {
          warning("incomplete type  for member " + member + ".");
          continue;
      }

      if (type.isPointer || type.isReference) {
          continue;
      }

      if (!type.kind || (type.kind != 'class' && type.kind != 'struct')) {
          continue;
      }

      var membertype = get_class(type, false);
      if (membertype.stack) {
        classattr.stack = true;
        break;
      }
    }
  }

  // Check for errors at declaration-time

  for each (base in bases) {
    if (base.final) {
      error("class '" + c.name + "' inherits from final class '" + base.name + "'.");
    }
  }

  // At the moment, any class that is .final has to have a constructor, or
  // we can't detect callsites... this may change with treehydra.
  if (classattr.stack) {
    foundConstructor = false;
    for each (member in c.members) {
      if (member.isConstructor) {
        foundConstructor = true;
        break;
      }
    }

    if (!foundConstructor) {
      warning(c.loc + ": class " + c.name + " is marked stack-only but doesn't have a constructor. Static checking can't detect instantiations of this class properly.");
    }
  }

  return classattr;
}

/**
 * Unwrap any array of types back to their base type.
 */
function unwrapArray(t)
{
  while (t.isArray) {
    t = t.type;
  }
  return t;
}

function process_function(f, stmts)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_function'))
      module.process_function(f, stmts);
}

function process_tree(fndecl)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_tree'))
      module.process_tree(fndecl);
}

function process_var(decl)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_var'))
      module.process_var(decl);
}

function input_end()
{
  for each (let module in modules)
    if (module.hasOwnProperty('input_end'))
      module.input_end();
}
