/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#ifndef nsXPIDLString_h___
#define nsXPIDLString_h___

#ifndef nsSharableString_h___
#include "nsSharableString.h"
#endif

#if DEBUG_STRING_STATS
#include <stdio.h>
#endif



  /**
   * |nsXPIDLC?String| extends |nsSharableC?String| with the ability
   * to defer calculation of its length.  This is crucial to allowing
   * the |getter_Copies| assignment behavior.
   *
   * The mechanism relies on the fact that |GetSharedBufferHandle|
   * must be called before any other object can share the buffer,
   * and as the default implementation for |GetBufferHandle|, which is
   * the operation on which all other flat string operations are based.
   * A valid handle will either have all |NULL| or all non-|NULL|
   * pointers.  After use as an `out' string pointer parameter, an
   * |nsXPIDLC?String|'s handle will have a non-|NULL| storage start, but
   * all other members will be |NULL|.  This is the signal that the
   * length needs to be recalculated.  |GetSharedBufferHandle| detects
   * this situation and repairs it.
   *
   * The one situation this _doesn't_ catch, is if no one ever tries to use
   * the string before it's destruction.  In this case, because the start of
   * storage is known, storage can still be freed in the usual way.
   *
   * An |nsXPIDLC?String| is now a sharable object, just like |nsSharableC?String|.
   * This simple implementation always allocates an intermediary handle
   * object.  This cost might turn out to be a burden, it's something we'll
   * want to measure.  A couple of optimizations spring to mind if allocation
   * of the handle objects shows up on the performance radar:
   * (1) introduce a custom allocator for the handles, e.g., keep free lists
   * or arena allocate them, or (2) fatten up the |nsXPIDLC?String| with a
   * local handle, and only allocate a shared handle in the event that
   * someone actually wants to share.  Both of these alternatives add
   * complexity or space costs, and so we start with the simplest thing
   * that could possibly work :-)
   */

class NS_COM nsXPIDLString
    : public nsSharableString
  {
    public:
      typedef nsXPIDLString self_type;

    public:
      nsXPIDLString()
        {
#if DEBUG_STRING_STATS
          ++sCreatedCount;
          if ( ++sAliveCount > sHighWaterCount )
            sHighWaterCount = sAliveCount;
#endif
        }

      nsXPIDLString( const self_type& aString )
          : nsSharableString(aString.GetSharedBufferHandle())
          // copy-constructor required (or else C++ generates one for us)
        {
#if DEBUG_STRING_STATS
          ++sCreatedCount;
          if ( ++sAliveCount > sHighWaterCount )
            sHighWaterCount = sAliveCount;
#endif
        }

#if DEBUG_STRING_STATS
      ~nsXPIDLString()
        {
          --sAliveCount;
        }
#endif

      self_type&
      operator=( const self_type& rhs )
          // copy-assignment operator required (or else C++ generates one for us)
        {
          // self-assignment is handled by the underlying |nsAutoBufferHandle|
          mBuffer = rhs.GetSharedBufferHandle();
          return *this;
        }

      void Adopt( char_type* aNewValue ) { *PrepareForUseAsOutParam() = aNewValue; }

        // deprecated, to be eliminated
      operator const char_type*() const { return get(); }
      char_type operator[]( int i ) const { return get()[ i ]; }


      class getter_Copies_t
        {
          public:
            getter_Copies_t( nsXPIDLString& aString ) : mString(&aString) { }
            // getter_Copies_t( const getter_Copies_t& );             // auto-generated copy-constructor OK
            // getter_Copies_t& operator=( const getter_Copies_t& );  // auto-generated assignment operator OK

            operator char_type**() const  { return mString->PrepareForUseAsOutParam(); }

          private:
            nsXPIDLString* mString;
        };

      friend class getter_Copies_t;

    protected:
#if DEBUG_STRING_STATS
      virtual const buffer_handle_type*  GetFlatBufferHandle() const;
      virtual const buffer_handle_type*  GetBufferHandle() const;
#endif
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;
        // overridden to fix the length after `out' parameter assignment, if necessary

      char_type** PrepareForUseAsOutParam();

#if DEBUG_STRING_STATS
      static size_t sCreatedCount;    // total number of |nsXPIDLString|s ever created
      static size_t sAliveCount;      // total number of |nsXPIDLStrings|s alive right now
      static size_t sHighWaterCount;  // greatest number of |nsXPIDLString|s alive at once
      static size_t sAssignCount;     // total number of times |nsXPIDLString|s were
                                      //  assigned into with |getter_Copies|
      static size_t sShareCount;      // total number times |nsXPIDLString|s were asked to share

    public:
      static void DebugPrintStats( FILE* );
#endif
  };

inline
nsXPIDLString::getter_Copies_t
getter_Copies( nsXPIDLString& aString )
  {
    return nsXPIDLString::getter_Copies_t(aString);
  }



class NS_COM nsXPIDLCString
    : public nsSharableCString
  {
    public:
      typedef nsXPIDLCString self_type;

    public:
      nsXPIDLCString()
        {
#if DEBUG_STRING_STATS
          ++sCreatedCount;
          if ( ++sAliveCount > sHighWaterCount )
            sHighWaterCount = sAliveCount;
#endif
        }

      nsXPIDLCString( const self_type& aString )
          : nsSharableCString(aString.GetSharedBufferHandle())
          // copy-constructor required (or else C++ generates one for us)
        {
#if DEBUG_STRING_STATS
          ++sCreatedCount;
          if ( ++sAliveCount > sHighWaterCount )
            sHighWaterCount = sAliveCount;
#endif
        }

#if DEBUG_STRING_STATS
      ~nsXPIDLCString()
        {
          --sAliveCount;
        }
#endif

      self_type&
      operator=( const self_type& rhs )
          // copy-assignment operator required (or else C++ generates one for us)
        {
          // self-assignment is handled by the underlying |nsAutoBufferHandle|
          mBuffer = rhs.GetSharedBufferHandle();
          return *this;
        }

      void Adopt( char_type* aNewValue ) { *PrepareForUseAsOutParam() = aNewValue; }

        // deprecated, to be eliminated
      operator const char_type*() const { return get(); }
      char_type operator[]( int i ) const      { return get()[ i ]; }


      class getter_Copies_t
        {
          public:
            getter_Copies_t( nsXPIDLCString& aString ) : mString(&aString) { }
            // getter_Copies_t( const getter_Copies_t& );             // auto-generated copy-constructor OK
            // getter_Copies_t& operator=( const getter_Copies_t& );  // auto-generated assignment operator OK

            operator char_type**() const  { return mString->PrepareForUseAsOutParam(); }

          private:
            nsXPIDLCString* mString;
        };

      friend class getter_Copies_t;

    protected:
#if DEBUG_STRING_STATS
      virtual const buffer_handle_type*  GetFlatBufferHandle() const;
      virtual const buffer_handle_type*  GetBufferHandle() const;
#endif
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;
        // overridden to fix the length after `out' parameter assignment, if necessary

      char_type** PrepareForUseAsOutParam();

#if DEBUG_STRING_STATS
      static size_t sCreatedCount;    // total number of |nsXPIDLCString|s ever created
      static size_t sAliveCount;      // total number of |nsXPIDLCStrings|s alive right now
      static size_t sHighWaterCount;  // greatest number of |nsXPIDLCString|s alive at once
      static size_t sAssignCount;     // total number of times |nsXPIDLCString|s were
                                      //  assigned into with |getter_Copies|
      static size_t sShareCount;      // total number times |nsXPIDLCString|s were asked to share 

    public:
      static void DebugPrintStats( FILE* );
#endif
  };

inline
nsXPIDLCString::getter_Copies_t
getter_Copies( nsXPIDLCString& aString )
  {
    return nsXPIDLCString::getter_Copies_t(aString);
  }

#endif /* !defined(nsXPIDLString_h___) */
