use super::super::*;
use {simd_cast, f32x2};

pub use sixty_four::{f64x2, i64x2, u64x2, bool64ix2, bool64fx2};
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct u32x2(u32, u32);
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct i32x2(i32, i32);

#[repr(simd)]
#[derive(Copy, Clone)]
pub struct u16x4(u16, u16, u16, u16);
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct i16x4(i16, i16, i16, i16);
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct u8x8(u8, u8, u8, u8,
            u8, u8, u8, u8);
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct i8x8(i8, i8, i8, i8,
                i8, i8, i8, i8);

#[repr(simd)]
#[derive(Copy, Clone)]
pub struct i64x1(i64);
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct u64x1(u64);
#[repr(simd)]
#[derive(Copy, Clone)]
pub struct f64x1(f64);

#[allow(dead_code)]
extern "platform-intrinsic" {
    fn aarch64_vhadd_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vhadd_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vhadd_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vhadd_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vhadd_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vhadd_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vhaddq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vhaddq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vhaddq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vhaddq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vhaddq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vhaddq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vrhadd_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vrhadd_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vrhadd_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vrhadd_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vrhadd_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vrhadd_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vrhaddq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vrhaddq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vrhaddq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vrhaddq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vrhaddq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vrhaddq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vqadd_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vqadd_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vqadd_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vqadd_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vqadd_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vqadd_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vqadd_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vqadd_u64(x: u64x1, y: u64x1) -> u64x1;
    fn aarch64_vqaddq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vqaddq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vqaddq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vqaddq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vqaddq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vqaddq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vqaddq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vqaddq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vuqadd_s8(x: i8x16, y: u8x16) -> i8x16;
    fn aarch64_vuqadd_s16(x: i16x8, y: u16x8) -> i16x8;
    fn aarch64_vuqadd_s32(x: i32x4, y: u32x4) -> i32x4;
    fn aarch64_vuqadd_s64(x: i64x2, y: u64x2) -> i64x2;
    fn aarch64_vsqadd_u8(x: u8x16, y: i8x16) -> u8x16;
    fn aarch64_vsqadd_u16(x: u16x8, y: i16x8) -> u16x8;
    fn aarch64_vsqadd_u32(x: u32x4, y: i32x4) -> u32x4;
    fn aarch64_vsqadd_u64(x: u64x2, y: i64x2) -> u64x2;
    fn aarch64_vraddhn_s16(x: i16x8, y: i16x8) -> i8x8;
    fn aarch64_vraddhn_u16(x: u16x8, y: u16x8) -> u8x8;
    fn aarch64_vraddhn_s32(x: i32x4, y: i32x4) -> i16x4;
    fn aarch64_vraddhn_u32(x: u32x4, y: u32x4) -> u16x4;
    fn aarch64_vraddhn_s64(x: i64x2, y: i64x2) -> i32x2;
    fn aarch64_vraddhn_u64(x: u64x2, y: u64x2) -> u32x2;
    fn aarch64_vfmulx_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vfmulx_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vfmulxq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vfmulxq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vfma_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vfma_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vfmaq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vfmaq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vqdmulh_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vqdmulh_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vqdmulhq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vqdmulhq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vqrdmulh_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vqrdmulh_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vqrdmulhq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vqrdmulhq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vmull_s8(x: i8x8, y: i8x8) -> i16x8;
    fn aarch64_vmull_u8(x: u8x8, y: u8x8) -> u16x8;
    fn aarch64_vmull_s16(x: i16x4, y: i16x4) -> i32x4;
    fn aarch64_vmull_u16(x: u16x4, y: u16x4) -> u32x4;
    fn aarch64_vmull_s32(x: i32x2, y: i32x2) -> i64x2;
    fn aarch64_vmull_u32(x: u32x2, y: u32x2) -> u64x2;
    fn aarch64_vqdmullq_s8(x: i8x8, y: i8x8) -> i16x8;
    fn aarch64_vqdmullq_s16(x: i16x4, y: i16x4) -> i32x4;
    fn aarch64_vhsub_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vhsub_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vhsub_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vhsub_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vhsub_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vhsub_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vhsubq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vhsubq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vhsubq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vhsubq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vhsubq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vhsubq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vqsub_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vqsub_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vqsub_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vqsub_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vqsub_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vqsub_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vqsub_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vqsub_u64(x: u64x1, y: u64x1) -> u64x1;
    fn aarch64_vqsubq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vqsubq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vqsubq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vqsubq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vqsubq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vqsubq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vqsubq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vqsubq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vrsubhn_s16(x: i16x8, y: i16x8) -> i8x8;
    fn aarch64_vrsubhn_u16(x: u16x8, y: u16x8) -> u8x8;
    fn aarch64_vrsubhn_s32(x: i32x4, y: i32x4) -> i16x4;
    fn aarch64_vrsubhn_u32(x: u32x4, y: u32x4) -> u16x4;
    fn aarch64_vrsubhn_s64(x: i64x2, y: i64x2) -> i32x2;
    fn aarch64_vrsubhn_u64(x: u64x2, y: u64x2) -> u32x2;
    fn aarch64_vabd_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vabd_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vabd_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vabd_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vabd_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vabd_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vabd_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vabd_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vabdq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vabdq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vabdq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vabdq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vabdq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vabdq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vabdq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vabdq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vmax_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vmax_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vmax_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vmax_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vmax_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vmax_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vmax_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vmax_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vmaxq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vmaxq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vmaxq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vmaxq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vmaxq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vmaxq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vmaxq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vmaxq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vmin_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vmin_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vmin_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vmin_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vmin_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vmin_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vmin_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vmin_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vminq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vminq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vminq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vminq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vminq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vminq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vminq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vminq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vmaxnm_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vmaxnm_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vmaxnmq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vmaxnmq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vminnm_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vminnm_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vminnmq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vminnmq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vshl_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vshl_u8(x: u8x8, y: i8x8) -> u8x8;
    fn aarch64_vshl_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vshl_u16(x: u16x4, y: i16x4) -> u16x4;
    fn aarch64_vshl_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vshl_u32(x: u32x2, y: i32x2) -> u32x2;
    fn aarch64_vshl_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vshl_u64(x: u64x1, y: i64x1) -> u64x1;
    fn aarch64_vshlq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vshlq_u8(x: u8x16, y: i8x16) -> u8x16;
    fn aarch64_vshlq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vshlq_u16(x: u16x8, y: i16x8) -> u16x8;
    fn aarch64_vshlq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vshlq_u32(x: u32x4, y: i32x4) -> u32x4;
    fn aarch64_vshlq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vshlq_u64(x: u64x2, y: i64x2) -> u64x2;
    fn aarch64_vqshl_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vqshl_u8(x: u8x8, y: i8x8) -> u8x8;
    fn aarch64_vqshl_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vqshl_u16(x: u16x4, y: i16x4) -> u16x4;
    fn aarch64_vqshl_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vqshl_u32(x: u32x2, y: i32x2) -> u32x2;
    fn aarch64_vqshl_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vqshl_u64(x: u64x1, y: i64x1) -> u64x1;
    fn aarch64_vqshlq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vqshlq_u8(x: u8x16, y: i8x16) -> u8x16;
    fn aarch64_vqshlq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vqshlq_u16(x: u16x8, y: i16x8) -> u16x8;
    fn aarch64_vqshlq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vqshlq_u32(x: u32x4, y: i32x4) -> u32x4;
    fn aarch64_vqshlq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vqshlq_u64(x: u64x2, y: i64x2) -> u64x2;
    fn aarch64_vrshl_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vrshl_u8(x: u8x8, y: i8x8) -> u8x8;
    fn aarch64_vrshl_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vrshl_u16(x: u16x4, y: i16x4) -> u16x4;
    fn aarch64_vrshl_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vrshl_u32(x: u32x2, y: i32x2) -> u32x2;
    fn aarch64_vrshl_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vrshl_u64(x: u64x1, y: i64x1) -> u64x1;
    fn aarch64_vrshlq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vrshlq_u8(x: u8x16, y: i8x16) -> u8x16;
    fn aarch64_vrshlq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vrshlq_u16(x: u16x8, y: i16x8) -> u16x8;
    fn aarch64_vrshlq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vrshlq_u32(x: u32x4, y: i32x4) -> u32x4;
    fn aarch64_vrshlq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vrshlq_u64(x: u64x2, y: i64x2) -> u64x2;
    fn aarch64_vqrshl_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vqrshl_u8(x: u8x8, y: i8x8) -> u8x8;
    fn aarch64_vqrshl_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vqrshl_u16(x: u16x4, y: i16x4) -> u16x4;
    fn aarch64_vqrshl_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vqrshl_u32(x: u32x2, y: i32x2) -> u32x2;
    fn aarch64_vqrshl_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vqrshl_u64(x: u64x1, y: i64x1) -> u64x1;
    fn aarch64_vqrshlq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vqrshlq_u8(x: u8x16, y: i8x16) -> u8x16;
    fn aarch64_vqrshlq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vqrshlq_u16(x: u16x8, y: i16x8) -> u16x8;
    fn aarch64_vqrshlq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vqrshlq_u32(x: u32x4, y: i32x4) -> u32x4;
    fn aarch64_vqrshlq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vqrshlq_u64(x: u64x2, y: i64x2) -> u64x2;
    fn aarch64_vqshrun_n_s16(x: i16x8, y: u32) -> i8x8;
    fn aarch64_vqshrun_n_s32(x: i32x4, y: u32) -> i16x4;
    fn aarch64_vqshrun_n_s64(x: i64x2, y: u32) -> i32x2;
    fn aarch64_vqrshrun_n_s16(x: i16x8, y: u32) -> i8x8;
    fn aarch64_vqrshrun_n_s32(x: i32x4, y: u32) -> i16x4;
    fn aarch64_vqrshrun_n_s64(x: i64x2, y: u32) -> i32x2;
    fn aarch64_vqshrn_n_s16(x: i16x8, y: u32) -> i8x8;
    fn aarch64_vqshrn_n_u16(x: u16x8, y: u32) -> u8x8;
    fn aarch64_vqshrn_n_s32(x: i32x4, y: u32) -> i16x4;
    fn aarch64_vqshrn_n_u32(x: u32x4, y: u32) -> u16x4;
    fn aarch64_vqshrn_n_s64(x: i64x2, y: u32) -> i32x2;
    fn aarch64_vqshrn_n_u64(x: u64x2, y: u32) -> u32x2;
    fn aarch64_vrshrn_n_s16(x: i16x8, y: u32) -> i8x8;
    fn aarch64_vrshrn_n_u16(x: u16x8, y: u32) -> u8x8;
    fn aarch64_vrshrn_n_s32(x: i32x4, y: u32) -> i16x4;
    fn aarch64_vrshrn_n_u32(x: u32x4, y: u32) -> u16x4;
    fn aarch64_vrshrn_n_s64(x: i64x2, y: u32) -> i32x2;
    fn aarch64_vrshrn_n_u64(x: u64x2, y: u32) -> u32x2;
    fn aarch64_vqrshrn_n_s16(x: i16x8, y: u32) -> i8x8;
    fn aarch64_vqrshrn_n_u16(x: u16x8, y: u32) -> u8x8;
    fn aarch64_vqrshrn_n_s32(x: i32x4, y: u32) -> i16x4;
    fn aarch64_vqrshrn_n_u32(x: u32x4, y: u32) -> u16x4;
    fn aarch64_vqrshrn_n_s64(x: i64x2, y: u32) -> i32x2;
    fn aarch64_vqrshrn_n_u64(x: u64x2, y: u32) -> u32x2;
    fn aarch64_vsri_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vsri_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vsri_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vsri_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vsri_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vsri_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vsri_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vsri_u64(x: u64x1, y: u64x1) -> u64x1;
    fn aarch64_vsriq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vsriq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vsriq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vsriq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vsriq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vsriq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vsriq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vsriq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vsli_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vsli_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vsli_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vsli_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vsli_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vsli_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vsli_s64(x: i64x1, y: i64x1) -> i64x1;
    fn aarch64_vsli_u64(x: u64x1, y: u64x1) -> u64x1;
    fn aarch64_vsliq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vsliq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vsliq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vsliq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vsliq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vsliq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vsliq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vsliq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vvqmovn_s16(x: i16x8) -> i8x8;
    fn aarch64_vvqmovn_u16(x: u16x8) -> u8x8;
    fn aarch64_vvqmovn_s32(x: i32x4) -> i16x4;
    fn aarch64_vvqmovn_u32(x: u32x4) -> u16x4;
    fn aarch64_vvqmovn_s64(x: i64x2) -> i32x2;
    fn aarch64_vvqmovn_u64(x: u64x2) -> u32x2;
    fn aarch64_vabs_s8(x: i8x8) -> i8x8;
    fn aarch64_vabs_s16(x: i16x4) -> i16x4;
    fn aarch64_vabs_s32(x: i32x2) -> i32x2;
    fn aarch64_vabs_s64(x: i64x1) -> i64x1;
    fn aarch64_vabsq_s8(x: i8x16) -> i8x16;
    fn aarch64_vabsq_s16(x: i16x8) -> i16x8;
    fn aarch64_vabsq_s32(x: i32x4) -> i32x4;
    fn aarch64_vabsq_s64(x: i64x2) -> i64x2;
    fn aarch64_vabs_f32(x: f32x2) -> f32x2;
    fn aarch64_vabs_f64(x: f64x1) -> f64x1;
    fn aarch64_vabsq_f32(x: f32x4) -> f32x4;
    fn aarch64_vabsq_f64(x: f64x2) -> f64x2;
    fn aarch64_vqabs_s8(x: i8x8) -> i8x8;
    fn aarch64_vqabs_s16(x: i16x4) -> i16x4;
    fn aarch64_vqabs_s32(x: i32x2) -> i32x2;
    fn aarch64_vqabs_s64(x: i64x1) -> i64x1;
    fn aarch64_vqabsq_s8(x: i8x16) -> i8x16;
    fn aarch64_vqabsq_s16(x: i16x8) -> i16x8;
    fn aarch64_vqabsq_s32(x: i32x4) -> i32x4;
    fn aarch64_vqabsq_s64(x: i64x2) -> i64x2;
    fn aarch64_vqneg_s8(x: i8x8) -> i8x8;
    fn aarch64_vqneg_s16(x: i16x4) -> i16x4;
    fn aarch64_vqneg_s32(x: i32x2) -> i32x2;
    fn aarch64_vqneg_s64(x: i64x1) -> i64x1;
    fn aarch64_vqnegq_s8(x: i8x16) -> i8x16;
    fn aarch64_vqnegq_s16(x: i16x8) -> i16x8;
    fn aarch64_vqnegq_s32(x: i32x4) -> i32x4;
    fn aarch64_vqnegq_s64(x: i64x2) -> i64x2;
    fn aarch64_vclz_s8(x: i8x8) -> i8x8;
    fn aarch64_vclz_u8(x: u8x8) -> u8x8;
    fn aarch64_vclz_s16(x: i16x4) -> i16x4;
    fn aarch64_vclz_u16(x: u16x4) -> u16x4;
    fn aarch64_vclz_s32(x: i32x2) -> i32x2;
    fn aarch64_vclz_u32(x: u32x2) -> u32x2;
    fn aarch64_vclzq_s8(x: i8x16) -> i8x16;
    fn aarch64_vclzq_u8(x: u8x16) -> u8x16;
    fn aarch64_vclzq_s16(x: i16x8) -> i16x8;
    fn aarch64_vclzq_u16(x: u16x8) -> u16x8;
    fn aarch64_vclzq_s32(x: i32x4) -> i32x4;
    fn aarch64_vclzq_u32(x: u32x4) -> u32x4;
    fn aarch64_vcls_s8(x: i8x8) -> i8x8;
    fn aarch64_vcls_u8(x: u8x8) -> u8x8;
    fn aarch64_vcls_s16(x: i16x4) -> i16x4;
    fn aarch64_vcls_u16(x: u16x4) -> u16x4;
    fn aarch64_vcls_s32(x: i32x2) -> i32x2;
    fn aarch64_vcls_u32(x: u32x2) -> u32x2;
    fn aarch64_vclsq_s8(x: i8x16) -> i8x16;
    fn aarch64_vclsq_u8(x: u8x16) -> u8x16;
    fn aarch64_vclsq_s16(x: i16x8) -> i16x8;
    fn aarch64_vclsq_u16(x: u16x8) -> u16x8;
    fn aarch64_vclsq_s32(x: i32x4) -> i32x4;
    fn aarch64_vclsq_u32(x: u32x4) -> u32x4;
    fn aarch64_vcnt_s8(x: i8x8) -> i8x8;
    fn aarch64_vcnt_u8(x: u8x8) -> u8x8;
    fn aarch64_vcntq_s8(x: i8x16) -> i8x16;
    fn aarch64_vcntq_u8(x: u8x16) -> u8x16;
    fn aarch64_vrecpe_u32(x: u32x2) -> u32x2;
    fn aarch64_vrecpe_f32(x: f32x2) -> f32x2;
    fn aarch64_vrecpe_f64(x: f64x1) -> f64x1;
    fn aarch64_vrecpeq_u32(x: u32x4) -> u32x4;
    fn aarch64_vrecpeq_f32(x: f32x4) -> f32x4;
    fn aarch64_vrecpeq_f64(x: f64x2) -> f64x2;
    fn aarch64_vrecps_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vrecps_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vrecpsq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vrecpsq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vsqrt_f32(x: f32x2) -> f32x2;
    fn aarch64_vsqrt_f64(x: f64x1) -> f64x1;
    fn aarch64_vsqrtq_f32(x: f32x4) -> f32x4;
    fn aarch64_vsqrtq_f64(x: f64x2) -> f64x2;
    fn aarch64_vrsqrte_u32(x: u32x2) -> u32x2;
    fn aarch64_vrsqrte_f32(x: f32x2) -> f32x2;
    fn aarch64_vrsqrte_f64(x: f64x1) -> f64x1;
    fn aarch64_vrsqrteq_u32(x: u32x4) -> u32x4;
    fn aarch64_vrsqrteq_f32(x: f32x4) -> f32x4;
    fn aarch64_vrsqrteq_f64(x: f64x2) -> f64x2;
    fn aarch64_vrsqrts_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vrsqrts_f64(x: f64x1, y: f64x1) -> f64x1;
    fn aarch64_vrsqrtsq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vrsqrtsq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vrbit_s8(x: i8x8) -> i8x8;
    fn aarch64_vrbit_u8(x: u8x8) -> u8x8;
    fn aarch64_vrbitq_s8(x: i8x16) -> i8x16;
    fn aarch64_vrbitq_u8(x: u8x16) -> u8x16;
    fn aarch64_vpadd_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vpadd_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vpadd_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vpadd_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vpadd_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vpadd_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vpadd_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vpaddq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vpaddq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vpaddq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vpaddq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vpaddq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vpaddq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vpaddq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vpaddq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vpaddq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vpaddq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vpaddl_s16(x: i8x8) -> i16x4;
    fn aarch64_vpaddl_u16(x: u8x8) -> u16x4;
    fn aarch64_vpaddl_s32(x: i16x4) -> i32x2;
    fn aarch64_vpaddl_u32(x: u16x4) -> u32x2;
    fn aarch64_vpaddl_s64(x: i32x2) -> i64x1;
    fn aarch64_vpaddl_u64(x: u32x2) -> u64x1;
    fn aarch64_vpaddlq_s16(x: i8x16) -> i16x8;
    fn aarch64_vpaddlq_u16(x: u8x16) -> u16x8;
    fn aarch64_vpaddlq_s32(x: i16x8) -> i32x4;
    fn aarch64_vpaddlq_u32(x: u16x8) -> u32x4;
    fn aarch64_vpaddlq_s64(x: i32x4) -> i64x2;
    fn aarch64_vpaddlq_u64(x: u32x4) -> u64x2;
    fn aarch64_vpmax_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vpmax_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vpmax_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vpmax_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vpmax_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vpmax_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vpmax_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vpmaxq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vpmaxq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vpmaxq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vpmaxq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vpmaxq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vpmaxq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vpmaxq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vpmaxq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vpmaxq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vpmaxq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vpmin_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vpmin_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vpmin_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vpmin_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vpmin_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vpmin_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vpmin_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vpminq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vpminq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vpminq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vpminq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vpminq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vpminq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vpminq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vpminq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vpminq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vpminq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vpmaxnm_s8(x: i8x8, y: i8x8) -> i8x8;
    fn aarch64_vpmaxnm_u8(x: u8x8, y: u8x8) -> u8x8;
    fn aarch64_vpmaxnm_s16(x: i16x4, y: i16x4) -> i16x4;
    fn aarch64_vpmaxnm_u16(x: u16x4, y: u16x4) -> u16x4;
    fn aarch64_vpmaxnm_s32(x: i32x2, y: i32x2) -> i32x2;
    fn aarch64_vpmaxnm_u32(x: u32x2, y: u32x2) -> u32x2;
    fn aarch64_vpmaxnm_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vpmaxnmq_s8(x: i8x16, y: i8x16) -> i8x16;
    fn aarch64_vpmaxnmq_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vpmaxnmq_s16(x: i16x8, y: i16x8) -> i16x8;
    fn aarch64_vpmaxnmq_u16(x: u16x8, y: u16x8) -> u16x8;
    fn aarch64_vpmaxnmq_s32(x: i32x4, y: i32x4) -> i32x4;
    fn aarch64_vpmaxnmq_u32(x: u32x4, y: u32x4) -> u32x4;
    fn aarch64_vpmaxnmq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vpmaxnmq_s64(x: i64x2, y: i64x2) -> i64x2;
    fn aarch64_vpmaxnmq_u64(x: u64x2, y: u64x2) -> u64x2;
    fn aarch64_vpmaxnmq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vpminnm_f32(x: f32x2, y: f32x2) -> f32x2;
    fn aarch64_vpminnmq_f32(x: f32x4, y: f32x4) -> f32x4;
    fn aarch64_vpminnmq_f64(x: f64x2, y: f64x2) -> f64x2;
    fn aarch64_vaddv_s8(x: i8x8) -> i8;
    fn aarch64_vaddv_u8(x: u8x8) -> u8;
    fn aarch64_vaddv_s16(x: i16x4) -> i16;
    fn aarch64_vaddv_u16(x: u16x4) -> u16;
    fn aarch64_vaddv_s32(x: i32x2) -> i32;
    fn aarch64_vaddv_u32(x: u32x2) -> u32;
    fn aarch64_vaddv_f32(x: f32x2) -> f32;
    fn aarch64_vaddvq_s8(x: i8x16) -> i8;
    fn aarch64_vaddvq_u8(x: u8x16) -> u8;
    fn aarch64_vaddvq_s16(x: i16x8) -> i16;
    fn aarch64_vaddvq_u16(x: u16x8) -> u16;
    fn aarch64_vaddvq_s32(x: i32x4) -> i32;
    fn aarch64_vaddvq_u32(x: u32x4) -> u32;
    fn aarch64_vaddvq_f32(x: f32x4) -> f32;
    fn aarch64_vaddvq_s64(x: i64x2) -> i64;
    fn aarch64_vaddvq_u64(x: u64x2) -> u64;
    fn aarch64_vaddvq_f64(x: f64x2) -> f64;
    fn aarch64_vaddlv_s8(x: i8x8) -> i16;
    fn aarch64_vaddlv_u8(x: u8x8) -> u16;
    fn aarch64_vaddlv_s16(x: i16x4) -> i32;
    fn aarch64_vaddlv_u16(x: u16x4) -> u32;
    fn aarch64_vaddlv_s32(x: i32x2) -> i64;
    fn aarch64_vaddlv_u32(x: u32x2) -> u64;
    fn aarch64_vaddlvq_s8(x: i8x16) -> i16;
    fn aarch64_vaddlvq_u8(x: u8x16) -> u16;
    fn aarch64_vaddlvq_s16(x: i16x8) -> i32;
    fn aarch64_vaddlvq_u16(x: u16x8) -> u32;
    fn aarch64_vaddlvq_s32(x: i32x4) -> i64;
    fn aarch64_vaddlvq_u32(x: u32x4) -> u64;
    fn aarch64_vmaxv_s8(x: i8x8) -> i8;
    fn aarch64_vmaxv_u8(x: u8x8) -> u8;
    fn aarch64_vmaxv_s16(x: i16x4) -> i16;
    fn aarch64_vmaxv_u16(x: u16x4) -> u16;
    fn aarch64_vmaxv_s32(x: i32x2) -> i32;
    fn aarch64_vmaxv_u32(x: u32x2) -> u32;
    fn aarch64_vmaxv_f32(x: f32x2) -> f32;
    fn aarch64_vmaxvq_s8(x: i8x16) -> i8;
    fn aarch64_vmaxvq_u8(x: u8x16) -> u8;
    fn aarch64_vmaxvq_s16(x: i16x8) -> i16;
    fn aarch64_vmaxvq_u16(x: u16x8) -> u16;
    fn aarch64_vmaxvq_s32(x: i32x4) -> i32;
    fn aarch64_vmaxvq_u32(x: u32x4) -> u32;
    fn aarch64_vmaxvq_f32(x: f32x4) -> f32;
    fn aarch64_vmaxvq_f64(x: f64x2) -> f64;
    fn aarch64_vminv_s8(x: i8x8) -> i8;
    fn aarch64_vminv_u8(x: u8x8) -> u8;
    fn aarch64_vminv_s16(x: i16x4) -> i16;
    fn aarch64_vminv_u16(x: u16x4) -> u16;
    fn aarch64_vminv_s32(x: i32x2) -> i32;
    fn aarch64_vminv_u32(x: u32x2) -> u32;
    fn aarch64_vminv_f32(x: f32x2) -> f32;
    fn aarch64_vminvq_s8(x: i8x16) -> i8;
    fn aarch64_vminvq_u8(x: u8x16) -> u8;
    fn aarch64_vminvq_s16(x: i16x8) -> i16;
    fn aarch64_vminvq_u16(x: u16x8) -> u16;
    fn aarch64_vminvq_s32(x: i32x4) -> i32;
    fn aarch64_vminvq_u32(x: u32x4) -> u32;
    fn aarch64_vminvq_f32(x: f32x4) -> f32;
    fn aarch64_vminvq_f64(x: f64x2) -> f64;
    fn aarch64_vmaxnmv_f32(x: f32x2) -> f32;
    fn aarch64_vmaxnmvq_f32(x: f32x4) -> f32;
    fn aarch64_vmaxnmvq_f64(x: f64x2) -> f64;
    fn aarch64_vminnmv_f32(x: f32x2) -> f32;
    fn aarch64_vminnmvq_f32(x: f32x4) -> f32;
    fn aarch64_vminnmvq_f64(x: f64x2) -> f64;
    fn aarch64_vqtbl1_s8(x: i8x16, y: u8x8) -> i8x8;
    fn aarch64_vqtbl1_u8(x: u8x16, y: u8x8) -> u8x8;
    fn aarch64_vqtbl1q_s8(x: i8x16, y: u8x16) -> i8x16;
    fn aarch64_vqtbl1q_u8(x: u8x16, y: u8x16) -> u8x16;
    fn aarch64_vqtbx1_s8(x: i8x8, y: i8x16, z: u8x8) -> i8x8;
    fn aarch64_vqtbx1_u8(x: u8x8, y: u8x16, z: u8x8) -> u8x8;
    fn aarch64_vqtbx1q_s8(x: i8x16, y: i8x16, z: u8x16) -> i8x16;
    fn aarch64_vqtbx1q_u8(x: u8x16, y: u8x16, z: u8x16) -> u8x16;
    fn aarch64_vqtbl2_s8(x: (i8x16, i8x16), y: u8x8) -> i8x8;
    fn aarch64_vqtbl2_u8(x: (u8x16, u8x16), y: u8x8) -> u8x8;
    fn aarch64_vqtbl2q_s8(x: (i8x16, i8x16), y: u8x16) -> i8x16;
    fn aarch64_vqtbl2q_u8(x: (u8x16, u8x16), y: u8x16) -> u8x16;
    fn aarch64_vqtbx2_s8(x: (i8x16, i8x16), y: u8x8) -> i8x8;
    fn aarch64_vqtbx2_u8(x: (u8x16, u8x16), y: u8x8) -> u8x8;
    fn aarch64_vqtbx2q_s8(x: (i8x16, i8x16), y: u8x16) -> i8x16;
    fn aarch64_vqtbx2q_u8(x: (u8x16, u8x16), y: u8x16) -> u8x16;
    fn aarch64_vqtbl3_s8(x: (i8x16, i8x16, i8x16), y: u8x8) -> i8x8;
    fn aarch64_vqtbl3_u8(x: (u8x16, u8x16, u8x16), y: u8x8) -> u8x8;
    fn aarch64_vqtbl3q_s8(x: (i8x16, i8x16, i8x16), y: u8x16) -> i8x16;
    fn aarch64_vqtbl3q_u8(x: (u8x16, u8x16, u8x16), y: u8x16) -> u8x16;
    fn aarch64_vqtbx3_s8(x: i8x8, y: (i8x16, i8x16, i8x16), z: u8x8) -> i8x8;
    fn aarch64_vqtbx3_u8(x: u8x8, y: (u8x16, u8x16, u8x16), z: u8x8) -> u8x8;
    fn aarch64_vqtbx3q_s8(x: i8x16, y: (i8x16, i8x16, i8x16), z: u8x16) -> i8x16;
    fn aarch64_vqtbx3q_u8(x: u8x16, y: (u8x16, u8x16, u8x16), z: u8x16) -> u8x16;
    fn aarch64_vqtbl4_s8(x: (i8x16, i8x16, i8x16, i8x16), y: u8x8) -> i8x8;
    fn aarch64_vqtbl4_u8(x: (u8x16, u8x16, u8x16, u8x16), y: u8x8) -> u8x8;
    fn aarch64_vqtbl4q_s8(x: (i8x16, i8x16, i8x16, i8x16), y: u8x16) -> i8x16;
    fn aarch64_vqtbl4q_u8(x: (u8x16, u8x16, u8x16, u8x16), y: u8x16) -> u8x16;
    fn aarch64_vqtbx4_s8(x: i8x8, y: (i8x16, i8x16, i8x16, i8x16), z: u8x8) -> i8x8;
    fn aarch64_vqtbx4_u8(x: u8x8, y: (u8x16, u8x16, u8x16, u8x16), z: u8x8) -> u8x8;
    fn aarch64_vqtbx4q_s8(x: i8x16, y: (i8x16, i8x16, i8x16, i8x16), z: u8x16) -> i8x16;
    fn aarch64_vqtbx4q_u8(x: u8x16, y: (u8x16, u8x16, u8x16, u8x16), z: u8x16) -> u8x16;
}

pub trait Aarch64F32x4 {
    fn to_f64(self) -> f64x2;
}
impl Aarch64F32x4 for f32x4 {
    #[inline]
    fn to_f64(self) -> f64x2 {
        unsafe {
            simd_cast(f32x2(self.0, self.1))
        }
    }
}

pub trait Aarch64U8x16 {
    fn table_lookup_1(self, t0: u8x16) -> u8x16;
}
impl Aarch64U8x16 for u8x16 {
    #[inline]
    fn table_lookup_1(self, t0: u8x16) -> u8x16 {
        unsafe {aarch64_vqtbl1q_u8(t0, self)}
    }
}
pub trait Aarch64I8x16 {
    fn table_lookup_1(self, t0: i8x16) -> i8x16;
}
impl Aarch64I8x16 for i8x16 {
    #[inline]
    fn table_lookup_1(self, t0: i8x16) -> i8x16 {
        unsafe {aarch64_vqtbl2q_s8((t0, t0), ::bitcast(self))}
    }
}

#[doc(hidden)]
pub mod common {
    use super::super::super::*;
    use std::mem;

    #[inline]
    pub fn f32x4_sqrt(x: f32x4) -> f32x4 {
        unsafe {super::aarch64_vsqrtq_f32(x)}
    }
    #[inline]
    pub fn f32x4_approx_rsqrt(x: f32x4) -> f32x4 {
        unsafe {super::aarch64_vrsqrteq_f32(x)}
    }
    #[inline]
    pub fn f32x4_approx_reciprocal(x: f32x4) -> f32x4 {
        unsafe {super::aarch64_vrecpeq_f32(x)}
    }
    #[inline]
    pub fn f32x4_max(x: f32x4, y: f32x4) -> f32x4 {
        unsafe {super::aarch64_vmaxq_f32(x, y)}
    }
    #[inline]
    pub fn f32x4_min(x: f32x4, y: f32x4) -> f32x4 {
        unsafe {super::aarch64_vminq_f32(x, y)}
    }

    macro_rules! bools {
        ($($ty: ty, $all: ident ($min: ident), $any: ident ($max: ident);)*) => {
            $(
                #[inline]
                pub fn $all(x: $ty) -> bool {
                    unsafe {
                        super::$min(mem::transmute(x)) != 0
                    }
                }
                #[inline]
                pub fn $any(x: $ty) -> bool {
                    unsafe {
                        super::$max(mem::transmute(x)) != 0
                    }
                }
                )*
        }
    }

    bools! {
        bool32fx4, bool32fx4_all(aarch64_vminvq_u32), bool32fx4_any(aarch64_vmaxvq_u32);
        bool8ix16, bool8ix16_all(aarch64_vminvq_u8), bool8ix16_any(aarch64_vmaxvq_u8);
        bool16ix8, bool16ix8_all(aarch64_vminvq_u16), bool16ix8_any(aarch64_vmaxvq_u16);
        bool32ix4, bool32ix4_all(aarch64_vminvq_u32), bool32ix4_any(aarch64_vmaxvq_u32);
    }
}
