static CONST1: i32 = 20091;
static CONST2: i32 = 35468;

pub fn idct4x4(block: &mut [i32]) {
    for i in 0usize..4 {
        let a1 = block[i] + block[8 + i];
        let b1 = block[i] - block[8 + i];

        let t1 = (block[4 + i] * CONST2) >> 16;
        let t2 = block[12 + i] + ((block[12 + i] * CONST1) >> 16);
        let c1 = t1 - t2;

        let t1 = block[4 + i] + ((block[4 + i] * CONST1) >> 16);
        let t2 = (block[12 + i] * CONST2) >> 16;
        let d1 = t1 + t2;

        block[i] = a1 + d1;
        block[4 + i] = b1 + c1;
        block[4 * 3 + i] = a1 - d1;
        block[4 * 2 + i] = b1 - c1;
    }

    for i in 0usize..4 {
        let a1 = block[4 * i] + block[4 * i + 2];
        let b1 = block[4 * i] - block[4 * i + 2];

        let t1 = (block[4 * i + 1] * CONST2) >> 16;
        let t2 = block[4 * i + 3] + ((block[4 * i + 3] * CONST1) >> 16);
        let c1 = t1 - t2;

        let t1 = block[4 * i + 1] + ((block[4 * i + 1] * CONST1) >> 16);
        let t2 = (block[4 * i + 3] * CONST2) >> 16;
        let d1 = t1 + t2;

        block[4 * i] = (a1 + d1 + 4) >> 3;
        block[4 * i + 3] = (a1 - d1 + 4) >> 3;
        block[4 * i + 1] = (b1 + c1 + 4) >> 3;
        block[4 * i + 2] = (b1 - c1 + 4) >> 3;
    }
}

// 14.3
pub fn iwht4x4(block: &mut [i32]) {
    for i in 0usize..4 {
        let a1 = block[i] + block[12 + i];
        let b1 = block[4 + i] + block[8 + i];
        let c1 = block[4 + i] - block[8 + i];
        let d1 = block[i] - block[12 + i];

        block[i] = a1 + b1;
        block[4 + i] = c1 + d1;
        block[8 + i] = a1 - b1;
        block[12 + i] = d1 - c1;
    }

    for i in 0usize..4 {
        let a1 = block[4 * i] + block[4 * i + 3];
        let b1 = block[4 * i + 1] + block[4 * i + 2];
        let c1 = block[4 * i + 1] - block[4 * i + 2];
        let d1 = block[4 * i] - block[4 * i + 3];

        let a2 = a1 + b1;
        let b2 = c1 + d1;
        let c2 = a1 - b1;
        let d2 = d1 - c1;

        block[4 * i] = (a2 + 3) >> 3;
        block[4 * i + 1] = (b2 + 3) >> 3;
        block[4 * i + 2] = (c2 + 3) >> 3;
        block[4 * i + 3] = (d2 + 3) >> 3;
    }
}
