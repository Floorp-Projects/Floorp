use core::mem::size_of;

use bytemuck::*;

#[test]
fn test_try_cast_slice() {
  // some align4 data
  let u32_slice: &[u32] = &[4, 5, 6];
  // the same data as align1
  let the_bytes: &[u8] = try_cast_slice(u32_slice).unwrap();

  assert_eq!(
    u32_slice.as_ptr() as *const u32 as usize,
    the_bytes.as_ptr() as *const u8 as usize
  );
  assert_eq!(
    u32_slice.len() * size_of::<u32>(),
    the_bytes.len() * size_of::<u8>()
  );

  // by taking one byte off the front, we're definitely mis-aligned for u32.
  let mis_aligned_bytes = &the_bytes[1..];
  assert_eq!(
    try_cast_slice::<u8, u32>(mis_aligned_bytes),
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  );

  // by taking one byte off the end, we're aligned but would have slop bytes for u32
  let the_bytes_len_minus1 = the_bytes.len() - 1;
  let slop_bytes = &the_bytes[..the_bytes_len_minus1];
  assert_eq!(
    try_cast_slice::<u8, u32>(slop_bytes),
    Err(PodCastError::OutputSliceWouldHaveSlop)
  );

  // if we don't mess with it we can up-alignment cast
  try_cast_slice::<u8, u32>(the_bytes).unwrap();
}

#[test]
fn test_try_cast_slice_mut() {
  // some align4 data
  let u32_slice: &mut [u32] = &mut [4, 5, 6];
  let u32_len = u32_slice.len();
  let u32_ptr = u32_slice.as_ptr();

  // the same data as align1
  let the_bytes: &mut [u8] = try_cast_slice_mut(u32_slice).unwrap();
  let the_bytes_len = the_bytes.len();
  let the_bytes_ptr = the_bytes.as_ptr();

  assert_eq!(
    u32_ptr as *const u32 as usize,
    the_bytes_ptr as *const u8 as usize
  );
  assert_eq!(u32_len * size_of::<u32>(), the_bytes_len * size_of::<u8>());

  // by taking one byte off the front, we're definitely mis-aligned for u32.
  let mis_aligned_bytes = &mut the_bytes[1..];
  assert_eq!(
    try_cast_slice_mut::<u8, u32>(mis_aligned_bytes),
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  );

  // by taking one byte off the end, we're aligned but would have slop bytes for u32
  let the_bytes_len_minus1 = the_bytes.len() - 1;
  let slop_bytes = &mut the_bytes[..the_bytes_len_minus1];
  assert_eq!(
    try_cast_slice_mut::<u8, u32>(slop_bytes),
    Err(PodCastError::OutputSliceWouldHaveSlop)
  );

  // if we don't mess with it we can up-alignment cast
  try_cast_slice_mut::<u8, u32>(the_bytes).unwrap();
}

#[test]
fn test_types() {
  let _: i32 = cast(1.0_f32);
  let _: &mut i32 = cast_mut(&mut 1.0_f32);
  let _: &i32 = cast_ref(&1.0_f32);
  let _: &[i32] = cast_slice(&[1.0_f32]);
  let _: &mut [i32] = cast_slice_mut(&mut [1.0_f32]);
  //
  let _: Result<i32, PodCastError> = try_cast(1.0_f32);
  let _: Result<&mut i32, PodCastError> = try_cast_mut(&mut 1.0_f32);
  let _: Result<&i32, PodCastError> = try_cast_ref(&1.0_f32);
  let _: Result<&[i32], PodCastError> = try_cast_slice(&[1.0_f32]);
  let _: Result<&mut [i32], PodCastError> = try_cast_slice_mut(&mut [1.0_f32]);
}
