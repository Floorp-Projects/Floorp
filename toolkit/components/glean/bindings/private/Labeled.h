/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_Labeled_h
#define mozilla_glean_Labeled_h

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/Boolean.h"
#include "mozilla/glean/bindings/Counter.h"
#include "mozilla/glean/bindings/GleanMetric.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/bindings/String.h"
#include "mozilla/glean/fog_ffi_generated.h"

enum class DynamicLabel : uint16_t;

namespace mozilla::glean {

namespace impl {

template <typename T, typename E>
class Labeled {
 public:
  constexpr explicit Labeled<T, E>(uint32_t id) : mId(id) {}

  /**
   * Gets a specific metric for a given label.
   *
   * If a set of acceptable labels were specified in the `metrics.yaml` file,
   * and the given label is not in the set, it will be recorded under the
   * special `OTHER_LABEL` label.
   *
   * If a set of acceptable labels was not specified in the `metrics.yaml` file,
   * only the first 16 unique labels will be used.
   * After that, any additional labels will be recorded under the special
   * `OTHER_LABEL` label.
   *
   * @param aLabel - a snake_case string under 30 characters in length,
   *                 otherwise the metric will be recorded under the special
   *                 `OTHER_LABEL` label and an error will be recorded.
   */
  T Get(const nsACString& aLabel) const;

  /**
   * Gets a specific metric for a given label, using the label's enum variant.
   *
   * @param aLabel - a variant of this label's label enum.
   */
  T EnumGet(E aLabel) const;

 private:
  const uint32_t mId;
};

static inline void UpdateLabeledMirror(Telemetry::ScalarID aMirrorId,
                                       uint32_t aSubmetricId,
                                       const nsACString& aLabel) {
  GetLabeledMirrorLock().apply([&](auto& lock) {
    auto tuple = std::make_tuple<Telemetry::ScalarID, nsString>(
        std::move(aMirrorId), NS_ConvertUTF8toUTF16(aLabel));
    lock.ref()->InsertOrUpdate(aSubmetricId, std::move(tuple));
  });
}

template <typename E>
class Labeled<BooleanMetric, E> {
 public:
  constexpr explicit Labeled(uint32_t id) : mId(id) {}

  BooleanMetric Get(const nsACString& aLabel) const {
    auto submetricId = fog_labeled_boolean_get(mId, &aLabel);
    // If this labeled metric is mirrored, we need to map the submetric id back
    // to the label string and mirrored scalar so we can mirror its operations.
    auto mirrorId = ScalarIdForMetric(mId);
    if (mirrorId) {
      UpdateLabeledMirror(mirrorId.extract(), submetricId, aLabel);
    }
    return BooleanMetric(submetricId);
  }

  BooleanMetric EnumGet(E aLabel) const {
    auto submetricId =
        fog_labeled_boolean_enum_get(mId, static_cast<uint16_t>(aLabel));
    auto mirrorId = ScalarIdForMetric(mId);
    if (mirrorId) {
      // Telemetry's keyed scalars operate on (16-bit) strings,
      // so we're going to need the string for this enum.
      nsCString label;
      fog_labeled_enum_to_str(mId, static_cast<uint16_t>(aLabel), &label);
      UpdateLabeledMirror(mirrorId.extract(), submetricId, label);
    }
    return BooleanMetric(submetricId);
  }

 private:
  const uint32_t mId;
};

template <typename E>
class Labeled<CounterMetric, E> {
 public:
  constexpr explicit Labeled(uint32_t id) : mId(id) {}

  CounterMetric Get(const nsACString& aLabel) const {
    auto submetricId = fog_labeled_counter_get(mId, &aLabel);
    // If this labeled metric is mirrored, we need to map the submetric id back
    // to the label string and mirrored scalar so we can mirror its operations.
    auto mirrorId = ScalarIdForMetric(mId);
    if (mirrorId) {
      GetLabeledMirrorLock().apply([&](auto& lock) {
        auto tuple = std::make_tuple<Telemetry::ScalarID, nsString>(
            mirrorId.extract(), NS_ConvertUTF8toUTF16(aLabel));
        lock.ref()->InsertOrUpdate(submetricId, std::move(tuple));
      });
    }
    return CounterMetric(submetricId);
  }

  CounterMetric EnumGet(E aLabel) const {
    auto submetricId =
        fog_labeled_counter_enum_get(mId, static_cast<uint16_t>(aLabel));
    auto mirrorId = ScalarIdForMetric(mId);
    if (mirrorId) {
      // Telemetry's keyed scalars operate on (16-bit) strings,
      // so we're going to need the string for this enum.
      nsCString label;
      fog_labeled_enum_to_str(mId, static_cast<uint16_t>(aLabel), &label);
      UpdateLabeledMirror(mirrorId.extract(), submetricId, label);
    }
    return CounterMetric(submetricId);
  }

 private:
  const uint32_t mId;
};

template <typename E>
class Labeled<StringMetric, E> {
 public:
  constexpr explicit Labeled(uint32_t id) : mId(id) {}

  StringMetric Get(const nsACString& aLabel) const {
    auto submetricId = fog_labeled_string_get(mId, &aLabel);
    // Why no GIFFT map here?
    // Labeled Strings can't be mirrored. Telemetry has no compatible probe.
    return StringMetric(submetricId);
  }

  StringMetric EnumGet(E aLabel) const {
    auto submetricId =
        fog_labeled_string_enum_get(mId, static_cast<uint16_t>(aLabel));
    // Why no GIFFT map here?
    // Labeled Strings can't be mirrored. Telemetry has no compatible probe.
    return StringMetric(submetricId);
  }

 private:
  const uint32_t mId;
};

template <>
class Labeled<BooleanMetric, DynamicLabel> {
 public:
  constexpr explicit Labeled(uint32_t id) : mId(id) {}

  BooleanMetric Get(const nsACString& aLabel) const {
    auto submetricId = fog_labeled_boolean_get(mId, &aLabel);
    // If this labeled metric is mirrored, we need to map the submetric id back
    // to the label string and mirrored scalar so we can mirror its operations.
    auto mirrorId = ScalarIdForMetric(mId);
    if (mirrorId) {
      UpdateLabeledMirror(mirrorId.extract(), submetricId, aLabel);
    }
    return BooleanMetric(submetricId);
  }

  BooleanMetric EnumGet(DynamicLabel aLabel) const = delete;

 private:
  const uint32_t mId;
};

template <>
class Labeled<CounterMetric, DynamicLabel> {
 public:
  constexpr explicit Labeled(uint32_t id) : mId(id) {}

  CounterMetric Get(const nsACString& aLabel) const {
    auto submetricId = fog_labeled_counter_get(mId, &aLabel);
    // If this labeled metric is mirrored, we need to map the submetric id back
    // to the label string and mirrored scalar so we can mirror its operations.
    auto mirrorId = ScalarIdForMetric(mId);
    if (mirrorId) {
      GetLabeledMirrorLock().apply([&](auto& lock) {
        auto tuple = std::make_tuple<Telemetry::ScalarID, nsString>(
            mirrorId.extract(), NS_ConvertUTF8toUTF16(aLabel));
        lock.ref()->InsertOrUpdate(submetricId, std::move(tuple));
      });
    }
    return CounterMetric(submetricId);
  }

  CounterMetric EnumGet(DynamicLabel aLabel) const = delete;

 private:
  const uint32_t mId;
};

template <>
class Labeled<StringMetric, DynamicLabel> {
 public:
  constexpr explicit Labeled(uint32_t id) : mId(id) {}

  StringMetric Get(const nsACString& aLabel) const {
    auto submetricId = fog_labeled_string_get(mId, &aLabel);
    // Why no GIFFT map here?
    // Labeled Strings can't be mirrored. Telemetry has no compatible probe.
    return StringMetric(submetricId);
  }

  StringMetric EnumGet(DynamicLabel aLabel) const = delete;

 private:
  const uint32_t mId;
};

}  // namespace impl

class GleanLabeled final : public GleanMetric {
 public:
  explicit GleanLabeled(uint32_t aId, uint32_t aTypeId, nsISupports* aParent)
      : GleanMetric(aParent), mId(aId), mTypeId(aTypeId) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override final;

  already_AddRefed<GleanMetric> NamedGetter(const nsAString& aName,
                                            bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  void GetSupportedNames(nsTArray<nsString>& aNames);

 private:
  virtual ~GleanLabeled() = default;

  const uint32_t mId;
  const uint32_t mTypeId;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_Labeled_h */
