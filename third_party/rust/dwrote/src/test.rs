/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use winapi;

use super::*;

#[test]
fn test_system_family_iter() {
    let system_fc = FontCollection::system();
    let count = system_fc.families_iter().count();
    assert!(count > 0);
    assert!(system_fc.families_iter().find(|f| f.name() == "Arial").is_some());
}

#[test]
fn test_descriptor_round_trip() {
    let system_fc = FontCollection::system();

    let arial_family = system_fc.get_font_family_by_name("Arial").unwrap();
    let arial_font = arial_family.get_first_matching_font(FontWeight::Regular,
                                                          FontStretch::Normal,
                                                          FontStyle::Normal);

    let descriptor = arial_font.to_descriptor();
    assert!(descriptor.family_name == "Arial");

    let arial_font_2 = system_fc.get_font_from_descriptor(&descriptor).unwrap();
    let descriptor2 = arial_font_2.to_descriptor();
    assert_eq!(descriptor, descriptor2);
}

#[test]
fn test_get_font_file_bytes() {
    let system_fc = FontCollection::system();

    let arial_family = system_fc.get_font_family_by_name("Arial").unwrap();
    let arial_font = arial_family.get_first_matching_font(FontWeight::Regular,
                                                          FontStretch::Normal,
                                                          FontStyle::Normal);
    let face = arial_font.create_font_face();
    let files = face.get_files();
    assert!(files.len() > 0);

    let bytes = files[0].get_font_file_bytes();
    println!("Bytes length: {}", bytes.len());
    assert!(bytes.len() > 0);
}

#[test]
fn test_glyph_image() {
    let system_fc = FontCollection::system();
    let arial_family = system_fc.get_font_family_by_name("Arial").unwrap();
    let arial_font = arial_family.get_first_matching_font(FontWeight::Regular,
                                                          FontStretch::Normal,
                                                          FontStyle::Normal);

    let face = arial_font.create_font_face();
    let a_index = face.get_glyph_indices(&['A' as u32])[0];

    let metrics = face.get_metrics();
    println!("Metrics:\n======\n{:?}\n======", metrics);

    let gm = face.get_design_glyph_metrics(&[a_index], false)[0];
    println!("Glyph metrics:\n======\n{:?}\n======", gm);

    let device_pixel_ratio = 1.0f32;
    let em_size = 10.0f32;

    let design_units_per_pixel = face.metrics().designUnitsPerEm as f32 / 16. as f32;
    let scaled_design_units_to_pixels = (em_size * device_pixel_ratio) / design_units_per_pixel;

    let width = (gm.advanceWidth as i32 - (gm.leftSideBearing + gm.rightSideBearing)) as f32 * scaled_design_units_to_pixels;
    let height = (gm.advanceHeight as i32 - (gm.topSideBearing + gm.bottomSideBearing)) as f32 * scaled_design_units_to_pixels;
    let x = (-gm.leftSideBearing) as f32 * scaled_design_units_to_pixels;
    let y = (gm.verticalOriginY - gm.topSideBearing) as f32 * scaled_design_units_to_pixels;

    // FIXME I'm pretty sure we need to do a proper RoundOut type
    // operation on this rect to properly handle any aliasing
    let left_i = x.floor() as i32;
    let top_i = (height - y).floor() as i32;
    let width_u = width.ceil() as u32;
    let height_u = height.ceil() as u32;

    println!("GlyphDimensions: {} {} {} {}", left_i, top_i, width_u, height_u);

    let gdi_interop = GdiInterop::create();
    let rt = gdi_interop.create_bitmap_render_target(width_u, height_u);
    let rp = RenderingParams::create_for_primary_monitor();
    rt.set_pixels_per_dip(device_pixel_ratio);
    rt.draw_glyph_run(x as f32, y as f32,
                      DWRITE_MEASURING_MODE_NATURAL,
                      &face,
                      em_size,
                      &[a_index],
                      &[0f32],
                      &[GlyphOffset { advanceOffset: 0., ascenderOffset: 0. }],
                      &rp,
                      &(255.0f32, 255.0f32, 255.0f32));
    let bytes = rt.get_opaque_values_as_mask();
    println!("bytes length: {}", bytes.len());
}
