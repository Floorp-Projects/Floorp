/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIContentViewerEdit.idl
 */

#ifndef __gen_nsIContentViewerEdit_h__
#define __gen_nsIContentViewerEdit_h__

#include "nsISupports.h"
#include "nsrootidl.h"

/* starting interface:    nsIContentViewerEdit */

#define NS_ICONTENTVIEWEREDIT_IID_STR "42d5215c-9bc7-11d3-bccc-0060b0fc76bd"

#define NS_ICONTENTVIEWEREDIT_IID \
  {0x42d5215c, 0x9bc7, 0x11d3, \
    { 0xbc, 0xcc, 0x00, 0x60, 0xb0, 0xfc, 0x76, 0xbd }}

class nsIContentViewerEdit : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENTVIEWEREDIT_IID)

  /* void Search (); */
  NS_IMETHOD Search(void) = 0;

  /* readonly attribute boolean searchable; */
  NS_IMETHOD GetSearchable(PRBool *aSearchable) = 0;

  /* void ClearSelection (); */
  NS_IMETHOD ClearSelection(void) = 0;

  /* void SelectAll (); */
  NS_IMETHOD SelectAll(void) = 0;

  /* void CopySelection (); */
  NS_IMETHOD CopySelection(void) = 0;

  /* readonly attribute boolean copyable; */
  NS_IMETHOD GetCopyable(PRBool *aCopyable) = 0;

  /* void CutSelection (); */
  NS_IMETHOD CutSelection(void) = 0;

  /* readonly attribute boolean cutable; */
  NS_IMETHOD GetCutable(PRBool *aCutable) = 0;

  /* void Paste (); */
  NS_IMETHOD Paste(void) = 0;

  /* readonly attribute boolean pasteable; */
  NS_IMETHOD GetPasteable(PRBool *aPasteable) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICONTENTVIEWEREDIT \
  NS_IMETHOD Search(void); \
  NS_IMETHOD GetSearchable(PRBool *aSearchable); \
  NS_IMETHOD ClearSelection(void); \
  NS_IMETHOD SelectAll(void); \
  NS_IMETHOD CopySelection(void); \
  NS_IMETHOD GetCopyable(PRBool *aCopyable); \
  NS_IMETHOD CutSelection(void); \
  NS_IMETHOD GetCutable(PRBool *aCutable); \
  NS_IMETHOD Paste(void); \
  NS_IMETHOD GetPasteable(PRBool *aPasteable); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSICONTENTVIEWEREDIT(_to) \
  NS_IMETHOD Search(void) { return _to ## Search(); } \
  NS_IMETHOD GetSearchable(PRBool *aSearchable) { return _to ## GetSearchable(aSearchable); } \
  NS_IMETHOD ClearSelection(void) { return _to ## ClearSelection(); } \
  NS_IMETHOD SelectAll(void) { return _to ## SelectAll(); } \
  NS_IMETHOD CopySelection(void) { return _to ## CopySelection(); } \
  NS_IMETHOD GetCopyable(PRBool *aCopyable) { return _to ## GetCopyable(aCopyable); } \
  NS_IMETHOD CutSelection(void) { return _to ## CutSelection(); } \
  NS_IMETHOD GetCutable(PRBool *aCutable) { return _to ## GetCutable(aCutable); } \
  NS_IMETHOD Paste(void) { return _to ## Paste(); } \
  NS_IMETHOD GetPasteable(PRBool *aPasteable) { return _to ## GetPasteable(aPasteable); } 


#endif /* __gen_nsIContentViewerEdit_h__ */
