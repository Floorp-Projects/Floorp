use crate::types::*;
type FIX4 = INT;       // 28.4 fixed point value

// constants for working with 28.4 fixed point values
macro_rules! FIX4_SHIFT { () => { 4 } }
macro_rules! FIX4_PRECISION { () => { 4 } }
macro_rules! FIX4_ONE { () => { (1 << FIX4_PRECISION!()) } }
macro_rules! FIX4_HALF { () => { (1 << (FIX4_PRECISION!()-1)) } }
macro_rules! FIX4_MASK { () => { (FIX4_ONE!() - 1) } }