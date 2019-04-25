//! NEUQUANT Neural-Net quantization algorithm by Anthony Dekker, 1994.
//! See "Kohonen neural networks for optimal colour quantization"
//! in "Network: Computation in Neural Systems" Vol. 5 (1994) pp 351-367.
//! for a discussion of the algorithm.
//! See also <http://www.acm.org/~dekker/NEUQUANT.HTML>

/* NeuQuant Neural-Net Quantization Algorithm
 * ------------------------------------------
 *
 * Copyright (c) 1994 Anthony Dekker
 *
 * NEUQUANT Neural-Net quantization algorithm by Anthony Dekker, 1994.
 * See "Kohonen neural networks for optimal colour quantization"
 * in "Network: Computation in Neural Systems" Vol. 5 (1994) pp 351-367.
 * for a discussion of the algorithm.
 * See also  http://www.acm.org/~dekker/NEUQUANT.HTML
 *
 * Any party obtaining a copy of these files from the author, directly or
 * indirectly, is granted, free of charge, a full and unrestricted irrevocable,
 * world-wide, paid up, royalty-free, nonexclusive right and license to deal
 * in this software and documentation files (the "Software"), including without
 * limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons who receive
 * copies from any such party to do so, with the only requirement being
 * that this copyright notice remain intact.
 *
 *
 * Incorporated bugfixes and alpha channel handling from pngnq
 * http://pngnq.sourceforge.net
 *
 */

use math::utils::clamp;
use std::cmp::{max, min};

const CHANNELS: usize = 4;

const RADIUS_DEC: i32 = 30; // factor of 1/30 each cycle

const ALPHA_BIASSHIFT: i32 = 10; // alpha starts at 1
const INIT_ALPHA: i32 = 1 << ALPHA_BIASSHIFT; // biased by 10 bits

const GAMMA: f64 = 1024.0;
const BETA: f64 = 1.0 / GAMMA;
const BETAGAMMA: f64 = BETA * GAMMA;

// four primes near 500 - assume no image has a length so large
// that it is divisible by all four primes
const PRIMES: [usize; 4] = [499, 491, 478, 503];

#[derive(Clone, Copy)]
struct Quad<T> {
    r: T,
    g: T,
    b: T,
    a: T,
}

type Neuron = Quad<f64>;
type Color = Quad<i32>;

/// Neural network color quantizer
pub struct NeuQuant {
    network: Vec<Neuron>,
    colormap: Vec<Color>,
    netindex: Vec<usize>,
    bias: Vec<f64>, // bias and freq arrays for learning
    freq: Vec<f64>,
    samplefac: i32,
    netsize: usize,
}

impl NeuQuant {
    /// Creates a new neural network and trains it with the supplied data
    pub fn new(samplefac: i32, colors: usize, pixels: &[u8]) -> Self {
        let netsize = colors;
        let mut this = NeuQuant {
            network: Vec::with_capacity(netsize),
            colormap: Vec::with_capacity(netsize),
            netindex: vec![0; 256],
            bias: Vec::with_capacity(netsize),
            freq: Vec::with_capacity(netsize),
            samplefac,
            netsize,
        };
        this.init(pixels);
        this
    }

    /// Initializes the neural network and trains it with the supplied data
    pub fn init(&mut self, pixels: &[u8]) {
        self.network.clear();
        self.colormap.clear();
        self.bias.clear();
        self.freq.clear();
        let freq = (self.netsize as f64).recip();
        for i in 0..self.netsize {
            let tmp = (i as f64) * 256.0 / (self.netsize as f64);
            // Sets alpha values at 0 for dark pixels.
            let a = if i < 16 { i as f64 * 16.0 } else { 255.0 };
            self.network.push(Neuron {
                r: tmp,
                g: tmp,
                b: tmp,
                a,
            });
            self.colormap.push(Color {
                r: 0,
                g: 0,
                b: 0,
                a: 255,
            });
            self.freq.push(freq);
            self.bias.push(0.0);
        }
        self.learn(pixels);
        self.build_colormap();
        self.build_netindex();
    }

    /// Maps the pixel in-place to the best-matching color in the color map
    #[inline(always)]
    pub fn map_pixel(&self, pixel: &mut [u8]) {
        assert_eq!(pixel.len(), 4);
        match (pixel[0], pixel[1], pixel[2], pixel[3]) {
            (r, g, b, a) => {
                let i = self.search_netindex(b, g, r, a);
                pixel[0] = self.colormap[i].r as u8;
                pixel[1] = self.colormap[i].g as u8;
                pixel[2] = self.colormap[i].b as u8;
                pixel[3] = self.colormap[i].a as u8;
            }
        }
    }

    /// Finds the best-matching index in the color map for `pixel`
    #[inline(always)]
    pub fn index_of(&self, pixel: &[u8]) -> usize {
        assert_eq!(pixel.len(), 4);
        match (pixel[0], pixel[1], pixel[2], pixel[3]) {
            (r, g, b, a) => self.search_netindex(b, g, r, a),
        }
    }

    /// Move neuron i towards biased (a,b,g,r) by factor alpha
    fn alter_single(&mut self, alpha: f64, i: i32, quad: Quad<f64>) {
        let n = &mut self.network[i as usize];
        n.b -= alpha * (n.b - quad.b);
        n.g -= alpha * (n.g - quad.g);
        n.r -= alpha * (n.r - quad.r);
        n.a -= alpha * (n.a - quad.a);
    }

    /// Move neuron adjacent neurons towards biased (a,b,g,r) by factor alpha
    fn alter_neighbour(&mut self, alpha: f64, rad: i32, i: i32, quad: Quad<f64>) {
        let lo = max(i - rad, 0);
        let hi = min(i + rad, self.netsize as i32);
        let mut j = i + 1;
        let mut k = i - 1;
        let mut q = 0;

        while (j < hi) || (k > lo) {
            let rad_sq = f64::from(rad) * f64::from(rad);
            let alpha = (alpha * (rad_sq - f64::from(q) * f64::from(q))) / rad_sq;
            q += 1;
            if j < hi {
                let p = &mut self.network[j as usize];
                p.b -= alpha * (p.b - quad.b);
                p.g -= alpha * (p.g - quad.g);
                p.r -= alpha * (p.r - quad.r);
                p.a -= alpha * (p.a - quad.a);
                j += 1;
            }
            if k > lo {
                let p = &mut self.network[k as usize];
                p.b -= alpha * (p.b - quad.b);
                p.g -= alpha * (p.g - quad.g);
                p.r -= alpha * (p.r - quad.r);
                p.a -= alpha * (p.a - quad.a);
                k -= 1;
            }
        }
    }

    /// Search for biased BGR values
    /// finds closest neuron (min dist) and updates freq
    /// finds best neuron (min dist-bias) and returns position
    /// for frequently chosen neurons, freq[i] is high and bias[i] is negative
    /// bias[i] = gamma*((1/self.netsize)-freq[i])
    fn contest(&mut self, b: f64, g: f64, r: f64, a: f64) -> i32 {
        use std::f64;

        let mut bestd = f64::MAX;
        let mut bestbiasd: f64 = bestd;
        let mut bestpos = -1;
        let mut bestbiaspos: i32 = bestpos;

        for i in 0..self.netsize {
            let bestbiasd_biased = bestbiasd + self.bias[i];
            let mut dist;
            let n = &self.network[i];
            dist = (n.b - b).abs();
            dist += (n.r - r).abs();
            if dist < bestd || dist < bestbiasd_biased {
                dist += (n.g - g).abs();
                dist += (n.a - a).abs();
                if dist < bestd {
                    bestd = dist;
                    bestpos = i as i32;
                }
                let biasdist = dist - self.bias[i];
                if biasdist < bestbiasd {
                    bestbiasd = biasdist;
                    bestbiaspos = i as i32;
                }
            }
            self.freq[i] -= BETA * self.freq[i];
            self.bias[i] += BETAGAMMA * self.freq[i];
        }
        self.freq[bestpos as usize] += BETA;
        self.bias[bestpos as usize] -= BETAGAMMA;
        bestbiaspos
    }

    /// Main learning loop
    /// Note: the number of learning cycles is crucial and the parameters are not
    /// optimized for net sizes < 26 or > 256. 1064 colors seems to work fine
    fn learn(&mut self, pixels: &[u8]) {
        let initrad: i32 = self.netsize as i32 / 8; // for 256 cols, radius starts at 32
        let radiusbiasshift: i32 = 6;
        let radiusbias: i32 = 1 << radiusbiasshift;
        let init_bias_radius: i32 = initrad * radiusbias;
        let mut bias_radius = init_bias_radius;
        let alphadec = 30 + ((self.samplefac - 1) / 3);
        let lengthcount = pixels.len() / CHANNELS;
        let samplepixels = lengthcount / self.samplefac as usize;
        // learning cycles
        let n_cycles = match self.netsize >> 1 {
            n if n <= 100 => 100,
            n => n,
        };
        let delta = match samplepixels / n_cycles {
            0 => 1,
            n => n,
        };
        let mut alpha = INIT_ALPHA;

        let mut rad = bias_radius >> radiusbiasshift;
        if rad <= 1 {
            rad = 0
        };

        let mut pos = 0;
        let step = *PRIMES
            .iter()
            .find(|&&prime| lengthcount % prime != 0)
            .unwrap_or(&PRIMES[3]);

        let mut i = 0;
        while i < samplepixels {
            let (r, g, b, a) = {
                let p = &pixels[CHANNELS * pos..][..CHANNELS];
                (
                    f64::from(p[0]),
                    f64::from(p[1]),
                    f64::from(p[2]),
                    f64::from(p[3]),
                )
            };

            let j = self.contest(b, g, r, a);

            let alpha_ = (1.0 * f64::from(alpha)) / f64::from(INIT_ALPHA);
            self.alter_single(alpha_, j, Quad { b, g, r, a });
            if rad > 0 {
                self.alter_neighbour(alpha_, rad, j, Quad { b, g, r, a })
            };

            pos += step;
            while pos >= lengthcount {
                pos -= lengthcount
            }

            i += 1;
            if i % delta == 0 {
                alpha -= alpha / alphadec;
                bias_radius -= bias_radius / RADIUS_DEC;
                rad = bias_radius >> radiusbiasshift;
                if rad <= 1 {
                    rad = 0
                };
            }
        }
    }

    /// initializes the color map
    fn build_colormap(&mut self) {
        for i in 0usize..self.netsize {
            self.colormap[i].b = clamp(self.network[i].b.round() as i32, 0, 255);
            self.colormap[i].g = clamp(self.network[i].g.round() as i32, 0, 255);
            self.colormap[i].r = clamp(self.network[i].r.round() as i32, 0, 255);
            self.colormap[i].a = clamp(self.network[i].a.round() as i32, 0, 255);
        }
    }

    /// Insertion sort of network and building of netindex[0..255]
    fn build_netindex(&mut self) {
        let mut previouscol = 0;
        let mut startpos = 0;

        for i in 0..self.netsize {
            let mut p = self.colormap[i];
            let mut q;
            let mut smallpos = i;
            let mut smallval = p.g as usize; // index on g
                                             // find smallest in i..netsize-1
            for j in (i + 1)..self.netsize {
                q = self.colormap[j];
                if (q.g as usize) < smallval {
                    // index on g
                    smallpos = j;
                    smallval = q.g as usize; // index on g
                }
            }
            q = self.colormap[smallpos];
            // swap p (i) and q (smallpos) entries
            if i != smallpos {
                ::std::mem::swap(&mut p, &mut q);
                self.colormap[i] = p;
                self.colormap[smallpos] = q;
            }
            // smallval entry is now in position i
            if smallval != previouscol {
                self.netindex[previouscol] = (startpos + i) >> 1;
                for j in (previouscol + 1)..smallval {
                    self.netindex[j] = i
                }
                previouscol = smallval;
                startpos = i;
            }
        }
        let max_netpos = self.netsize - 1;
        self.netindex[previouscol] = (startpos + max_netpos) >> 1;
        for j in (previouscol + 1)..256 {
            self.netindex[j] = max_netpos
        } // really 256
    }

    /// Search for best matching color
    fn search_netindex(&self, b: u8, g: u8, r: u8, a: u8) -> usize {
        let mut bestd = 1 << 30; // ~ 1_000_000
        let mut best = 0;
        // start at netindex[g] and work outwards
        let mut i = self.netindex[g as usize];
        let mut j = if i > 0 { i - 1 } else { 0 };

        while (i < self.netsize) || (j > 0) {
            if i < self.netsize {
                let p = self.colormap[i];
                let mut e = p.g - i32::from(g);
                let mut dist = e * e; // index key
                if dist >= bestd {
                    break;
                } else {
                    e = p.b - i32::from(b);
                    dist += e * e;
                    if dist < bestd {
                        e = p.r - i32::from(r);
                        dist += e * e;
                        if dist < bestd {
                            e = p.a - i32::from(a);
                            dist += e * e;
                            if dist < bestd {
                                bestd = dist;
                                best = i;
                            }
                        }
                    }
                    i += 1;
                }
            }
            if j > 0 {
                let p = self.colormap[j];
                let mut e = p.g - i32::from(g);
                let mut dist = e * e; // index key
                if dist >= bestd {
                    break;
                } else {
                    e = p.b - i32::from(b);
                    dist += e * e;
                    if dist < bestd {
                        e = p.r - i32::from(r);
                        dist += e * e;
                        if dist < bestd {
                            e = p.a - i32::from(a);
                            dist += e * e;
                            if dist < bestd {
                                bestd = dist;
                                best = j;
                            }
                        }
                    }
                    j -= 1;
                }
            }
        }
        best
    }
}
