use aa_stroke::{StrokeStyle, LineCap, LineJoin, Point, Stroker, tri_rasterize::rasterize_to_mask};



fn write_image(data: &[u8], path: &str, width: u32, height: u32) {
    use std::path::Path;
    use std::fs::File;
    use std::io::BufWriter;

    /*let mut png_data: Vec<u8> = vec![0; (width * height * 3) as usize];
    let mut i = 0;
    for pixel in data {
        png_data[i] = ((pixel >> 16) & 0xff) as u8;
        png_data[i + 1] = ((pixel >> 8) & 0xff) as u8;
        png_data[i + 2] = ((pixel >> 0) & 0xff) as u8;
        i += 3;
    }*/


    let path = Path::new(path);
    let file = File::create(path).unwrap();
    let w = &mut BufWriter::new(file);

    let mut encoder = png::Encoder::new(w, width, height); // Width is 2 pixels and height is 1.
    encoder.set_color(png::ColorType::Grayscale);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().unwrap();

    writer.write_image_data(&data).unwrap(); // Save
}


// WpfGfx uses CShapeBase which has a set of Figures
// Figures have FigureFlags which include FigureFlagClosed
// so that we can know ahead of time whether the figure/subpath
// is closed

// How do we handle transformed paths? D2D seems to only support transforms that
// can be applied before stroking. (one's with uniform scale?)
fn main() {
    let mut stroker = Stroker::new(&StrokeStyle{
        cap: LineCap::Round, 
        join: LineJoin::Bevel, 
        width: 20.,
         ..Default::default()});
    stroker.move_to(Point::new(20., 20.), false);
    stroker.line_to(Point::new(100., 100.));
    stroker.line_to_capped(Point::new(110., 20.));
 
    
    stroker.move_to(Point::new(120., 20.), true);
    stroker.line_to(Point::new(120., 50.));
    stroker.line_to(Point::new(140., 50.));
    stroker.close();

    stroker.move_to(Point::new(20., 160.), true);
    stroker.curve_to(Point::new(100., 160.), Point::new(100., 180.), Point::new(20., 180.));
    stroker.close();

    let stroked = stroker.finish();
    dbg!(&stroked);

    let mask = rasterize_to_mask(&stroked, 200, 200);
    write_image(&mask,"out.png", 200, 200);
/* 
    struct Target;
    impl CFlatteningSink for Target {
        fn AcceptPointAndTangent(&mut self,
        pt: &GpPointR,
            // The point
        vec: &GpPointR,
            // The tangent there
        fLast: bool
            // Is this the last point on the curve?
        ) -> HRESULT {
        todo!()
    }

        fn AcceptPoint(&mut self,
            pt: &GpPointR,
                // The point
            t: f64,
                // Parameter we're at
            fAborted: &mut bool) -> HRESULT {
        println!("{} {}", pt.x, pt.y);
        return S_OK;
    }
    }
    let bezier = CBezier::new([GpPointR { x: 0., y: 0. },
        GpPointR { x: 0., y: 0. },
        GpPointR { x: 0., y: 0. },
        GpPointR { x: 100., y: 100. }]);
        let mut t = Target{};
    let mut f = CBezierFlattener::new(&bezier, &mut t, 0.1);
    f.Flatten(false);*/

}