/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"

#include "mozilla/Maybe.h"
#include "nsPrintfCString.h"
#include "public/GeckoTraceEvent.h"

using namespace mozilla;
using webrtc::trace_event_internal::TraceValueUnion;

void uprofiler_register_thread(const char* name, void* stacktop) {
#ifdef MOZ_GECKO_PROFILER
  profiler_register_thread(name, stacktop);
#endif  // MOZ_GECKO_PROFILER
}

void uprofiler_unregister_thread() {
#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_thread();
#endif  // MOZ_GECKO_PROFILER
}

#ifdef MOZ_GECKO_PROFILER
namespace {
Maybe<MarkerTiming> ToTiming(char phase) {
  switch (phase) {
    case 'B':
      return Some(MarkerTiming::IntervalStart());
    case 'E':
      return Some(MarkerTiming::IntervalEnd());
    case 'I':
      return Some(MarkerTiming::InstantNow());
    default:
      return Nothing();
  }
}

struct TraceOption {
  bool mPassed = false;
  ProfilerString8View mName;
  Variant<int64_t, bool, double, ProfilerString8View> mValue = AsVariant(false);
};

struct TraceMarker {
  static constexpr int MAX_NUM_ARGS = 2;
  using OptionsType = std::tuple<TraceOption, TraceOption>;
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return MakeStringSpan("TraceEvent");
  }
  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const OptionsType& aArgs) {
    auto writeValue = [&](const auto& aName, const auto& aVariant) {
      aVariant.match(
          [&](const int64_t& aValue) { aWriter.IntProperty(aName, aValue); },
          [&](const bool& aValue) { aWriter.BoolProperty(aName, aValue); },
          [&](const double& aValue) { aWriter.DoubleProperty(aName, aValue); },
          [&](const ProfilerString8View& aValue) {
            aWriter.StringProperty(aName, aValue);
          });
    };
    if (const auto& arg = std::get<0>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name1", arg.mName);
      writeValue("val1", arg.mValue);
    }
    if (const auto& arg = std::get<1>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name2", arg.mName);
      writeValue("val2", arg.mValue);
    }
  }
  static mozilla::MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.SetChartLabel("{marker.name}");
    schema.SetTableLabel(
        "{marker.name}  {marker.data.name1} {marker.data.val1}  "
        "{marker.data.name2} {marker.data.val2}");
    schema.AddKeyLabelFormatSearchable("name1", "Key 1", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val1", "Value 1", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("name2", "Key 2", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val2", "Value 2", MS::Format::String,
                                       MS::Searchable::Searchable);
    return schema;
  }
};
}  // namespace

namespace mozilla {
template <>
struct ProfileBufferEntryWriter::Serializer<TraceOption> {
  static Length Bytes(const TraceOption& aOption) {
    // 1 byte to store passed flag, then object size if passed.
    return aOption.mPassed ? (1 + SumBytes(aOption.mName, aOption.mValue)) : 1;
  }

  static void Write(ProfileBufferEntryWriter& aEW, const TraceOption& aOption) {
    // 'T'/'t' is just an arbitrary 1-byte value to distinguish states.
    if (aOption.mPassed) {
      aEW.WriteObject<char>('T');
      // Use the Serializer for the name/value pair.
      aEW.WriteObject(aOption.mName);
      aEW.WriteObject(aOption.mValue);
    } else {
      aEW.WriteObject<char>('t');
    }
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<TraceOption> {
  static void ReadInto(ProfileBufferEntryReader& aER, TraceOption& aOption) {
    char c = aER.ReadObject<char>();
    if ((aOption.mPassed = (c == 'T'))) {
      aER.ReadIntoObject(aOption.mName);
      aER.ReadIntoObject(aOption.mValue);
    } else {
      MOZ_ASSERT(c == 't');
    }
  }

  static TraceOption Read(ProfileBufferEntryReader& aER) {
    TraceOption option;
    ReadInto(aER, option);
    return option;
  }
};
}  // namespace mozilla
#endif  // MOZ_GECKO_PROFILER

void uprofiler_simple_event_marker(const char* name, char phase, int num_args,
                                   const char** arg_names,
                                   const unsigned char* arg_types,
                                   const unsigned long long* arg_values) {
#ifdef MOZ_GECKO_PROFILER
  if (!profiler_thread_is_being_profiled_for_markers()) {
    return;
  }
  Maybe<MarkerTiming> timing = ToTiming(phase);
  if (!timing) {
    if (getenv("MOZ_LOG_UNKNOWN_TRACE_EVENT_PHASES")) {
      fprintf(stderr, "XXX UProfiler: phase not handled: '%c'\n", phase);
    }
    return;
  }
  MOZ_ASSERT(num_args <= TraceMarker::MAX_NUM_ARGS);
  TraceMarker::OptionsType tuple;
  TraceOption* args[2] = {&std::get<0>(tuple), &std::get<1>(tuple)};
  for (int i = 0; i < std::min(num_args, TraceMarker::MAX_NUM_ARGS); ++i) {
    auto& arg = *args[i];
    arg.mPassed = true;
    arg.mName = ProfilerString8View::WrapNullTerminatedString(arg_names[i]);
    switch (arg_types[i]) {
      case TRACE_VALUE_TYPE_UINT:
        MOZ_ASSERT(arg_values[i] <= std::numeric_limits<int64_t>::max());
        arg.mValue = AsVariant(static_cast<int64_t>(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])->as_uint));
        break;
      case TRACE_VALUE_TYPE_INT:
        arg.mValue = AsVariant(static_cast<int64_t>(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])->as_int));
        break;
      case TRACE_VALUE_TYPE_BOOL:
        arg.mValue = AsVariant(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])->as_bool);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        arg.mValue =
            AsVariant(reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                          ->as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        arg.mValue = AsVariant(ProfilerString8View(nsPrintfCString(
            "%p", reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                      ->as_pointer)));
        break;
      case TRACE_VALUE_TYPE_STRING:
        arg.mValue = AsVariant(ProfilerString8View::WrapNullTerminatedString(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                ->as_string));
        break;
      case TRACE_VALUE_TYPE_COPY_STRING:
        arg.mValue = AsVariant(ProfilerString8View(
            nsCString(reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                          ->as_string)));
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected trace value type");
        arg.mValue = AsVariant(ProfilerString8View(
            nsPrintfCString("Unexpected type: %u", arg_types[i])));
        break;
    }
  }
  profiler_add_marker(ProfilerString8View::WrapNullTerminatedString(name),
                      geckoprofiler::category::MEDIA_RT, {timing.extract()},
                      TraceMarker{}, tuple);
#endif  // MOZ_GECKO_PROFILER
}
