// You know this is black magic, I know this is black magic, the code knows
// it's actually black magic - but let's just leave it alone.

// Thank you Manisheart for the trait type part.

pub struct Maybe<T: MaybeTrait> {
    pub data: T::Data,
    pub is_some: ::std::os::raw::c_char,
}

impl<T: MaybeTrait, U: Into<T>> From<Option<U>> for Maybe<T> {
    fn from(o: Option<U>) -> Self {
        T::from_option(o)
    }
}

pub trait MaybeTrait {
    type Data;
    fn from_option<U: Into<Self>>(x: Option<U>) -> Maybe<Self>
    where
        Self: Sized;
}

#[macro_export]
macro_rules! maybe {
    ($t:ident) => {
        impl MaybeTrait for $t {
            type Data = [::std::os::raw::c_char; ::std::mem::size_of::<$t>()];
            fn from_option<U: Into<Self>>(o: Option<U>) -> Maybe<Self> {
                match o {
                    Some(x) => Maybe {
                        data: unsafe { ::std::mem::transmute(x.into()) },
                        is_some: 1,
                    },
                    None => Maybe {
                        data: unsafe { ::std::mem::zeroed() },
                        is_some: 0,
                    },
                }
            }
        }
    };
}
