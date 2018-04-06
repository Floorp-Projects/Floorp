use std::collections::HashMap;
use std::cell::RefCell;
use std::fmt::{Debug, Display, Error, Formatter};
use std::cmp::{Ord, Ordering, PartialOrd};

#[cfg(test)]
mod test;

thread_local! {
    static INTERNER_TLS: RefCell<Interner> =
        RefCell::new(Interner::new())
}

pub struct Interner {
    map: HashMap<String, InternedString>,
    strings: Vec<String>,
}

#[derive(Copy, Clone, Hash, Eq, PartialEq)]
pub struct InternedString {
    index: u32,
}

pub fn intern(s: &str) -> InternedString {
    write(|interner| {
        match interner.map.get(s) {
            Some(&v) => {
                return v;
            }
            None => {}
        }

        let index = interner.strings.len() as u32;
        let result = InternedString { index: index };
        interner.map.insert(s.to_string(), result);
        interner.strings.push(s.to_string());
        return result;
    })
}

pub fn read<F, R>(f: F) -> R
where
    F: FnOnce(&Interner) -> R,
{
    INTERNER_TLS.with(|interner| f(&*interner.borrow()))
}

fn write<F, R>(f: F) -> R
where
    F: FnOnce(&mut Interner) -> R,
{
    INTERNER_TLS.with(|interner| f(&mut *interner.borrow_mut()))
}

impl Interner {
    fn new() -> Interner {
        Interner {
            map: HashMap::new(),
            strings: vec![],
        }
    }

    pub fn data(&self, i: InternedString) -> &str {
        &self.strings[i.index()]
    }
}

impl InternedString {
    fn index(&self) -> usize {
        self.index as usize
    }

    pub fn len(&self) -> usize {
        read(|interner| interner.data(*self).len())
    }
}

impl Debug for InternedString {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        read(|interner| Debug::fmt(&interner.data(*self), fmt))
    }
}

impl Display for InternedString {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        read(|interner| Display::fmt(&interner.data(*self), fmt))
    }
}

impl PartialOrd<InternedString> for InternedString {
    fn partial_cmp(&self, other: &InternedString) -> Option<Ordering> {
        read(|interner| PartialOrd::partial_cmp(interner.data(*self), interner.data(*other)))
    }
}

impl Ord for InternedString {
    fn cmp(&self, other: &InternedString) -> Ordering {
        read(|interner| Ord::cmp(interner.data(*self), interner.data(*other)))
    }
}
