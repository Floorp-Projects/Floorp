use x86::avx::*;

#[allow(dead_code)]
extern "platform-intrinsic" {
    fn x86_mm256_abs_epi8(x: i8x32) -> i8x32;
    fn x86_mm256_abs_epi16(x: i16x16) -> i16x16;
    fn x86_mm256_abs_epi32(x: i32x8) -> i32x8;
    fn x86_mm256_adds_epi8(x: i8x32, y: i8x32) -> i8x32;
    fn x86_mm256_adds_epu8(x: u8x32, y: u8x32) -> u8x32;
    fn x86_mm256_adds_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_adds_epu16(x: u16x16, y: u16x16) -> u16x16;
    fn x86_mm256_avg_epu8(x: u8x32, y: u8x32) -> u8x32;
    fn x86_mm256_avg_epu16(x: u16x16, y: u16x16) -> u16x16;
    fn x86_mm256_hadd_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_hadd_epi32(x: i32x8, y: i32x8) -> i32x8;
    fn x86_mm256_hadds_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_hsub_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_hsub_epi32(x: i32x8, y: i32x8) -> i32x8;
    fn x86_mm256_hsubs_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_madd_epi16(x: i16x16, y: i16x16) -> i32x8;
    fn x86_mm256_maddubs_epi16(x: i8x32, y: i8x32) -> i16x16;
    fn x86_mm256_max_epi8(x: i8x32, y: i8x32) -> i8x32;
    fn x86_mm256_max_epu8(x: u8x32, y: u8x32) -> u8x32;
    fn x86_mm256_max_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_max_epu16(x: u16x16, y: u16x16) -> u16x16;
    fn x86_mm256_max_epi32(x: i32x8, y: i32x8) -> i32x8;
    fn x86_mm256_max_epu32(x: u32x8, y: u32x8) -> u32x8;
    fn x86_mm256_min_epi8(x: i8x32, y: i8x32) -> i8x32;
    fn x86_mm256_min_epu8(x: u8x32, y: u8x32) -> u8x32;
    fn x86_mm256_min_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_min_epu16(x: u16x16, y: u16x16) -> u16x16;
    fn x86_mm256_min_epi32(x: i32x8, y: i32x8) -> i32x8;
    fn x86_mm256_min_epu32(x: u32x8, y: u32x8) -> u32x8;
    fn x86_mm256_mul_epi64(x: i32x8, y: i32x8) -> i64x4;
    fn x86_mm256_mul_epu64(x: u32x8, y: u32x8) -> u64x4;
    fn x86_mm256_mulhi_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_mulhi_epu16(x: u16x16, y: u16x16) -> u16x16;
    fn x86_mm256_mulhrs_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_packs_epi16(x: i16x16, y: i16x16) -> i8x32;
    fn x86_mm256_packus_epi16(x: i16x16, y: i16x16) -> u8x32;
    fn x86_mm256_packs_epi32(x: i32x8, y: i32x8) -> i16x16;
    fn x86_mm256_packus_epi32(x: i32x8, y: i32x8) -> u16x16;
    fn x86_mm256_permutevar8x32_epi32(x: i32x8, y: i32x8) -> i32x8;
    fn x86_mm256_permutevar8x32_ps(x: f32x8, y: i32x8) -> f32x8;
    fn x86_mm256_sad_epu8(x: u8x32, y: u8x32) -> u8x32;
    fn x86_mm256_shuffle_epi8(x: i8x32, y: i8x32) -> i8x32;
    fn x86_mm256_sign_epi8(x: i8x32, y: i8x32) -> i8x32;
    fn x86_mm256_sign_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_sign_epi32(x: i32x8, y: i32x8) -> i32x8;
    fn x86_mm256_subs_epi8(x: i8x32, y: i8x32) -> i8x32;
    fn x86_mm256_subs_epu8(x: u8x32, y: u8x32) -> u8x32;
    fn x86_mm256_subs_epi16(x: i16x16, y: i16x16) -> i16x16;
    fn x86_mm256_subs_epu16(x: u16x16, y: u16x16) -> u16x16;
}

// broken on rustc 1.7.0-nightly (1ddaf8bdf 2015-12-12)
// pub trait Avx2F32x8 {
//     fn permutevar(self, other: i32x8) -> f32x8;
// }
//
// impl Avx2F32x8 for f32x8 {
//     fn permutevar(self, other: i32x8) -> f32x8 {
//         unsafe { x86_mm256_permutevar8x32_ps(self, other) }
//     }
// }
