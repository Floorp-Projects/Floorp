/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Detect classes that should have overridden members of their parent
 * classes but didn't.
 *
 * Example:
 *
 * struct S {
 *   virtual NS_MUST_OVERRIDE void f();
 *   virtual void g();
 * };
 *
 * struct A : S { virtual void f(); }; // ok
 * struct B : S { virtual NS_MUST_OVERRIDE void f(); }; // also ok
 *
 * struct C : S { virtual void g(); }; // ERROR: must override f()
 * struct D : S { virtual void f(int); }; // ERROR: different overload
 * struct E : A { }; // ok: A's definition of f() is good for subclasses
 * struct F : B { }; // ERROR: B's definition of f() is still must-override
 *
 * We don't care if you define the method or not.
 */

function get_must_overrides(cls)
{
  let mos = {};
  for each (let base in cls.bases)
    for each (let m in base.type.members)
      if (hasAttribute(m, 'NS_must_override')) 
       mos[m.shortName] = m;

  return mos;
}

function process_type(t)
{
  if (t.isIncomplete || (t.kind != 'class' && t.kind != 'struct'))
    return;

  let mos = get_must_overrides(t);
  for each (let m in t.members) {
    let mos_m = mos[m.shortName]
    if (mos_m && signaturesMatch(mos_m, m))
      delete mos[m.shortName];
  }

  for each (let u in mos)
    error(t.kind + " " + t.name + " must override " + u.name, t.loc);
}
