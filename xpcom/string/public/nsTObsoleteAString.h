/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


  /**
   * nsTObsoleteAString_CharT : binary compatible with old nsAC?String vtable
   *
   * @status FROZEN
   */
class nsTObsoleteAString_CharT
  {
    public:
      /**
       * This is holds the address of the vtable for the canonical string
       * implementation (i.e., nsTString).
       */
      NS_COM static const void *sCanonicalVTable;

        /**
         * An |nsFragmentRequest| is used to tell |GetReadableFragment| and
         * |GetWritableFragment| what to do.
         *
         * @see GetReadableFragment
         */
      enum nsFragmentRequest { kPrevFragment, kFirstFragment, kLastFragment, kNextFragment, kFragmentAt };

        /**
         * A |nsReadableFragment| provides |const| access to a contiguous hunk of
         * string of homogenous units, e.g., bytes (|char|).  This doesn't mean it
         * represents a flat hunk.  It could be a variable length encoding, for
         * instance UTF-8.  And the fragment itself need not be zero-terminated.
         *
         * An |nsReadableFragment| is the underlying machinery that lets
         * |nsReadingIterator|s work.
         *
         * @see nsReadingIterator
         * @status FROZEN
         */
      struct nsReadableFragment
        {
          const CharT*  mStart;
          const CharT*  mEnd;
          const void*   mFragmentIdentifier;

          nsReadableFragment() : mStart(0), mEnd(0), mFragmentIdentifier(0) {}
        };


        /**
         * A |nsWritableFragment| provides non-|const| access to a contiguous hunk of
         * string of homogenous units, e.g., bytes (|char|).  This doesn't mean it
         * represents a flat hunk.  It could be a variable length encoding, for
         * instance UTF-8.  And the fragment itself need not be zero-terminated.
         *
         * An |nsWritableFragment| is the underlying machinery that lets
         * |nsWritingIterator|s work.
         *
         * @see nsWritingIterator
         * @status FROZEN
         */
      struct nsWritableFragment
        {
          CharT*    mStart;
          CharT*    mEnd;
          void*     mFragmentIdentifier;

          nsWritableFragment() : mStart(0), mEnd(0), mFragmentIdentifier(0) {}
        };

    protected:

      typedef CharT                                char_type;

      typedef void                                 buffer_handle_type;
      typedef void                                 shared_buffer_handle_type;
      typedef nsReadableFragment                   const_fragment_type;
      typedef nsWritableFragment                   fragment_type;

      typedef nsTAString_CharT                     abstract_string_type;

      typedef PRUint32                             size_type;
      typedef PRUint32                             index_type;

    protected:

      friend class nsTAString_CharT;
      friend class nsTSubstring_CharT;

      /** here's the old nsAC?String vtable **/

      virtual ~nsTObsoleteAString_CharT() { }

      virtual PRUint32                          GetImplementationFlags() const = 0;
      virtual const        buffer_handle_type*  GetFlatBufferHandle()    const = 0;
      virtual const        buffer_handle_type*  GetBufferHandle()        const = 0;
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle()  const = 0;

      virtual size_type Length() const = 0;

      virtual PRBool IsVoid() const = 0;
      virtual void SetIsVoid( PRBool ) = 0;

      virtual void SetCapacity( size_type ) = 0;
      virtual void SetLength( size_type ) = 0;

      virtual void Cut( index_type cutStart, size_type cutLength ) = 0;

      virtual void do_AssignFromReadable( const abstract_string_type& ) = 0;
      virtual void do_AssignFromElementPtr( const char_type* ) = 0;
      virtual void do_AssignFromElementPtrLength( const char_type*, size_type ) = 0;
      virtual void do_AssignFromElement( char_type ) = 0;

      virtual void do_AppendFromReadable( const abstract_string_type& ) = 0;
      virtual void do_AppendFromElementPtr( const char_type* ) = 0;
      virtual void do_AppendFromElementPtrLength( const char_type*, size_type ) = 0;
      virtual void do_AppendFromElement( char_type ) = 0;

      virtual void do_InsertFromReadable( const abstract_string_type&, index_type ) = 0;
      virtual void do_InsertFromElementPtr( const char_type*, index_type ) = 0;
      virtual void do_InsertFromElementPtrLength( const char_type*, index_type, size_type ) = 0;
      virtual void do_InsertFromElement( char_type, index_type ) = 0;

      virtual void do_ReplaceFromReadable( index_type, size_type, const abstract_string_type& ) = 0;

      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 = 0 ) const = 0;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 = 0 ) = 0;
  };

  // forward declare implementation
class nsTObsoleteAStringThunk_CharT;
