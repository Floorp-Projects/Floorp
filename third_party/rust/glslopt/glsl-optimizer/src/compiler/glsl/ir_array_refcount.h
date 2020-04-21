/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file ir_array_refcount.h
 *
 * Provides a visitor which produces a list of variables referenced.
 */

#ifndef GLSL_IR_ARRAY_REFCOUNT_H
#define GLSL_IR_ARRAY_REFCOUNT_H

#include "ir.h"
#include "ir_visitor.h"
#include "compiler/glsl_types.h"
#include "util/bitset.h"

/**
 * Describes an access of an array element or an access of the whole array
 */
struct array_deref_range {
   /**
    * Index that was accessed.
    *
    * All valid array indices are less than the size of the array.  If index
    * is equal to the size of the array, this means the entire array has been
    * accessed (e.g., due to use of a non-constant index).
    */
   unsigned index;

   /** Size of the array.  Used for offset calculations. */
   unsigned size;
};

class ir_array_refcount_entry
{
public:
   ir_array_refcount_entry(ir_variable *var);
   ~ir_array_refcount_entry();

   ir_variable *var; /* The key: the variable's pointer. */

   /** Has the variable been referenced? */
   bool is_referenced;

   /**
    * Mark a set of array elements as accessed.
    *
    * If every \c array_deref_range is for a single index, only a single
    * element will be marked.  If any \c array_deref_range is for an entire
    * array-of-, then multiple elements will be marked.
    *
    * Items in the \c array_deref_range list appear in least- to
    * most-significant order.  This is the \b opposite order the indices
    * appear in the GLSL shader text.  An array access like
    *
    *     x = y[1][i][3];
    *
    * would appear as
    *
    *     { { 3, n }, { m, m }, { 1, p } }
    *
    * where n, m, and p are the sizes of the arrays-of-arrays.
    *
    * The set of marked array elements can later be queried by
    * \c ::is_linearized_index_referenced.
    *
    * \param dr     List of array_deref_range elements to be processed.
    * \param count  Number of array_deref_range elements to be processed.
    */
   void mark_array_elements_referenced(const array_deref_range *dr,
                                       unsigned count);

   /** Has a linearized array index been referenced? */
   bool is_linearized_index_referenced(unsigned linearized_index) const
   {
      assert(bits != 0);
      assert(linearized_index <= num_bits);

      return BITSET_TEST(bits, linearized_index);
   }

private:
   /** Set of bit-flags to note which array elements have been accessed. */
   BITSET_WORD *bits;

   /**
    * Total number of bits referenced by \c bits.
    *
    * Also the total number of array(s-of-arrays) elements of \c var.
    */
   unsigned num_bits;

   /** Count of nested arrays in the type. */
   unsigned array_depth;

   /**
    * Recursive part of the public mark_array_elements_referenced method.
    *
    * The recursion occurs when an entire array-of- is accessed.  See the
    * implementation for more details.
    *
    * \param dr                List of array_deref_range elements to be
    *                          processed.
    * \param count             Number of array_deref_range elements to be
    *                          processed.
    * \param scale             Current offset scale.
    * \param linearized_index  Current accumulated linearized array index.
    */
   void mark_array_elements_referenced(const array_deref_range *dr,
                                       unsigned count,
                                       unsigned scale,
                                       unsigned linearized_index);

   friend class array_refcount_test;
};

class ir_array_refcount_visitor : public ir_hierarchical_visitor {
public:
   ir_array_refcount_visitor(void);
   ~ir_array_refcount_visitor(void);

   virtual ir_visitor_status visit(ir_dereference_variable *);

   virtual ir_visitor_status visit_enter(ir_function_signature *);
   virtual ir_visitor_status visit_enter(ir_dereference_array *);

   /**
    * Find variable in the hash table, and insert it if not present
    */
   ir_array_refcount_entry *get_variable_entry(ir_variable *var);

   /**
    * Hash table mapping ir_variable to ir_array_refcount_entry.
    */
   struct hash_table *ht;

   void *mem_ctx;

private:
   /** Get an array_deref_range element from private tracking. */
   array_deref_range *get_array_deref();

   /**
    * Last ir_dereference_array that was visited
    *
    * Used to prevent some redundant calculations.
    *
    * \sa ::visit_enter(ir_dereference_array *)
    */
   ir_dereference_array *last_array_deref;

   /**
    * \name array_deref_range tracking
    */
   /*@{*/
   /** Currently allocated block of derefs. */
   array_deref_range *derefs;

   /** Number of derefs used in current processing. */
   unsigned num_derefs;

   /** Size of the derefs buffer in bytes. */
   unsigned derefs_size;
   /*@}*/
};

#endif /* GLSL_IR_ARRAY_REFCOUNT_H */
