use wpf_gpu_raster::PathBuilder;
fn main() {
    let mut p = PathBuilder::new();
    p.move_to(10., 10.);
    p.line_to(10., 30.);
    p.line_to(30., 30.);
    p.line_to(30., 10.);
    p.close();
    let _result = p.rasterize_to_tri_list(0, 0, 100, 100);
    //dbg!(result);
}
