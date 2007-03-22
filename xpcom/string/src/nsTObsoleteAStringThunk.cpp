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


class nsTObsoleteAStringThunk_CharT : public nsTObsoleteAString_CharT
  {
    public:
      typedef nsTObsoleteAStringThunk_CharT    self_type; 
      typedef nsTSubstring_CharT               substring_type;

    public:

      nsTObsoleteAStringThunk_CharT() {}


      static const void* get_vptr()
        {
          const void* result;
          new (&result) self_type();
          return result;
        }


        /**
         * we are a nsTSubstring in disguise!
         */

            substring_type* concrete_self()       { return NS_REINTERPRET_CAST(      substring_type*, this); }
      const substring_type* concrete_self() const { return NS_REINTERPRET_CAST(const substring_type*, this); }


        /**
         * all virtual methods need to be redirected to appropriate nsString methods
         */

      virtual ~nsTObsoleteAStringThunk_CharT()
        {
          concrete_self()->Finalize();
        }

      virtual PRUint32 GetImplementationFlags() const
        {
          return 0;
        }

      virtual const buffer_handle_type* GetFlatBufferHandle() const
        {
          return (const buffer_handle_type*) (concrete_self()->IsTerminated() != PR_FALSE);
        }

      virtual const buffer_handle_type*  GetBufferHandle() const
        {
          return 0;
        }

      virtual const shared_buffer_handle_type* GetSharedBufferHandle() const
        {
          return 0;
        }

      virtual size_type Length() const
        {
          return concrete_self()->Length();
        }

      virtual PRBool IsVoid() const
        {
          return concrete_self()->IsVoid();
        }

      virtual void SetIsVoid(PRBool val)
        {
          concrete_self()->SetIsVoid(val);
        }

      virtual void SetCapacity(size_type size)
        {
          concrete_self()->SetCapacity(size);
        }

      virtual void SetLength(size_type size)
        {
          concrete_self()->SetLength(size);
        }

      virtual void Cut(index_type cutStart, size_type cutLength)
        {
          concrete_self()->Cut(cutStart, cutLength);
        }

      virtual void do_AssignFromReadable(const abstract_string_type &s)
        {
          concrete_self()->Assign(s);
        }

      virtual void do_AssignFromElementPtr(const char_type *data)
        {
          concrete_self()->Assign(data);
        }

      virtual void do_AssignFromElementPtrLength(const char_type *data, size_type length)
        {
          concrete_self()->Assign(data, length);
        }

      virtual void do_AssignFromElement(char_type c)
        {
          concrete_self()->Assign(c);
        }

      virtual void do_AppendFromReadable(const abstract_string_type &s)
        {
          concrete_self()->Append(s);
        }

      virtual void do_AppendFromElementPtr(const char_type *data)
        {
          concrete_self()->Append(data);
        }

      virtual void do_AppendFromElementPtrLength(const char_type *data, size_type length)
        {
          concrete_self()->Append(data, length);
        }

      virtual void do_AppendFromElement(char_type c)
        {
          concrete_self()->Append(c);
        }

      virtual void do_InsertFromReadable(const abstract_string_type &s, index_type pos)
        {
          concrete_self()->Insert(s, pos);
        }

      virtual void do_InsertFromElementPtr(const char_type *data, index_type pos)
        {
          concrete_self()->Insert(data, pos);
        }

      virtual void do_InsertFromElementPtrLength(const char_type *data, index_type pos, size_type length)
        {
          concrete_self()->Insert(data, pos, length);
        }

      virtual void do_InsertFromElement(char_type c, index_type pos)
        {
          concrete_self()->Insert(c, pos);
        }

      virtual void do_ReplaceFromReadable(index_type cutStart, size_type cutLength, const abstract_string_type &s)
        {
          concrete_self()->Replace(cutStart, cutLength, s);
        }

      virtual const char_type *GetReadableFragment(const_fragment_type& frag, nsFragmentRequest which, PRUint32 offset) const
        {
          const substring_type* s = concrete_self();
          switch (which)
            {
              case kFirstFragment:
              case kLastFragment:
              case kFragmentAt:
                frag.mStart = s->Data();
                frag.mEnd = frag.mStart + s->Length();
                return frag.mStart + offset;
              case kPrevFragment:
              case kNextFragment:
              default:
                return 0;
            }
        }

      virtual char_type *GetWritableFragment(fragment_type& frag, nsFragmentRequest which, PRUint32 offset)
        {
          substring_type* s = concrete_self();
          switch (which)
            {
              case kFirstFragment:
              case kLastFragment:
              case kFragmentAt:
                char_type* start;
                s->BeginWriting(start);
                frag.mStart = start;
                frag.mEnd = start + s->Length();
                return frag.mStart + offset;
              case kPrevFragment:
              case kNextFragment:
              default:
                return 0;
            }
        }
  };


  /**
   * initialize the pointer to the canonical vtable...
   */

const void *nsTObsoleteAString_CharT::sCanonicalVTable = nsTObsoleteAStringThunk_CharT::get_vptr();
