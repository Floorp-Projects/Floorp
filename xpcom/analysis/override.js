/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NS_OVERRIDE may be marked on class methods which are intended to override
 * a method in a base class. If the method is removed or altered in the base
 * class, the compiler will force all subclass overrides to be modified.
 */

/**
 * Generate all the base classes recursively of class `c`.
 */
function all_bases(c)
{
  for each (let b in c.bases) {
    yield b.type;
    for (let bb in all_bases(b.type))
      yield bb;
  }
}

function process_decl(d)
{
  if (!hasAttribute(d, 'NS_override'))
    return;

  if (!d.memberOf || !d.isFunction) {
    error("%s is marked NS_OVERRIDE but is not a class function.".format(d.name), d.loc);
    return;
  }

  if (d.isStatic) {
    error("Marking NS_OVERRIDE on static function %s is meaningless.".format(d.name), d.loc);
    return;
  }

  for (let base in all_bases(d.memberOf)) {
    for each (let m in base.members) {
      if (m.shortName == d.shortName && signaturesMatch(m, d))
	return;
    }
  }

  error("NS_OVERRIDE function %s does not override a base class method with the same name and signature".format(d.name), d.loc);
}
