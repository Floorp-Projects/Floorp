pub mod CFloatFPU {
    // Maximum allowed argument for SmallRound
    // const sc_uSmallMax: u32 = 0xFFFFF;

    // Binary representation of static_cast<float>(sc_uSmallMax)
    const sc_uBinaryFloatSmallMax: u32 = 0x497ffff0;

    fn LargeRound(x: f32) -> i32 {
        //XXX: the SSE2 version is probably slower than a naive SSE4 implementation that can use roundss
        #[cfg(target_feature = "sse2")]
        unsafe {
            #[cfg(target_arch = "x86")]
            use std::arch::x86::{__m128, _mm_set_ss, _mm_cvtss_si32, _mm_cvtsi32_ss, _mm_sub_ss, _mm_cmple_ss, _mm_store_ss, _mm_setzero_ps};
            #[cfg(target_arch = "x86_64")]
            use std::arch::x86_64::{__m128, _mm_set_ss, _mm_cvtss_si32, _mm_cvtsi32_ss, _mm_sub_ss, _mm_cmple_ss, _mm_store_ss, _mm_setzero_ps};

            let given: __m128 = _mm_set_ss(x);                       // load given value
            let result = _mm_cvtss_si32(given);
            let rounded: __m128 = _mm_setzero_ps();             // convert it to integer (rounding mode doesn't matter)
            let rounded = _mm_cvtsi32_ss(rounded, result);   // convert back to float
            let diff = _mm_sub_ss(rounded, given);           // diff = (rounded - given)
            let negHalf = _mm_set_ss(-0.5);                 // load -0.5f
            let mask = _mm_cmple_ss(diff, negHalf);          // get all-ones if (rounded - given) < -0.5f
            let mut correction: i32 = 0;                                 
            _mm_store_ss((&mut correction) as *mut _ as *mut _, mask); // get comparison result as integer
            return result - correction;                         // correct the result of rounding
        }
        #[cfg(not(target_feature = "sse2"))]
        return (x + 0.5).floor() as i32;
    }


    //+------------------------------------------------------------------------
//
//  Function:   CFloatFPU::SmallRound
//
//  Synopsis:   Convert given floating point value to nearest integer.
//              Half-integers are rounded up.
//
//              Important: this routine is fast but restricted:
//              given x should be within (-(0x100000-.5) < x < (0x100000-.5))
//
//  Details:    Implementation has abnormal looking that use to confuse
//              many people. However, it indeed works, being tested
//              thoroughly on x86 and ia64 platforms for literally
//              each possible argument values in the given range.
//              
//  More details:
//      Implementation is based on the knowledge of floating point
//      value representation. This 32-bits value consists of three parts:
//      v & 0x80000000 = sign
//      v & 0x7F800000 = exponent
//      v & 0x007FFFFF - mantissa
//
//      Let N to be a floating point number within -0x400000 <= N <= 0x3FFFFF.
//      The sum (S = 0xC00000 + N) thus will satisfy Ox800000 <= S <= 0xFFFFFF.
//      All the numbers within this range (sometimes referred to as "binade")
//      have same position of most significant bit, i.e. 0x800000.
//      Therefore they are normalized equal way, thus
//      providing the weights on mantissa's bits to be the same
//      as integer numbers have. In other words, to get
//      integer value of floating point S, when Ox800000 <= S <= 0xFFFFFF,
//      we can just throw away the exponent and sign, and add assumed
//      most significant bit (that is always 1 and therefore is not stored
//      in floating point value):
//      (int)S = (<float S as int> & 0x7FFFFF | 0x800000);
//      To get given N in as integer, we need to subtract back
//      the value 0xC00000 that was added in order to obtain
//      proper normalization:
//      N = (<float S as int> & 0x7FFFFF | 0x800000) - 0xC00000.
//      or
//      N = (<float S as int> & 0x7FFFFF           ) - 0x400000.
//
//      Hopefully, the text above explains how
//      following routine works:
//        int SmallRound1(float x)
//        {
//            union
//            {
//                __int32 i;
//                float f;
//            } u;
//
//            u.f = x + float(0x00C00000);
//            return ((u.i - (int)0x00400000) << 9) >> 9;
//        }
//      Unfortunatelly it is imperfect, due to the way how FPU
//      use to round intermediate calculation results.
//      By default, rounding mode is set to "nearest".
//      This means that when it calculates N+float(0x00C00000),
//      the 80-bit precise result will not fit in 32-bit float,
//      so some least significant bits will be thrown away.
//      Rounding to nearest means that S consisting of intS + fraction,
//      where 0 <= fraction < 1, will be converted to intS
//      when fraction < 0.5 and to intS+1 if fraction > 0.5.
//      What would happen with fraction exactly equal to 0.5?
//      Smart thing: S will go to intS if intS is even and
//      to intS+1 if intS is odd. In other words, half-integers
//      are rounded to nearest even number.
//      This FPU feature apparently is useful to minimize
//      average rounding error when somebody is, say,
//      digitally simulating electrons' behavior in plasma.
//      However for graphics this is not desired.
//
//      We want to move half-integers up, therefore
//      define SmallRound(x) as {return SmallRound1(x*2+.5) >> 1;}.
//      This may require more comments.
//      Let given x = i+f, where i is integer and f is fraction, 0 <= f < 1.
//      Let's wee what is y = x*2+.5:
//          y = i*2 + (f*2 + .5) = i*2 + g, where g = f*2 + .5;
//      If "f" is in the range 0 <= f < .5 (so correct rounding result should be "i"),
//      then range for "g" is .5 <= g < 1.5. The very first value, .5 will force
//      SmallRound1 result to be "i*2", due to round-to-even rule; the remaining
//      will lead to "i*2+1". Consequent shift will throw away extra "1" and give
//      us desired "i".
//      When "f" in in the range .5 <= f < 1, then 1.5 <= g < 2.5.
//      All these values will round to 2, so SmallRound1 will return (2*i+2),
//      and the final shift will give desired 1+1.
//
//      To get final routine looking we need to transform the combines
//      expression for u.f:
//            (x*2) + .5 + float(0x00C00000) ==
//             (x + (.25 + double(0x00600000)) )*2
//      Note that the ratio "2" means nothing for following operations,
//      since it affects only exponent bits that are ignored anyway.
//      So we can save some processor cycles avoiding this multiplication.
//
//      And, the very final beautification:
//      to avoid subtracting 0x00400000 let's ignore this bit.
//      This mean that we effectively decrease available range by 1 bit,
//      but we're chasing for performance and found it acceptable.
//      So 
//         return ((u.i - (int)0x00400000) << 9) >> 9;
//      is converted to
//         return ((u.i                  ) << 10) >> 10;
//      Eventually, will found that final shift by 10 bits may be combined
//      with shift by 1 in the definition {return SmallRound1(x*2+.5) >> 1;},
//      we'll just shift by 11 bits. That's it.
//      
//-------------------------------------------------------------------------
fn SmallRound(x: f32) -> i32
{
    //AssertPrecisionAndRoundingMode();
    debug_assert!(-(0x100000 as f64 -0.5) < x as f64 && (x as f64) < (0x100000 as f64 -0.5));

 
    let fi = (x as f64 + (0x00600000 as f64 + 0.25)) as f32;
    let result = ((fi.to_bits() as i32) << 10) >> 11;

    debug_assert!(x < (result as f32) + 0.5 && x >= (result as f32) - 0.5);
    return result;
}

pub fn Round(x: f32) -> i32
{
    // cut off sign
    let xAbs: u32 = x.to_bits() & 0x7FFFFFFF;

    return if xAbs <= sc_uBinaryFloatSmallMax {SmallRound(x)} else {LargeRound(x)};
}
}

macro_rules! TOREAL { ($e: expr) => { $e as REAL } }
