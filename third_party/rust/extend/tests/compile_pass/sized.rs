use extend::ext_sized;

#[ext_sized(name = One)]
impl i32 {
    fn requires_sized(self) -> Option<Self> {
        Some(self)
    }
}

#[ext_sized(name = Two, supertraits = Default)]
impl i32 {
    fn with_another_supertrait(self) -> Option<Self> {
        Some(self)
    }
}

#[ext_sized(name = Three, supertraits = Default + Clone + Copy)]
impl i32 {
    fn multiple_supertraits(self) -> Option<Self> {
        Some(self)
    }
}

#[ext_sized(name = Four, supertraits = Sized)]
impl i32 {
    fn already_sized(self) -> Option<Self> {
        Some(self)
    }
}

fn main() {
    1.requires_sized();
    1.with_another_supertrait();
    1.multiple_supertraits();
    1.already_sized();
}
