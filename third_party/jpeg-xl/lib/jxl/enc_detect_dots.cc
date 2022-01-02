// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_detect_dots.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_detect_dots.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/common.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/linalg.h"
#include "lib/jxl/optimize.h"

// Set JXL_DEBUG_DOT_DETECT to 1 to enable debugging.
#ifndef JXL_DEBUG_DOT_DETECT
#define JXL_DEBUG_DOT_DETECT 0
#endif

#if JXL_DEBUG_DOT_DETECT
#include "lib/jxl/aux_out.h"
#endif

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

ImageF SumOfSquareDifferences(const Image3F& forig, const Image3F& smooth,
                              ThreadPool* pool) {
  const HWY_FULL(float) d;
  const auto color_coef0 = Set(d, 0.0f);
  const auto color_coef1 = Set(d, 10.0f);
  const auto color_coef2 = Set(d, 0.0f);

  ImageF sum_of_squares(forig.xsize(), forig.ysize());
  RunOnPool(
      pool, 0, forig.ysize(), ThreadPool::SkipInit(),
      [&](const int task, const int thread) {
        const size_t y = static_cast<size_t>(task);
        const float* JXL_RESTRICT orig_row0 = forig.Plane(0).ConstRow(y);
        const float* JXL_RESTRICT orig_row1 = forig.Plane(1).ConstRow(y);
        const float* JXL_RESTRICT orig_row2 = forig.Plane(2).ConstRow(y);
        const float* JXL_RESTRICT smooth_row0 = smooth.Plane(0).ConstRow(y);
        const float* JXL_RESTRICT smooth_row1 = smooth.Plane(1).ConstRow(y);
        const float* JXL_RESTRICT smooth_row2 = smooth.Plane(2).ConstRow(y);
        float* JXL_RESTRICT sos_row = sum_of_squares.Row(y);

        for (size_t x = 0; x < forig.xsize(); x += Lanes(d)) {
          auto v0 = Load(d, orig_row0 + x) - Load(d, smooth_row0 + x);
          auto v1 = Load(d, orig_row1 + x) - Load(d, smooth_row1 + x);
          auto v2 = Load(d, orig_row2 + x) - Load(d, smooth_row2 + x);
          v0 *= v0;
          v1 *= v1;
          v2 *= v2;
          v0 *= color_coef0;  // FMA doesn't help here.
          v1 *= color_coef1;
          v2 *= color_coef2;
          const auto sos = v0 + v1 + v2;  // weighted sum of square diffs
          Store(sos, d, sos_row + x);
        }
      },
      "ComputeEnergyImage");
  return sum_of_squares;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(SumOfSquareDifferences);  // Local function

const int kEllipseWindowSize = 5;

namespace {
struct GaussianEllipse {
  double x;                         // position in x
  double y;                         // position in y
  double sigma_x;                   // scale in x
  double sigma_y;                   // scale in y
  double angle;                     // ellipse rotation in radians
  std::array<double, 3> intensity;  // intensity in each channel

  // The following variables do not need to be encoded
  double l2_loss;  // error after the Gaussian was fit
  double l1_loss;
  double ridge_loss;              // the l2_loss plus regularization term
  double custom_loss;             // experimental custom loss
  std::array<double, 3> bgColor;  // best background color
  size_t neg_pixels;  // number of negative pixels when subtracting dot
  std::array<double, 3> neg_value;  // debt due to channel truncation
};
double DotGaussianModel(double dx, double dy, double ct, double st,
                        double sigma_x, double sigma_y, double intensity) {
  double rx = ct * dx + st * dy;
  double ry = -st * dx + ct * dy;
  double md = (rx * rx / sigma_x) + (ry * ry / sigma_y);
  double value = intensity * exp(-0.5 * md);
  return value;
}

constexpr bool kOptimizeBackground = true;

// Gaussian that smooths noise but preserves dots
const WeightsSeparable5& WeightsSeparable5Gaussian0_65() {
  constexpr float w0 = 0.558311f;
  constexpr float w1 = 0.210395f;
  constexpr float w2 = 0.010449f;
  static constexpr WeightsSeparable5 weights = {
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)},
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)}};
  return weights;
}

// (Iterated) Gaussian that removes dots.
const WeightsSeparable5& WeightsSeparable5Gaussian3() {
  constexpr float w0 = 0.222338f;
  constexpr float w1 = 0.210431f;
  constexpr float w2 = 0.1784f;
  static constexpr WeightsSeparable5 weights = {
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)},
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)}};
  return weights;
}

ImageF ComputeEnergyImage(const Image3F& orig, Image3F* smooth,
                          ThreadPool* pool) {
  PROFILER_FUNC;

  // Prepare guidance images for dot selection.
  Image3F forig(orig.xsize(), orig.ysize());
  Image3F tmp(orig.xsize(), orig.ysize());
  *smooth = Image3F(orig.xsize(), orig.ysize());

  const auto& weights1 = WeightsSeparable5Gaussian0_65();
  const auto& weights3 = WeightsSeparable5Gaussian3();

  Separable5_3(orig, Rect(orig), weights1, pool, &forig);

  Separable5_3(orig, Rect(orig), weights3, pool, &tmp);
  Separable5_3(tmp, Rect(tmp), weights3, pool, smooth);

#if JXL_DEBUG_DOT_DETECT
  AuxOut aux;
  aux.debug_prefix = "/tmp/sebastian/";
  aux.DumpImage("filtered", forig);
  aux.DumpImage("sm", *smooth);
#endif

  return HWY_DYNAMIC_DISPATCH(SumOfSquareDifferences)(forig, *smooth, pool);
}

struct Pixel {
  int x;
  int y;
};

Pixel operator+(const Pixel& a, const Pixel& b) {
  return Pixel{a.x + b.x, a.y + b.y};
}

// Maximum area in pixels of a ellipse
const size_t kMaxCCSize = 1000;

// Extracts a connected component from a Binary image where seed is part
// of the component
bool ExtractComponent(ImageF* img, std::vector<Pixel>* pixels,
                      const Pixel& seed, double threshold) {
  PROFILER_FUNC;
  static const std::vector<Pixel> neighbors{{1, -1}, {1, 0},   {1, 1},  {0, -1},
                                            {0, 1},  {-1, -1}, {-1, 1}, {1, 0}};
  std::vector<Pixel> q{seed};
  while (!q.empty()) {
    Pixel current = q.back();
    q.pop_back();
    pixels->push_back(current);
    if (pixels->size() > kMaxCCSize) return false;
    for (const Pixel& delta : neighbors) {
      Pixel child = current + delta;
      if (child.x >= 0 && static_cast<size_t>(child.x) < img->xsize() &&
          child.y >= 0 && static_cast<size_t>(child.y) < img->ysize()) {
        float* value = &img->Row(child.y)[child.x];
        if (*value > threshold) {
          *value = 0.0;
          q.push_back(child);
        }
      }
    }
  }
  return true;
}

inline bool PointInRect(const Rect& r, const Pixel& p) {
  return (static_cast<size_t>(p.x) >= r.x0() &&
          static_cast<size_t>(p.x) < (r.x0() + r.xsize()) &&
          static_cast<size_t>(p.y) >= r.y0() &&
          static_cast<size_t>(p.y) < (r.y0() + r.ysize()));
}

struct ConnectedComponent {
  ConnectedComponent(const Rect& bounds, const std::vector<Pixel>&& pixels)
      : bounds(bounds), pixels(pixels) {}
  Rect bounds;
  std::vector<Pixel> pixels;
  float maxEnergy;
  float meanEnergy;
  float varEnergy;
  float meanBg;
  float varBg;
  float score;
  Pixel mode;

  void CompStats(const ImageF& energy, int extra) {
    PROFILER_FUNC;
    maxEnergy = 0.0;
    meanEnergy = 0.0;
    varEnergy = 0.0;
    meanBg = 0.0;
    varBg = 0.0;
    int nIn = 0;
    int nOut = 0;
    mode.x = 0;
    mode.y = 0;
    for (int sy = -extra; sy < (static_cast<int>(bounds.ysize()) + extra);
         sy++) {
      int y = sy + static_cast<int>(bounds.y0());
      if (y < 0 || static_cast<size_t>(y) >= energy.ysize()) continue;
      const float* JXL_RESTRICT erow = energy.ConstRow(y);
      for (int sx = -extra; sx < (static_cast<int>(bounds.xsize()) + extra);
           sx++) {
        int x = sx + static_cast<int>(bounds.x0());
        if (x < 0 || static_cast<size_t>(x) >= energy.xsize()) continue;
        if (erow[x] > maxEnergy) {
          maxEnergy = erow[x];
          mode.x = x;
          mode.y = y;
        }
        if (PointInRect(bounds, Pixel{x, y})) {
          meanEnergy += erow[x];
          varEnergy += erow[x] * erow[x];
          nIn++;
        } else {
          meanBg += erow[x];
          varBg += erow[x] * erow[x];
          nOut++;
        }
      }
    }
    meanEnergy = meanEnergy / nIn;
    meanBg = meanBg / nOut;
    varEnergy = (varEnergy / nIn) - meanEnergy * meanEnergy;
    varBg = (varBg / nOut) - meanBg * meanBg;
    score = (meanEnergy - meanBg) / std::sqrt(varBg);
  }
};

Rect BoundingRectangle(const std::vector<Pixel>& pixels) {
  PROFILER_FUNC;
  JXL_ASSERT(!pixels.empty());
  int low_x, high_x, low_y, high_y;
  low_x = high_x = pixels[0].x;
  low_y = high_y = pixels[0].y;
  for (const Pixel& p : pixels) {
    low_x = std::min(low_x, p.x);
    high_x = std::max(high_x, p.x);
    low_y = std::min(low_y, p.y);
    high_y = std::max(high_y, p.y);
  }
  return Rect(low_x, low_y, high_x - low_x + 1, high_y - low_y + 1);
}

std::vector<ConnectedComponent> FindCC(const ImageF& energy, double t_low,
                                       double t_high, uint32_t maxWindow,
                                       double minScore) {
  PROFILER_FUNC;
  const int kExtraRect = 4;
  ImageF img = CopyImage(energy);
  std::vector<ConnectedComponent> ans;
  for (size_t y = 0; y < img.ysize(); y++) {
    float* JXL_RESTRICT row = img.Row(y);
    for (size_t x = 0; x < img.xsize(); x++) {
      if (row[x] > t_high) {
        std::vector<Pixel> pixels;
        row[x] = 0.0;
        bool success = ExtractComponent(
            &img, &pixels, Pixel{static_cast<int>(x), static_cast<int>(y)},
            t_low);
        if (!success) continue;
#if JXL_DEBUG_DOT_DETECT
        for (size_t i = 0; i < pixels.size(); i++) {
          fprintf(stderr, "(%d,%d) ", pixels[i].x, pixels[i].y);
        }
        fprintf(stderr, "\n");
#endif  // JXL_DEBUG_DOT_DETECT
        Rect bounds = BoundingRectangle(pixels);
        if (bounds.xsize() < maxWindow && bounds.ysize() < maxWindow) {
          ConnectedComponent cc{bounds, std::move(pixels)};
          cc.CompStats(energy, kExtraRect);
          if (cc.score < minScore) continue;
          JXL_DEBUG(JXL_DEBUG_DOT_DETECT,
                    "cc mode: (%d,%d), max: %f, bgMean: %f bgVar: "
                    "%f bound:(%" PRIuS ",%" PRIuS ",%" PRIuS ",%" PRIuS ")\n",
                    cc.mode.x, cc.mode.y, cc.maxEnergy, cc.meanEnergy,
                    cc.varEnergy, cc.bounds.x0(), cc.bounds.y0(),
                    cc.bounds.xsize(), cc.bounds.ysize());
          ans.push_back(cc);
        }
      }
    }
  }
  return ans;
}

// TODO (sggonzalez): Adapt this function for the different color spaces or
// remove it if the color space with the best performance does not need it
void ComputeDotLosses(GaussianEllipse* ellipse, const ConnectedComponent& cc,
                      const Image3F& img, const Image3F& background) {
  PROFILER_FUNC;
  const int rectBounds = 2;
  const double kIntensityR = 0.0;   // 0.015;
  const double kSigmaR = 0.0;       // 0.01;
  const double kZeroEpsilon = 0.1;  // Tolerance to consider a value negative
  double ct = cos(ellipse->angle), st = sin(ellipse->angle);
  const std::array<double, 3> channelGains{{1.0, 1.0, 1.0}};
  int N = 0;
  ellipse->l1_loss = 0.0;
  ellipse->l2_loss = 0.0;
  ellipse->neg_pixels = 0;
  ellipse->neg_value.fill(0.0);
  double distMeanModeSq = (cc.mode.x - ellipse->x) * (cc.mode.x - ellipse->x) +
                          (cc.mode.y - ellipse->y) * (cc.mode.y - ellipse->y);
  ellipse->custom_loss = 0.0;
  for (int c = 0; c < 3; c++) {
    for (int sy = -rectBounds;
         sy < (static_cast<int>(cc.bounds.ysize()) + rectBounds); sy++) {
      int y = sy + cc.bounds.y0();
      if (y < 0 || static_cast<size_t>(y) >= img.ysize()) continue;
      const float* JXL_RESTRICT row = img.ConstPlaneRow(c, y);
      // bgrow is only used if kOptimizeBackground is false.
      // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
      const float* JXL_RESTRICT bgrow = background.ConstPlaneRow(c, y);
      for (int sx = -rectBounds;
           sx < (static_cast<int>(cc.bounds.xsize()) + rectBounds); sx++) {
        int x = sx + cc.bounds.x0();
        if (x < 0 || static_cast<size_t>(x) >= img.xsize()) continue;
        double target = row[x];
        double dotDelta = DotGaussianModel(
            x - ellipse->x, y - ellipse->y, ct, st, ellipse->sigma_x,
            ellipse->sigma_y, ellipse->intensity[c]);
        if (dotDelta > target + kZeroEpsilon) {
          ellipse->neg_pixels++;
          ellipse->neg_value[c] += dotDelta - target;
        }
        double bkg = kOptimizeBackground ? ellipse->bgColor[c] : bgrow[x];
        double pred = bkg + dotDelta;
        double diff = target - pred;
        double l2 = channelGains[c] * diff * diff;
        double l1 = channelGains[c] * std::fabs(diff);
        ellipse->l2_loss += l2;
        ellipse->l1_loss += l1;
        double w = DotGaussianModel(x - cc.mode.x, y - cc.mode.y, 1.0, 0.0,
                                    1.0 + ellipse->sigma_x,
                                    1.0 + ellipse->sigma_y, 1.0);
        ellipse->custom_loss += w * l2;
        N++;
      }
    }
  }
  ellipse->l2_loss /= N;
  ellipse->custom_loss /= N;
  ellipse->custom_loss += 20.0 * distMeanModeSq + ellipse->neg_value[1];
  ellipse->l1_loss /= N;
  double ridgeTerm = kSigmaR * ellipse->sigma_x + kSigmaR * ellipse->sigma_y;
  for (int c = 0; c < 3; c++) {
    ridgeTerm += kIntensityR * ellipse->intensity[c] * ellipse->intensity[c];
  }
  ellipse->ridge_loss = ellipse->l2_loss + ridgeTerm;
}

GaussianEllipse FitGaussianFast(const ConnectedComponent& cc,
                                const ImageF& energy, const Image3F& img,
                                const Image3F& background) {
  PROFILER_FUNC;
  constexpr bool leastSqIntensity = true;
  constexpr double kEpsilon = 1e-6;
  GaussianEllipse ans;
  constexpr int kRectBounds = (kEllipseWindowSize >> 1);

  // Compute the 1st and 2nd moments of the CC
  double sum = 0.0;
  int N = 0;
  std::array<double, 3> m1{{0.0, 0.0, 0.0}};
  std::array<double, 3> m2{{0.0, 0.0, 0.0}};
  std::array<double, 3> color{{0.0, 0.0, 0.0}};
  std::array<double, 3> bgColor{{0.0, 0.0, 0.0}};

  JXL_DEBUG(JXL_DEBUG_DOT_DETECT,
            "%" PRIuS " %" PRIuS " %" PRIuS " %" PRIuS "\n", cc.bounds.x0(),
            cc.bounds.y0(), cc.bounds.xsize(), cc.bounds.ysize());
  for (int c = 0; c < 3; c++) {
    color[c] = img.ConstPlaneRow(c, cc.mode.y)[cc.mode.x] -
               background.ConstPlaneRow(c, cc.mode.y)[cc.mode.x];
  }
  double sign = (color[1] > 0) ? 1 : -1;
  for (int sy = -kRectBounds; sy <= kRectBounds; sy++) {
    int y = sy + cc.mode.y;
    if (y < 0 || static_cast<size_t>(y) >= energy.ysize()) continue;
    const float* JXL_RESTRICT row = img.ConstPlaneRow(1, y);
    const float* JXL_RESTRICT bgrow = background.ConstPlaneRow(1, y);
    for (int sx = -kRectBounds; sx <= kRectBounds; sx++) {
      int x = sx + cc.mode.x;
      if (x < 0 || static_cast<size_t>(x) >= energy.xsize()) continue;
      double w = std::max(kEpsilon, sign * (row[x] - bgrow[x]));
      sum += w;

      m1[0] += w * x;
      m1[1] += w * y;
      m2[0] += w * x * x;
      m2[1] += w * x * y;
      m2[2] += w * y * y;
      for (int c = 0; c < 3; c++) {
        bgColor[c] += background.ConstPlaneRow(c, y)[x];
      }
      N++;
    }
  }
  JXL_CHECK(N > 0);

  for (int i = 0; i < 3; i++) {
    m1[i] /= sum;
    m2[i] /= sum;
    bgColor[i] /= N;
  }

  // Some magic constants
  constexpr double kSigmaMult = 1.0;
  constexpr std::array<double, 3> kScaleMult{{1.1, 1.1, 1.1}};

  // Now set the parameters of the Gaussian
  ans.x = m1[0];
  ans.y = m1[1];
  for (int j = 0; j < 3; j++) {
    ans.intensity[j] = kScaleMult[j] * color[j];
  }

  ImageD Sigma(2, 2), D(1, 2), U(2, 2);
  Sigma.Row(0)[0] = m2[0] - m1[0] * m1[0];
  Sigma.Row(1)[1] = m2[2] - m1[1] * m1[1];
  Sigma.Row(0)[1] = Sigma.Row(1)[0] = m2[1] - m1[0] * m1[1];
  ConvertToDiagonal(Sigma, &D, &U);
  const double* JXL_RESTRICT d = D.ConstRow(0);
  const double* JXL_RESTRICT u = U.ConstRow(1);
  int p1 = 0, p2 = 1;
  if (d[0] < d[1]) std::swap(p1, p2);
  ans.sigma_x = kSigmaMult * d[p1];
  ans.sigma_y = kSigmaMult * d[p2];
  ans.angle = std::atan2(u[p1], u[p2]);
  ans.l2_loss = 0.0;
  ans.bgColor = bgColor;
  if (leastSqIntensity) {
    GaussianEllipse* ellipse = &ans;
    double ct = cos(ans.angle), st = sin(ans.angle);
    // Estimate intensity with least squares (fixed background)
    for (int c = 0; c < 3; c++) {
      double gg = 0.0;
      double gd = 0.0;
      int yc = static_cast<int>(cc.mode.y);
      int xc = static_cast<int>(cc.mode.x);
      for (int y = yc - kRectBounds; y <= yc + kRectBounds; y++) {
        if (y < 0 || static_cast<size_t>(y) >= img.ysize()) continue;
        const float* JXL_RESTRICT row = img.ConstPlaneRow(c, y);
        const float* JXL_RESTRICT bgrow = background.ConstPlaneRow(c, y);
        for (int x = xc - kRectBounds; x <= xc + kRectBounds; x++) {
          if (x < 0 || static_cast<size_t>(x) >= img.xsize()) continue;
          double target = row[x] - bgrow[x];
          double gaussian =
              DotGaussianModel(x - ellipse->x, y - ellipse->y, ct, st,
                               ellipse->sigma_x, ellipse->sigma_y, 1.0);
          gg += gaussian * gaussian;
          gd += gaussian * target;
        }
      }
      ans.intensity[c] = gd / (gg + 1e-6);  // Regularized least squares
    }
  }
  ComputeDotLosses(&ans, cc, img, background);
  return ans;
}

GaussianEllipse FitGaussian(const ConnectedComponent& cc, const ImageF& energy,
                            const Image3F& img, const Image3F& background) {
  auto ellipse = FitGaussianFast(cc, energy, img, background);
  if (ellipse.sigma_x < ellipse.sigma_y) {
    std::swap(ellipse.sigma_x, ellipse.sigma_y);
    ellipse.angle += kPi / 2.0;
  }
  ellipse.angle -= kPi * std::floor(ellipse.angle / kPi);
  if (fabs(ellipse.angle - kPi) < 1e-6 || fabs(ellipse.angle) < 1e-6) {
    ellipse.angle = 0.0;
  }
  JXL_CHECK(ellipse.angle >= 0 && ellipse.angle <= kPi &&
            ellipse.sigma_x >= ellipse.sigma_y);
  JXL_DEBUG(JXL_DEBUG_DOT_DETECT,
            "Ellipse mu=(%lf,%lf) sigma=(%lf,%lf) angle=%lf "
            "intensity=(%lf,%lf,%lf) bg=(%lf,%lf,%lf) l2_loss=%lf "
            "custom_loss=%lf, neg_pix=%" PRIuS ", neg_v=(%lf,%lf,%lf)\n",
            ellipse.x, ellipse.y, ellipse.sigma_x, ellipse.sigma_y,
            ellipse.angle, ellipse.intensity[0], ellipse.intensity[1],
            ellipse.intensity[2], ellipse.bgColor[0], ellipse.bgColor[1],
            ellipse.bgColor[2], ellipse.l2_loss, ellipse.custom_loss,
            ellipse.neg_pixels, ellipse.neg_value[0], ellipse.neg_value[1],
            ellipse.neg_value[2]);
  return ellipse;
}

}  // namespace

std::vector<PatchInfo> DetectGaussianEllipses(
    const Image3F& opsin, const GaussianDetectParams& params,
    const EllipseQuantParams& qParams, ThreadPool* pool) {
  PROFILER_FUNC;
  std::vector<PatchInfo> dots;
  Image3F smooth(opsin.xsize(), opsin.ysize());
  ImageF energy = ComputeEnergyImage(opsin, &smooth, pool);
#if JXL_DEBUG_DOT_DETECT
  AuxOut aux;
  aux.debug_prefix = "/tmp/sebastian/";
  aux.DumpXybImage("smooth", smooth);
  aux.DumpPlaneNormalized("energy", energy);
#endif  // JXL_DEBUG_DOT_DETECT
  std::vector<ConnectedComponent> components = FindCC(
      energy, params.t_low, params.t_high, params.maxWinSize, params.minScore);
  size_t numCC =
      std::min(params.maxCC, (components.size() * params.percCC) / 100);
  if (components.size() > numCC) {
    std::sort(
        components.begin(), components.end(),
        [](const ConnectedComponent& a, const ConnectedComponent& b) -> bool {
          return a.score > b.score;
        });
    components.erase(components.begin() + numCC, components.end());
  }
  for (const auto& cc : components) {
    GaussianEllipse ellipse = FitGaussian(cc, energy, opsin, smooth);
    if (ellipse.x < 0.0 ||
        std::ceil(ellipse.x) >= static_cast<double>(opsin.xsize()) ||
        ellipse.y < 0.0 ||
        std::ceil(ellipse.y) >= static_cast<double>(opsin.ysize())) {
      continue;
    }
    if (ellipse.neg_pixels > params.maxNegPixels) continue;
    double intensity = 0.21 * ellipse.intensity[0] +
                       0.72 * ellipse.intensity[1] +
                       0.07 * ellipse.intensity[2];
    double intensitySq = intensity * intensity;
    // for (int c = 0; c < 3; c++) {
    //  intensitySq += ellipse.intensity[c] * ellipse.intensity[c];
    //}
    double sqDistMeanMode = (ellipse.x - cc.mode.x) * (ellipse.x - cc.mode.x) +
                            (ellipse.y - cc.mode.y) * (ellipse.y - cc.mode.y);
    if (ellipse.l2_loss < params.maxL2Loss &&
        ellipse.custom_loss < params.maxCustomLoss &&
        intensitySq > (params.minIntensity * params.minIntensity) &&
        sqDistMeanMode < params.maxDistMeanMode * params.maxDistMeanMode) {
      size_t x0 = cc.bounds.x0();
      size_t y0 = cc.bounds.y0();
      dots.emplace_back();
      dots.back().second.emplace_back(x0, y0);
      QuantizedPatch& patch = dots.back().first;
      patch.xsize = cc.bounds.xsize();
      patch.ysize = cc.bounds.ysize();
      for (size_t y = 0; y < patch.ysize; y++) {
        for (size_t x = 0; x < patch.xsize; x++) {
          for (size_t c = 0; c < 3; c++) {
            patch.fpixels[c][y * patch.xsize + x] =
                opsin.ConstPlaneRow(c, y0 + y)[x0 + x] -
                smooth.ConstPlaneRow(c, y0 + y)[x0 + x];
          }
        }
      }
    }
  }
#if JXL_DEBUG_DOT_DETECT
  JXL_DEBUG(JXL_DEBUG_DOT_DETECT, "Candidates: %" PRIuS ", Dots: %" PRIuS "\n",
            components.size(), dots.size());
  ApplyGaussianEllipses(&smooth, dots, 1.0);
  aux.DumpXybImage("draw", smooth);
  ApplyGaussianEllipses(&smooth, dots, -1.0);

  auto qdots = QuantizeGaussianEllipses(dots, qParams);
  auto deq = DequantizeGaussianEllipses(qdots, qParams);
  ApplyGaussianEllipses(&smooth, deq, 1.0);
  aux.DumpXybImage("qdraw", smooth);
  ApplyGaussianEllipses(&smooth, deq, -1.0);
#endif  // JXL_DEBUG_DOT_DETECT
  return dots;
}

}  // namespace jxl
#endif  // HWY_ONCE
