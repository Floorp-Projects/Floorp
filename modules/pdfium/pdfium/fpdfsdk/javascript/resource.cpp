// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/resource.h"

CFX_WideString JSGetStringFromID(uint32_t id) {
  switch (id) {
    case IDS_STRING_JSALERT:
      return L"Alert";
    case IDS_STRING_JSPARAMERROR:
      return L"Incorrect number of parameters passed to function.";
    case IDS_STRING_JSAFNUMBER_KEYSTROKE:
      return L"The input value is invalid.";
    case IDS_STRING_JSPARAM_TOOLONG:
      return L"The input value is too long.";
    case IDS_STRING_JSPARSEDATE:
      return L"The input value can't be parsed as a valid date/time (%s).";
    case IDS_STRING_JSRANGE1:
      return L"The input value must be greater than or equal to %s"
             L" and less than or equal to %s.";
    case IDS_STRING_JSRANGE2:
      return L"The input value must be greater than or equal to %s.";
    case IDS_STRING_JSRANGE3:
      return L"The input value must be less than or equal to %s.";
    case IDS_STRING_JSNOTSUPPORT:
      return L"Operation not supported.";
    case IDS_STRING_JSBUSY:
      return L"System is busy.";
    case IDS_STRING_JSEVENT:
      return L"Duplicate formfield event found.";
    case IDS_STRING_RUN:
      return L"Script ran successfully.";
    case IDS_STRING_JSPRINT1:
      return L"The second parameter can't be converted to a Date.";
    case IDS_STRING_JSPRINT2:
      return L"The second parameter is an invalid Date!";
    case IDS_STRING_JSNOGLOBAL:
      return L"Global value not found.";
    case IDS_STRING_JSREADONLY:
      return L"Cannot assign to readonly property.";
    case IDS_STRING_JSTYPEERROR:
      return L"Incorrect parameter type.";
    case IDS_STRING_JSVALUEERROR:
      return L"Incorrect parameter value.";
    case IDS_STRING_JSNOPERMISSION:
      return L"Permission denied.";
    case IDS_STRING_JSBADOBJECT:
      return L"Object no longer exists.";
    default:
      return L"";
  }
}

CFX_WideString JSFormatErrorString(const char* class_name,
                                   const char* property_name,
                                   const CFX_WideString& details) {
  CFX_WideString result = CFX_WideString::FromLocal(class_name);
  if (property_name) {
    result += L".";
    result += CFX_WideString::FromLocal(property_name);
  }
  result += L": ";
  result += details;
  return result;
}
