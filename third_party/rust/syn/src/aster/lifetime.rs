use {Ident, Lifetime, LifetimeDef};
use aster::invoke::{Invoke, Identity};

// ////////////////////////////////////////////////////////////////////////////

pub trait IntoLifetime {
    fn into_lifetime(self) -> Lifetime;
}

impl IntoLifetime for Lifetime {
    fn into_lifetime(self) -> Lifetime {
        self
    }
}

impl<'a> IntoLifetime for &'a str {
    fn into_lifetime(self) -> Lifetime {
        Lifetime { ident: self.into() }
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub trait IntoLifetimeDef {
    fn into_lifetime_def(self) -> LifetimeDef;
}

impl IntoLifetimeDef for LifetimeDef {
    fn into_lifetime_def(self) -> LifetimeDef {
        self
    }
}

impl IntoLifetimeDef for Lifetime {
    fn into_lifetime_def(self) -> LifetimeDef {
        LifetimeDef {
            lifetime: self,
            bounds: vec![],
        }
    }
}

impl<'a> IntoLifetimeDef for &'a str {
    fn into_lifetime_def(self) -> LifetimeDef {
        self.into_lifetime().into_lifetime_def()
    }
}

impl IntoLifetimeDef for String {
    fn into_lifetime_def(self) -> LifetimeDef {
        (*self).into_lifetime().into_lifetime_def()
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct LifetimeDefBuilder<F = Identity> {
    callback: F,
    lifetime: Lifetime,
    bounds: Vec<Lifetime>,
}

impl LifetimeDefBuilder {
    pub fn new<N>(name: N) -> Self
        where N: Into<Ident>
    {
        LifetimeDefBuilder::with_callback(name, Identity)
    }
}

impl<F> LifetimeDefBuilder<F>
    where F: Invoke<LifetimeDef>
{
    pub fn with_callback<N>(name: N, callback: F) -> Self
        where N: Into<Ident>
    {
        let lifetime = Lifetime { ident: name.into() };

        LifetimeDefBuilder {
            callback: callback,
            lifetime: lifetime,
            bounds: Vec::new(),
        }
    }

    pub fn bound<N>(mut self, name: N) -> Self
        where N: Into<Ident>
    {
        let lifetime = Lifetime { ident: name.into() };

        self.bounds.push(lifetime);
        self
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(LifetimeDef {
            lifetime: self.lifetime,
            bounds: self.bounds,
        })
    }
}
