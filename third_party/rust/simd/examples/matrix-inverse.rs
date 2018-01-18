extern crate simd;
use simd::f32x4;

fn mul(x: &[f32x4; 4], y: &[f32x4; 4]) -> [f32x4; 4] {
    let y0 = y[0];
    let y1 = y[1];
    let y2 = y[2];
    let y3 = y[3];
    [f32x4::splat(y0.extract(0)) * x[0] +
     f32x4::splat(y0.extract(1)) * x[1] +
     f32x4::splat(y0.extract(2)) * x[2] +
     f32x4::splat(y0.extract(3)) * x[3],
     f32x4::splat(y1.extract(0)) * x[0] +
     f32x4::splat(y1.extract(1)) * x[1] +
     f32x4::splat(y1.extract(2)) * x[2] +
     f32x4::splat(y1.extract(3)) * x[3],
     f32x4::splat(y2.extract(0)) * x[0] +
     f32x4::splat(y2.extract(1)) * x[1] +
     f32x4::splat(y2.extract(2)) * x[2] +
     f32x4::splat(y2.extract(3)) * x[3],
     f32x4::splat(y3.extract(0)) * x[0] +
     f32x4::splat(y3.extract(1)) * x[1] +
     f32x4::splat(y3.extract(2)) * x[2] +
     f32x4::splat(y3.extract(3)) * x[3],
     ]
}

#[allow(dead_code)]
fn inverse_naive(x: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let mut t = [[0_f32; 4]; 4];
    for i in 0..4 {
        t[0][i] = x[i][0];
        t[1][i] = x[i][1];
        t[2][i] = x[i][2];
        t[3][i] = x[i][3];
    }
    println!("{:?}", t);

    let _0 = t[2][2] * t[3][3];
    let _1 = t[2][3] * t[3][2];
    let _2 = t[2][1] * t[3][3];
    let _3 = t[2][3] * t[3][1];
    let _4 = t[2][1] * t[3][2];
    let _5 = t[2][2] * t[3][1];
    let _6 = t[2][0] * t[3][3];
    let _7 = t[2][3] * t[3][0];
    let _8 = t[2][0] * t[3][2];
    let _9 = t[2][2] * t[3][0];
    let _10 = t[2][0] * t[3][1];
    let _11 = t[2][1] * t[3][0];
    let v = [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11];
    println!("{:?}", v);

    let d00 = _0 * t[1][1] + _3 * t[1][2] + _4 * t[1][3] -
        (_1 * t[1][1] + _2 * t[1][2] + _5 * t[1][3]);
    let d01 = _1 * t[1][0] + _6 * t[1][2] + _9 * t[1][3] -
        (_0 * t[1][0] + _7 * t[1][2] + _8 * t[1][3]);
    let d02 = _2 * t[1][0] + _7 * t[1][1] + _10 * t[1][3] -
        (_3 * t[1][0] + _6 * t[1][1] + _11 * t[1][3]);
    let d03 = _5 * t[1][0] + _8 * t[1][1] + _11 * t[1][2] -
        (_4 * t[1][0] + _9 * t[1][1] + _10 * t[1][2]);
    let d10 = _1 * t[0][1] + _2 * t[0][2] + _5 * t[0][3] -
        (_0 * t[0][1] + _3 * t[0][2] + _4 * t[0][3]);
    let d11 = _0 * t[0][0] + _7 * t[0][2] + _8 * t[0][3] -
        (_1 * t[0][0] + _6 * t[0][2] + _9 * t[0][3]);
    let d12 = _3 * t[0][0] + _6 * t[0][1] + _11 * t[0][3] -
        (_2 * t[0][0] + _7 * t[0][1] + _10 * t[0][3]);
    let d13 = _4 * t[0][0] + _9 * t[0][1] + _10 * t[0][2] -
        (_5 * t[0][0] + _8 * t[0][1] + _11 * t[0][2]);

    println!("{:?}", [d00, d01, d02, d03, d10, d11, d12, d13]);

    let _0 = t[0][2] * t[1][3];
    let _1 = t[0][3] * t[1][2];
    let _2 = t[0][1] * t[1][3];
    let _3 = t[0][3] * t[1][1];
    let _4 = t[0][1] * t[1][2];
    let _5 = t[0][2] * t[1][1];
    let _6 = t[0][0] * t[1][3];
    let _7 = t[0][3] * t[1][0];
    let _8 = t[0][0] * t[1][2];
    let _9 = t[0][2] * t[1][0];
    let _10 = t[0][0] * t[1][1];
    let _11 = t[0][1] * t[1][0];
    let v = [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11];
    println!("{:?}", v);

    let d20  = _0*t[3][1]  + _3*t[3][2]  + _4*t[3][3]-
        (_1*t[3][1]  + _2*t[3][2]  + _5*t[3][3]);
    let d21  = _1*t[3][0]  + _6*t[3][2]  + _9*t[3][3]-
        (_0*t[3][0]  + _7*t[3][2]  + _8*t[3][3]);
    let d22 = _2*t[3][0]  + _7*t[3][1]  + _10*t[3][3]-
        (_3*t[3][0]  + _6*t[3][1]  + _11*t[3][3]);
    let d23 = _5*t[3][0]  + _8*t[3][1]  + _11*t[3][2]-
        (_4*t[3][0]  + _9*t[3][1]  + _10*t[3][2]);
    let d30 = _2*t[2][2]  + _5*t[2][3]  + _1*t[2][1]-
        (_4*t[2][3]  + _0*t[2][1]   + _3*t[2][2]);
    let d31 = _8*t[2][3]  + _0*t[2][0]   + _7*t[2][2]-
        (_6*t[2][2]  + _9*t[2][3]  + _1*t[2][0]);
    let d32 = _6*t[2][1]   + _11*t[2][3] + _3*t[2][0]-
        (_10*t[2][3] + _2*t[2][0]   + _7*t[2][1]);
    let d33 = _10*t[2][2] + _4*t[2][0]   + _9*t[2][1]-
        (_8*t[2][1]   + _11*t[2][2] + _5*t[2][0]);

    println!("{:?}", [d20, d21, d22, d23, d30, d31, d32, d33]);

    let det = t[0][0] * d00 + t[0][1] * d01 + t[0][2] * d02 + t[0][3] * d03;

    let det = 1.0 / det;
    let mut ret = [[d00, d01, d02, d03],
                   [d10, d11, d12, d13],
                   [d20, d21, d22, d23],
                   [d30, d31, d32, d33]];
    for i in 0..4 {
        for j in 0..4 {
            ret[i][j] *= det;
        }
    }
    ret
}

fn inverse_simd4(x: &[f32x4; 4]) -> [f32x4; 4] {
    let src0 = x[0];
    let src1 = x[1];
    let src2 = x[2];
    let src3 = x[3];

    let tmp1 = f32x4::new(src0.extract(0), src0.extract(1),
                          src1.extract(4 - 4), src1.extract(5 - 4));
    let row1 = f32x4::new(src2.extract(0), src2.extract(1),
                          src3.extract(4 - 4), src3.extract(5 - 4));
    let row0 = f32x4::new(tmp1.extract(0), tmp1.extract(2),
                          row1.extract(4 - 4), row1.extract(6 - 4));
    let row1 = f32x4::new(row1.extract(1), row1.extract(3),
                          tmp1.extract(5 - 4), tmp1.extract(7 - 4));

    let tmp1 = f32x4::new(src0.extract(2), src0.extract(3),
                          src1.extract(6 - 4), src1.extract(7 - 4));
    let row3 = f32x4::new(src2.extract(2), src2.extract(3),
                          src3.extract(6 - 4), src3.extract(7 - 4));
    let row2 = f32x4::new(tmp1.extract(0), tmp1.extract(2),
                          row3.extract(4 - 4), row3.extract(6 - 4));
    let row3 = f32x4::new(row3.extract(1), row3.extract(3),
                          tmp1.extract(5 - 4), tmp1.extract(7 - 4));


    let tmp1 = row2 * row3;
    let tmp1 = f32x4::new(tmp1.extract(1), tmp1.extract(0),
                          tmp1.extract(3), tmp1.extract(2));
    let minor0 = row1 * tmp1;
    let minor1 = row0 * tmp1;
    let tmp1 = f32x4::new(tmp1.extract(2), tmp1.extract(3),
                          tmp1.extract(0), tmp1.extract(1));
    let minor0 = (row1 * tmp1) - minor0;
    let minor1 = (row0 * tmp1) - minor1;
    let minor1 = f32x4::new(minor1.extract(2), minor1.extract(3),
                            minor1.extract(0), minor1.extract(1));
    //println!("{:?}", minor1);


    let tmp1 = row1 * row2;
    let tmp1 = f32x4::new(tmp1.extract(1), tmp1.extract(0),
                          tmp1.extract(3), tmp1.extract(2));
    let minor0 = (row3 * tmp1) + minor0;
    let minor3 = row0 * tmp1;
    let tmp1 = f32x4::new(tmp1.extract(2), tmp1.extract(3),
                          tmp1.extract(0), tmp1.extract(1));

    let minor0 = minor0 - row3 * tmp1;
    let minor3 = row0 * tmp1 - minor3;
    let minor3 = f32x4::new(minor3.extract(2), minor3.extract(3),
                            minor3.extract(0), minor3.extract(1));
    //println!("{:?}", minor1);


    let tmp1 = row3 * f32x4::new(row1.extract(2), row1.extract(3),
                                 row1.extract(0), row1.extract(1));
    let tmp1 = f32x4::new(tmp1.extract(1), tmp1.extract(0),
                          tmp1.extract(3), tmp1.extract(2));
    let row2 = f32x4::new(row2.extract(2), row2.extract(3),
                          row2.extract(0), row2.extract(1));
    let minor0 = row2 * tmp1 + minor0;
    let minor2 = row0 * tmp1;
    let tmp1 = f32x4::new(tmp1.extract(2), tmp1.extract(3),
                          tmp1.extract(0), tmp1.extract(1));
    let minor0 = minor0 - row2 * tmp1;
    let minor2 = row0 * tmp1 - minor2;
    let minor2 = f32x4::new(minor2.extract(2), minor2.extract(3),
                            minor2.extract(0), minor2.extract(1));
    //println!("{:?}", minor1);


    let tmp1 = row0 * row1;
    let tmp1 = f32x4::new(tmp1.extract(1), tmp1.extract(0),
                          tmp1.extract(3), tmp1.extract(2));
    let minor2 = minor2 + row3 * tmp1;
    let minor3 = row2 * tmp1 - minor3;
    let tmp1 = f32x4::new(tmp1.extract(2), tmp1.extract(3),
                          tmp1.extract(0), tmp1.extract(1));
    let minor2 = row3 * tmp1 - minor2;
    let minor3 = minor3 - row2 * tmp1;
    //println!("{:?}", minor1);



    let tmp1 = row0 * row3;
    let tmp1 = f32x4::new(tmp1.extract(1), tmp1.extract(0),
                          tmp1.extract(3), tmp1.extract(2));
    let minor1 = minor1 - row2 * tmp1;
    let minor2 = row1 * tmp1 + minor2;
    let tmp1 = f32x4::new(tmp1.extract(2), tmp1.extract(3),
                          tmp1.extract(0), tmp1.extract(1));
    let minor1 = row2 * tmp1 + minor1;
    let minor2 = minor2 - row1 * tmp1;
    //println!("{:?}", minor1);

    let tmp1 = row0 * row2;
    let tmp1 = f32x4::new(tmp1.extract(1), tmp1.extract(0),
                          tmp1.extract(3), tmp1.extract(2));
    let minor1 = row3 * tmp1 + minor1;
    let minor3 = minor3 - row1 * tmp1;
    let tmp1 = f32x4::new(tmp1.extract(2), tmp1.extract(3),
                          tmp1.extract(0), tmp1.extract(1));
    let minor1 = minor1 - row3 * tmp1;
    let minor3 = row1 * tmp1 + minor3;
    //println!("{:?}", minor1);

    let det = row0 * minor0;
    let det = f32x4::new(det.extract(2), det.extract(3),
                         det.extract(0), det.extract(1)) + det;
    let det = f32x4::new(det.extract(1), det.extract(0),
                         det.extract(3), det.extract(2)) + det;
    let tmp1 = det.approx_reciprocal();
    let det = tmp1 + tmp1 - det * tmp1 * tmp1;

//    let det = f32x4::splat(det.extract(0));

    [minor0 * det, minor1 * det, minor2 * det, minor3 * det]
}

fn p(x: &[f32x4; 4]) {
    for xx in x {
        for i in 0..4 {
            let v = xx.extract(i);
            if v == 0.0 {
                print!("{}{:6.2}", if i > 0 {", "} else {"|"}, "");
            } else {
                print!("{}{:6.2}", if i > 0 {", "} else {"|"}, xx.extract(i));
            }
        }
        println!(" |");
    }
}

fn main() {
    let x = [f32x4::new(-100.0, 6.0, 100.0, 1.0),
             f32x4::new(3.0, 1.0, 0.0, 1.0),
             f32x4::new(2.0, 1.0, 1.0, 1.0),
             f32x4::new(-10.0, 1.0, 1.0, 1.0)];

  /*  let mut x_ = [[0.0; 4]; 4];
    for i in 0..4 {
        for j in 0..4 {
            x_[i][j] = x[i].extract(j as u32)
        }
    }

    let ret = inverse_naive(&x_);
    let mut y = [f32x4::splat(0.0); 4];
    for i in 0..4 {
        for j in 0..4 {
            y[i] = y[i].replace(j as u32, ret[i][j])
        }
}*/
    let y = inverse_simd4(&x);
    p(&x);
    println!("");
    p(&y);
    println!("");
    p(&mul(&x, &y))
}
