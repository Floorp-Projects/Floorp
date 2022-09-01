/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

#if defined(_WIN32) || defined(_WINDOWS)
#  pragma pack(push, cryptoki, 1)
#endif

#define CK_PTR *
#define CK_DECLARE_FUNCTION(returnType, name) returnType name
#define CK_DECLARE_FUNCTION_POINTER(returnType, name) returnType(*name)
#define CK_CALLBACK_FUNCTION(returnType, name) returnType(*name)
#define NULL_PTR 0

#include "pkcs11.h"

#if defined(_WIN32) || defined(_WINDOWS)
#  pragma pack(pop, cryptoki)
#endif
