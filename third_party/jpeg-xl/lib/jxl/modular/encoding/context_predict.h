// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_MODULAR_ENCODING_CONTEXT_PREDICT_H_
#define LIB_JXL_MODULAR_ENCODING_CONTEXT_PREDICT_H_

#include <utility>
#include <vector>

#include "lib/jxl/fields.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/options.h"

namespace jxl {

namespace weighted {
constexpr static size_t kNumPredictors = 4;
constexpr static int64_t kPredExtraBits = 3;
constexpr static int64_t kPredictionRound = ((1 << kPredExtraBits) >> 1) - 1;
constexpr static size_t kNumProperties = 1;

struct Header : public Fields {
  const char *Name() const override { return "WeightedPredictorHeader"; }
  // TODO(janwas): move to cc file, avoid including fields.h.
  Header() { Bundle::Init(this); }

  Status VisitFields(Visitor *JXL_RESTRICT visitor) override {
    if (visitor->AllDefault(*this, &all_default)) {
      // Overwrite all serialized fields, but not any nonserialized_*.
      visitor->SetDefault(this);
      return true;
    }
    auto visit_p = [visitor](pixel_type val, pixel_type *p) {
      uint32_t up = *p;
      JXL_QUIET_RETURN_IF_ERROR(visitor->Bits(5, val, &up));
      *p = up;
      return Status(true);
    };
    JXL_QUIET_RETURN_IF_ERROR(visit_p(16, &p1C));
    JXL_QUIET_RETURN_IF_ERROR(visit_p(10, &p2C));
    JXL_QUIET_RETURN_IF_ERROR(visit_p(7, &p3Ca));
    JXL_QUIET_RETURN_IF_ERROR(visit_p(7, &p3Cb));
    JXL_QUIET_RETURN_IF_ERROR(visit_p(7, &p3Cc));
    JXL_QUIET_RETURN_IF_ERROR(visit_p(0, &p3Cd));
    JXL_QUIET_RETURN_IF_ERROR(visit_p(0, &p3Ce));
    JXL_QUIET_RETURN_IF_ERROR(visitor->Bits(4, 0xd, &w[0]));
    JXL_QUIET_RETURN_IF_ERROR(visitor->Bits(4, 0xc, &w[1]));
    JXL_QUIET_RETURN_IF_ERROR(visitor->Bits(4, 0xc, &w[2]));
    JXL_QUIET_RETURN_IF_ERROR(visitor->Bits(4, 0xc, &w[3]));
    return true;
  }

  bool all_default;
  pixel_type p1C = 0, p2C = 0, p3Ca = 0, p3Cb = 0, p3Cc = 0, p3Cd = 0, p3Ce = 0;
  uint32_t w[kNumPredictors] = {};
};

struct State {
  pixel_type_w prediction[kNumPredictors] = {};
  pixel_type_w pred = 0;  // *before* removing the added bits.
  std::vector<uint32_t> pred_errors[kNumPredictors];
  std::vector<int32_t> error;
  Header header;

  // Allows to approximate division by a number from 1 to 64.
  uint32_t divlookup[64];

  constexpr static pixel_type_w AddBits(pixel_type_w x) {
    return uint64_t(x) << kPredExtraBits;
  }

  State(Header header, size_t xsize, size_t ysize) : header(header) {
    // Extra margin to avoid out-of-bounds writes.
    // All have space for two rows of data.
    for (size_t i = 0; i < 4; i++) {
      pred_errors[i].resize((xsize + 2) * 2);
    }
    error.resize((xsize + 2) * 2);
    // Initialize division lookup table.
    for (int i = 0; i < 64; i++) {
      divlookup[i] = (1 << 24) / (i + 1);
    }
  }

  // Approximates 4+(maxweight<<24)/(x+1), avoiding division
  JXL_INLINE uint32_t ErrorWeight(uint64_t x, uint32_t maxweight) const {
    int shift = FloorLog2Nonzero(x + 1) - 5;
    if (shift < 0) shift = 0;
    return 4 + ((maxweight * divlookup[x >> shift]) >> shift);
  }

  // Approximates the weighted average of the input values with the given
  // weights, avoiding division. Weights must sum to at least 16.
  JXL_INLINE pixel_type_w
  WeightedAverage(const pixel_type_w *JXL_RESTRICT p,
                  std::array<uint32_t, kNumPredictors> w) const {
    uint32_t weight_sum = 0;
    for (size_t i = 0; i < kNumPredictors; i++) {
      weight_sum += w[i];
    }
    JXL_DASSERT(weight_sum > 15);
    uint32_t log_weight = FloorLog2Nonzero(weight_sum);  // at least 4.
    weight_sum = 0;
    for (size_t i = 0; i < kNumPredictors; i++) {
      w[i] >>= log_weight - 4;
      weight_sum += w[i];
    }
    // for rounding.
    pixel_type_w sum = (weight_sum >> 1) - 1;
    for (size_t i = 0; i < kNumPredictors; i++) {
      sum += p[i] * w[i];
    }
    return (sum * divlookup[weight_sum - 1]) >> 24;
  }

  template <bool compute_properties>
  JXL_INLINE pixel_type_w Predict(size_t x, size_t y, size_t xsize,
                                  pixel_type_w N, pixel_type_w W,
                                  pixel_type_w NE, pixel_type_w NW,
                                  pixel_type_w NN, Properties *properties,
                                  size_t offset) {
    size_t cur_row = y & 1 ? 0 : (xsize + 2);
    size_t prev_row = y & 1 ? (xsize + 2) : 0;
    size_t pos_N = prev_row + x;
    size_t pos_NE = x < xsize - 1 ? pos_N + 1 : pos_N;
    size_t pos_NW = x > 0 ? pos_N - 1 : pos_N;
    std::array<uint32_t, kNumPredictors> weights;
    for (size_t i = 0; i < kNumPredictors; i++) {
      // pred_errors[pos_N] also contains the error of pixel W.
      // pred_errors[pos_NW] also contains the error of pixel WW.
      weights[i] = pred_errors[i][pos_N] + pred_errors[i][pos_NE] +
                   pred_errors[i][pos_NW];
      weights[i] = ErrorWeight(weights[i], header.w[i]);
    }

    N = AddBits(N);
    W = AddBits(W);
    NE = AddBits(NE);
    NW = AddBits(NW);
    NN = AddBits(NN);

    pixel_type_w teW = x == 0 ? 0 : error[cur_row + x - 1];
    pixel_type_w teN = error[pos_N];
    pixel_type_w teNW = error[pos_NW];
    pixel_type_w sumWN = teN + teW;
    pixel_type_w teNE = error[pos_NE];

    if (compute_properties) {
      pixel_type_w p = teW;
      if (std::abs(teN) > std::abs(p)) p = teN;
      if (std::abs(teNW) > std::abs(p)) p = teNW;
      if (std::abs(teNE) > std::abs(p)) p = teNE;
      (*properties)[offset++] = p;
    }

    prediction[0] = W + NE - N;
    prediction[1] = N - (((sumWN + teNE) * header.p1C) >> 5);
    prediction[2] = W - (((sumWN + teNW) * header.p2C) >> 5);
    prediction[3] =
        N - ((teNW * header.p3Ca + teN * header.p3Cb + teNE * header.p3Cc +
              (NN - N) * header.p3Cd + (NW - W) * header.p3Ce) >>
             5);

    pred = WeightedAverage(prediction, weights);

    // If all three have the same sign, skip clamping.
    if (((teN ^ teW) | (teN ^ teNW)) > 0) {
      return (pred + kPredictionRound) >> kPredExtraBits;
    }

    // Otherwise, clamp to min/max of neighbouring pixels (just W, NE, N).
    pixel_type_w mx = std::max(W, std::max(NE, N));
    pixel_type_w mn = std::min(W, std::min(NE, N));
    pred = std::max(mn, std::min(mx, pred));
    return (pred + kPredictionRound) >> kPredExtraBits;
  }

  JXL_INLINE void UpdateErrors(pixel_type_w val, size_t x, size_t y,
                               size_t xsize) {
    size_t cur_row = y & 1 ? 0 : (xsize + 2);
    size_t prev_row = y & 1 ? (xsize + 2) : 0;
    val = AddBits(val);
    error[cur_row + x] = ClampToRange<pixel_type>(pred - val);
    for (size_t i = 0; i < kNumPredictors; i++) {
      pixel_type_w err =
          (std::abs(prediction[i] - val) + kPredictionRound) >> kPredExtraBits;
      // For predicting in the next row.
      pred_errors[i][cur_row + x] = err;
      // Add the error on this pixel to the error on the NE pixel. This has the
      // effect of adding the error on this pixel to the E and EE pixels.
      pred_errors[i][prev_row + x + 1] += err;
    }
  }
};

// Encoder helper function to set the parameters to some presets.
inline void PredictorMode(int i, Header *header) {
  switch (i) {
    case 0:
      // ~ lossless16 predictor
      header->w[0] = 0xd;
      header->w[1] = 0xc;
      header->w[2] = 0xc;
      header->w[3] = 0xc;
      header->p1C = 16;
      header->p2C = 10;
      header->p3Ca = 7;
      header->p3Cb = 7;
      header->p3Cc = 7;
      header->p3Cd = 0;
      header->p3Ce = 0;
      break;
    case 1:
      // ~ default lossless8 predictor
      header->w[0] = 0xd;
      header->w[1] = 0xc;
      header->w[2] = 0xc;
      header->w[3] = 0xb;
      header->p1C = 8;
      header->p2C = 8;
      header->p3Ca = 4;
      header->p3Cb = 0;
      header->p3Cc = 3;
      header->p3Cd = 23;
      header->p3Ce = 2;
      break;
    case 2:
      // ~ west lossless8 predictor
      header->w[0] = 0xd;
      header->w[1] = 0xc;
      header->w[2] = 0xd;
      header->w[3] = 0xc;
      header->p1C = 10;
      header->p2C = 9;
      header->p3Ca = 7;
      header->p3Cb = 0;
      header->p3Cc = 0;
      header->p3Cd = 16;
      header->p3Ce = 9;
      break;
    case 3:
      // ~ north lossless8 predictor
      header->w[0] = 0xd;
      header->w[1] = 0xd;
      header->w[2] = 0xc;
      header->w[3] = 0xc;
      header->p1C = 16;
      header->p2C = 8;
      header->p3Ca = 0;
      header->p3Cb = 16;
      header->p3Cc = 0;
      header->p3Cd = 23;
      header->p3Ce = 0;
      break;
    case 4:
    default:
      // something else, because why not
      header->w[0] = 0xd;
      header->w[1] = 0xc;
      header->w[2] = 0xc;
      header->w[3] = 0xc;
      header->p1C = 10;
      header->p2C = 10;
      header->p3Ca = 5;
      header->p3Cb = 5;
      header->p3Cc = 5;
      header->p3Cd = 12;
      header->p3Ce = 4;
      break;
  }
}
}  // namespace weighted

// Stores a node and its two children at the same time. This significantly
// reduces the number of branches needed during decoding.
struct FlatDecisionNode {
  // Property + splitval of the top node.
  int32_t property0;  // -1 if leaf.
  union {
    PropertyVal splitval0;
    Predictor predictor;
  };
  uint32_t childID;  // childID is ctx id if leaf.
  // Property+splitval of the two child nodes.
  union {
    PropertyVal splitvals[2];
    int32_t multiplier;
  };
  union {
    int32_t properties[2];
    int64_t predictor_offset;
  };
};
using FlatTree = std::vector<FlatDecisionNode>;

class MATreeLookup {
 public:
  explicit MATreeLookup(const FlatTree &tree) : nodes_(tree) {}
  struct LookupResult {
    uint32_t context;
    Predictor predictor;
    int64_t offset;
    int32_t multiplier;
  };
  LookupResult Lookup(const Properties &properties) const {
    uint32_t pos = 0;
    while (true) {
      const FlatDecisionNode &node = nodes_[pos];
      if (node.property0 < 0) {
        return {node.childID, node.predictor, node.predictor_offset,
                node.multiplier};
      }
      bool p0 = properties[node.property0] <= node.splitval0;
      uint32_t off0 = properties[node.properties[0]] <= node.splitvals[0];
      uint32_t off1 = 2 | (properties[node.properties[1]] <= node.splitvals[1]);
      pos = node.childID + (p0 ? off1 : off0);
    }
  }

 private:
  const FlatTree &nodes_;
};

static constexpr size_t kExtraPropsPerChannel = 4;
static constexpr size_t kNumNonrefProperties =
    kNumStaticProperties + 13 + weighted::kNumProperties;

constexpr size_t kWPProp = kNumNonrefProperties - weighted::kNumProperties;
constexpr size_t kGradientProp = 9;

// Clamps gradient to the min/max of n, w (and l, implicitly).
static JXL_INLINE int32_t ClampedGradient(const int32_t n, const int32_t w,
                                          const int32_t l) {
  const int32_t m = std::min(n, w);
  const int32_t M = std::max(n, w);
  // The end result of this operation doesn't overflow or underflow if the
  // result is between m and M, but the intermediate value may overflow, so we
  // do the intermediate operations in uint32_t and check later if we had an
  // overflow or underflow condition comparing m, M and l directly.
  // grad = M + m - l = n + w - l
  const int32_t grad =
      static_cast<int32_t>(static_cast<uint32_t>(n) + static_cast<uint32_t>(w) -
                           static_cast<uint32_t>(l));
  // We use two sets of ternary operators to force the evaluation of them in
  // any case, allowing the compiler to avoid branches and use cmovl/cmovg in
  // x86.
  const int32_t grad_clamp_M = (l < m) ? M : grad;
  return (l > M) ? m : grad_clamp_M;
}

inline pixel_type_w Select(pixel_type_w a, pixel_type_w b, pixel_type_w c) {
  pixel_type_w p = a + b - c;
  pixel_type_w pa = std::abs(p - a);
  pixel_type_w pb = std::abs(p - b);
  return pa < pb ? a : b;
}

inline void PrecomputeReferences(const Channel &ch, size_t y,
                                 const Image &image, uint32_t i,
                                 Channel *references) {
  ZeroFillImage(&references->plane);
  uint32_t offset = 0;
  size_t num_extra_props = references->w;
  intptr_t onerow = references->plane.PixelsPerRow();
  for (int32_t j = static_cast<int32_t>(i) - 1;
       j >= 0 && offset < num_extra_props; j--) {
    if (image.channel[j].w != image.channel[i].w ||
        image.channel[j].h != image.channel[i].h) {
      continue;
    }
    if (image.channel[j].hshift != image.channel[i].hshift) continue;
    if (image.channel[j].vshift != image.channel[i].vshift) continue;
    pixel_type *JXL_RESTRICT rp = references->Row(0) + offset;
    const pixel_type *JXL_RESTRICT rpp = image.channel[j].Row(y);
    const pixel_type *JXL_RESTRICT rpprev = image.channel[j].Row(y ? y - 1 : 0);
    for (size_t x = 0; x < ch.w; x++, rp += onerow) {
      pixel_type_w v = rpp[x];
      rp[0] = std::abs(v);
      rp[1] = v;
      pixel_type_w vleft = (x ? rpp[x - 1] : 0);
      pixel_type_w vtop = (y ? rpprev[x] : vleft);
      pixel_type_w vtopleft = (x && y ? rpprev[x - 1] : vleft);
      pixel_type_w vpredicted = ClampedGradient(vleft, vtop, vtopleft);
      rp[2] = std::abs(v - vpredicted);
      rp[3] = v - vpredicted;
    }

    offset += kExtraPropsPerChannel;
  }
}

struct PredictionResult {
  int context = 0;
  pixel_type_w guess = 0;
  Predictor predictor;
  int32_t multiplier;
};

inline std::string PropertyName(size_t i) {
  static_assert(kNumNonrefProperties == 16, "Update this function");
  switch (i) {
    case 0:
      return "c";
    case 1:
      return "g";
    case 2:
      return "y";
    case 3:
      return "x";
    case 4:
      return "|N|";
    case 5:
      return "|W|";
    case 6:
      return "N";
    case 7:
      return "W";
    case 8:
      return "W-WW-NW+NWW";
    case 9:
      return "W+N-NW";
    case 10:
      return "W-NW";
    case 11:
      return "NW-N";
    case 12:
      return "N-NE";
    case 13:
      return "N-NN";
    case 14:
      return "W-WW";
    case 15:
      return "WGH";
    default:
      return "ch[" + ToString(15 - (int)i) + "]";
  }
}

inline void InitPropsRow(
    Properties *p,
    const std::array<pixel_type, kNumStaticProperties> &static_props,
    const int y) {
  for (size_t i = 0; i < kNumStaticProperties; i++) {
    (*p)[i] = static_props[i];
  }
  (*p)[2] = y;
  (*p)[9] = 0;  // local gradient.
}

namespace detail {
enum PredictorMode {
  kUseTree = 1,
  kUseWP = 2,
  kForceComputeProperties = 4,
  kAllPredictions = 8,
};

template <int mode>
inline PredictionResult Predict(
    Properties *p, size_t w, const pixel_type *JXL_RESTRICT pp,
    const intptr_t onerow, const size_t x, const size_t y, Predictor predictor,
    const MATreeLookup *lookup, const Channel *references,
    weighted::State *wp_state, pixel_type_w *predictions) {
  // We start in position 3 because of 2 static properties + y.
  size_t offset = 3;
  constexpr bool compute_properties =
      mode & kUseTree || mode & kForceComputeProperties;
  pixel_type_w left = (x ? pp[-1] : (y ? pp[-onerow] : 0));
  pixel_type_w top = (y ? pp[-onerow] : left);
  pixel_type_w topleft = (x && y ? pp[-1 - onerow] : left);
  pixel_type_w topright = (x + 1 < w && y ? pp[1 - onerow] : top);
  pixel_type_w leftleft = (x > 1 ? pp[-2] : left);
  pixel_type_w toptop = (y > 1 ? pp[-onerow - onerow] : top);
  pixel_type_w toprightright = (x + 2 < w && y ? pp[2 - onerow] : topright);

  if (compute_properties) {
    // location
    (*p)[offset++] = x;
    // neighbors
    (*p)[offset++] = std::abs(top);
    (*p)[offset++] = std::abs(left);
    (*p)[offset++] = top;
    (*p)[offset++] = left;

    // local gradient
    (*p)[offset] = left - (*p)[offset + 1];
    offset++;
    // local gradient
    (*p)[offset++] = left + top - topleft;

    // FFV1 context properties
    (*p)[offset++] = left - topleft;
    (*p)[offset++] = topleft - top;
    (*p)[offset++] = top - topright;
    (*p)[offset++] = top - toptop;
    (*p)[offset++] = left - leftleft;
  }

  pixel_type_w wp_pred = 0;
  if (mode & kUseWP) {
    wp_pred = wp_state->Predict<compute_properties>(
        x, y, w, top, left, topright, topleft, toptop, p, offset);
  }
  if (compute_properties) {
    offset += weighted::kNumProperties;
    // Extra properties.
    const pixel_type *JXL_RESTRICT rp = references->Row(x);
    for (size_t i = 0; i < references->w; i++) {
      (*p)[offset++] = rp[i];
    }
  }
  PredictionResult result;
  if (mode & kUseTree) {
    MATreeLookup::LookupResult lr = lookup->Lookup(*p);
    result.context = lr.context;
    result.guess = lr.offset;
    result.multiplier = lr.multiplier;
    predictor = lr.predictor;
  }
  pixel_type_w pred_storage[kNumModularPredictors];
  if (!(mode & kAllPredictions)) {
    predictions = pred_storage;
  }
  predictions[(int)Predictor::Zero] = 0;
  predictions[(int)Predictor::Left] = left;
  predictions[(int)Predictor::Top] = top;
  predictions[(int)Predictor::Select] = Select(left, top, topleft);
  predictions[(int)Predictor::Weighted] = wp_pred;
  predictions[(int)Predictor::Gradient] = ClampedGradient(left, top, topleft);
  predictions[(int)Predictor::TopLeft] = topleft;
  predictions[(int)Predictor::TopRight] = topright;
  predictions[(int)Predictor::LeftLeft] = leftleft;
  predictions[(int)Predictor::Average0] = (left + top) / 2;
  predictions[(int)Predictor::Average1] = (left + topleft) / 2;
  predictions[(int)Predictor::Average2] = (topleft + top) / 2;
  predictions[(int)Predictor::Average3] = (top + topright) / 2;
  predictions[(int)Predictor::Average4] =
      (6 * top - 2 * toptop + 7 * left + 1 * leftleft + 1 * toprightright +
       3 * topright + 8) /
      16;
  result.guess += predictions[(int)predictor];
  result.predictor = predictor;

  return result;
}
}  // namespace detail

inline PredictionResult PredictNoTreeNoWP(size_t w,
                                          const pixel_type *JXL_RESTRICT pp,
                                          const intptr_t onerow, const int x,
                                          const int y, Predictor predictor) {
  return detail::Predict</*mode=*/0>(
      /*p=*/nullptr, w, pp, onerow, x, y, predictor, /*lookup=*/nullptr,
      /*references=*/nullptr, /*wp_state=*/nullptr, /*predictions=*/nullptr);
}

inline PredictionResult PredictNoTreeWP(size_t w,
                                        const pixel_type *JXL_RESTRICT pp,
                                        const intptr_t onerow, const int x,
                                        const int y, Predictor predictor,
                                        weighted::State *wp_state) {
  return detail::Predict<detail::kUseWP>(
      /*p=*/nullptr, w, pp, onerow, x, y, predictor, /*lookup=*/nullptr,
      /*references=*/nullptr, wp_state, /*predictions=*/nullptr);
}

inline PredictionResult PredictTreeNoWP(Properties *p, size_t w,
                                        const pixel_type *JXL_RESTRICT pp,
                                        const intptr_t onerow, const int x,
                                        const int y,
                                        const MATreeLookup &tree_lookup,
                                        const Channel &references) {
  return detail::Predict<detail::kUseTree>(
      p, w, pp, onerow, x, y, Predictor::Zero, &tree_lookup, &references,
      /*wp_state=*/nullptr, /*predictions=*/nullptr);
}

inline PredictionResult PredictTreeWP(Properties *p, size_t w,
                                      const pixel_type *JXL_RESTRICT pp,
                                      const intptr_t onerow, const int x,
                                      const int y,
                                      const MATreeLookup &tree_lookup,
                                      const Channel &references,
                                      weighted::State *wp_state) {
  return detail::Predict<detail::kUseTree | detail::kUseWP>(
      p, w, pp, onerow, x, y, Predictor::Zero, &tree_lookup, &references,
      wp_state, /*predictions=*/nullptr);
}

inline PredictionResult PredictLearn(Properties *p, size_t w,
                                     const pixel_type *JXL_RESTRICT pp,
                                     const intptr_t onerow, const int x,
                                     const int y, Predictor predictor,
                                     const Channel &references,
                                     weighted::State *wp_state) {
  return detail::Predict<detail::kForceComputeProperties | detail::kUseWP>(
      p, w, pp, onerow, x, y, predictor, /*lookup=*/nullptr, &references,
      wp_state, /*predictions=*/nullptr);
}

inline void PredictLearnAll(Properties *p, size_t w,
                            const pixel_type *JXL_RESTRICT pp,
                            const intptr_t onerow, const int x, const int y,
                            const Channel &references,
                            weighted::State *wp_state,
                            pixel_type_w *predictions) {
  detail::Predict<detail::kForceComputeProperties | detail::kUseWP |
                  detail::kAllPredictions>(
      p, w, pp, onerow, x, y, Predictor::Zero,
      /*lookup=*/nullptr, &references, wp_state, predictions);
}

inline void PredictAllNoWP(size_t w, const pixel_type *JXL_RESTRICT pp,
                           const intptr_t onerow, const int x, const int y,
                           pixel_type_w *predictions) {
  detail::Predict<detail::kAllPredictions>(
      /*p=*/nullptr, w, pp, onerow, x, y, Predictor::Zero,
      /*lookup=*/nullptr,
      /*references=*/nullptr, /*wp_state=*/nullptr, predictions);
}
}  // namespace jxl

#endif  // LIB_JXL_MODULAR_ENCODING_CONTEXT_PREDICT_H_
