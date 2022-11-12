// Output an .obj file of the generated mesh. Viewable at https://3dviewer.net/

fn output_obj_file(data: &[OutputVertex]) {
        for v in data {
                let color = v.coverage;
                println!("v {} {} {} {} {} {}", v.x, v.y, 0., color, color, color);
        }

        // output a standard triangle strip face list
        for n in (1..data.len()-1).step_by(3) {
                println!("f {} {} {}", n, n+1, n+2);
        }
}

use wpf_gpu_raster::{PathBuilder, OutputVertex};
fn main() {
    let mut p = PathBuilder::new();
    p.move_to(10., 10.0);
    p.line_to(30., 10.);
    p.line_to(50., 20.);
    p.line_to(30., 30.);
    p.line_to(10., 30.);
    p.close();
    let result = p.rasterize_to_tri_list(0, 0, 100, 100);
    output_obj_file(&result)
}
