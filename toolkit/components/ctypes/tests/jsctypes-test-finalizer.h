/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define EXPORT_CDECL(type)   NS_EXPORT type

NS_EXTERN_C
{
  EXPORT_CDECL(void) test_finalizer_start(size_t size);
  EXPORT_CDECL(void) test_finalizer_stop();
  EXPORT_CDECL(bool) test_finalizer_resource_is_acquired(size_t i);

  EXPORT_CDECL(size_t) test_finalizer_acq_size_t(size_t i);
  EXPORT_CDECL(void) test_finalizer_rel_size_t(size_t i);
  EXPORT_CDECL(size_t) test_finalizer_rel_size_t_return_size_t(size_t i);
  EXPORT_CDECL(RECT) test_finalizer_rel_size_t_return_struct_t(size_t i);
  EXPORT_CDECL(bool) test_finalizer_cmp_size_t(size_t a, size_t b);

  EXPORT_CDECL(int32_t) test_finalizer_acq_int32_t(size_t i);
  EXPORT_CDECL(void) test_finalizer_rel_int32_t(int32_t i);
  EXPORT_CDECL(bool) test_finalizer_cmp_int32_t(int32_t a, int32_t b);

  EXPORT_CDECL(int64_t) test_finalizer_acq_int64_t(size_t i);
  EXPORT_CDECL(void) test_finalizer_rel_int64_t(int64_t i);
  EXPORT_CDECL(bool) test_finalizer_cmp_int64_t(int64_t a, int64_t b);

  EXPORT_CDECL(void*) test_finalizer_acq_ptr_t(size_t i);
  EXPORT_CDECL(void) test_finalizer_rel_ptr_t(void *i);
  EXPORT_CDECL(bool) test_finalizer_cmp_ptr_t(void *a, void *b);

  EXPORT_CDECL(int32_t*) test_finalizer_acq_int32_ptr_t(size_t i);
  EXPORT_CDECL(void) test_finalizer_rel_int32_ptr_t(int32_t *i);
  EXPORT_CDECL(bool) test_finalizer_cmp_int32_ptr_t(int32_t *a, int32_t *b);

  EXPORT_CDECL(char*) test_finalizer_acq_string_t(int i);
  EXPORT_CDECL(void) test_finalizer_rel_string_t(char *i);
  EXPORT_CDECL(bool) test_finalizer_cmp_string_t(char *a, char *b);

  EXPORT_CDECL(void*) test_finalizer_acq_null_t(size_t i);
  EXPORT_CDECL(void) test_finalizer_rel_null_t(void *i);
  EXPORT_CDECL(bool) test_finalizer_cmp_null_t(void *a, void *b);
  EXPORT_CDECL(bool) test_finalizer_null_resource_is_acquired(size_t i);

  EXPORT_CDECL(RECT) test_finalizer_acq_struct_t(int i);
  EXPORT_CDECL(void) test_finalizer_rel_struct_t(RECT i);
  EXPORT_CDECL(bool) test_finalizer_cmp_struct_t(RECT a, RECT b);

  typedef void (*afun)(size_t);
  EXPORT_CDECL(afun*) test_finalizer_rel_null_function();

  EXPORT_CDECL(void) test_finalizer_rel_size_t_set_errno(size_t i);
  EXPORT_CDECL(void) reset_errno();

}
