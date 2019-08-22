/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryScalarInfo_h__
#define TelemetryScalarInfo_h__

#include "TelemetryCommon.h"

// This module is internal to Telemetry. It defines a structure that holds the
// scalar info. It should only be used by TelemetryScalarData.h automatically
// generated file and TelemetryScalar.cpp. This should not be used anywhere
// else. For the public interface to Telemetry functionality, see Telemetry.h.

namespace {

/**
 * Base scalar information, common to both "static" and dynamic scalars.
 */
struct BaseScalarInfo {
  uint32_t kind;
  uint32_t dataset;
  mozilla::Telemetry::Common::RecordedProcessType record_in_processes;
  bool keyed;
  mozilla::Telemetry::Common::SupportedProduct products;
  bool builtin;

  constexpr BaseScalarInfo(
      uint32_t aKind, uint32_t aDataset,
      mozilla::Telemetry::Common::RecordedProcessType aRecordInProcess,
      bool aKeyed, mozilla::Telemetry::Common::SupportedProduct aProducts,
      bool aBuiltin = true)
      : kind(aKind),
        dataset(aDataset),
        record_in_processes(aRecordInProcess),
        keyed(aKeyed),
        products(aProducts),
        builtin(aBuiltin) {}
  virtual ~BaseScalarInfo() {}

  virtual const char* name() const = 0;
  virtual const char* expiration() const = 0;

  virtual uint32_t storeOffset() const = 0;

  virtual uint32_t storeCount() const = 0;
};

/**
 * "Static" scalar definition: these are the ones riding
 * the trains.
 */
struct ScalarInfo : BaseScalarInfo {
  uint32_t name_offset;
  uint32_t expiration_offset;
  uint32_t store_count;
  uint16_t store_offset;

  // In order to cleanly support dynamic scalars in TelemetryScalar.cpp, we need
  // to use virtual functions for |name| and |expiration|, as they won't be
  // looked up in the static tables in that case. However, using virtual
  // functions makes |ScalarInfo| non-aggregate and prevents from using
  // aggregate initialization (curly brackets) in the generated
  // TelemetryScalarData.h. To work around this problem we define a constructor
  // that takes the exact number of parameters we need.
  constexpr ScalarInfo(uint32_t aKind, uint32_t aNameOffset, uint32_t aExpirationOffset,
             uint32_t aDataset,
             mozilla::Telemetry::Common::RecordedProcessType aRecordInProcess,
             bool aKeyed,
             mozilla::Telemetry::Common::SupportedProduct aProducts,
             uint32_t aStoreCount, uint16_t aStoreOffset)
      : BaseScalarInfo(aKind, aDataset, aRecordInProcess, aKeyed, aProducts),
        name_offset(aNameOffset),
        expiration_offset(aExpirationOffset),
        store_count(aStoreCount),
        store_offset(aStoreOffset) {}

  const char* name() const override;
  const char* expiration() const override;

  uint32_t storeOffset() const override { return store_offset; };

  uint32_t storeCount() const override { return store_count; };
};

}  // namespace

#endif  // TelemetryScalarInfo_h__
