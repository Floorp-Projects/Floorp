/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// List of possible interests for a domain.  Domains can have be associated with one or multiple
/// interests.  `Inconclusive` is used for domains in the user's top sites that we can't classify
/// because there's no corresponding entry in the interest database.
#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
#[repr(u32)]
pub enum Interest {
    Animals,
    Arts,
    Autos,
    Business,
    Career,
    Education,
    Fashion,
    Finance,
    Food,
    Government,
    Health,
    Hobbies,
    Home,
    News,
    RealEstate,
    Society,
    Sports,
    Tech,
    Travel,
    Inconclusive,
}

impl From<Interest> for u32 {
    fn from(interest: Interest) -> Self {
        interest as u32
    }
}

impl From<Interest> for usize {
    fn from(interest: Interest) -> Self {
        interest as usize
    }
}

impl From<u32> for Interest {
    fn from(code: u32) -> Self {
        if code as usize > Self::COUNT {
            panic!("Invalid interest code: {code}")
        }
        // Safety: This is safe since Interest has a u32 representation and we've done a bounds
        // check
        unsafe { std::mem::transmute(code) }
    }
}

impl Interest {
    const COUNT: usize = 20;

    pub fn all() -> [Interest; Self::COUNT] {
        [
            Self::Animals,
            Self::Arts,
            Self::Autos,
            Self::Business,
            Self::Career,
            Self::Education,
            Self::Fashion,
            Self::Finance,
            Self::Food,
            Self::Government,
            Self::Health,
            Self::Hobbies,
            Self::Home,
            Self::News,
            Self::RealEstate,
            Self::Society,
            Self::Sports,
            Self::Tech,
            Self::Travel,
            Self::Inconclusive,
        ]
    }
}

/// Vector storing a count value for each interest
///
/// Here "vector" refers to the mathematical object, not a Rust `Vec`.  It always has a fixed
/// number of elements.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct InterestVector {
    pub animals: u32,
    pub arts: u32,
    pub autos: u32,
    pub business: u32,
    pub career: u32,
    pub education: u32,
    pub fashion: u32,
    pub finance: u32,
    pub food: u32,
    pub government: u32,
    pub health: u32,
    pub hobbies: u32,
    pub home: u32,
    pub news: u32,
    pub real_estate: u32,
    pub society: u32,
    pub sports: u32,
    pub tech: u32,
    pub travel: u32,
    pub inconclusive: u32,
}

impl std::ops::Index<Interest> for InterestVector {
    type Output = u32;

    fn index(&self, index: Interest) -> &u32 {
        match index {
            Interest::Animals => &self.animals,
            Interest::Arts => &self.arts,
            Interest::Autos => &self.autos,
            Interest::Business => &self.business,
            Interest::Career => &self.career,
            Interest::Education => &self.education,
            Interest::Fashion => &self.fashion,
            Interest::Finance => &self.finance,
            Interest::Food => &self.food,
            Interest::Government => &self.government,
            Interest::Health => &self.health,
            Interest::Hobbies => &self.hobbies,
            Interest::Home => &self.home,
            Interest::News => &self.news,
            Interest::RealEstate => &self.real_estate,
            Interest::Society => &self.society,
            Interest::Sports => &self.sports,
            Interest::Tech => &self.tech,
            Interest::Travel => &self.travel,
            Interest::Inconclusive => &self.inconclusive,
        }
    }
}

impl std::ops::IndexMut<Interest> for InterestVector {
    fn index_mut(&mut self, index: Interest) -> &mut u32 {
        match index {
            Interest::Animals => &mut self.animals,
            Interest::Arts => &mut self.arts,
            Interest::Autos => &mut self.autos,
            Interest::Business => &mut self.business,
            Interest::Career => &mut self.career,
            Interest::Education => &mut self.education,
            Interest::Fashion => &mut self.fashion,
            Interest::Finance => &mut self.finance,
            Interest::Food => &mut self.food,
            Interest::Government => &mut self.government,
            Interest::Health => &mut self.health,
            Interest::Hobbies => &mut self.hobbies,
            Interest::Home => &mut self.home,
            Interest::News => &mut self.news,
            Interest::RealEstate => &mut self.real_estate,
            Interest::Society => &mut self.society,
            Interest::Sports => &mut self.sports,
            Interest::Tech => &mut self.tech,
            Interest::Travel => &mut self.travel,
            Interest::Inconclusive => &mut self.inconclusive,
        }
    }
}
