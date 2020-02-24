//! The integration tests seem to always have `std` linked, so things that would
//! depend on that can go here.

use bytemuck::*;

#[test]
fn test_transparent_vtabled() {
  use core::fmt::Display;

  #[repr(transparent)]
  struct DisplayTraitObj(dyn Display);

  unsafe impl TransparentWrapper<dyn Display> for DisplayTraitObj {}

  impl Display for DisplayTraitObj {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
      self.0.fmt(f)
    }
  }

  let v = DisplayTraitObj::wrap_ref(&5i32);
  let s = format!("{}", v);
  assert_eq!(s, "5");

  let mut x = 100i32;
  let v_mut = DisplayTraitObj::wrap_mut(&mut x);
  let s = format!("{}", v_mut);
  assert_eq!(s, "100");
}
