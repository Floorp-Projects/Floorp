//! Utility macros
#![allow(unused)]


macro_rules! impl_unary_ {
    // implementation mapping 1:1
    (vec | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    transmute($fun(transmute(self)))
                }
            }
        }
    };
    // implementation mapping 1:1 for when `$fun` is a generic function
    // like some of the fp math rustc intrinsics (e.g. `fn fun<T>(x: T) -> T`).
    (gen | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    transmute($fun(self.0))
                }
            }
        }
    };
    (scalar | $trait_id:ident, $trait_method:ident,
     $vec_id:ident, [$sid:ident; $scount:expr], $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self) -> Self {
                unsafe {
                    union U {
                        vec: $vec_id,
                        scalars: [$sid; $scount],
                    }
                    let mut scalars = U { vec: self }.scalars;
                    for i in &mut scalars {
                        *i = $fun(*i);
                    }
                    U { scalars }.vec
                }
            }
        }
    };
    // implementation calling fun twice on each of the vector halves:
    (halves | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vech_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    union U {
                        vec: $vec_id,
                        halves: [$vech_id; 2],
                    }

                    let mut halves = U { vec: self }.halves;

                    *halves.get_unchecked_mut(0) =
                        transmute($fun(transmute(*halves.get_unchecked(0))));
                    *halves.get_unchecked_mut(1) =
                        transmute($fun(transmute(*halves.get_unchecked(1))));

                    U { halves }.vec
                }
            }
        }
    };
    // implementation calling fun four times on each of the vector quarters:
    (quarter | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vecq_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    union U {
                        vec: $vec_id,
                        quarters: [$vecq_id; 4],
                    }

                    let mut quarters = U { vec: self }.quarters;

                    *quarters.get_unchecked_mut(0) =
                        transmute($fun(transmute(*quarters.get_unchecked(0))));
                    *quarters.get_unchecked_mut(1) =
                        transmute($fun(transmute(*quarters.get_unchecked(1))));
                    *quarters.get_unchecked_mut(2) =
                        transmute($fun(transmute(*quarters.get_unchecked(2))));
                    *quarters.get_unchecked_mut(3) =
                        transmute($fun(transmute(*quarters.get_unchecked(3))));

                    U { quarters }.vec
                }
            }
        }
    };
    // implementation calling fun once on a vector twice as large:
    (twice | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vect_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self) -> Self {
                unsafe {
                    use crate::mem::{transmute, uninitialized};

                    union U {
                        vec: [$vec_id; 2],
                        twice: $vect_id,
                    }

                    let twice = U { vec: [self, uninitialized()] }.twice;
                    let twice = transmute($fun(transmute(twice)));

                    *(U { twice }.vec.get_unchecked(0))
                }
            }
        }
    };
}

macro_rules! gen_unary_impl_table {
    ($trait_id:ident, $trait_method:ident) => {
        macro_rules! impl_unary {
            ($vid:ident: $fun:ident) => {
                impl_unary_!(vec | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[g]: $fun:ident) => {
                impl_unary_!(gen | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[$sid:ident; $sc:expr]: $fun:ident) => {
                impl_unary_!(
                    scalar | $trait_id,
                    $trait_method,
                    $vid,
                    [$sid; $sc],
                    $fun
                );
            };
            ($vid:ident[s]: $fun:ident) => {
                impl_unary_!(scalar | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[h => $vid_h:ident]: $fun:ident) => {
                impl_unary_!(
                    halves | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_h,
                    $fun
                );
            };
            ($vid:ident[q => $vid_q:ident]: $fun:ident) => {
                impl_unary_!(
                    quarter | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_q,
                    $fun
                );
            };
            ($vid:ident[t => $vid_t:ident]: $fun:ident) => {
                impl_unary_!(
                    twice | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_t,
                    $fun
                );
            };
        }
    };
}

macro_rules! impl_tertiary_ {
    // implementation mapping 1:1
    (vec | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self, z: Self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    transmute($fun(
                        transmute(self),
                        transmute(y),
                        transmute(z),
                    ))
                }
            }
        }
    };
    (scalar | $trait_id:ident, $trait_method:ident,
     $vec_id:ident, [$sid:ident; $scount:expr], $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self, z: Self) -> Self {
                unsafe {
                    union U {
                        vec: $vec_id,
                        scalars: [$sid; $scount],
                    }
                    let mut x = U { vec: self }.scalars;
                    let y = U { vec: y }.scalars;
                    let z = U { vec: z }.scalars;
                    for (x, (y, z)) in (&mut scalars).zip(&y).zip(&z) {
                        *i = $fun(*i, *y, *z);
                    }
                    U { vec: x }.vec
                }
            }
        }
    };
    // implementation calling fun twice on each of the vector halves:
    (halves | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vech_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self, z: Self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    union U {
                        vec: $vec_id,
                        halves: [$vech_id; 2],
                    }

                    let mut x_halves = U { vec: self }.halves;
                    let y_halves = U { vec: y }.halves;
                    let z_halves = U { vec: z }.halves;

                    *x_halves.get_unchecked_mut(0) = transmute($fun(
                        transmute(*x_halves.get_unchecked(0)),
                        transmute(*y_halves.get_unchecked(0)),
                        transmute(*z_halves.get_unchecked(0)),
                    ));
                    *x_halves.get_unchecked_mut(1) = transmute($fun(
                        transmute(*x_halves.get_unchecked(1)),
                        transmute(*y_halves.get_unchecked(1)),
                        transmute(*z_halves.get_unchecked(1)),
                    ));

                    U { halves: x_halves }.vec
                }
            }
        }
    };
    // implementation calling fun four times on each of the vector quarters:
    (quarter | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vecq_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self, z: Self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    union U {
                        vec: $vec_id,
                        quarters: [$vecq_id; 4],
                    }

                    let mut x_quarters = U { vec: self }.quarters;
                    let y_quarters = U { vec: y }.quarters;
                    let z_quarters = U { vec: z }.quarters;

                    *x_quarters.get_unchecked_mut(0) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(0)),
                        transmute(*y_quarters.get_unchecked(0)),
                        transmute(*z_quarters.get_unchecked(0)),
                    ));

                    *x_quarters.get_unchecked_mut(1) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(1)),
                        transmute(*y_quarters.get_unchecked(1)),
                        transmute(*z_quarters.get_unchecked(1)),
                    ));

                    *x_quarters.get_unchecked_mut(2) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(2)),
                        transmute(*y_quarters.get_unchecked(2)),
                        transmute(*z_quarters.get_unchecked(2)),
                    ));

                    *x_quarters.get_unchecked_mut(3) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(3)),
                        transmute(*y_quarters.get_unchecked(3)),
                        transmute(*z_quarters.get_unchecked(3)),
                    ));

                    U { quarters: x_quarters }.vec
                }
            }
        }
    };
    // implementation calling fun once on a vector twice as large:
    (twice | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vect_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self, z: Self) -> Self {
                unsafe {
                    use crate::mem::{transmute, uninitialized};

                    union U {
                        vec: [$vec_id; 2],
                        twice: $vect_id,
                    }

                    let x_twice = U { vec: [self, uninitialized()] }.twice;
                    let y_twice = U { vec: [y, uninitialized()] }.twice;
                    let z_twice = U { vec: [z, uninitialized()] }.twice;
                    let twice: $vect_id = transmute($fun(
                        transmute(x_twice),
                        transmute(y_twice),
                        transmute(z_twice),
                    ));

                    *(U { twice }.vec.get_unchecked(0))
                }
            }
        }
    };
}

macro_rules! gen_tertiary_impl_table {
    ($trait_id:ident, $trait_method:ident) => {
        macro_rules! impl_tertiary {
            ($vid:ident: $fun:ident) => {
                impl_tertiary_!(vec | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[$sid:ident; $sc:expr]: $fun:ident) => {
                impl_tertiary_!(
                    scalar | $trait_id,
                    $trait_method,
                    $vid,
                    [$sid; $sc],
                    $fun
                );
            };
            ($vid:ident[s]: $fun:ident) => {
                impl_tertiary_!(scalar | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[h => $vid_h:ident]: $fun:ident) => {
                impl_tertiary_!(
                    halves | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_h,
                    $fun
                );
            };
            ($vid:ident[q => $vid_q:ident]: $fun:ident) => {
                impl_tertiary_!(
                    quarter | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_q,
                    $fun
                );
            };
            ($vid:ident[t => $vid_t:ident]: $fun:ident) => {
                impl_tertiary_!(
                    twice | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_t,
                    $fun
                );
            };
        }
    };
}

macro_rules! impl_binary_ {
    // implementation mapping 1:1
    (vec | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    transmute($fun(transmute(self), transmute(y)))
                }
            }
        }
    };
    (scalar | $trait_id:ident, $trait_method:ident,
     $vec_id:ident, [$sid:ident; $scount:expr], $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self) -> Self {
                unsafe {
                    union U {
                        vec: $vec_id,
                        scalars: [$sid; $scount],
                    }
                    let mut x = U { vec: self }.scalars;
                    let y = U { vec: y }.scalars;
                    for (x, y) in x.iter_mut().zip(&y) {
                        *x = $fun(*x, *y);
                    }
                    U { scalars: x }.vec
                }
            }
        }
    };
    // implementation calling fun twice on each of the vector halves:
    (halves | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vech_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    union U {
                        vec: $vec_id,
                        halves: [$vech_id; 2],
                    }

                    let mut x_halves = U { vec: self }.halves;
                    let y_halves = U { vec: y }.halves;

                    *x_halves.get_unchecked_mut(0) = transmute($fun(
                        transmute(*x_halves.get_unchecked(0)),
                        transmute(*y_halves.get_unchecked(0)),
                    ));
                    *x_halves.get_unchecked_mut(1) = transmute($fun(
                        transmute(*x_halves.get_unchecked(1)),
                        transmute(*y_halves.get_unchecked(1)),
                    ));

                    U { halves: x_halves }.vec
                }
            }
        }
    };
    // implementation calling fun four times on each of the vector quarters:
    (quarter | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vecq_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self) -> Self {
                unsafe {
                    use crate::mem::transmute;
                    union U {
                        vec: $vec_id,
                        quarters: [$vecq_id; 4],
                    }

                    let mut x_quarters = U { vec: self }.quarters;
                    let y_quarters = U { vec: y }.quarters;

                    *x_quarters.get_unchecked_mut(0) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(0)),
                        transmute(*y_quarters.get_unchecked(0)),
                    ));

                    *x_quarters.get_unchecked_mut(1) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(1)),
                        transmute(*y_quarters.get_unchecked(1)),
                    ));

                    *x_quarters.get_unchecked_mut(2) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(2)),
                        transmute(*y_quarters.get_unchecked(2)),
                    ));

                    *x_quarters.get_unchecked_mut(3) = transmute($fun(
                        transmute(*x_quarters.get_unchecked(3)),
                        transmute(*y_quarters.get_unchecked(3)),
                    ));

                    U { quarters: x_quarters }.vec
                }
            }
        }
    };
    // implementation calling fun once on a vector twice as large:
    (twice | $trait_id:ident, $trait_method:ident, $vec_id:ident,
     $vect_id:ident, $fun:ident) => {
        impl $trait_id for $vec_id {
            #[inline]
            fn $trait_method(self, y: Self) -> Self {
                unsafe {
                    use crate::mem::{transmute, uninitialized};

                    union U {
                        vec: [$vec_id; 2],
                        twice: $vect_id,
                    }

                    let x_twice = U { vec: [self, uninitialized()] }.twice;
                    let y_twice = U { vec: [y, uninitialized()] }.twice;
                    let twice: $vect_id = transmute($fun(
                        transmute(x_twice),
                        transmute(y_twice),
                    ));

                    *(U { twice }.vec.get_unchecked(0))
                }
            }
        }
    };
}

macro_rules! gen_binary_impl_table {
    ($trait_id:ident, $trait_method:ident) => {
        macro_rules! impl_binary {
            ($vid:ident: $fun:ident) => {
                impl_binary_!(vec | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[$sid:ident; $sc:expr]: $fun:ident) => {
                impl_binary_!(
                    scalar | $trait_id,
                    $trait_method,
                    $vid,
                    [$sid; $sc],
                    $fun
                );
            };
            ($vid:ident[s]: $fun:ident) => {
                impl_binary_!(scalar | $trait_id, $trait_method, $vid, $fun);
            };
            ($vid:ident[h => $vid_h:ident]: $fun:ident) => {
                impl_binary_!(
                    halves | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_h,
                    $fun
                );
            };
            ($vid:ident[q => $vid_q:ident]: $fun:ident) => {
                impl_binary_!(
                    quarter | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_q,
                    $fun
                );
            };
            ($vid:ident[t => $vid_t:ident]: $fun:ident) => {
                impl_binary_!(
                    twice | $trait_id,
                    $trait_method,
                    $vid,
                    $vid_t,
                    $fun
                );
            };
        }
    };
}
