/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Visual event tracer, creates a log of events on each thread for visualization */

/**
 * The event tracer code is by default disabled in both build and run time.
 *
 * To enable this code at build time, add --enable-visual-profiling to your
 * configure options.
 *
 * To enable this code at run time, export MOZ_TRACE_FILE env var with a
 * path to the file to write the log to. This is all you need to * produce
 * the log of all events instrumentation in the mozilla code. Check
 * MOZ_EVENT_TRACER_* macros below to add your own.
 *
 * To let the event tracer log only some events to save disk space, export
 * MOZ_PROFILING_EVENTS with comma separated list of event names you want
 * to record in the log.
 */

#ifndef VisualEventTracer_h___
#define VisualEventTracer_h___

#include <stdint.h>
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"

#ifdef MOZ_VISUAL_EVENT_TRACER
#include "nsIVisualEventTracer.h"

// Bind an object instance, usually |this|, to a name, usually URL or
// host name, the instance deals with for its lifetime.  The name string
// is duplicated.
// IMPORTANT: it is up to the caller to pass the correct static_cast
// of the |instance| pointer to all these macros ; otherwise the linking
// of events and objects will not work!
// The name will show in details of the events on the timeline and also
// will allow events filtering by host names, URLs etc.
#define MOZ_EVENT_TRACER_NAME_OBJECT(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eName, instance, name)

// The same as MOZ_EVENT_TRACER_NAME_OBJECT, just simplifies building
// names when it consists of two parts
#define MOZ_EVENT_TRACER_COMPOUND_NAME(instance, name, name2) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eName, instance, name, name2)


// Call the followings with the same |instance| reference as you have
// previously called MOZ_EVENT_TRACER_NAME_OBJECT.
// Let |name| be the name of the event happening, like "resolving",
// "connecting", "loading" etc.

// This will crate a single-point-in-time event marked with an arrow
// on the timeline, this is a way to indicate an event without a duration.
#define MOZ_EVENT_TRACER_MARK(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eShot, instance, name)

// Following macros are used to log events with duration.
// There always has to be complete WAIT,EXEC,DONE or EXEC,DONE
// uninterrupted and non-interferring tuple(s) for an event to be correctly
// shown on the timeline.  Each can be called on any thread, the event tape is
// finally displayed on the thread where it has been EXECuted.

// Example of 3 phases usage for "HTTP request channel":
// WAIT: we've just created the channel and called AsyncOpen on it
// EXEC: we've just got first call to OnDataAvailable, so we are actually
//       receiving the content after some time like connection establising,
//       opening a cache entry, sending the GET request...
// DONE: we've just got OnStopRequest call that indicates the content
//       has been completely delivered and the request is now finished

// Indicate an event pending start, on the timeline this will
// start the event's interval tape with a pale color, the time will
// show in details.  Usually used when an event is being posted or
// we wait for a lock acquisition.
// WAITING is not mandatory, some events don't have a pending phase.
#define MOZ_EVENT_TRACER_WAIT(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eWait, instance, name)

// Indicate start of an event actual execution, on the timeline this will
// change the event's tape to a deeper color, the time will show in details.
#define MOZ_EVENT_TRACER_EXEC(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eExec, instance, name)

// Indicate the end of an event execution (the event has been done),
// on the timeline this will end the event's tape and show the time in
// event details.
// NOTE: when the event duration is extremely short, like 1ms, it will convert
// to an event w/o a duration - the same as MOZ_EVENT_TRACER_MARK would be used.
#define MOZ_EVENT_TRACER_DONE(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eDone, instance, name)

// The same meaning as the above macros, just for concurent events.
// Concurent event means it can happen for the same instance on more
// then just a single thread, e.g. a service method call, a global lock
// acquisition, enter and release.
#define MOZ_EVENT_TRACER_WAIT_THREADSAFE(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eWait | mozilla::eventtracer::eThreadConcurrent, instance, name)
#define MOZ_EVENT_TRACER_EXEC_THREADSAFE(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eExec | mozilla::eventtracer::eThreadConcurrent, instance, name)
#define MOZ_EVENT_TRACER_DONE_THREASAFE(instance, name) \
  mozilla::eventtracer::Mark(mozilla::eventtracer::eDone | mozilla::eventtracer::eThreadConcurrent, instance, name)

#else

// MOZ_VISUAL_EVENT_TRACER disabled

#define MOZ_EVENT_TRACER_NAME_OBJECT(instance, name) (void)0
#define MOZ_EVENT_TRACER_COMPOUND_NAME(instance, name, name2) (void)0
#define MOZ_EVENT_TRACER_MARK(instance, name) (void)0
#define MOZ_EVENT_TRACER_WAIT(instance, name) (void)0
#define MOZ_EVENT_TRACER_EXEC(instance, name) (void)0
#define MOZ_EVENT_TRACER_DONE(instance, name) (void)0
#define MOZ_EVENT_TRACER_WAIT_THREADSAFE(instance, name) (void)0
#define MOZ_EVENT_TRACER_EXEC_THREADSAFE(instance, name) (void)0
#define MOZ_EVENT_TRACER_DONE_THREASAFE(instance, name) (void)0

#endif


namespace mozilla {
namespace eventtracer {

// Initialize the event tracer engine, called automatically on XPCOM startup.
void Init();

// Shuts the event tracer engine down and closes the log file, called
// automatically during XPCOM shutdown.
void Shutdown();

enum MarkType {
  eNone, // No operation, ignored
  eName, // This is used to bind an object instance with a name

  eShot, // An event with no duration
  eWait, // Start of waiting for execution (lock acquire, post...)
  eExec, // Start of the execution it self
  eDone, // End of the execution
  eLast, // Sentinel

  // Flags

  // The same object can execute the same event on several threads concurently
  eThreadConcurrent = 0x10000
};

// Records an event on the calling thread.
// @param aType
//    One of MarkType fields, can be bitwise or'ed with the flags.
// @param aItem
//    Reference to the object we want to bind a name to or the event is
//    happening on.  Can be actually anything, but valid poitners should
//    be used.
// @param aText
//    Text of the name (for eName) or event's name for others.  The string
//    is duplicated.
// @param aText2
//    Optional second part of the instnace name, or event name.
//    Event filtering does apply only to the first part (aText).
void Mark(uint32_t aType, void* aItem,
          const char* aText, const char* aText2 = 0);


// Helper guard object.  Use to mark an event in the constructor and a different
// event in the destructor.
//
// Example:
// int class::func()
// {
//    AutoEventTracer tracer(this, eventtracer::eExec, eventtracer::eDone, "func");
//
//    do_something_taking_a_long_time();
// }
class MOZ_STACK_CLASS AutoEventTracer
{
public:
  AutoEventTracer(void* aInstance,
                  uint32_t aTypeOn, // MarkType marked in constructor
                  uint32_t aTypeOff, // MarkType marked in destructor
                  const char* aName,
                  const char* aName2 = 0
                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mInstance(aInstance)
    , mName(aName)
    , mName2(aName2)
    , mTypeOn(aTypeOn)
    , mTypeOff(aTypeOff)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    ::mozilla::eventtracer::Mark(mTypeOn, mInstance, mName, mName2);
  }

  ~AutoEventTracer()
  {
    ::mozilla::eventtracer::Mark(mTypeOff, mInstance, mName, mName2);
  }

private:
  void* mInstance;
  const char* mName;
  const char* mName2;
  uint32_t mTypeOn;
  uint32_t mTypeOff;

  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#ifdef MOZ_VISUAL_EVENT_TRACER

// The scriptable class that drives the event tracer

class VisualEventTracer : public nsIVisualEventTracer
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVISUALEVENTTRACER
};

#define NS_VISUALEVENTTRACER_CID \
  { 0xb9e5e102, 0xc2f4, 0x497a,  \
    { 0xa6, 0xe4, 0x54, 0xde, 0xf3, 0x71, 0xf3, 0x9d } }
#define NS_VISUALEVENTTRACER_CONTRACTID "@mozilla.org/base/visual-event-tracer;1"
#define NS_VISUALEVENTTRACER_CLASSNAME "Visual Event Tracer"

#endif

} // eventtracer
} // mozilla

#endif /* VisualEventTracer_h___ */
