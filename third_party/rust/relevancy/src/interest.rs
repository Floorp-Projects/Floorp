/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::Error;

/// List of possible interests for a domain.  Domains can have be associated with one or multiple
/// interests.  `Inconclusive` is used for domains in the user's top sites that we can't classify
/// because there's no corresponding entry in the interest database.
#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
#[repr(u32)]
pub enum Interest {
    // Note: if you change these codes, make sure to update the `TryFrom<u32>` implementation and
    // the `test_interest_code_conversion` test.
    Inconclusive = 0,
    Animals = 1,
    Arts = 2,
    Autos = 3,
    Business = 4,
    Career = 5,
    Education = 6,
    Fashion = 7,
    Finance = 8,
    Food = 9,
    Government = 10,
    //Disable this per policy consultation
    // Health = 11,
    Hobbies = 12,
    Home = 13,
    News = 14,
    RealEstate = 15,
    Society = 16,
    Sports = 17,
    Tech = 18,
    Travel = 19,
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

impl TryFrom<u32> for Interest {
    // On error, return the invalid code back
    type Error = Error;

    fn try_from(code: u32) -> Result<Self, Self::Error> {
        match code {
            0 => Ok(Self::Inconclusive),
            1 => Ok(Self::Animals),
            2 => Ok(Self::Arts),
            3 => Ok(Self::Autos),
            4 => Ok(Self::Business),
            5 => Ok(Self::Career),
            6 => Ok(Self::Education),
            7 => Ok(Self::Fashion),
            8 => Ok(Self::Finance),
            9 => Ok(Self::Food),
            10 => Ok(Self::Government),
            //Disable this per policy consultation
            // 11 => Ok(Self::Health),
            12 => Ok(Self::Hobbies),
            13 => Ok(Self::Home),
            14 => Ok(Self::News),
            15 => Ok(Self::RealEstate),
            16 => Ok(Self::Society),
            17 => Ok(Self::Sports),
            18 => Ok(Self::Tech),
            19 => Ok(Self::Travel),
            n => Err(Error::InvalidInterestCode(n)),
        }
    }
}

impl Interest {
    const COUNT: usize = 19;

    pub fn all() -> [Interest; Self::COUNT] {
        [
            Self::Inconclusive,
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
            // Self::Health,
            Self::Hobbies,
            Self::Home,
            Self::News,
            Self::RealEstate,
            Self::Society,
            Self::Sports,
            Self::Tech,
            Self::Travel,
        ]
    }
}

/// Vector storing a count value for each interest
///
/// Here "vector" refers to the mathematical object, not a Rust `Vec`.  It always has a fixed
/// number of elements.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct InterestVector {
    pub inconclusive: u32,
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
    // pub health: u32,
    pub hobbies: u32,
    pub home: u32,
    pub news: u32,
    pub real_estate: u32,
    pub society: u32,
    pub sports: u32,
    pub tech: u32,
    pub travel: u32,
}

impl std::ops::Add for InterestVector {
    type Output = Self;

    fn add(self, other: Self) -> Self {
        Self {
            inconclusive: self.inconclusive + other.inconclusive,
            animals: self.animals + other.animals,
            arts: self.arts + other.arts,
            autos: self.autos + other.autos,
            business: self.business + other.business,
            career: self.career + other.career,
            education: self.education + other.education,
            fashion: self.fashion + other.fashion,
            finance: self.finance + other.finance,
            food: self.food + other.food,
            government: self.government + other.government,
            hobbies: self.hobbies + other.hobbies,
            home: self.home + other.home,
            news: self.news + other.news,
            real_estate: self.real_estate + other.real_estate,
            society: self.society + other.society,
            sports: self.sports + other.sports,
            tech: self.tech + other.tech,
            travel: self.travel + other.travel,
        }
    }
}

impl std::ops::Index<Interest> for InterestVector {
    type Output = u32;

    fn index(&self, index: Interest) -> &u32 {
        match index {
            Interest::Inconclusive => &self.inconclusive,
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
            // Interest::Health => &self.health,
            Interest::Hobbies => &self.hobbies,
            Interest::Home => &self.home,
            Interest::News => &self.news,
            Interest::RealEstate => &self.real_estate,
            Interest::Society => &self.society,
            Interest::Sports => &self.sports,
            Interest::Tech => &self.tech,
            Interest::Travel => &self.travel,
        }
    }
}

impl std::ops::IndexMut<Interest> for InterestVector {
    fn index_mut(&mut self, index: Interest) -> &mut u32 {
        match index {
            Interest::Inconclusive => &mut self.inconclusive,
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
            // Interest::Health => &mut self.health,
            Interest::Hobbies => &mut self.hobbies,
            Interest::Home => &mut self.home,
            Interest::News => &mut self.news,
            Interest::RealEstate => &mut self.real_estate,
            Interest::Society => &mut self.society,
            Interest::Sports => &mut self.sports,
            Interest::Tech => &mut self.tech,
            Interest::Travel => &mut self.travel,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_interest_code_conversion() {
        for interest in Interest::all() {
            assert_eq!(Interest::try_from(u32::from(interest)).unwrap(), interest)
        }
        // try_from() for out of bounds codes should return an error
        assert!(matches!(
            Interest::try_from(20),
            Err(Error::InvalidInterestCode(20))
        ));
        assert!(matches!(
            Interest::try_from(100),
            Err(Error::InvalidInterestCode(100))
        ));
        // Health is currently disabled, so it's code should return None for now
        assert!(matches!(
            Interest::try_from(11),
            Err(Error::InvalidInterestCode(11))
        ));
    }
}
