/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectionParticipant_h__
#define nsCycleCollectionParticipant_h__

#include "nsCycleCollectionNoteChild.h"

#define NS_CYCLECOLLECTIONPARTICIPANT_IID                                      \
{                                                                              \
    0x9674489b,                                                                \
    0x1f6f,                                                                    \
    0x4550,                                                                    \
    { 0xa7, 0x30, 0xcc, 0xae, 0xdd, 0x10, 0x4c, 0xf9 }                         \
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

/**
 * Forward declarations
 */
class nsCycleCollectionParticipant;
class nsScriptObjectTracer;
class nsXPCOMCycleCollectionParticipant;

typedef void
(* TraceCallback)(void *p, const char* name, void *closure);

/**
 * VTables
 *
 * When using global scope static initialization for simple types with virtual
 * member functions, GCC creates static initializer functions. In order to
 * avoid this from happening, cycle collection participants are defined as
 * function tables.
 *
 * The Traverse function may require calling another function from the cycle
 * collection participant function table, so a pointer to the function table
 * is given to it. Using member function pointers would be less awkward, but
 * in MSVC, the size of such a member function pointer depends on the class
 * the function is member of. This makes it hard to make them compatible with
 * a generic function table. Moreover, static initialization of the function
 * table then uses a static initializer function.
 *
 * Finally, it is not possible to use an initializer list for non-aggregate
 * types. Separate types are thus required for static initialization. For
 * convenience and to avoid repetitions that could lead to discrepancies,
 * function table members for sub-types are declared independently, and
 * different aggregate types are defined for static initialization.
 */

/* Base functions for nsCycleCollectionParticipant */
template <typename T>
struct nsCycleCollectionParticipantVTableCommon
{
    nsresult (NS_STDCALL *TraverseReal)
        (T *that, void *p, nsCycleCollectionTraversalCallback &cb);

    nsresult (NS_STDCALL *Root)(void *p);
    nsresult (NS_STDCALL *Unlink)(void *p);
    nsresult (NS_STDCALL *Unroot)(void *p);

    void (NS_STDCALL *UnmarkIfPurple)(void *p);

    bool (NS_STDCALL *CanSkipReal)(void *p, bool aRemovingAllowed);
    bool (NS_STDCALL *CanSkipInCCReal)(void *p);
    bool (NS_STDCALL *CanSkipThisReal)(void *p);
};

typedef nsCycleCollectionParticipantVTableCommon<nsCycleCollectionParticipant>
    nsCycleCollectionParticipantVTable;

/* Additional functions for nsScriptObjectTracer */
struct nsScriptObjectTracerVTable
{
    void (NS_STDCALL *Trace)(void *p, TraceCallback cb, void *closure);
};

/**
 * Types for static initialization
 *
 * Considering T, the cycle collection participant class, subclass of either
 * nsCycleCollectionParticipant, nsScriptObjectTracer or
 * nsXPCOMCycleCollectionParticipant, static initialization goes as follows:
 *
 *   CCParticipantVTable<T>::type T::_cycleCollectorGlobal = {{...} ...};
 *
 * CCParticipantVTable<T>::type automatically defines the right type considering
 * what particular cycle collection participant class T derives from.
 *
 * The NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(classname),
 * NS_IMPL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_VTABLE(classname), and
 * NS_IMPL_CYCLE_COLLECTION_VTABLE(classname) macros may be used as helpers
 * for static initialization:
 *
 *   CCParticipantVTable<T>::type T::_cycleCollectorGlobal = {
 *     NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(classname);
 *   };
 */

enum nsCycleCollectionParticipantType
{
  eInvalid,
  eCycleCollectionParticipant,
  eScriptObjectTracer,
  eXPCOMCycleCollectionParticipant
};

template <typename T, enum nsCycleCollectionParticipantType ParticipantType>
struct CCParticipantVTableImpl { };

/* CCParticipantVTable for nsCycleCollectionParticipant */
template <typename T>
struct CCParticipantVTableImpl<T, eCycleCollectionParticipant>
{
    nsCycleCollectionParticipant *GetParticipant() const
    {
        return const_cast<nsCycleCollectionParticipant *>
               (reinterpret_cast<const nsCycleCollectionParticipant *>(this));
    }
    nsCycleCollectionParticipantVTableCommon<T> cycleCollectionParticipant;
};

/* CCParticipantVTable for nsScriptObjectTracer */
template <typename T>
struct CCParticipantVTableImpl<T, eScriptObjectTracer>
{
    nsScriptObjectTracer *GetParticipant() const
    {
        return const_cast<nsScriptObjectTracer *>
               (reinterpret_cast<const nsScriptObjectTracer *>(this));
    }
    nsCycleCollectionParticipantVTableCommon<T> cycleCollectionParticipant;
    nsScriptObjectTracerVTable scriptObjectTracer;
};

/* CCParticipantVTable for nsXPCOMCycleCollectionParticipant */
template <typename T>
struct CCParticipantVTableImpl<T, eXPCOMCycleCollectionParticipant>
{
    nsXPCOMCycleCollectionParticipant *GetParticipant() const
    {
        return const_cast<nsXPCOMCycleCollectionParticipant *>
               (reinterpret_cast<const nsXPCOMCycleCollectionParticipant *>(this));
    }
    nsCycleCollectionParticipantVTableCommon<T> cycleCollectionParticipant;
    nsScriptObjectTracerVTable scriptObjectTracer;
};

template <typename T>
struct CCParticipantVTable
{
    static const enum nsCycleCollectionParticipantType ParticipantType =
        mozilla::IsBaseOf<nsXPCOMCycleCollectionParticipant, T>::value ? eXPCOMCycleCollectionParticipant :
        mozilla::IsBaseOf<nsScriptObjectTracer, T>::value ? eScriptObjectTracer :
        mozilla::IsBaseOf<nsCycleCollectionParticipant, T>::value ? eCycleCollectionParticipant :
        eInvalid;
    typedef CCParticipantVTableImpl<T, ParticipantType> Type;
};

/**
 * Participant implementation classes
 */
class nsCycleCollectionParticipant : public nsCycleCollectionParticipantVTable
{
public:
    static const bool isSkippable = false;

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CYCLECOLLECTIONPARTICIPANT_IID)

    // Helper function to avoid painful syntax for member function call using
    // the VTable entry.
    NS_METHOD Traverse(void *p, nsCycleCollectionTraversalCallback &cb) {
        return TraverseReal(this, p, cb);
    }
    // If CanSkip returns true, p is removed from the purple buffer during
    // a call to nsCycleCollector_forgetSkippable().
    // Note, calling CanSkip may remove objects from the purple buffer!
    // If aRemovingAllowed is true, p can be removed from the purple buffer.
    bool CanSkip(void *p, bool aRemovingAllowed)
    {
        return CanSkipReal ? CanSkipReal(p, aRemovingAllowed) : false;
    }

    // If CanSkipInCC returns true, p is skipped when selecting roots for the
    // cycle collector graph.
    // Note, calling CanSkipInCC may remove other objects from the purple buffer!
    bool CanSkipInCC(void *p)
    {
        return CanSkipInCCReal ? CanSkipInCCReal(p) : false;
    }

    // If CanSkipThis returns true, p is not added to the graph.
    // This method is called during cycle collection, so don't
    // change the state of any objects!
    bool CanSkipThis(void *p)
    {
        return CanSkipThisReal ? CanSkipThisReal(p) : false;
    }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCycleCollectionParticipant, 
                              NS_CYCLECOLLECTIONPARTICIPANT_IID)

class nsScriptObjectTracer
    : public nsCycleCollectionParticipant, public nsScriptObjectTracerVTable
{
public:
    static void NS_COM_GLUE NoteJSChild(void *aScriptThing, const char *name,
                                        void *aClosure);
};

class nsXPCOMCycleCollectionParticipant : public nsScriptObjectTracer
{
public:
    static NS_METHOD RootImpl(void *p);
    static NS_METHOD UnrootImpl(void *p);

    static NS_METHOD_(void) TraceImpl(void *p, TraceCallback cb, void *closure);

    static bool CheckForRightISupports(nsISupports *s);
};

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

#define NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION(_class)                        \
  NS_IMPL_QUERY_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION_ISUPPORTS(_class)              \
  NS_IMPL_QUERY_CYCLE_COLLECTION_ISUPPORTS(_class)

#define NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)                      \
  NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION(_class)                              \
  NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION_ISUPPORTS(_class)

#define NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(_class)                        \
  NS_INTERFACE_MAP_BEGIN(_class)                                               \
    NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(_class)              \
  NS_INTERFACE_MAP_BEGIN(_class)                                               \
    NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(_class)  \
  if (rv == NS_OK) return rv; \
  nsISupports* foundInterface; \
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(_class)            \
  NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)    \
  {                                                                           \
    NS_PRECONDITION(aInstancePtr, "null out param");                          \
                                                                              \
    if ( aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant)) ) {       \
      *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(_class);                \
      return NS_OK;                                                           \
    }                                                                         \
    nsresult rv;

#define NS_CYCLE_COLLECTION_UPCAST(obj, clazz)                                 \
  NS_CYCLE_COLLECTION_CLASSNAME(clazz)::Upcast(obj)

// The default implementation of this class template is empty, because it
// should never be used: see the partial specializations below.
template <typename T,
          bool IsXPCOM = mozilla::IsBaseOf<nsISupports, T>::value>
struct DowncastCCParticipantImpl
{
};

// Specialization for XPCOM CC participants
template <typename T>
struct DowncastCCParticipantImpl<T, true>
{
  static T* Run(void *p)
  {
    nsISupports *s = static_cast<nsISupports*>(p);
    MOZ_ASSERT(NS_CYCLE_COLLECTION_CLASSNAME(T)::CheckForRightISupports(s),
               "not the nsISupports pointer we expect");
    return NS_CYCLE_COLLECTION_CLASSNAME(T)::Downcast(s);
  }
};

// Specialization for native CC participants
template <typename T>
struct DowncastCCParticipantImpl<T, false>
{
  static T* Run(void *p)
  {
    return static_cast<T*>(p);
  }
};

template <typename T>
T* DowncastCCParticipant(void *p)
{
  return DowncastCCParticipantImpl<T>::Run(p);
}

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing CanSkip methods
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(_class)                        \
  NS_METHOD_(bool)                                                             \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::CanSkipImpl(void *p,                  \
                                                     bool aRemovingAllowed)    \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END                                  \
    (void)tmp;                                                                 \
    return false;                                                              \
  }

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(_class)                  \
  NS_METHOD_(bool)                                                             \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::CanSkipInCCImpl(void *p)              \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END                            \
    (void)tmp;                                                                 \
    return false;                                                              \
  }

#define NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(_class)                   \
  NS_METHOD_(bool)                                                             \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::CanSkipThisImpl(void *p)              \
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
  NS_METHOD                                                                    \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::UnlinkImpl(void *p)                   \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base_class)   \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::UnlinkImpl(s);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK(_field)                                \
    ImplCycleCollectionUnlink(tmp->_field);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                    \
    (void)tmp;                                                                 \
    return NS_OK;                                                              \
  }

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(_base_class)             \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::UnlinkImpl(s);                 \
    (void)tmp;                                                                 \
    return NS_OK;                                                              \
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
  NS_METHOD                                                                    \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::TraverseImpl                          \
                         (NS_CYCLE_COLLECTION_CLASSNAME(_class) *that, void *p,\
                          nsCycleCollectionTraversalCallback &cb)              \
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
    if (NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::TraverseImpl(that, s, cb)  \
        == NS_SUCCESS_INTERRUPTED_TRAVERSE) {                                  \
      return NS_SUCCESS_INTERRUPTED_TRAVERSE;                                  \
    }

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field)                              \
  ImplCycleCollectionTraverse(cb, tmp->_field, #_field, 0);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(_field)                       \
  CycleCollectionNoteChild(cb, tmp->_field, #_field);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS                       \
    that->Trace(p, &nsScriptObjectTracer::NoteJSChild, &cb);

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                  \
    (void)tmp;                                                                 \
    return NS_OK;                                                              \
  }

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing nsScriptObjectTracer::Trace
///////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_class)                           \
  void                                                                         \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::TraceImpl(void *p,                    \
                                                   TraceCallback aCallback,    \
                                                   void *aClosure)             \
  {                                                                            \
    _class *tmp = DowncastCCParticipant<_class >(p);

#define NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(_class, _base_class)    \
  NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_class)                                 \
    nsISupports *s = static_cast<nsISupports*>(p);                             \
    NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::TraceImpl(s,                   \
                                                          aCallback,           \
                                                          aClosure);

#define NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(_object, _name)             \
  if (_object)                                                                 \
    aCallback(_object, _name, aClosure);

#define NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(_field)              \
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(tmp->_field, #_field)

#define NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(_field)           \
  if (JSVAL_IS_TRACEABLE(tmp->_field)) {                                       \
    void *gcThing = JSVAL_TO_TRACEABLE(tmp->_field);                           \
    aCallback(gcThing, #_field, aClosure);                                     \
  }

// NB: The (void)tmp; hack in the TRACE_END macro exists to support
// implementations that don't need to do anything in their Trace method.
// Without this hack, some compilers warn about the unused tmp local.
#define NS_IMPL_CYCLE_COLLECTION_TRACE_END                                     \
      (void)tmp;                                                               \
  }

///////////////////////////////////////////////////////////////////////////////
// Helpers for implementing a concrete nsCycleCollectionParticipant 
///////////////////////////////////////////////////////////////////////////////

#define NS_DECL_CYCLE_COLLECTION_CLASS_BODY_NO_UNLINK(_class, _base)           \
public:                                                                        \
  static NS_METHOD TraverseImpl(NS_CYCLE_COLLECTION_CLASSNAME(_class) *that,   \
                            void *p, nsCycleCollectionTraversalCallback &cb);  \
  static NS_METHOD_(void) UnmarkIfPurpleImpl(void *s)                          \
  {                                                                            \
    Downcast(static_cast<nsISupports *>(s))->UnmarkIfPurple();                 \
  }                                                                            \
  static _class* Downcast(nsISupports* s)                                      \
  {                                                                            \
    return static_cast<_class*>(static_cast<_base*>(s));                       \
  }                                                                            \
  static nsISupports* Upcast(_class *p)                                        \
  {                                                                            \
    return NS_ISUPPORTS_CAST(_base*, p);                                       \
  }                                                                            \
  template <typename T>                                                        \
  friend nsISupports*                                                          \
  ToSupports(T* p, NS_CYCLE_COLLECTION_INNERCLASS* dummy);

#define NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                     \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY_NO_UNLINK(_class, _base)                 \
  static NS_METHOD UnlinkImpl(void *p);

#define NS_PARTICIPANT_AS(type, participant)                                   \
  const_cast<type*>(reinterpret_cast<const type*>(participant))

#define NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                 \
  static nsXPCOMCycleCollectionParticipant* GetParticipant()                   \
  {                                                                            \
    static const CCParticipantVTable<NS_CYCLE_COLLECTION_CLASSNAME(_class)>    \
      ::Type p = {                                                             \
        NS_IMPL_CYCLE_COLLECTION_VTABLE(NS_CYCLE_COLLECTION_CLASSNAME(_class)) \
      };                                                                       \
    return NS_PARTICIPANT_AS(nsXPCOMCycleCollectionParticipant, &p);           \
  }

/**
 * We use this macro to force that classes that inherit from a ccable class and
 * declare their own participant declare themselves as inherited cc classes.
 * To avoid possibly unnecessary vtables we only do this checking in debug
 * builds.
 */
#ifdef DEBUG
#define NOT_INHERITED_CANT_OVERRIDE virtual void BaseCycleCollectable() MOZ_FINAL {}
#else
#define NOT_INHERITED_CANT_OVERRIDE
#endif

#define NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(_class, _base)                \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public nsXPCOMCycleCollectionParticipant                                    \
{                                                                              \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                           \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};                                                                             \
  NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_CLASS(_class)                                 \
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(_class, _class)

// Cycle collector helper for ambiguous classes that can sometimes be skipped.
#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(_class, _base)        \
class NS_CYCLE_COLLECTION_INNERCLASS                                             \
 : public nsXPCOMCycleCollectionParticipant                                      \
{                                                                                \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                             \
  static const bool isSkippable = true;                                          \
  static NS_METHOD_(bool) CanSkipImpl(void *p, bool aRemovingAllowed);           \
  static NS_METHOD_(bool) CanSkipInCCImpl(void *p);                              \
  static NS_METHOD_(bool) CanSkipThisImpl(void *p);                              \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};                                                                             \
NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS(_class)                       \
        NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _base)  \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public nsXPCOMCycleCollectionParticipant                                    \
{                                                                              \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                           \
  static NS_METHOD_(void) TraceImpl(void *p, TraceCallback cb, void *closure); \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};                                                                             \
NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _base)   \
class NS_CYCLE_COLLECTION_INNERCLASS                                                      \
 : public nsXPCOMCycleCollectionParticipant                                               \
{                                                                                         \
  NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                                      \
  static const bool isSkippable = true;                                                   \
  static NS_METHOD_(void) TraceImpl(void *p, TraceCallback cb, void *closure);            \
  static NS_METHOD_(bool) CanSkipImpl(void *p, bool aRemovingAllowed);                    \
  static NS_METHOD_(bool) CanSkipInCCImpl(void *p);                                       \
  static NS_METHOD_(bool) CanSkipThisImpl(void *p);                                       \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                        \
};                                                                             \
NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(_class)  \
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(_class,      \
                                                                         _base_class) \
class NS_CYCLE_COLLECTION_INNERCLASS                                                  \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                                  \
{                                                                                     \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)                  \
  static const bool isSkippable = true;                                               \
  static NS_METHOD_(void) TraceImpl(void *p, TraceCallback cb, void *closure);        \
  static NS_METHOD_(bool) CanSkipImpl(void *p, bool aRemovingAllowed);                \
  static NS_METHOD_(bool) CanSkipInCCImpl(void *p);                                   \
  static NS_METHOD_(bool) CanSkipThisImpl(void *p);                                   \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(_class)  \
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY_NO_UNLINK(_class,        \
                                                                _base_class)   \
public:                                                                        \
  static NS_METHOD TraverseImpl(NS_CYCLE_COLLECTION_CLASSNAME(_class) *that,   \
                         void *p, nsCycleCollectionTraversalCallback &cb);     \
  static _class* Downcast(nsISupports* s)                                      \
  {                                                                            \
    return static_cast<_class*>(static_cast<_base_class*>(                     \
      NS_CYCLE_COLLECTION_CLASSNAME(_base_class)::Downcast(s)));               \
  }

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)     \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY_NO_UNLINK(_class, _base_class) \
  static NS_METHOD UnlinkImpl(void *p);

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(_class, _base_class)          \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                           \
{                                                                              \
public:                                                                        \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)           \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};

#define NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(_class,             \
                                                           _base_class)        \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                           \
{                                                                              \
public:                                                                        \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY_NO_UNLINK(_class, _base_class) \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(_class,         \
                                                               _base_class)    \
class NS_CYCLE_COLLECTION_INNERCLASS                                           \
 : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class)                           \
{                                                                              \
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)           \
  static NS_METHOD_(void) TraceImpl(void *p, TraceCallback cb, void *closure); \
  NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                       \
};

/**
 * This implements a stub UnmarkIfPurple function for classes that want to be
 * traversed but whose AddRef/Release functions don't add/remove them to/from
 * the purple buffer. If you're just using NS_DECL_CYCLE_COLLECTING_ISUPPORTS
 * then you don't need this.
 */
#define NS_DECL_CYCLE_COLLECTION_UNMARK_PURPLE_STUB(_class)                    \
  NS_IMETHODIMP_(void) UnmarkIfPurple()                                        \
  {                                                                            \
  }                                                                            \

/**
 * Dummy class with a definition for CanSkip* function members, but no
 * implementation.
 * Implementation was added to please Win PGO. (See bug 765159)
 */
struct SkippableDummy
{
  static NS_METHOD_(bool) CanSkipImpl(void *p, bool aRemovingAllowed) { return false; }
  static NS_METHOD_(bool) CanSkipInCCImpl(void *p) { return false; }
  static NS_METHOD_(bool) CanSkipThisImpl(void *p) { return false; }
};

/**
 * Skippable<T> defines a class that always has definitions for CanSkip*
 * function members, so that T::isSkippable ? &Skippable<T>::CanSkip* : NULL
 * can compile when T::isSkippable is false and T doesn't have CanSkip*
 * definitions (which, as not being skippable, it's not supposed to have). 
 */
template <class T>
struct Skippable
 : public mozilla::Conditional<T::isSkippable, T, SkippableDummy>::Type
{ };


// VTables for the cycle collector participant implementations.

#define NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(_class)                         \
  {                                                                            \
    &_class::TraverseImpl,                                                     \
    &_class::RootImpl,                                                         \
    &_class::UnlinkImpl,                                                       \
    &_class::UnrootImpl,                                                       \
    &_class::UnmarkIfPurpleImpl,                                               \
    _class::isSkippable ? &Skippable<_class>::CanSkipImpl : NULL,              \
    _class::isSkippable ? &Skippable<_class>::CanSkipInCCImpl : NULL,          \
    _class::isSkippable ? &Skippable<_class>::CanSkipThisImpl : NULL           \
  }

#define NS_IMPL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_VTABLE(_class)           \
  NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(_class),                              \
  { &_class::TraceImpl }

#define NS_IMPL_CYCLE_COLLECTION_VTABLE(_class)                                \
  NS_IMPL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_VTABLE(_class)


// Cycle collector participant declarations.

#define NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                     \
  public:                                                                      \
    static NS_METHOD RootImpl(void *n);                                        \
    static NS_METHOD UnlinkImpl(void *n);                                      \
    static NS_METHOD UnrootImpl(void *n);                                      \
    static NS_METHOD TraverseImpl(NS_CYCLE_COLLECTION_CLASSNAME(_class) *that, \
                           void *n, nsCycleCollectionTraversalCallback &cb);

#define NS_DECL_CYCLE_COLLECTION_NATIVE_UNMARK_IF_PURPLE(_class)               \
    static NS_METHOD_(void) UnmarkIfPurpleImpl(void *p)                        \
    {                                                                          \
        _class *tmp = static_cast<_class *>(p);                                \
        if (MOZ_LIKELY(tmp->mRefCnt.HasPurpleBufferEntry()))                   \
            tmp->mRefCnt.ReleasePurpleBufferEntry();                           \
    }

#define NS_DECL_CYCLE_COLLECTION_STUB_UNMARK_IF_PURPLE(_class)                 \
    static NS_METHOD_(void) UnmarkIfPurpleImpl(void *p) {}

#define NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(_class)                          \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
   : public nsCycleCollectionParticipant                                       \
  {                                                                            \
     NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                        \
     NS_DECL_CYCLE_COLLECTION_NATIVE_UNMARK_IF_PURPLE(_class)                  \
     static nsCycleCollectionParticipant* GetParticipant()                     \
     {                                                                         \
        static const CCParticipantVTable<NS_CYCLE_COLLECTION_CLASSNAME(_class)> \
        ::Type p = {                                                          \
           NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(                             \
              NS_CYCLE_COLLECTION_CLASSNAME(_class))                           \
        };                                                                     \
        return NS_PARTICIPANT_AS(nsCycleCollectionParticipant, &p);            \
    }                                                                          \
  };

#define NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(_class)            \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
   : public nsScriptObjectTracer                                               \
  {                                                                            \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                         \
    NS_DECL_CYCLE_COLLECTION_NATIVE_UNMARK_IF_PURPLE(_class)                   \
    static NS_METHOD_(void) TraceImpl(void *p, TraceCallback cb,               \
                                      void *closure);                          \
    static nsScriptObjectTracer* GetParticipant()                              \
    {                                                                          \
      static const CCParticipantVTable<NS_CYCLE_COLLECTION_CLASSNAME(_class)>  \
        ::Type participant = {                                                 \
          NS_IMPL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_VTABLE(                \
            NS_CYCLE_COLLECTION_CLASSNAME(_class))                             \
        };                                                                     \
      return NS_PARTICIPANT_AS(nsScriptObjectTracer, &participant);            \
    }                                  \
  };

#define NS_DECL_CYCLE_COLLECTION_LEGACY_NATIVE_CLASS(_class)                   \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
   : public nsCycleCollectionParticipant                                       \
  {                                                                            \
     NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                        \
     NS_DECL_CYCLE_COLLECTION_STUB_UNMARK_IF_PURPLE(_class)                    \
    static nsCycleCollectionParticipant* GetParticipant()                      \
    {                                                                          \
      static const CCParticipantVTable<NS_CYCLE_COLLECTION_CLASSNAME(_class)>  \
        ::Type p = {                                                           \
          NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(                              \
              NS_CYCLE_COLLECTION_CLASSNAME(_class))                           \
        };                                                                     \
      return NS_PARTICIPANT_AS(nsCycleCollectionParticipant, &p);              \
    }                                                                          \
  };

#define NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(_class, _root_function)           \
  NS_METHOD                                                                    \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::RootImpl(void *p)                     \
  {                                                                            \
    _class *tmp = static_cast<_class*>(p);                                     \
    tmp->_root_function();                                                     \
    return NS_OK;                                                              \
  }

#define NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(_class, _unroot_function)       \
  NS_METHOD                                                                    \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::UnrootImpl(void *p)                   \
  {                                                                            \
    _class *tmp = static_cast<_class*>(p);                                     \
    tmp->_unroot_function();                                                   \
    return NS_OK;                                                              \
  }

// NS_IMPL_CYCLE_COLLECTION_0 is not defined because most of the time it doesn't
// make sense to add something to the CC that doesn't traverse to anything.

#define NS_IMPL_CYCLE_COLLECTION_1(_class, _f)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f)                                           \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f)                                         \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_2(_class, _f1, _f2)                           \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_3(_class, _f1, _f2, _f3)                      \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_4(_class, _f1, _f2, _f3, _f4)                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_5(_class, _f1, _f2, _f3, _f4, _f5)            \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_6(_class, _f1, _f2, _f3, _f4, _f5, _f6)       \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_7(_class, _f1, _f2, _f3, _f4, _f5, _f6, _f7)  \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_8(_class, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f8)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f8)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_9(_class, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f8)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f9)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f8)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f9)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_10(_class, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f8)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f9)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f10)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f8)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f9)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f10)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_17(_class, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, \
                                    _f10, _f11, _f12, _f13, _f14, _f15, _f16, _f17)      \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                                 \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f8)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f9)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f10)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f11)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f12)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f13)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f14)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f15)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f16)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f17)                                         \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                               \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f8)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f9)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f10)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f11)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f12)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f13)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f14)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f15)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f16)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f17)                                       \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_1(_class, _base, _f1)               \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_2(_class, _base, _f1, _f2)          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_3(_class, _base, _f1, _f2, _f3)     \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_4(_class, _base, _f1, _f2, _f3, _f4) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_5(_class, _base, _f1, _f2, _f3, _f4, _f5) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_6(_class, _base, _f1, _f2, _f3, _f4, _f5, _f6) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_7(_class, _base, _f1, _f2, _f3, _f4, _f5, _f6, _f7) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_INHERITED_8(_class, _base, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8) \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)                \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f1)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f2)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f3)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f4)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f5)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f6)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f7)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK(_f8)                                          \
 NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                           \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)              \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f1)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f2)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f3)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f4)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f5)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f6)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f7)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_f8)                                        \
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_CYCLE_COLLECTION_NOTE_EDGE_NAME CycleCollectionNoteEdgeName

#endif // nsCycleCollectionParticipant_h__
