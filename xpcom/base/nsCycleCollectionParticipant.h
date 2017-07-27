/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectionParticipant_h__
#define nsCycleCollectionParticipant_h__

#include "mozilla/Assertions.h"
#include "mozilla/MacroArgs.h"
#include "mozilla/MacroForEach.h"
#include "nsCycleCollectionNoteChild.h"
#include "js/RootingAPI.h"

/**
 * Note: the following two IIDs only differ in one bit in the last byte.  This
 * is a hack and is intentional in order to speed up the comparison inside
 * NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED.
 */
#define NS_XPCOMCYCLECOLLECTIONPARTICIPANT_IID                                 \
{                                                                              \
    0xc61eac14,                                                                \
    0x5f7a,                                                                    \
    0x4481,                                                                    \
    { 0x96, 0x5e, 0x7e, 0xaa, 0x6e, 0xff, 0xa8, 0x5e }                         \
}

/**
 * Special IID to get at the base nsISupports for a class. Usually this is the
 * canonical nsISupports pointer, but in the case of tearoffs for example it is
 * the base nsISupports pointer of the tearoff. This allow the cycle collector
 * to have separate nsCycleCollectionParticipant's for tearoffs or aggregated
 * classes.
 */
#define NS_CYCLECOLLECTIONISUPPORTS_IID                                        \
{                                                                              \
    0xc61eac14,                                                                \
    0x5f7a,                                                                    \
    0x4481,                                                                    \
    { 0x96, 0x5e, 0x7e, 0xaa, 0x6e, 0xff, 0xa8, 0x5f }                         \
}

/**
 * Just holds the IID so NS_GET_IID works.
 */
class nsCycleCollectionISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CYCLECOLLECTIONISUPPORTS_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCycleCollectionISupports,
                              NS_CYCLECOLLECTIONISUPPORTS_IID)

namespace JS {
template<class T> class Heap;
} /* namespace JS */

/*
 * A struct defining pure virtual methods which are called when tracing cycle
 * collection paticipants.  The appropriate method is called depending on the
 * type of JS GC thing.
 */
struct TraceCallbacks
{
  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JS::Heap<jsid>* aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JSObject** aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                     void* aClosure) const = 0;
  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                     void* aClosure) const = 0;
};

/*
 * An implementation of TraceCallbacks that calls a single function for all JS
 * GC thing types encountered. Implemented in nsCycleCollectorTraceJSHelpers.cpp.
 */
struct TraceCallbackFunc : public TraceCallbacks
{
  typedef void (*Func)(JS::GCCellPtr aPtr, const char* aName, void* aClosure);

  explicit TraceCallbackFunc(Func aCb) : mCallback(aCb) {}

  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JS::Heap<jsid>* aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JSObject** aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                     void* aClosure) const override;
  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                     void* aClosure) const override;

private:
  Func mCallback;
};

/**
 * Participant implementation classes
 */
class NS_NO_VTABLE nsCycleCollectionParticipant
{
public:
  constexpr explicit nsCycleCollectionParticipant(bool aSkip,
                                                  bool aTraverseShouldTrace = false)
    : mMightSkip(aSkip)
    , mTraverseShouldTrace(aTraverseShouldTrace)
  {
  }

  NS_IMETHOD TraverseNative(void* aPtr, nsCycleCollectionTraversalCallback& aCb) = 0;

  nsresult TraverseNativeAndJS(void* aPtr,
                               nsCycleCollectionTraversalCallback& aCb)
  {
    nsresult rv = TraverseNative(aPtr, aCb);
    if (mTraverseShouldTrace) {
      // Note, we always call Trace, even if Traverse returned
      // NS_SUCCESS_INTERRUPTED_TRAVERSE.
      TraceCallbackFunc noteJsChild(&nsCycleCollectionParticipant::NoteJSChild);
      Trace(aPtr, noteJsChild, &aCb);
    }
    return rv;
  }

    // Implemented in nsCycleCollectorTraceJSHelpers.cpp.
  static void NoteJSChild(JS::GCCellPtr aGCThing, const char* aName,
                          void* aClosure);

  NS_IMETHOD_(void) Root(void* aPtr) = 0;
  NS_IMETHOD_(void) Unlink(void* aPtr) = 0;
  NS_IMETHOD_(void) Unroot(void* aPtr) = 0;
  NS_IMETHOD_(const char*) ClassName() = 0;

  NS_IMETHOD_(void) Trace(void* aPtr, const TraceCallbacks& aCb,
                          void* aClosure) {}

  // CanSkip is called during nsCycleCollector_forgetSkippable.  If it returns
  // true, aPtr is removed from the purple buffer and therefore might be left
  // out from the cycle collector graph the next time that's constructed (unless
  // it's reachable in some other way).
  //
  // CanSkip is allowed to expand the set of certainly-alive objects by removing
  // other objects from the purple buffer, marking JS things black (in the GC
  // sense), and so forth.  Furthermore, if aRemovingAllowed is true, this call
  // is allowed to remove aPtr itself from the purple buffer.
  //
  // Things can return true from CanSkip if either they know they have no
  // outgoing edges at all in the cycle collection graph (because then they
  // can't be parts of a cycle) or they know for sure they're alive.
  bool CanSkip(void* aPtr, bool aRemovingAllowed)
  {
    return mMightSkip ? CanSkipReal(aPtr, aRemovingAllowed) : false;
  }

  // CanSkipInCC is called during construction of the initial set of roots for
  // the cycle collector graph.  If it returns true, aPtr is left out of that
  // set of roots.  Note that the set of roots includes whatever is in the
  // purple buffer (after earlier CanSkip calls) plus various other sources of
  // roots, so an object can end up having CanSkipInCC called on it even if it
  // returned true from CanSkip.  One example of this would be an object that
  // can potentially trace JS things.
  //
  // CanSkipInCC is allowed to remove other objects from the purple buffer but
  // should not remove aPtr and should not mark JS things black.  It should also
  // not modify any reference counts.
  //
  // Things can return true from CanSkipInCC if either they know they have no
  // outgoing edges at all in the cycle collection graph or they know for sure
  // they're alive _and_ none of their outgoing edges are to gray (in the GC
  // sense) gcthings.  See also nsWrapperCache::HasNothingToTrace and
  // nsWrapperCache::IsBlackAndDoesNotNeedTracing.  The restriction on not
  // having outgoing edges to gray gcthings is because if we _do_ have them that
  // means we have a "strong" edge to a JS thing and since we're alive we need
  // to trace through it and mark keep them alive.  Outgoing edges to C++ things
  // don't matter here, because the criteria for when a CC participant is
  // considered alive are slightly different for JS and C++ things: JS things
  // are only considered alive when reachable via an edge from a live thing,
  // while C++ things are also considered alive when their refcount exceeds the
  // number of edges via which they are reachable.
  bool CanSkipInCC(void* aPtr)
  {
    return mMightSkip ? CanSkipInCCReal(aPtr) : false;
  }

  // CanSkipThis is called during construction of the cycle collector graph,
  // when we traverse an edge to aPtr and consider adding it to the graph.  If
  // it returns true, aPtr is not added to the graph.
  //
  // CanSkipThis is not allowed to change the liveness or reference count of any
  // objects.
  //
  // Things can return true from CanSkipThis if either they know they have no
  // outgoing edges at all in the cycle collection graph or they know for sure
  // they're alive.
  //
  // Note that CanSkipThis doesn't have to worry about outgoing edges to gray GC
  // things, because if this object could have those it already got added to the
  // graph during root set construction.  An object should never have
  // CanSkipThis called on it if it has outgoing strong references to JS things.
  bool CanSkipThis(void* aPtr)
  {
    return mMightSkip ? CanSkipThisReal(aPtr) : false;
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void* aPtr) = 0;

protected:
  NS_IMETHOD_(bool) CanSkipReal(void* aPtr, bool aRemovingAllowed)
  {
    NS_ASSERTION(false, "Forgot to implement CanSkipReal?");
    return false;
  }
  NS_IMETHOD_(bool) CanSkipInCCReal(void* aPtr)
  {
    NS_ASSERTION(false, "Forgot to implement CanSkipInCCReal?");
    return false;
  }
  NS_IMETHOD_(bool) CanSkipThisReal(void* aPtr)
  {
    NS_ASSERTION(false, "Forgot to implement CanSkipThisReal?");
    return false;
  }

private:
  const bool mMightSkip;
  const bool mTraverseShouldTrace;
};

class NS_NO_VTABLE nsScriptObjectTracer : public nsCycleCollectionParticipant
{
public:
  constexpr explicit nsScriptObjectTracer(bool aSkip)
    : nsCycleCollectionParticipant(aSkip, true)
  {
  }

  NS_IMETHOD_(void) Trace(void* aPtr, const TraceCallbacks& aCb,
                          void* aClosure) override = 0;

};

class NS_NO_VTABLE nsXPCOMCycleCollectionParticipant : public nsScriptObjectTracer
{
public:
  constexpr explicit nsXPCOMCycleCollectionParticipant(bool aSkip)
    : nsScriptObjectTracer(aSkip)
  {
  }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XPCOMCYCLECOLLECTIONPARTICIPANT_IID)

  NS_IMETHOD_(void) Root(void* aPtr) override;
  NS_IMETHOD_(void) Unroot(void* aPtr) override;

  NS_IMETHOD_(void) Trace(void* aPtr, const TraceCallbacks& aCb,
                          void* aClosure) override;

  static bool CheckForRightISupports(nsISupports* aSupports);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXPCOMCycleCollectionParticipant,
                              NS_XPCOMCYCLECOLLECTIONPARTICIPANT_IID)

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing a QI to nsXPCOMCycleCollectionParticipant
///////////////////////////////////////////////////////////////////////////////

#define NS_CYCLE_COLLECTION_CLASSNAME(_class)                                  \
        _class::NS_CYCLE_COLLECTION_INNERCLASS

#define NS_IMPL_QUERY_CYCLE_COLLECTION(_class)                                 \
  if ( aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant)) ) {          \
    *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(_class);                   \
    return NS_OK;                                                              \
  } else

#define NS_IMPL_QUERY_CYCLE_COLLECTION_ISUPPORTS(_class)                       \
  if ( aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ) {                 \
    *aInstancePtr = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);       \
    return NS_OK;                                                              \
  } else

#define NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION_ISUPPORTS(_class)              \
  NS_IMPL_QUERY_CYCLE_COLLECTION_ISUPPORTS(_class)

#define NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)                      \
  if (TopThreeWordsEquals(aIID, NS_GET_IID(nsXPCOMCycleCollectionParticipant), \
                                NS_GET_IID(nsCycleCollectionISupports))) {     \
    if (LowWordEquals(aIID, NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {  \
      *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(_class);                 \
      return NS_OK;                                                            \
    }                                                                          \
    MOZ_ASSERT(LowWordEquals(aIID, NS_GET_IID(nsCycleCollectionISupports)));   \
    *aInstancePtr = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);       \
    return NS_OK;                                                              \
  }

#define NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(_class)                        \
  NS_INTERFACE_MAP_BEGIN(_class)                                               \
    NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(_class)              \
  NS_INTERFACE_MAP_BEGIN(_class)                                               \
    NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(_class)  \
  if (rv == NS_OK) return rv; \
  nsISupports* foundInterface; \
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)

// The IIDs for nsXPCOMCycleCollectionParticipant and nsCycleCollectionISupports
// are special in that they only differ in their last byte.  This allows for the
// optimization below where we first check the first three words of the IID and
// if we find a match we check the last word to decide which case we have to
// deal with.
// It would be nice to have a similar optimization for the
// NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED case above as well, but
// NS_TableDrivenQI isn't aware of this special case so we don't have an easy
// option there.
#define NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(_class)            \
  NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)    \
  {                                                                           \
    NS_PRECONDITION(aInstancePtr, "null out param");                          \
                                                                              \
    if (TopThreeWordsEquals(aIID, NS_GET_IID(nsXPCOMCycleCollectionParticipant), \
                                  NS_GET_IID(nsCycleCollectionISupports))) {  \
      if (LowWordEquals(aIID, NS_GET_IID(nsXPCOMCycleCollectionParticipant))) { \
        *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(_class);              \
        return NS_OK;                                                         \
      }                                                                       \
      MOZ_ASSERT(LowWordEquals(aIID, NS_GET_IID(nsCycleCollectionISupports)));\
      *aInstancePtr = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);    \
      return NS_OK;                                                           \
    }                                                                         \
    nsresult rv;

#define NS_CYCLE_COLLECTION_UPCAST(obj, clazz)                                \
  NS_CYCLE_COLLECTION_CLASSNAME(clazz)::Upcast(obj)

#ifdef DEBUG
#define NS_CHECK_FOR_RIGHT_PARTICIPANT(_ptr) _ptr->CheckForRightParticipant()
#else
#define NS_CHECK_FOR_RIGHT_PARTICIPANT(_ptr)
#endif

// The default implementation of this class template is empty, because it
// should never be used: see the partial specializations below.
template<typename T,
         bool IsXPCOM = mozilla::IsBaseOf<nsISupports, T>::value>
struct DowncastCCParticipantImpl
{
};

// Specialization for XPCOM CC participants
template<typename T>
struct DowncastCCParticipantImpl<T, true>
{
  static T* Run(void* aPtr)
  {
    nsISupports* s = static_cast<nsISupports*>(aPtr);
    MOZ_ASSERT(NS_CYCLE_COLLECTION_CLASSNAME(T)::CheckForRightISupports(s),
               "not the nsISupports pointer we expect");
    T* rval =  NS_CYCLE_COLLECTION_CLASSNAME(T)::Downcast(s);
    NS_CHECK_FOR_RIGHT_PARTICIPANT(rval);
    return rval;
  }
};

// Specialization for native CC participants
template<typename T>
struct DowncastCCParticipantImpl<T, false>
{
  static T* Run(void* aPtr) { return static_cast<T*>(aPtr); }
};

template<typename T>
T*
DowncastCCParticipant(void* aPtr)
{
  return DowncastCCParticipantImpl<T>::Run(aPtr);
}

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing CanSkip methods
///////////////////////////////////////////////////////////////////////////////

// See documentation for nsCycleCollectionParticipant::CanSkip for documentation
// about this method.
#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(_class)                        \
  NS_IMETHODIMP_(bool)                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::CanSkipReal(void *p,                  \
                                                     bool aRemovingAllowed)    \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END                                  \
    (void)tmp;                                                                 \
    return false;                                                              \
  }

// See documentation for nsCycleCollectionParticipant::CanSkipInCC for
// documentation about this method.
#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(_class)                  \
  NS_IMETHODIMP_(bool)                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::CanSkipInCCReal(void *p)              \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END                            \
    (void)tmp;                                                                 \
    return false;                                                              \
  }

// See documentation for nsCycleCollectionParticipant::CanSkipThis for
// documentation about this method.
#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(_class)                   \
  NS_IMETHODIMP_(bool)                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::CanSkipThisReal(void *p)              \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END                             \
    (void)tmp;                                                                 \
    return false;                                                              \
  }

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing nsCycleCollectionParticipant::Unlink
//
// You need to use NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED if you want
// the base class Unlink version to be called before your own implementation.
// You can use NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED if you want the
// base class Unlink to get called after your own implementation.  You should
// never use them together.
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                          \
  NS_IMETHODIMP_(void)                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Unlink(void *p)                       \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base_class)   \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::Unlink(s);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_HELPER(_field)                        \
  ImplCycleCollectionUnlink(tmp->_field);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK(...)                                   \
  MOZ_FOR_EACH(NS_IMPL_CYCLE_COLLECTION_UNLINK_HELPER, (), (__VA_ARGS__))

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                    \
    (void)tmp;                                                                 \
  }

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(_base_class)             \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::Unlink(s);                     \
    (void)tmp;                                                                 \
  }

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_0(_class)                              \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END


///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing nsCycleCollectionParticipant::Traverse
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_DESCRIBE(_class, _refcnt)                     \
    cb.DescribeRefCountedNode(_refcnt, #_class);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(_class)               \
  NS_IMETHODIMP                                                                \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::TraverseNative                        \
                         (void *p, nsCycleCollectionTraversalCallback &cb)     \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                        \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(_class)                     \
  NS_IMPL_CYCLE_COLLECTION_DESCRIBE(_class, tmp->mRefCnt.get())

// Base class' CC participant should return NS_SUCCESS_INTERRUPTED_TRAVERSE
// from Traverse if it wants derived classes to not traverse anything from
// their CC participant.

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base_class) \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(_class)                     \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    if (NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::TraverseNative(s, cb)      \
        == NS_SUCCESS_INTERRUPTED_TRAVERSE) {                                  \
      return NS_SUCCESS_INTERRUPTED_TRAVERSE;                                  \
    }

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_HELPER(_field)                       \
  ImplCycleCollectionTraverse(cb, tmp->_field, #_field, 0);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE(...)                                 \
  MOZ_FOR_EACH(NS_IMPL_CYCLE_COLLECTION_TRAVERSE_HELPER, (), (__VA_ARGS__))

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(_field)                       \
  CycleCollectionNoteChild(cb, tmp->_field, #_field);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                  \
    (void)tmp;                                                                 \
    return NS_OK;                                                              \
  }

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing nsScriptObjectTracer::Trace
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_class)                                 \
  void                                                                               \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Trace(void *p,                              \
                                               const TraceCallbacks &aCallbacks,     \
                                               void *aClosure)                       \
  {                                                                                  \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(_class, _base_class)    \
  NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_class)                                 \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::Trace(s, aCallbacks, aClosure);

#define NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(_field)              \
  aCallbacks.Trace(&tmp->_field, #_field, aClosure);

// NB: The (void)tmp; hack in the TRACE_END macro exists to support
// implementations that don't need to do anything in their Trace method.
// Without this hack, some compilers warn about the unused tmp local.
#define NS_IMPL_CYCLE_COLLECTION_TRACE_END                                     \
      (void)tmp;                                                               \
  }

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing a concrete nsCycleCollectionParticipant
///////////////////////////////////////////////////////////////////////////////

// If a class defines a participant, then QIing an instance of that class to
// nsXPCOMCycleCollectionParticipant should produce that participant.
#ifdef DEBUG
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_BASE                                    \
    virtual void CheckForRightParticipant()
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_DERIVED                                 \
    virtual void CheckForRightParticipant() override
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_BODY(_class)                            \
    {                                                                          \
      nsXPCOMCycleCollectionParticipant *p;                                    \
      CallQueryInterface(this, &p);                                            \
      MOZ_ASSERT(p == &NS_CYCLE_COLLECTION_INNERNAME,                          \
                 #_class " should QI to its own CC participant");              \
    }
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                            \
    NS_CHECK_FOR_RIGHT_PARTICIPANT_BASE                                        \
    NS_CHECK_FOR_RIGHT_PARTICIPANT_BODY(_class)
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)                  \
    NS_CHECK_FOR_RIGHT_PARTICIPANT_DERIVED                                     \
    NS_CHECK_FOR_RIGHT_PARTICIPANT_BODY(_class)
#else
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)
#define NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)
#endif

#define NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(_class) \
  NS_IMETHOD_(const char*) ClassName() override { return #_class; };


#define NS_DECL_CYCLE_COLLECTION_CLASS_BODY_NO_UNLINK(_class, _base)           \
public:                                                                        \
  NS_IMETHOD TraverseNative(void *p, nsCycleCollectionTraversalCallback &cb)   \
    override;                                                                  \
  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(_class)                           \
  NS_IMETHOD_(void) DeleteCycleCollectable(void *p) override                   \
  {                                                                            \
    DowncastCCParticipant<_class>(p)->DeleteCycleCollectable();                \
  }                                                                            \
  static _class* Downcast(nsISupports* s)                                      \
  {                                                                            \
    return static_cast<_class*>(static_cast<_base*>(s));                       \
  }                                                                            \
  static nsISupports* Upcast(_class *p)                                        \
  {                                                                            \
    return NS_ISUPPORTS_CAST(_base*, p);                                       \
  }                                                                            \
  template<typename T>                                                         \
  friend nsISupports*                                                          \
  ToSupports(T* p, NS_CYCLE_COLLECTION_INNERCLASS* dummy);

#define NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                     \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY_NO_UNLINK(_class, _base)                 \
  NS_IMETHOD_(void) Unlink(void *p) override;

#define NS_PARTICIPANT_AS(type, participant)                                   \
  const_cast<type*>(reinterpret_cast<const type*>(participant))

#define NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                 \
  static constexpr nsXPCOMCycleCollectionParticipant* GetParticipant()         \
  {                                                                            \
    return &_class::NS_CYCLE_COLLECTION_INNERNAME;                             \
  }

/**
 * We use this macro to force that classes that inherit from a ccable class and
 * declare their own participant declare themselves as inherited cc classes.
 * To avoid possibly unnecessary vtables we only do this checking in debug
 * builds.
 */
#ifdef DEBUG
#define NOT_INHERITED_CANT_OVERRIDE virtual void BaseCycleCollectable() final {}
#else
#define NOT_INHERITED_CANT_OVERRIDE
#endif

#define NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(_class, _base)                \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public nsXPCOMCycleCollectionParticipant                                    \
{                                                                              \
public:                                                                        \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)       \
    : nsXPCOMCycleCollectionParticipant(aSkip) {}                              \
private:                                                                       \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                           \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};                                                                             \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                                    \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;           \
  NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_CLASS(_class)                                 \
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(_class, _class)

// Cycle collector helper for ambiguous classes that can sometimes be skipped.
#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(_class, _base)        \
class NS_CYCLE_COLLECTION_INNERCLASS                                             \
 : public nsXPCOMCycleCollectionParticipant                                      \
{                                                                                \
public:                                                                          \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = true)          \
    /* Ignore aSkip: we always want skippability. */                             \
  : nsXPCOMCycleCollectionParticipant(true) {}                                   \
private:                                                                         \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                             \
  NS_IMETHOD_(bool) CanSkipReal(void *p, bool aRemovingAllowed) override;        \
  NS_IMETHOD_(bool) CanSkipInCCReal(void *p) override;                           \
  NS_IMETHOD_(bool) CanSkipThisReal(void *p) override;                           \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                         \
};                                                                               \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                                      \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;             \
NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS(_class)                       \
        NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _base)          \
class NS_CYCLE_COLLECTION_INNERCLASS                                                   \
 : public nsXPCOMCycleCollectionParticipant                                            \
{                                                                                      \
public:                                                                                \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)               \
  : nsXPCOMCycleCollectionParticipant(aSkip) {}                                        \
private:                                                                               \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                                   \
  NS_IMETHOD_(void) Trace(void *p, const TraceCallbacks &cb, void *closure) override;  \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                               \
};                                                                                     \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                                            \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME; \
NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _base)   \
class NS_CYCLE_COLLECTION_INNERCLASS                                                      \
 : public nsXPCOMCycleCollectionParticipant                                               \
{                                                                                         \
public:                                                                                   \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = true)                   \
    /* Ignore aSkip: we always want skippability. */                                      \
  : nsXPCOMCycleCollectionParticipant(true) {}                                            \
private:                                                                                  \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                                      \
  NS_IMETHOD_(void) Trace(void *p, const TraceCallbacks &cb, void *closure) override;     \
  NS_IMETHOD_(bool) CanSkipReal(void *p, bool aRemovingAllowed) override;                 \
  NS_IMETHOD_(bool) CanSkipInCCReal(void *p) override;                                    \
  NS_IMETHOD_(bool) CanSkipThisReal(void *p) override;                                    \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                                  \
};                                                                                        \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                                               \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME; \
NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(_class)  \
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(_class,       \
                                                                         _base_class)  \
class NS_CYCLE_COLLECTION_INNERCLASS                                                   \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                                   \
{                                                                                      \
public:                                                                                \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = true)                \
    /* Ignore aSkip: we always want skippability. */                                   \
    : NS_CYCLE_COLLECTION_CLASSNAME(_base_class) (true) {}                             \
private:                                                                               \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)                   \
  NS_IMETHOD_(void) Trace(void *p, const TraceCallbacks &cb, void *closure) override;  \
  NS_IMETHOD_(bool) CanSkipReal(void *p, bool aRemovingAllowed) override;              \
  NS_IMETHOD_(bool) CanSkipInCCReal(void *p) override;                                 \
  NS_IMETHOD_(bool) CanSkipThisReal(void *p) override;                                 \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                               \
}; \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)  \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(_class)  \
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY_NO_UNLINK(_class,        \
                                                                _base_class)   \
public:                                                                        \
  NS_IMETHOD TraverseNative(void *p, nsCycleCollectionTraversalCallback &cb)   \
    override;                                                                  \
  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(_class)                           \
  static _class* Downcast(nsISupports* s)                                      \
  {                                                                            \
    return static_cast<_class*>(static_cast<_base_class*>(                     \
      NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::Downcast(s)));               \
  }

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)     \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY_NO_UNLINK(_class, _base_class) \
  NS_IMETHOD_(void) Unlink(void *p) override;

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(_class, _base_class)          \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                           \
{                                                                              \
public:                                                                        \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)       \
    : NS_CYCLE_COLLECTION_CLASSNAME(_base_class) (aSkip) {}                    \
private:                                                                       \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)           \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};                                                                             \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)                          \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(_class,             \
                                                           _base_class)        \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                           \
{                                                                              \
public:                                                                        \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)       \
    : NS_CYCLE_COLLECTION_CLASSNAME(_base_class) (aSkip) {}                    \
private:                                                                       \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY_NO_UNLINK(_class, _base_class) \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};                                                                             \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)                          \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(_class,                 \
                                                               _base_class)            \
class NS_CYCLE_COLLECTION_INNERCLASS                                                   \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                                   \
{                                                                                      \
public:                                                                                \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)               \
    : NS_CYCLE_COLLECTION_CLASSNAME(_base_class) (aSkip) {}                            \
private:                                                                               \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)                   \
  NS_IMETHOD_(void) Trace(void *p, const TraceCallbacks &cb, void *closure)            \
    override;                                                                          \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                               \
};                                                                                     \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)                                  \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

// Cycle collector participant declarations.

#define NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                     \
  public:                                                                      \
    NS_IMETHOD_(void) Root(void *n) override;                                  \
    NS_IMETHOD_(void) Unlink(void *n) override;                                \
    NS_IMETHOD_(void) Unroot(void *n) override;                                \
    NS_IMETHOD TraverseNative(void *n, nsCycleCollectionTraversalCallback &cb) \
      override;                                                                \
    NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(_class)                         \
    NS_IMETHOD_(void) DeleteCycleCollectable(void *n) override                 \
    {                                                                          \
      DowncastCCParticipant<_class>(n)->DeleteCycleCollectable();              \
    }                                                                          \
    static _class* Downcast(void* s)                                           \
    {                                                                          \
      return DowncastCCParticipant<_class>(s);                                 \
    }                                                                          \
    static void* Upcast(_class *p)                                             \
    {                                                                          \
      return static_cast<void*>(p);                                            \
    }

#define NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(_class)                          \
  void DeleteCycleCollectable(void)                                            \
  {                                                                            \
    delete this;                                                               \
  }                                                                            \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
   : public nsCycleCollectionParticipant                                       \
  {                                                                            \
public:                                                                        \
  constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)       \
    : nsCycleCollectionParticipant(aSkip) {}                                   \
private:                                                                       \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                         \
    static constexpr nsCycleCollectionParticipant* GetParticipant()            \
    {                                                                          \
      return &_class::NS_CYCLE_COLLECTION_INNERNAME;                           \
    }                                                                          \
  };                                                                           \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_NATIVE_CLASS(_class)                \
  void DeleteCycleCollectable(void)                                            \
  {                                                                            \
    delete this;                                                               \
  }                                                                            \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
   : public nsCycleCollectionParticipant                                       \
  {                                                                            \
  public:                                                                      \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = true)      \
      /* Ignore aSkip: we always want skippability. */                         \
    : nsCycleCollectionParticipant(true) {}                                    \
  private:                                                                     \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                         \
    NS_IMETHOD_(bool) CanSkipReal(void *p, bool aRemovingAllowed) override;    \
    NS_IMETHOD_(bool) CanSkipInCCReal(void *p) override;                       \
    NS_IMETHOD_(bool) CanSkipThisReal(void *p) override;                       \
    static nsCycleCollectionParticipant* GetParticipant()                      \
    {                                                                          \
      return &_class::NS_CYCLE_COLLECTION_INNERNAME;                           \
    }                                                                          \
  };                                                                           \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_NATIVE_CLASS_WITH_CUSTOM_DELETE(_class) \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public nsCycleCollectionParticipant                                         \
{                                                                              \
public:                                                                        \
  constexpr NS_CYCLE_COLLECTION_INNERCLASS ()                                  \
  : nsCycleCollectionParticipant(true) {}                                      \
private:                                                                       \
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                           \
  NS_IMETHOD_(bool) CanSkipReal(void *p, bool aRemovingAllowed) override;      \
  NS_IMETHOD_(bool) CanSkipInCCReal(void *p) override;                         \
  NS_IMETHOD_(bool) CanSkipThisReal(void *p) override;                         \
  static nsCycleCollectionParticipant* GetParticipant()                        \
  {                                                                            \
    return &_class::NS_CYCLE_COLLECTION_INNERNAME;                             \
  }                                                                            \
};                                                                             \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(_class)            \
  void DeleteCycleCollectable(void)                                            \
  {                                                                            \
    delete this;                                                               \
  }                                                                            \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
   : public nsScriptObjectTracer                                               \
  {                                                                            \
  public:                                                                      \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS (bool aSkip = false)     \
      : nsScriptObjectTracer(aSkip) {}                                         \
  private:                                                                     \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                         \
    NS_IMETHOD_(void) Trace(void *p, const TraceCallbacks &cb, void *closure)  \
      override;                                                                \
    static constexpr nsScriptObjectTracer* GetParticipant()                    \
    {                                                                          \
      return &_class::NS_CYCLE_COLLECTION_INNERNAME;                           \
    }                                                                          \
  };                                                                           \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(_class, _root_function)           \
  NS_IMETHODIMP_(void)                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Root(void *p)                         \
  {                                                                            \
    _class *tmp = static_cast<_class*>(p);                                     \
    tmp->_root_function();                                                     \
  }

#define NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(_class, _unroot_function)       \
  NS_IMETHODIMP_(void)                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Unroot(void *p)                       \
  {                                                                            \
    _class *tmp = static_cast<_class*>(p);                                     \
    tmp->_unroot_function();                                                   \
  }

#define NS_IMPL_CYCLE_COLLECTION_CLASS(_class) \
 _class::NS_CYCLE_COLLECTION_INNERCLASS _class::NS_CYCLE_COLLECTION_INNERNAME;

// NB: This is not something you usually want to use.  It is here to allow
// adding things to the CC graph to help debugging via CC logs, but it does not
// traverse or unlink anything, so it is useless for anything else.
#define NS_IMPL_CYCLE_COLLECTION_0(_class)                                     \
  NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                       \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                          \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                              \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION(_class, ...)                                  \
  NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                       \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                \
  NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                                 \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                          \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                              \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                               \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// If you are looking for NS_IMPL_CYCLE_COLLECTION_INHERITED_0(_class, _base)
// you should instead not declare any cycle collected stuff in _class, so it
// will just inherit the CC declarations from _base.

#define NS_IMPL_CYCLE_COLLECTION_INHERITED(_class, _base, ...)                 \
  NS_IMPL_CYCLE_COLLECTION_CLASS(_class)                                       \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)               \
  NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                                 \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                          \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)             \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                               \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_CYCLE_COLLECTION_NOTE_EDGE_NAME CycleCollectionNoteEdgeName

/**
 * Equivalency of the high three words where two IIDs have the same
 * top three words but not the same low word.
 */
inline bool TopThreeWordsEquals(const nsID& aID,
                                const nsID& aOther1,
                                const nsID& aOther2)
{
  MOZ_ASSERT((((uint32_t*)&aOther1.m0)[0] == ((uint32_t*)&aOther2.m0)[0]) &&
             (((uint32_t*)&aOther1.m0)[1] == ((uint32_t*)&aOther2.m0)[1]) &&
             (((uint32_t*)&aOther1.m0)[2] == ((uint32_t*)&aOther2.m0)[2]) &&
             (((uint32_t*)&aOther1.m0)[3] != ((uint32_t*)&aOther2.m0)[3]));

  return ((((uint32_t*)&aID.m0)[0] == ((uint32_t*)&aOther1.m0)[0]) &&
          (((uint32_t*)&aID.m0)[1] == ((uint32_t*)&aOther1.m0)[1]) &&
          (((uint32_t*)&aID.m0)[2] == ((uint32_t*)&aOther1.m0)[2]));
}

/**
 * Equivalency of the fourth word where the two IIDs have the same
 * top three words but not the same low word.
 */
inline bool LowWordEquals(const nsID& aID, const nsID& aOther)
{
  return (((uint32_t*)&aID.m0)[3] == ((uint32_t*)&aOther.m0)[3]);
}

#endif // nsCycleCollectionParticipant_h__
