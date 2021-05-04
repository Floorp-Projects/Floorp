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

#include "lib/jxl/fields.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>

#include "hwy/base.h"
#include "lib/jxl/base/bits.h"

namespace jxl {

namespace {

// A bundle can be in one of three states concerning extensions: not-begun,
// active, ended. Bundles may be nested, so we need a stack of states.
class ExtensionStates {
 public:
  void Push() {
    // Initial state = not-begun.
    begun_ <<= 1;
    ended_ <<= 1;
  }

  // Clears current state; caller must check IsEnded beforehand.
  void Pop() {
    begun_ >>= 1;
    ended_ >>= 1;
  }

  // Returns true if state == active || state == ended.
  Status IsBegun() const { return (begun_ & 1) != 0; }
  // Returns true if state != not-begun && state != active.
  Status IsEnded() const { return (ended_ & 1) != 0; }

  void Begin() {
    JXL_ASSERT(!IsBegun());
    JXL_ASSERT(!IsEnded());
    begun_ += 1;
  }

  void End() {
    JXL_ASSERT(IsBegun());
    JXL_ASSERT(!IsEnded());
    ended_ += 1;
  }

 private:
  // Current state := least-significant bit of begun_ and ended_.
  uint64_t begun_ = 0;
  uint64_t ended_ = 0;
};

// Visitors generate Init/AllDefault/Read/Write logic for all fields. Each
// bundle's VisitFields member function calls visitor->U32 etc. We do not
// overload operator() because a function name is easier to search for.

class VisitorBase : public Visitor {
 public:
  explicit VisitorBase(bool print_bundles = false)
      : print_bundles_(print_bundles) {}
  ~VisitorBase() override { JXL_ASSERT(depth_ == 0); }

  // This is the only call site of Fields::VisitFields. Adds tracing and
  // ensures EndExtensions was called.
  Status Visit(Fields* fields, const char* visitor_name) override {
    fputs(visitor_name, stdout);  // No newline; no effect if empty
    if (print_bundles_) {
      Trace("%s\n", print_bundles_ ? fields->Name() : "");
    }

    depth_ += 1;
    JXL_ASSERT(depth_ <= Bundle::kMaxExtensions);
    extension_states_.Push();

    const Status ok = fields->VisitFields(this);

    if (ok) {
      // If VisitFields called BeginExtensions, must also call
      // EndExtensions.
      JXL_ASSERT(!extension_states_.IsBegun() || extension_states_.IsEnded());
    } else {
      // Failed, undefined state: don't care whether EndExtensions was
      // called.
    }

    extension_states_.Pop();
    JXL_ASSERT(depth_ != 0);
    depth_ -= 1;

    return ok;
  }

  // For visitors accepting a const Visitor, need to const-cast so we can call
  // the non-const Visitor::VisitFields. NOTE: C is not modified except the
  // `all_default` field by CanEncodeVisitor.
  Status VisitConst(const Fields& t, const char* message) {
    return Visit(const_cast<Fields*>(&t), message);
  }

  // Derived types (overridden by InitVisitor because it is unsafe to read
  // from *value there)

  Status Bool(bool default_value, bool* JXL_RESTRICT value) override {
    uint32_t bits = *value ? 1 : 0;
    JXL_RETURN_IF_ERROR(Bits(1, static_cast<uint32_t>(default_value), &bits));
    JXL_DASSERT(bits <= 1);
    *value = bits == 1;
    return true;
  }

  // Overridden by ReadVisitor and WriteVisitor.
  // Called before any conditional visit based on "extensions".
  // Overridden by ReadVisitor, CanEncodeVisitor and WriteVisitor.
  Status BeginExtensions(uint64_t* JXL_RESTRICT extensions) override {
    JXL_RETURN_IF_ERROR(U64(0, extensions));

    extension_states_.Begin();
    return true;
  }

  // Called after all extension fields (if any). Although non-extension
  // fields could be visited afterward, we prefer the convention that
  // extension fields are always the last to be visited. Overridden by
  // ReadVisitor.
  Status EndExtensions() override {
    extension_states_.End();
    return true;
  }

 protected:
  // Prints indentation, <format>.
  JXL_FORMAT(2, 3)  // 1-based plus one because member function
  void Trace(const char* format, ...) const {
    // Indentation.
    printf("%*s", static_cast<int>(2 * depth_), "");

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
  }

 private:
  size_t depth_ = 0;  // for indentation.
  ExtensionStates extension_states_;
  const bool print_bundles_;
};

struct InitVisitor : public VisitorBase {
  Status Bits(const size_t /*unused*/, const uint32_t default_value,
              uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U32(const U32Enc /*unused*/, const uint32_t default_value,
             uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U64(const uint64_t default_value,
             uint64_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status Bool(bool default_value, bool* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status F16(const float default_value, float* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  // Always visit conditional fields to ensure they are initialized.
  Status Conditional(bool /*condition*/) override { return true; }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT all_default) override {
    // Just initialize this field and don't skip initializing others.
    JXL_RETURN_IF_ERROR(Bool(true, all_default));
    return false;
  }

  Status VisitNested(Fields* /*fields*/) override {
    // Avoid re-initializing nested bundles (their ctors already called
    // Bundle::Init for their fields).
    return true;
  }

  const char* VisitorName() override { return "InitVisitor"; }
};

// Similar to InitVisitor, but also initializes nested fields.
struct SetDefaultVisitor : public VisitorBase {
  Status Bits(const size_t /*unused*/, const uint32_t default_value,
              uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U32(const U32Enc /*unused*/, const uint32_t default_value,
             uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U64(const uint64_t default_value,
             uint64_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status Bool(bool default_value, bool* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status F16(const float default_value, float* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  // Always visit conditional fields to ensure they are initialized.
  Status Conditional(bool /*condition*/) override { return true; }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT all_default) override {
    // Just initialize this field and don't skip initializing others.
    JXL_RETURN_IF_ERROR(Bool(true, all_default));
    return false;
  }

  const char* VisitorName() override { return "SetDefaultVisitor"; }
};

class AllDefaultVisitor : public VisitorBase {
 public:
  explicit AllDefaultVisitor(bool print_all_default)
      : VisitorBase(print_all_default), print_all_default_(print_all_default) {}

  Status Bits(const size_t bits, const uint32_t default_value,
              uint32_t* JXL_RESTRICT value) override {
    if (print_all_default_) {
      Trace("  u(%zu) = %u, default %u\n", bits, *value, default_value);
    }

    all_default_ &= *value == default_value;
    return true;
  }

  Status U32(const U32Enc /*unused*/, const uint32_t default_value,
             uint32_t* JXL_RESTRICT value) override {
    if (print_all_default_) {
      Trace("  U32 = %u, default %u\n", *value, default_value);
    }

    all_default_ &= *value == default_value;
    return true;
  }

  Status U64(const uint64_t default_value,
             uint64_t* JXL_RESTRICT value) override {
    if (print_all_default_) {
      Trace("  U64 = %" PRIu64 ", default %" PRIu64 "\n", *value,
            default_value);
    }

    all_default_ &= *value == default_value;
    return true;
  }

  Status F16(const float default_value, float* JXL_RESTRICT value) override {
    if (print_all_default_) {
      Trace("  F16 = %.6f, default %.6f\n", static_cast<double>(*value),
            static_cast<double>(default_value));
    }
    all_default_ &= std::abs(*value - default_value) < 1E-6f;
    return true;
  }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT /*all_default*/) override {
    // Visit all fields so we can compute the actual all_default_ value.
    return false;
  }

  bool AllDefault() const { return all_default_; }

  const char* VisitorName() override { return "AllDefaultVisitor"; }

 private:
  const bool print_all_default_;
  bool all_default_ = true;
};

class ReadVisitor : public VisitorBase {
 public:
  ReadVisitor(BitReader* reader, bool print_read)
      : VisitorBase(print_read), print_read_(print_read), reader_(reader) {}

  Status Bits(const size_t bits, const uint32_t /*default_value*/,
              uint32_t* JXL_RESTRICT value) override {
    *value = BitsCoder::Read(bits, reader_);
    if (!reader_->AllReadsWithinBounds()) {
      return JXL_STATUS(StatusCode::kNotEnoughBytes,
                        "Not enough bytes for header");
    }
    if (print_read_) Trace("  u(%zu) = %u\n", bits, *value);
    return true;
  }

  Status U32(const U32Enc dist, const uint32_t /*default_value*/,
             uint32_t* JXL_RESTRICT value) override {
    *value = U32Coder::Read(dist, reader_);
    if (!reader_->AllReadsWithinBounds()) {
      return JXL_STATUS(StatusCode::kNotEnoughBytes,
                        "Not enough bytes for header");
    }
    if (print_read_) Trace("  U32 = %u\n", *value);
    return true;
  }

  Status U64(const uint64_t /*default_value*/,
             uint64_t* JXL_RESTRICT value) override {
    *value = U64Coder::Read(reader_);
    if (!reader_->AllReadsWithinBounds()) {
      return JXL_STATUS(StatusCode::kNotEnoughBytes,
                        "Not enough bytes for header");
    }
    if (print_read_) Trace("  U64 = %" PRIu64 "\n", *value);
    return true;
  }

  Status F16(const float /*default_value*/,
             float* JXL_RESTRICT value) override {
    ok_ &= F16Coder::Read(reader_, value);
    if (!reader_->AllReadsWithinBounds()) {
      return JXL_STATUS(StatusCode::kNotEnoughBytes,
                        "Not enough bytes for header");
    }
    if (print_read_) Trace("  F16 = %f\n", static_cast<double>(*value));
    return true;
  }

  void SetDefault(Fields* fields) override { Bundle::SetDefault(fields); }

  bool IsReading() const override { return true; }

  // This never fails because visitors are expected to keep reading until
  // EndExtensions, see comment there.
  Status BeginExtensions(uint64_t* JXL_RESTRICT extensions) override {
    JXL_QUIET_RETURN_IF_ERROR(VisitorBase::BeginExtensions(extensions));
    if (*extensions == 0) return true;

    // For each nonzero bit, i.e. extension that is present:
    for (uint64_t remaining_extensions = *extensions; remaining_extensions != 0;
         remaining_extensions &= remaining_extensions - 1) {
      const size_t idx_extension =
          Num0BitsBelowLS1Bit_Nonzero(remaining_extensions);
      // Read additional U64 (one per extension) indicating the number of bits
      // (allows skipping individual extensions).
      JXL_RETURN_IF_ERROR(U64(0, &extension_bits_[idx_extension]));
      if (!SafeAdd(total_extension_bits_, extension_bits_[idx_extension],
                   total_extension_bits_)) {
        return JXL_FAILURE("Extension bits overflowed, invalid codestream");
      }
    }
    // Used by EndExtensions to skip past any _remaining_ extensions.
    pos_after_ext_size_ = reader_->TotalBitsConsumed();
    JXL_ASSERT(pos_after_ext_size_ != 0);
    return true;
  }

  Status EndExtensions() override {
    JXL_QUIET_RETURN_IF_ERROR(VisitorBase::EndExtensions());
    // Happens if extensions == 0: don't read size, done.
    if (pos_after_ext_size_ == 0) return true;

    // Not enough bytes as set by BeginExtensions or earlier. Do not return
    // this as an JXL_FAILURE or false (which can also propagate to error
    // through e.g. JXL_RETURN_IF_ERROR), since this may be used while
    // silently checking whether there are enough bytes. If this case must be
    // treated as an error, reader_>Close() will do this, just like is already
    // done for non-extension fields.
    if (!enough_bytes_) return true;

    // Skip new fields this (old?) decoder didn't know about, if any.
    const size_t bits_read = reader_->TotalBitsConsumed();
    uint64_t end;
    if (!SafeAdd(pos_after_ext_size_, total_extension_bits_, end)) {
      return JXL_FAILURE("Invalid extension size, caused overflow");
    }
    if (bits_read > end) {
      return JXL_FAILURE("Read more extension bits than budgeted");
    }
    const size_t remaining_bits = end - bits_read;
    if (remaining_bits != 0) {
      JXL_WARNING("Skipping %zu-bit extension(s)", remaining_bits);
      reader_->SkipBits(remaining_bits);
      if (!reader_->AllReadsWithinBounds()) {
        return JXL_STATUS(StatusCode::kNotEnoughBytes,
                          "Not enough bytes for header");
      }
    }
    return true;
  }

  Status OK() const { return ok_; }

  const char* VisitorName() override { return "ReadVisitor"; }

 private:
  const bool print_read_;

  // Whether any error other than not enough bytes occurred.
  bool ok_ = true;

  // Whether there are enough input bytes to read from.
  bool enough_bytes_ = true;
  BitReader* const reader_;
  // May be 0 even if the corresponding extension is present.
  uint64_t extension_bits_[Bundle::kMaxExtensions] = {0};
  uint64_t total_extension_bits_ = 0;
  size_t pos_after_ext_size_ = 0;  // 0 iff extensions == 0.
};

class MaxBitsVisitor : public VisitorBase {
 public:
  Status Bits(const size_t bits, const uint32_t /*default_value*/,
              uint32_t* JXL_RESTRICT /*value*/) override {
    max_bits_ += BitsCoder::MaxEncodedBits(bits);
    return true;
  }

  Status U32(const U32Enc enc, const uint32_t /*default_value*/,
             uint32_t* JXL_RESTRICT /*value*/) override {
    max_bits_ += U32Coder::MaxEncodedBits(enc);
    return true;
  }

  Status U64(const uint64_t /*default_value*/,
             uint64_t* JXL_RESTRICT /*value*/) override {
    max_bits_ += U64Coder::MaxEncodedBits();
    return true;
  }

  Status F16(const float /*default_value*/,
             float* JXL_RESTRICT /*value*/) override {
    max_bits_ += F16Coder::MaxEncodedBits();
    return true;
  }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT all_default) override {
    JXL_RETURN_IF_ERROR(Bool(true, all_default));
    return false;  // For max bits, assume nothing is default
  }

  // Always visit conditional fields to get a (loose) upper bound.
  Status Conditional(bool /*condition*/) override { return true; }

  Status BeginExtensions(uint64_t* JXL_RESTRICT /*extensions*/) override {
    // Skip - extensions are not included in "MaxBits" because their length
    // is potentially unbounded.
    return true;
  }

  Status EndExtensions() override { return true; }

  size_t MaxBits() const { return max_bits_; }

  const char* VisitorName() override { return "MaxBitsVisitor"; }

 private:
  size_t max_bits_ = 0;
};

class CanEncodeVisitor : public VisitorBase {
 public:
  explicit CanEncodeVisitor(bool print_sizes)
      : VisitorBase(print_sizes), print_sizes_(print_sizes) {}

  Status Bits(const size_t bits, const uint32_t /*default_value*/,
              uint32_t* JXL_RESTRICT value) override {
    size_t encoded_bits = 0;
    ok_ &= BitsCoder::CanEncode(bits, *value, &encoded_bits);
    if (print_sizes_) Trace("u(%zu) = %u\n", bits, *value);
    encoded_bits_ += encoded_bits;
    return true;
  }

  Status U32(const U32Enc enc, const uint32_t /*default_value*/,
             uint32_t* JXL_RESTRICT value) override {
    size_t encoded_bits = 0;
    ok_ &= U32Coder::CanEncode(enc, *value, &encoded_bits);
    if (print_sizes_) Trace("U32(%zu) = %u\n", encoded_bits, *value);
    encoded_bits_ += encoded_bits;
    return true;
  }

  Status U64(const uint64_t /*default_value*/,
             uint64_t* JXL_RESTRICT value) override {
    size_t encoded_bits = 0;
    ok_ &= U64Coder::CanEncode(*value, &encoded_bits);
    if (print_sizes_) {
      Trace("U64(%zu) = %" PRIu64 "\n", encoded_bits, *value);
    }
    encoded_bits_ += encoded_bits;
    return true;
  }

  Status F16(const float /*default_value*/,
             float* JXL_RESTRICT value) override {
    size_t encoded_bits = 0;
    ok_ &= F16Coder::CanEncode(*value, &encoded_bits);
    if (print_sizes_) {
      Trace("F16(%zu) = %.6f\n", encoded_bits, static_cast<double>(*value));
    }
    encoded_bits_ += encoded_bits;
    return true;
  }

  Status AllDefault(const Fields& fields,
                    bool* JXL_RESTRICT all_default) override {
    *all_default = Bundle::AllDefault(fields);
    JXL_RETURN_IF_ERROR(Bool(true, all_default));
    return *all_default;
  }

  Status BeginExtensions(uint64_t* JXL_RESTRICT extensions) override {
    JXL_QUIET_RETURN_IF_ERROR(VisitorBase::BeginExtensions(extensions));
    extensions_ = *extensions;
    if (*extensions != 0) {
      JXL_ASSERT(pos_after_ext_ == 0);
      pos_after_ext_ = encoded_bits_;
      JXL_ASSERT(pos_after_ext_ != 0);  // visited "extensions"
    }
    return true;
  }
  // EndExtensions = default.

  Status GetSizes(size_t* JXL_RESTRICT extension_bits,
                  size_t* JXL_RESTRICT total_bits) {
    JXL_RETURN_IF_ERROR(ok_);
    *extension_bits = 0;
    *total_bits = encoded_bits_;
    // Only if extension field was nonzero will we encode their sizes.
    if (pos_after_ext_ != 0) {
      JXL_ASSERT(encoded_bits_ >= pos_after_ext_);
      *extension_bits = encoded_bits_ - pos_after_ext_;
      // Also need to encode *extension_bits and bill it to *total_bits.
      size_t encoded_bits = 0;
      ok_ &= U64Coder::CanEncode(*extension_bits, &encoded_bits);
      *total_bits += encoded_bits;

      // TODO(janwas): support encoding individual extension sizes. We
      // currently ascribe all bits to the first and send zeros for the
      // others.
      for (size_t i = 1; i < hwy::PopCount(extensions_); ++i) {
        encoded_bits = 0;
        ok_ &= U64Coder::CanEncode(0, &encoded_bits);
        *total_bits += encoded_bits;
      }
    }
    return true;
  }

  const char* VisitorName() override { return "CanEncodeVisitor"; }

 private:
  const bool print_sizes_;
  bool ok_ = true;
  size_t encoded_bits_ = 0;
  uint64_t extensions_ = 0;
  // Snapshot of encoded_bits_ after visiting the extension field, but NOT
  // including the hidden extension sizes.
  uint64_t pos_after_ext_ = 0;
};

class WriteVisitor : public VisitorBase {
 public:
  WriteVisitor(const size_t extension_bits, BitWriter* JXL_RESTRICT writer)
      : extension_bits_(extension_bits), writer_(writer) {}

  Status Bits(const size_t bits, const uint32_t /*default_value*/,
              uint32_t* JXL_RESTRICT value) override {
    ok_ &= BitsCoder::Write(bits, *value, writer_);
    return true;
  }
  Status U32(const U32Enc enc, const uint32_t /*default_value*/,
             uint32_t* JXL_RESTRICT value) override {
    ok_ &= U32Coder::Write(enc, *value, writer_);
    return true;
  }

  Status U64(const uint64_t /*default_value*/,
             uint64_t* JXL_RESTRICT value) override {
    ok_ &= U64Coder::Write(*value, writer_);
    return true;
  }

  Status F16(const float /*default_value*/,
             float* JXL_RESTRICT value) override {
    ok_ &= F16Coder::Write(*value, writer_);
    return true;
  }

  Status BeginExtensions(uint64_t* JXL_RESTRICT extensions) override {
    JXL_QUIET_RETURN_IF_ERROR(VisitorBase::BeginExtensions(extensions));
    if (*extensions == 0) {
      JXL_ASSERT(extension_bits_ == 0);
      return true;
    }
    // TODO(janwas): extend API to pass in array of extension_bits, one per
    // extension. We currently ascribe all bits to the first extension, but
    // this is only an encoder limitation. NOTE: extension_bits_ can be zero
    // if an extension does not require any additional fields.
    ok_ &= U64Coder::Write(extension_bits_, writer_);
    // For each nonzero bit except the lowest/first (already written):
    for (uint64_t remaining_extensions = *extensions & (*extensions - 1);
         remaining_extensions != 0;
         remaining_extensions &= remaining_extensions - 1) {
      ok_ &= U64Coder::Write(0, writer_);
    }
    return true;
  }
  // EndExtensions = default.

  Status OK() const { return ok_; }

  const char* VisitorName() override { return "WriteVisitor"; }

 private:
  const size_t extension_bits_;
  BitWriter* JXL_RESTRICT writer_;
  bool ok_ = true;
};

}  // namespace

void Bundle::Init(Fields* fields) {
  InitVisitor visitor;
  if (!visitor.Visit(fields, PrintVisitors() ? "-- Init\n" : "")) {
    JXL_ABORT("Init should never fail");
  }
}
void Bundle::SetDefault(Fields* fields) {
  SetDefaultVisitor visitor;
  if (!visitor.Visit(fields, PrintVisitors() ? "-- SetDefault\n" : "")) {
    JXL_ABORT("SetDefault should never fail");
  }
}
bool Bundle::AllDefault(const Fields& fields) {
  AllDefaultVisitor visitor(/*print_all_default=*/PrintAllDefault());
  const char* name =
      (PrintVisitors() || PrintAllDefault()) ? "[[AllDefault\n" : "";
  if (!visitor.VisitConst(fields, name)) {
    JXL_ABORT("AllDefault should never fail");
  }

  if (PrintAllDefault()) printf("  %d]]\n", visitor.AllDefault());
  return visitor.AllDefault();
}
size_t Bundle::MaxBits(const Fields& fields) {
  MaxBitsVisitor visitor;
#if JXL_ENABLE_ASSERT
  Status ret =
#else
  (void)
#endif  // JXL_ENABLE_ASSERT
      visitor.VisitConst(fields, PrintVisitors() ? "-- MaxBits\n" : "");
  JXL_ASSERT(ret);
  return visitor.MaxBits();
}
Status Bundle::CanEncode(const Fields& fields, size_t* extension_bits,
                         size_t* total_bits) {
  CanEncodeVisitor visitor(/*print_sizes=*/PrintSizes());
  const char* name = (PrintVisitors() || PrintSizes()) ? "[[CanEncode\n" : "";
  JXL_QUIET_RETURN_IF_ERROR(visitor.VisitConst(fields, name));
  JXL_QUIET_RETURN_IF_ERROR(visitor.GetSizes(extension_bits, total_bits));
  if (PrintSizes()) printf("  %zu]]\n", *total_bits);
  return true;
}
Status Bundle::Read(BitReader* reader, Fields* fields) {
  ReadVisitor visitor(reader, /*print_read=*/PrintRead());
  JXL_RETURN_IF_ERROR(
      visitor.Visit(fields, PrintVisitors() ? "-- Read\n" : ""));
  return visitor.OK();
}
bool Bundle::CanRead(BitReader* reader, Fields* fields) {
  ReadVisitor visitor(reader, /*print_read=*/PrintRead());
  Status status = visitor.Visit(fields, PrintVisitors() ? "-- Read\n" : "");
  // We are only checking here whether there are enough bytes. We still return
  // true for other errors because it means there are enough bytes to determine
  // there's an error. Use Read() to determine which error it is.
  return status.code() != StatusCode::kNotEnoughBytes;
}
Status Bundle::Write(const Fields& fields, BitWriter* writer, size_t layer,
                     AuxOut* aux_out) {
  size_t extension_bits, total_bits;
  JXL_RETURN_IF_ERROR(CanEncode(fields, &extension_bits, &total_bits));

  BitWriter::Allotment allotment(writer, total_bits);
  WriteVisitor visitor(extension_bits, writer);
  JXL_RETURN_IF_ERROR(
      visitor.VisitConst(fields, PrintVisitors() ? "-- Write\n" : ""));
  JXL_RETURN_IF_ERROR(visitor.OK());
  ReclaimAndCharge(writer, &allotment, layer, aux_out);
  return true;
}

size_t U32Coder::MaxEncodedBits(const U32Enc enc) {
  size_t extra_bits = 0;
  for (uint32_t selector = 0; selector < 4; ++selector) {
    const U32Distr d = enc.GetDistr(selector);
    if (d.IsDirect()) {
      continue;
    } else {
      extra_bits = std::max<size_t>(extra_bits, d.ExtraBits());
    }
  }
  return 2 + extra_bits;
}

Status U32Coder::CanEncode(const U32Enc enc, const uint32_t value,
                           size_t* JXL_RESTRICT encoded_bits) {
  uint32_t selector;
  size_t total_bits;
  const Status ok = ChooseSelector(enc, value, &selector, &total_bits);
  *encoded_bits = ok ? total_bits : 0;
  return ok;
}

uint32_t U32Coder::Read(const U32Enc enc, BitReader* JXL_RESTRICT reader) {
  const uint32_t selector = reader->ReadFixedBits<2>();
  const U32Distr d = enc.GetDistr(selector);
  if (d.IsDirect()) {
    return d.Direct();
  } else {
    return reader->ReadBits(d.ExtraBits()) + d.Offset();
  }
}

// Returns false if the value is too large to encode.
Status U32Coder::Write(const U32Enc enc, const uint32_t value,
                       BitWriter* JXL_RESTRICT writer) {
  uint32_t selector;
  size_t total_bits;
  JXL_RETURN_IF_ERROR(ChooseSelector(enc, value, &selector, &total_bits));

  writer->Write(2, selector);

  const U32Distr d = enc.GetDistr(selector);
  if (!d.IsDirect()) {  // Nothing more to write for direct encoding
    const uint32_t offset = d.Offset();
    JXL_ASSERT(value >= offset);
    writer->Write(total_bits - 2, value - offset);
  }

  return true;
}

Status U32Coder::ChooseSelector(const U32Enc enc, const uint32_t value,
                                uint32_t* JXL_RESTRICT selector,
                                size_t* JXL_RESTRICT total_bits) {
#if JXL_ENABLE_ASSERT
  const size_t bits_required = 32 - Num0BitsAboveMS1Bit(value);
#endif  // JXL_ENABLE_ASSERT
  JXL_ASSERT(bits_required <= 32);

  *selector = 0;
  *total_bits = 0;

  // It is difficult to verify whether Dist32Byte are sorted, so check all
  // selectors and keep the one with the fewest total_bits.
  *total_bits = 64;  // more than any valid encoding
  for (uint32_t s = 0; s < 4; ++s) {
    const U32Distr d = enc.GetDistr(s);
    if (d.IsDirect()) {
      if (d.Direct() == value) {
        *selector = s;
        *total_bits = 2;
        return true;  // Done, direct is always the best possible.
      }
      continue;
    }
    const size_t extra_bits = d.ExtraBits();
    const uint32_t offset = d.Offset();
    if (value < offset || value >= offset + (1ULL << extra_bits)) continue;

    // Better than prior encoding, remember it:
    if (2 + extra_bits < *total_bits) {
      *selector = s;
      *total_bits = 2 + extra_bits;
    }
  }

  if (*total_bits == 64) {
    return JXL_FAILURE("No feasible selector for %u", value);
  }

  return true;
}

uint64_t U64Coder::Read(BitReader* JXL_RESTRICT reader) {
  uint64_t selector = reader->ReadFixedBits<2>();
  if (selector == 0) {
    return 0;
  }
  if (selector == 1) {
    return 1 + reader->ReadFixedBits<4>();
  }
  if (selector == 2) {
    return 17 + reader->ReadFixedBits<8>();
  }

  // selector 3, varint, groups have first 12, then 8, and last 4 bits.
  uint64_t result = reader->ReadFixedBits<12>();

  uint64_t shift = 12;
  while (reader->ReadFixedBits<1>()) {
    if (shift == 60) {
      result |= static_cast<uint64_t>(reader->ReadFixedBits<4>()) << shift;
      break;
    }
    result |= static_cast<uint64_t>(reader->ReadFixedBits<8>()) << shift;
    shift += 8;
  }

  return result;
}

// Returns false if the value is too large to encode.
Status U64Coder::Write(uint64_t value, BitWriter* JXL_RESTRICT writer) {
  if (value == 0) {
    // Selector: use 0 bits, value 0
    writer->Write(2, 0);
  } else if (value <= 16) {
    // Selector: use 4 bits, value 1..16
    writer->Write(2, 1);
    writer->Write(4, value - 1);
  } else if (value <= 272) {
    // Selector: use 8 bits, value 17..272
    writer->Write(2, 2);
    writer->Write(8, value - 17);
  } else {
    // Selector: varint, first a 12-bit group, after that per 8-bit group.
    writer->Write(2, 3);
    writer->Write(12, value & 4095);
    value >>= 12;
    int shift = 12;
    while (value > 0 && shift < 60) {
      // Indicate varint not done
      writer->Write(1, 1);
      writer->Write(8, value & 255);
      value >>= 8;
      shift += 8;
    }
    if (value > 0) {
      // This only could happen if shift == N - 4.
      writer->Write(1, 1);
      writer->Write(4, value & 15);
      // Implicitly closed sequence, no extra stop bit is required.
    } else {
      // Indicate end of varint
      writer->Write(1, 0);
    }
  }

  return true;
}

// Can always encode, but useful because it also returns bit size.
Status U64Coder::CanEncode(uint64_t value, size_t* JXL_RESTRICT encoded_bits) {
  if (value == 0) {
    *encoded_bits = 2;  // 2 selector bits
  } else if (value <= 16) {
    *encoded_bits = 2 + 4;  // 2 selector bits + 4 payload bits
  } else if (value <= 272) {
    *encoded_bits = 2 + 8;  // 2 selector bits + 8 payload bits
  } else {
    *encoded_bits = 2 + 12;  // 2 selector bits + 12 payload bits
    value >>= 12;
    int shift = 12;
    while (value > 0 && shift < 60) {
      *encoded_bits += 1 + 8;  // 1 continuation bit + 8 payload bits
      value >>= 8;
      shift += 8;
    }
    if (value > 0) {
      // This only could happen if shift == N - 4.
      *encoded_bits += 1 + 4;  // 1 continuation bit + 4 payload bits
    } else {
      *encoded_bits += 1;  // 1 stop bit
    }
  }

  return true;
}

Status F16Coder::Read(BitReader* JXL_RESTRICT reader,
                      float* JXL_RESTRICT value) {
  const uint32_t bits16 = reader->ReadFixedBits<16>();
  const uint32_t sign = bits16 >> 15;
  const uint32_t biased_exp = (bits16 >> 10) & 0x1F;
  const uint32_t mantissa = bits16 & 0x3FF;

  if (JXL_UNLIKELY(biased_exp == 31)) {
    return JXL_FAILURE("F16 infinity or NaN are not supported");
  }

  // Subnormal or zero
  if (JXL_UNLIKELY(biased_exp == 0)) {
    *value = (1.0f / 16384) * (mantissa * (1.0f / 1024));
    if (sign) *value = -*value;
    return true;
  }

  // Normalized: convert the representation directly (faster than ldexp/tables).
  const uint32_t biased_exp32 = biased_exp + (127 - 15);
  const uint32_t mantissa32 = mantissa << (23 - 10);
  const uint32_t bits32 = (sign << 31) | (biased_exp32 << 23) | mantissa32;
  memcpy(value, &bits32, sizeof(bits32));
  return true;
}

Status F16Coder::Write(float value, BitWriter* JXL_RESTRICT writer) {
  uint32_t bits32;
  memcpy(&bits32, &value, sizeof(bits32));
  const uint32_t sign = bits32 >> 31;
  const uint32_t biased_exp32 = (bits32 >> 23) & 0xFF;
  const uint32_t mantissa32 = bits32 & 0x7FFFFF;

  const int32_t exp = static_cast<int32_t>(biased_exp32) - 127;
  if (JXL_UNLIKELY(exp > 15)) {
    return JXL_FAILURE("Too big to encode, CanEncode should return false");
  }

  // Tiny or zero => zero.
  if (exp < -24) {
    writer->Write(16, 0);
    return true;
  }

  uint32_t biased_exp16, mantissa16;

  // exp = [-24, -15] => subnormal
  if (JXL_UNLIKELY(exp < -14)) {
    biased_exp16 = 0;
    const uint32_t sub_exp = static_cast<uint32_t>(-14 - exp);
    JXL_ASSERT(1 <= sub_exp && sub_exp < 11);
    mantissa16 = (1 << (10 - sub_exp)) + (mantissa32 >> (13 + sub_exp));
  } else {
    // exp = [-14, 15]
    biased_exp16 = static_cast<uint32_t>(exp + 15);
    JXL_ASSERT(1 <= biased_exp16 && biased_exp16 < 31);
    mantissa16 = mantissa32 >> 13;
  }

  JXL_ASSERT(mantissa16 < 1024);
  const uint32_t bits16 = (sign << 15) | (biased_exp16 << 10) | mantissa16;
  JXL_ASSERT(bits16 < 0x10000);
  writer->Write(16, bits16);
  return true;
}

Status F16Coder::CanEncode(float value, size_t* JXL_RESTRICT encoded_bits) {
  *encoded_bits = MaxEncodedBits();
  if (std::isnan(value) || std::isinf(value)) {
    return JXL_FAILURE("Should not attempt to store NaN and infinity");
  }
  return std::abs(value) <= 65504.0f;
}

}  // namespace jxl
