#![feature(plugin)]
#![plugin(stainless)]

extern crate ordered_float;
extern crate num_traits;

pub use ordered_float::*;
pub use num_traits::Float;
pub use std::cmp::Ordering::*;
pub use std::{f32, f64, panic};

describe! ordered_float32 {
    it "should compare regular floats" {
        assert_eq!(OrderedFloat(7.0f32).cmp(&OrderedFloat(7.0)), Equal);
        assert_eq!(OrderedFloat(8.0f32).cmp(&OrderedFloat(7.0)), Greater);
        assert_eq!(OrderedFloat(4.0f32).cmp(&OrderedFloat(7.0)), Less);
    }

    it "should compare NaN" {
        let f32_nan: f32 = Float::nan();
        assert_eq!(OrderedFloat(f32_nan).cmp(&OrderedFloat(Float::nan())), Equal);
        assert_eq!(OrderedFloat(f32_nan).cmp(&OrderedFloat(-100000.0f32)), Greater);
        assert_eq!(OrderedFloat(-100.0f32).cmp(&OrderedFloat(Float::nan())), Less);
    }
}

describe! ordered_float64 {
    it "should compare regular floats" {
        assert_eq!(OrderedFloat(7.0f64).cmp(&OrderedFloat(7.0)), Equal);
        assert_eq!(OrderedFloat(8.0f64).cmp(&OrderedFloat(7.0)), Greater);
        assert_eq!(OrderedFloat(4.0f64).cmp(&OrderedFloat(7.0)), Less);
    }

    it "should compare NaN" {
        let f64_nan: f64 = Float::nan();
        assert_eq!(OrderedFloat(f64_nan).cmp(&OrderedFloat(Float::nan())), Equal);
        assert_eq!(OrderedFloat(f64_nan).cmp(&OrderedFloat(-100000.0f64)), Greater);
        assert_eq!(OrderedFloat(-100.0f64).cmp(&OrderedFloat(Float::nan())), Less);
    }
}

describe! not_nan32 {
    it "should compare regular floats" {
        assert_eq!(NotNaN::from(7.0f32).cmp(&NotNaN::from(7.0)), Equal);
        assert_eq!(NotNaN::from(8.0f32).cmp(&NotNaN::from(7.0)), Greater);
        assert_eq!(NotNaN::from(4.0f32).cmp(&NotNaN::from(7.0)), Less);
    }

    it "should fail when constructing NotNaN with NaN" {
        let f32_nan: f32 = Float::nan();
        assert!(NotNaN::new(f32_nan).is_err());
    }
    
    it "should calculate correctly" {
        assert_eq!(*(NotNaN::from(5.0f32) + NotNaN::from(4.0f32)), 5.0f32 + 4.0f32);
        assert_eq!(*(NotNaN::from(5.0f32) + 4.0f32), 5.0f32 + 4.0f32);
        assert_eq!(*(NotNaN::from(5.0f32) - NotNaN::from(4.0f32)), 5.0f32 - 4.0f32);
        assert_eq!(*(NotNaN::from(5.0f32) - 4.0f32), 5.0f32 - 4.0f32);
        assert_eq!(*(NotNaN::from(5.0f32) * NotNaN::from(4.0f32)), 5.0f32 * 4.0f32);
        assert_eq!(*(NotNaN::from(5.0f32) * 4.0f32), 5.0f32 * 4.0f32);
        assert_eq!(*(NotNaN::from(8.0f32) / NotNaN::from(4.0f32)), 8.0f32 / 4.0f32);
        assert_eq!(*(NotNaN::from(8.0f32) / 4.0f32), 8.0f32 / 4.0f32);
        assert_eq!(*(NotNaN::from(8.0f32) % NotNaN::from(4.0f32)), 8.0f32 % 4.0f32);
        assert_eq!(*(NotNaN::from(8.0f32) % 4.0f32), 8.0f32 % 4.0f32);
        assert_eq!(*(-NotNaN::from(1.0f32)), -1.0f32);
        
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f32) + f32::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f32) - f32::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f32) * f32::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f32) / f32::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f32) % f32::NAN}).is_err());
        
        let mut number = NotNaN::from(5.0f32);
        number += NotNaN::from(4.0f32);
        assert_eq!(*number, 9.0f32);
        number -= NotNaN::from(4.0f32);
        assert_eq!(*number, 5.0f32);
        number *= NotNaN::from(4.0f32);
        assert_eq!(*number, 20.0f32);
        number /= NotNaN::from(4.0f32);
        assert_eq!(*number, 5.0f32);
        number %= NotNaN::from(4.0f32);
        assert_eq!(*number, 1.0f32);
        
        number = NotNaN::from(5.0f32);
        number += 4.0f32;
        assert_eq!(*number, 9.0f32);
        number -= 4.0f32;
        assert_eq!(*number, 5.0f32);
        number *= 4.0f32;
        assert_eq!(*number, 20.0f32);
        number /= 4.0f32;
        assert_eq!(*number, 5.0f32);
        number %= 4.0f32;
        assert_eq!(*number, 1.0f32);
        
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f32); tmp += f32::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f32); tmp -= f32::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f32); tmp *= f32::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f32); tmp /= f32::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f32); tmp %= f32::NAN;}).is_err());
    }
}

describe! not_nan64 {
    it "should compare regular floats" {
        assert_eq!(NotNaN::from(7.0f64).cmp(&NotNaN::from(7.0)), Equal);
        assert_eq!(NotNaN::from(8.0f64).cmp(&NotNaN::from(7.0)), Greater);
        assert_eq!(NotNaN::from(4.0f64).cmp(&NotNaN::from(7.0)), Less);
    }

    it "should fail when constructing NotNaN with NaN" {
        let f64_nan: f64 = Float::nan();
        assert!(NotNaN::new(f64_nan).is_err());
    }
    
    it "should calculate correctly" {
        assert_eq!(*(NotNaN::from(5.0f64) + NotNaN::from(4.0f64)), 5.0f64 + 4.0f64);
        assert_eq!(*(NotNaN::from(5.0f64) + 4.0f64), 5.0f64 + 4.0f64);
        assert_eq!(*(NotNaN::from(5.0f64) - NotNaN::from(4.0f64)), 5.0f64 - 4.0f64);
        assert_eq!(*(NotNaN::from(5.0f64) - 4.0f64), 5.0f64 - 4.0f64);
        assert_eq!(*(NotNaN::from(5.0f64) * NotNaN::from(4.0f64)), 5.0f64 * 4.0f64);
        assert_eq!(*(NotNaN::from(5.0f64) * 4.0f64), 5.0f64 * 4.0f64);
        assert_eq!(*(NotNaN::from(8.0f64) / NotNaN::from(4.0f64)), 8.0f64 / 4.0f64);
        assert_eq!(*(NotNaN::from(8.0f64) / 4.0f64), 8.0f64 / 4.0f64);
        assert_eq!(*(NotNaN::from(8.0f64) % NotNaN::from(4.0f64)), 8.0f64 % 4.0f64);
        assert_eq!(*(NotNaN::from(8.0f64) % 4.0f64), 8.0f64 % 4.0f64);
        assert_eq!(*(-NotNaN::from(1.0f64)), -1.0f64);
        
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f64) + f64::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f64) - f64::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f64) * f64::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f64) / f64::NAN}).is_err());
        assert!(panic::catch_unwind(|| {NotNaN::from(0.0f64) % f64::NAN}).is_err());
        
        let mut number = NotNaN::from(5.0f64);
        number += NotNaN::from(4.0f64);
        assert_eq!(*number, 9.0f64);
        number -= NotNaN::from(4.0f64);
        assert_eq!(*number, 5.0f64);
        number *= NotNaN::from(4.0f64);
        assert_eq!(*number, 20.0f64);
        number /= NotNaN::from(4.0f64);
        assert_eq!(*number, 5.0f64);
        number %= NotNaN::from(4.0f64);
        assert_eq!(*number, 1.0f64);
        
        number = NotNaN::from(5.0f64);
        number += 4.0f64;
        assert_eq!(*number, 9.0f64);
        number -= 4.0f64;
        assert_eq!(*number, 5.0f64);
        number *= 4.0f64;
        assert_eq!(*number, 20.0f64);
        number /= 4.0f64;
        assert_eq!(*number, 5.0f64);
        number %= 4.0f64;
        assert_eq!(*number, 1.0f64);
        
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f64); tmp += f64::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f64); tmp -= f64::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f64); tmp *= f64::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f64); tmp /= f64::NAN;}).is_err());
        assert!(panic::catch_unwind(|| {let mut tmp = NotNaN::from(0.0f64); tmp %= f64::NAN;}).is_err());
    }
}
