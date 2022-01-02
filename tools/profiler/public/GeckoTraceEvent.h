// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file under third_party_mods/chromium or at:
// http://src.chromium.org/svn/trunk/src/LICENSE

#ifndef GECKO_TRACE_EVENT_H_
#define GECKO_TRACE_EVENT_H_

#include "MicroGeckoProfiler.h"

// Extracted from Chromium's src/base/debug/trace_event.h, modified to talk to
// the Gecko profiler.

#if defined(RTC_DISABLE_TRACE_EVENTS)
#define RTC_TRACE_EVENTS_ENABLED 0
#else
#define RTC_TRACE_EVENTS_ENABLED 1
#endif

// Type values for identifying types in the TraceValue union.
#define TRACE_VALUE_TYPE_BOOL         (static_cast<unsigned char>(1))
#define TRACE_VALUE_TYPE_UINT         (static_cast<unsigned char>(2))
#define TRACE_VALUE_TYPE_INT          (static_cast<unsigned char>(3))
#define TRACE_VALUE_TYPE_DOUBLE       (static_cast<unsigned char>(4))
#define TRACE_VALUE_TYPE_POINTER      (static_cast<unsigned char>(5))
#define TRACE_VALUE_TYPE_STRING       (static_cast<unsigned char>(6))
#define TRACE_VALUE_TYPE_COPY_STRING  (static_cast<unsigned char>(7))

#if RTC_TRACE_EVENTS_ENABLED

// This header is designed to give you trace_event macros without specifying
// how the events actually get collected and stored. If you need to expose trace
// event to some other universe, you can copy-and-paste this file,
// implement the TRACE_EVENT_API macros, and do any other necessary fixup for
// the target platform. The end result is that multiple libraries can funnel
// events through to a shared trace event collector.

// Trace events are for tracking application performance and resource usage.
// Macros are provided to track:
//    Begin and end of function calls
//    Counters
//
// Events are issued against categories. Whereas RTC_LOG's
// categories are statically defined, TRACE categories are created
// implicitly with a string. For example:
//   TRACE_EVENT_INSTANT0("MY_SUBSYSTEM", "SomeImportantEvent")
//
// Events can be INSTANT, or can be pairs of BEGIN and END in the same scope:
//   TRACE_EVENT_BEGIN0("MY_SUBSYSTEM", "SomethingCostly")
//   doSomethingCostly()
//   TRACE_EVENT_END0("MY_SUBSYSTEM", "SomethingCostly")
// Note: our tools can't always determine the correct BEGIN/END pairs unless
// these are used in the same scope. Use ASYNC_BEGIN/ASYNC_END macros if you
// need them to be in separate scopes.
//
// A common use case is to trace entire function scopes. This
// issues a trace BEGIN and END automatically:
//   void doSomethingCostly() {
//     TRACE_EVENT0("MY_SUBSYSTEM", "doSomethingCostly");
//     ...
//   }
//
// Additional parameters can be associated with an event:
//   void doSomethingCostly2(int howMuch) {
//     TRACE_EVENT1("MY_SUBSYSTEM", "doSomethingCostly",
//         "howMuch", howMuch);
//     ...
//   }
//
// The trace system will automatically add to this information the
// current process id, thread id, and a timestamp in microseconds.
//
// To trace an asynchronous procedure such as an IPC send/receive, use
// ASYNC_BEGIN and ASYNC_END:
//   [single threaded sender code]
//     static int send_count = 0;
//     ++send_count;
//     TRACE_EVENT_ASYNC_BEGIN0("ipc", "message", send_count);
//     Send(new MyMessage(send_count));
//   [receive code]
//     void OnMyMessage(send_count) {
//       TRACE_EVENT_ASYNC_END0("ipc", "message", send_count);
//     }
// The third parameter is a unique ID to match ASYNC_BEGIN/ASYNC_END pairs.
// ASYNC_BEGIN and ASYNC_END can occur on any thread of any traced process.
// Pointers can be used for the ID parameter, and they will be mangled
// internally so that the same pointer on two different processes will not
// match. For example:
//   class MyTracedClass {
//    public:
//     MyTracedClass() {
//       TRACE_EVENT_ASYNC_BEGIN0("category", "MyTracedClass", this);
//     }
//     ~MyTracedClass() {
//       TRACE_EVENT_ASYNC_END0("category", "MyTracedClass", this);
//     }
//   }
//
// Trace event also supports counters, which is a way to track a quantity
// as it varies over time. Counters are created with the following macro:
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter", g_myCounterValue);
//
// Counters are process-specific. The macro itself can be issued from any
// thread, however.
//
// Sometimes, you want to track two counters at once. You can do this with two
// counter macros:
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter0", g_myCounterValue[0]);
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter1", g_myCounterValue[1]);
// Or you can do it with a combined macro:
//   TRACE_COUNTER2("MY_SUBSYSTEM", "myCounter",
//       "bytesPinned", g_myCounterValue[0],
//       "bytesAllocated", g_myCounterValue[1]);
// This indicates to the tracing UI that these counters should be displayed
// in a single graph, as a summed area chart.
//
// Since counters are in a global namespace, you may want to disembiguate with a
// unique ID, by using the TRACE_COUNTER_ID* variations.
//
// By default, trace collection is compiled in, but turned off at runtime.
// Collecting trace data is the responsibility of the embedding
// application. In Chrome's case, navigating to about:tracing will turn on
// tracing and display data collected across all active processes.
//
//
// Memory scoping note:
// Tracing copies the pointers, not the string content, of the strings passed
// in for category, name, and arg_names.  Thus, the following code will
// cause problems:
//     char* str = strdup("impprtantName");
//     TRACE_EVENT_INSTANT0("SUBSYSTEM", str);  // BAD!
//     free(str);                   // Trace system now has dangling pointer
//
// To avoid this issue with the |name| and |arg_name| parameters, use the
// TRACE_EVENT_COPY_XXX overloads of the macros at additional runtime overhead.
// Notes: The category must always be in a long-lived char* (i.e. static const).
//        The |arg_values|, when used, are always deep copied with the _COPY
//        macros.
//
// When are string argument values copied:
// const char* arg_values are only referenced by default:
//     TRACE_EVENT1("category", "name",
//                  "arg1", "literal string is only referenced");
// Use TRACE_STR_COPY to force copying of a const char*:
//     TRACE_EVENT1("category", "name",
//                  "arg1", TRACE_STR_COPY("string will be copied"));
// std::string arg_values are always copied:
//     TRACE_EVENT1("category", "name",
//                  "arg1", std::string("string will be copied"));
//
//
// Thread Safety:
// Thread safety is provided by methods defined in event_tracer.h. See the file
// for details.

// By default, const char* argument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) \
    webrtc::trace_event_internal::TraceStringWithCopy(str)

// This will mark the trace event as disabled by default. The user will need
// to explicitly enable the event.
#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

// By default, uint64 ID argument values are not mangled with the Process ID in
// TRACE_EVENT_ASYNC macros. Use this macro to force Process ID mangling.
#define TRACE_ID_MANGLE(id) \
    webrtc::trace_event_internal::TraceID::ForceMangle(id)

// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT0(category, name) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name)
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, arg1_name, arg1_val)
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Records a single event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_INSTANT0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, \
        arg2_name, arg2_val)
#define TRACE_EVENT_COPY_INSTANT0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_INSTANT1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Records a single BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_BEGIN0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, \
        arg2_name, arg2_val)
#define TRACE_EVENT_COPY_BEGIN0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Records a single END event for "name" immediately. If the category
// is not enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_END0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, \
        arg2_name, arg2_val)
#define TRACE_EVENT_COPY_END0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Records the value of a counter called "name" immediately. Value
// must be representable as a 32 bit integer.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_COUNTER1(category, name, value) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, \
        category, name, TRACE_EVENT_FLAG_NONE, \
        "value", static_cast<int>(value))
#define TRACE_COPY_COUNTER1(category, name, value) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, \
        category, name, TRACE_EVENT_FLAG_COPY, \
        "value", static_cast<int>(value))

// Records the values of a multi-parted counter called "name" immediately.
// The UI will treat value1 and value2 as parts of a whole, displaying their
// values as a stacked-bar chart.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, \
        category, name, TRACE_EVENT_FLAG_NONE, \
        value1_name, static_cast<int>(value1_val), \
        value2_name, static_cast<int>(value2_val))
#define TRACE_COPY_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, \
        category, name, TRACE_EVENT_FLAG_COPY, \
        value1_name, static_cast<int>(value1_val), \
        value2_name, static_cast<int>(value2_val))

// Records the value of a counter called "name" immediately. Value
// must be representable as a 32 bit integer.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to disambiguate counters with the same name. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_COUNTER_ID1(category, name, id, value) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, \
        category, name, id, TRACE_EVENT_FLAG_NONE, \
        "value", static_cast<int>(value))
#define TRACE_COPY_COUNTER_ID1(category, name, id, value) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        "value", static_cast<int>(value))

// Records the values of a multi-parted counter called "name" immediately.
// The UI will treat value1 and value2 as parts of a whole, displaying their
// values as a stacked-bar chart.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to disambiguate counters with the same name. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_COUNTER_ID2(category, name, id, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, \
        category, name, id, TRACE_EVENT_FLAG_NONE, \
        value1_name, static_cast<int>(value1_val), \
        value2_name, static_cast<int>(value2_val))
#define TRACE_COPY_COUNTER_ID2(category, name, id, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        value1_name, static_cast<int>(value1_val), \
        value2_name, static_cast<int>(value2_val))


// Records a single ASYNC_BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to match the ASYNC_BEGIN event with the ASYNC_END event. ASYNC
//   events are considered to match if their category, name and id values all
//   match. |id| must either be a pointer or an integer value up to 64 bits. If
//   it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
// An asynchronous operation can consist of multiple phases. The first phase is
// defined by the ASYNC_BEGIN calls. Additional phases can be defined using the
// ASYNC_STEP macros. When the operation completes, call ASYNC_END.
// An ASYNC trace typically occur on a single thread (if not, they will only be
// drawn on the thread defined in the ASYNC_BEGIN event), but all events in that
// operation must use the same |name| and |id|. Each event can have its own
// args.
#define TRACE_EVENT_ASYNC_BEGIN0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_NONE, \
        arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val, arg2_name, arg2_val)

// Records a single ASYNC_STEP event for |step| immediately. If the category
// is not enabled, then this does nothing. The |name| and |id| must match the
// ASYNC_BEGIN event above. The |step| param identifies this step within the
// async event. This should be called at the beginning of the next phase of an
// asynchronous operation.
#define TRACE_EVENT_ASYNC_STEP0(category, name, id, step) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, \
        category, name, id, TRACE_EVENT_FLAG_NONE, "step", step)
#define TRACE_EVENT_ASYNC_STEP1(category, name, id, step, \
                                      arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, \
        category, name, id, TRACE_EVENT_FLAG_NONE, "step", step, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_ASYNC_STEP0(category, name, id, step) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, \
        category, name, id, TRACE_EVENT_FLAG_COPY, "step", step)
#define TRACE_EVENT_COPY_ASYNC_STEP1(category, name, id, step, \
        arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, \
        category, name, id, TRACE_EVENT_FLAG_COPY, "step", step, \
        arg1_name, arg1_val)

// Records a single ASYNC_END event for "name" immediately. If the category
// is not enabled, then this does nothing.
#define TRACE_EVENT_ASYNC_END0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
        category, name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_END1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
        category, name, id, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_ASYNC_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
        category, name, id, TRACE_EVENT_FLAG_NONE, \
        arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_END0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
        category, name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_ASYNC_END1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_ASYNC_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val, arg2_name, arg2_val)


// Records a single FLOW_BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to match the FLOW_BEGIN event with the FLOW_END event. FLOW
//   events are considered to match if their category, name and id values all
//   match. |id| must either be a pointer or an integer value up to 64 bits. If
//   it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
// FLOW events are different from ASYNC events in how they are drawn by the
// tracing UI. A FLOW defines asynchronous data flow, such as posting a task
// (FLOW_BEGIN) and later executing that task (FLOW_END). Expect FLOWs to be
// drawn as lines or arrows from FLOW_BEGIN scopes to FLOW_END scopes. Similar
// to ASYNC, a FLOW can consist of multiple phases. The first phase is defined
// by the FLOW_BEGIN calls. Additional phases can be defined using the FLOW_STEP
// macros. When the operation completes, call FLOW_END. An async operation can
// span threads and processes, but all events in that operation must use the
// same |name| and |id|. Each event can have its own args.
#define TRACE_EVENT_FLOW_BEGIN0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_FLOW_BEGIN1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_FLOW_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_NONE, \
        arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_FLOW_BEGIN0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_FLOW_BEGIN1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_FLOW_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val, arg2_name, arg2_val)

// Records a single FLOW_STEP event for |step| immediately. If the category
// is not enabled, then this does nothing. The |name| and |id| must match the
// FLOW_BEGIN event above. The |step| param identifies this step within the
// async event. This should be called at the beginning of the next phase of an
// asynchronous operation.
#define TRACE_EVENT_FLOW_STEP0(category, name, id, step) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_STEP, \
        category, name, id, TRACE_EVENT_FLAG_NONE, "step", step)
#define TRACE_EVENT_FLOW_STEP1(category, name, id, step, \
        arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_STEP, \
        category, name, id, TRACE_EVENT_FLAG_NONE, "step", step, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_FLOW_STEP0(category, name, id, step) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_STEP, \
        category, name, id, TRACE_EVENT_FLAG_COPY, "step", step)
#define TRACE_EVENT_COPY_FLOW_STEP1(category, name, id, step, \
        arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_STEP, \
        category, name, id, TRACE_EVENT_FLAG_COPY, "step", step, \
        arg1_name, arg1_val)

// Records a single FLOW_END event for "name" immediately. If the category
// is not enabled, then this does nothing.
#define TRACE_EVENT_FLOW_END0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, \
        category, name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_FLOW_END1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, \
        category, name, id, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_FLOW_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, \
        category, name, id, TRACE_EVENT_FLAG_NONE, \
        arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_FLOW_END0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, \
        category, name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_FLOW_END1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_FLOW_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, \
        category, name, id, TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val, arg2_name, arg2_val)


////////////////////////////////////////////////////////////////////////////////
// Implementation specific tracing API definitions.

// Get a pointer to the enabled state of the given trace category. Only
// long-lived literal strings should be given as the category name. The returned
// pointer can be held permanently in a local static for example. If the
// unsigned char is non-zero, tracing is enabled. If tracing is enabled,
// TRACE_EVENT_API_ADD_TRACE_EVENT can be called. It's OK if tracing is disabled
// between the load of the tracing state and the call to
// TRACE_EVENT_API_ADD_TRACE_EVENT, because this flag only provides an early out
// for best performance when tracing is disabled.
// const unsigned char*
//     TRACE_EVENT_API_GET_CATEGORY_ENABLED(const char* category_name)
#define TRACE_EVENT_API_GET_CATEGORY_ENABLED \
    webrtc::EventTracer::GetCategoryEnabled

// Add a trace event to the platform tracing system.
// void TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    char phase,
//                    const unsigned char* category_enabled,
//                    const char* name,
//                    unsigned long long id,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    unsigned char flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT webrtc::EventTracer::AddTraceEvent

////////////////////////////////////////////////////////////////////////////////

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collissions.
#define INTERNAL_TRACE_EVENT_UID3(a,b) \
    trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a,b) \
    INTERNAL_TRACE_EVENT_UID3(a,b)
#define INTERNAL_TRACE_EVENT_UID(name_prefix) \
    INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

#if WEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS
#define INTERNAL_TRACE_EVENT_INFO_TYPE const unsigned char*
#else
#define INTERNAL_TRACE_EVENT_INFO_TYPE static const unsigned char*
#endif  // WEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS

// Implementation detail: internal macro to create static category.
#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category) \
    INTERNAL_TRACE_EVENT_INFO_TYPE INTERNAL_TRACE_EVENT_UID(catstatic) = \
        TRACE_EVENT_API_GET_CATEGORY_ENABLED(category);

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD(phase, category, name, flags, ...) \
  MOZ_INTERNAL_UPROFILER_SIMPLE_EVENT(name, category, phase)

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, ...) \
  MOZ_INTERNAL_UPROFILER_AUTO_TRACE(category, name)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category, name, id, flags, \
                                         ...)                              \
  MOZ_INTERNAL_UPROFILER_SIMPLE_EVENT(name, category, phase)

#ifdef MOZ_GECKO_PROFILER
#define MOZ_INTERNAL_UPROFILER_SIMPLE_EVENT(name, category, phase) \
  uprofiler_simple_event_marker(name, category, phase);
#define MOZ_INTERNAL_UPROFILER_AUTO_TRACE(category, name) \
  AutoTrace INTERNAL_TRACE_EVENT_UID(trace)(name, category);
#else
#define MOZ_INTERNAL_UPROFILER_SIMPLE_EVENT(name, category, phase)
#define MOZ_INTERNAL_UPROFILER_AUTO_TRACE(name, category)
#endif

// Notes regarding the following definitions:
// New values can be added and propagated to third party libraries, but existing
// definitions must never be changed, because third party libraries may use old
// definitions.

// Phase indicates the nature of an event entry. E.g. part of a begin/end pair.
#define TRACE_EVENT_PHASE_BEGIN    ('B')
#define TRACE_EVENT_PHASE_END      ('E')
#define TRACE_EVENT_PHASE_INSTANT  ('I')
#define TRACE_EVENT_PHASE_ASYNC_BEGIN ('S')
#define TRACE_EVENT_PHASE_ASYNC_STEP  ('T')
#define TRACE_EVENT_PHASE_ASYNC_END   ('F')
#define TRACE_EVENT_PHASE_FLOW_BEGIN ('s')
#define TRACE_EVENT_PHASE_FLOW_STEP  ('t')
#define TRACE_EVENT_PHASE_FLOW_END   ('f')
#define TRACE_EVENT_PHASE_METADATA ('M')
#define TRACE_EVENT_PHASE_COUNTER  ('C')

// Flags for changing the behavior of TRACE_EVENT_API_ADD_TRACE_EVENT.
#define TRACE_EVENT_FLAG_NONE        (static_cast<unsigned char>(0))
#define TRACE_EVENT_FLAG_COPY        (static_cast<unsigned char>(1 << 0))
#define TRACE_EVENT_FLAG_HAS_ID      (static_cast<unsigned char>(1 << 1))
#define TRACE_EVENT_FLAG_MANGLE_ID   (static_cast<unsigned char>(1 << 2))

#else

////////////////////////////////////////////////////////////////////////////////
// This section defines no-op alternatives to the tracing macros when
// RTC_DISABLE_TRACE_EVENTS is defined.

#define RTC_NOOP() do {} while (0)

#define TRACE_STR_COPY(str) RTC_NOOP()

#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

#define TRACE_ID_MANGLE(id) 0

#define TRACE_EVENT0(category, name) RTC_NOOP()
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    RTC_NOOP()

#define TRACE_EVENT_INSTANT0(category, name) RTC_NOOP()
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) RTC_NOOP()

#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_COPY_INSTANT0(category, name) RTC_NOOP()
#define TRACE_EVENT_COPY_INSTANT1(category, name, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_BEGIN0(category, name) RTC_NOOP()
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()
#define TRACE_EVENT_COPY_BEGIN0(category, name) RTC_NOOP()
#define TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_END0(category, name) RTC_NOOP()
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()
#define TRACE_EVENT_COPY_END0(category, name) RTC_NOOP()
#define TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_COUNTER1(category, name, value) RTC_NOOP()
#define TRACE_COPY_COUNTER1(category, name, value) RTC_NOOP()

#define TRACE_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) RTC_NOOP()
#define TRACE_COPY_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) RTC_NOOP()

#define TRACE_COUNTER_ID1(category, name, id, value) RTC_NOOP()
#define TRACE_COPY_COUNTER_ID1(category, name, id, value) RTC_NOOP()

#define TRACE_COUNTER_ID2(category, name, id, value1_name, value1_val, \
        value2_name, value2_val) RTC_NOOP()
#define TRACE_COPY_COUNTER_ID2(category, name, id, value1_name, value1_val, \
        value2_name, value2_val) RTC_NOOP()

#define TRACE_EVENT_ASYNC_BEGIN0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_BEGIN0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_ASYNC_STEP0(category, name, id, step) RTC_NOOP()
#define TRACE_EVENT_ASYNC_STEP1(category, name, id, step, \
                                      arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_STEP0(category, name, id, step) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_STEP1(category, name, id, step, \
        arg1_name, arg1_val) RTC_NOOP()

#define TRACE_EVENT_ASYNC_END0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_ASYNC_END1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_ASYNC_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_END0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_END1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_FLOW_BEGIN0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_FLOW_BEGIN1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_FLOW_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_BEGIN0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_BEGIN1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_BEGIN2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_FLOW_STEP0(category, name, id, step) RTC_NOOP()
#define TRACE_EVENT_FLOW_STEP1(category, name, id, step, \
        arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_STEP0(category, name, id, step) RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_STEP1(category, name, id, step, \
        arg1_name, arg1_val) RTC_NOOP()

#define TRACE_EVENT_FLOW_END0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_FLOW_END1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_FLOW_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_END0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_END1(category, name, id, arg1_name, arg1_val) \
    RTC_NOOP()
#define TRACE_EVENT_COPY_FLOW_END2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) RTC_NOOP()

#define TRACE_EVENT_API_GET_CATEGORY_ENABLED ""

#define TRACE_EVENT_API_ADD_TRACE_EVENT RTC_NOOP()

#endif  // RTC_TRACE_EVENTS_ENABLED

#endif  // GECKO_TRACE_EVENT_H_
