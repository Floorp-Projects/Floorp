/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMarkupDocumentViewer.idl
 */

#ifndef __gen_nsIMarkupDocumentViewer_h__
#define __gen_nsIMarkupDocumentViewer_h__

#include "nsISupports.h"
#include "domstubs.h"
#include "nsrootidl.h"

/* starting interface:    nsIMarkupDocumentViewer */

#define NS_IMARKUPDOCUMENTVIEWER_IID_STR "69e5de03-7b8b-11d3-af61-00a024ffc08c"

#define NS_IMARKUPDOCUMENTVIEWER_IID \
  {0x69e5de03, 0x7b8b, 0x11d3, \
    { 0xaf, 0x61, 0x00, 0xa0, 0x24, 0xff, 0xc0, 0x8c }}

class nsIMarkupDocumentViewer : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMARKUPDOCUMENTVIEWER_IID)

  /* void scrollToNode (in nsIDOMNode node); */
  NS_IMETHOD ScrollToNode(nsIDOMNode *node) = 0;

  /* attribute boolean allowPlugins; */
  NS_IMETHOD GetAllowPlugins(PRBool *aAllowPlugins) = 0;
  NS_IMETHOD SetAllowPlugins(PRBool aAllowPlugins) = 0;

  /* attribute boolean isFrame; */
  NS_IMETHOD GetIsFrame(PRBool *aIsFrame) = 0;
  NS_IMETHOD SetIsFrame(PRBool aIsFrame) = 0;

  /* attribute wstring defaultCharacterSet; */
  NS_IMETHOD GetDefaultCharacterSet(PRUnichar * *aDefaultCharacterSet) = 0;
  NS_IMETHOD SetDefaultCharacterSet(const PRUnichar * aDefaultCharacterSet) = 0;

  /* attribute wstring forceCharacterSet; */
  NS_IMETHOD GetForceCharacterSet(PRUnichar * *aForceCharacterSet) = 0;
  NS_IMETHOD SetForceCharacterSet(const PRUnichar * aForceCharacterSet) = 0;

  /* attribute wstring hintCharacterSet; */
  NS_IMETHOD GetHintCharacterSet(PRUnichar * *aHintCharacterSet) = 0;
  NS_IMETHOD SetHintCharacterSet(const PRUnichar * aHintCharacterSet) = 0;

  /* attribute PRInt32 hintCharacterSetSource; */
  NS_IMETHOD GetHintCharacterSetSource(PRInt32 *aHintCharacterSetSource) = 0;
  NS_IMETHOD SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource) = 0;

  /* void sizeToContent (); */
  NS_IMETHOD SizeToContent(void) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIMARKUPDOCUMENTVIEWER \
  NS_IMETHOD ScrollToNode(nsIDOMNode *node); \
  NS_IMETHOD GetAllowPlugins(PRBool *aAllowPlugins); \
  NS_IMETHOD SetAllowPlugins(PRBool aAllowPlugins); \
  NS_IMETHOD GetIsFrame(PRBool *aIsFrame); \
  NS_IMETHOD SetIsFrame(PRBool aIsFrame); \
  NS_IMETHOD GetDefaultCharacterSet(PRUnichar * *aDefaultCharacterSet); \
  NS_IMETHOD SetDefaultCharacterSet(const PRUnichar * aDefaultCharacterSet); \
  NS_IMETHOD GetForceCharacterSet(PRUnichar * *aForceCharacterSet); \
  NS_IMETHOD SetForceCharacterSet(const PRUnichar * aForceCharacterSet); \
  NS_IMETHOD GetHintCharacterSet(PRUnichar * *aHintCharacterSet); \
  NS_IMETHOD SetHintCharacterSet(const PRUnichar * aHintCharacterSet); \
  NS_IMETHOD GetHintCharacterSetSource(PRInt32 *aHintCharacterSetSource); \
  NS_IMETHOD SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource); \
  NS_IMETHOD SizeToContent(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIMARKUPDOCUMENTVIEWER(_to) \
  NS_IMETHOD ScrollToNode(nsIDOMNode *node) { return _to ## ScrollToNode(node); } \
  NS_IMETHOD GetAllowPlugins(PRBool *aAllowPlugins) { return _to ## GetAllowPlugins(aAllowPlugins); } \
  NS_IMETHOD SetAllowPlugins(PRBool aAllowPlugins) { return _to ## SetAllowPlugins(aAllowPlugins); } \
  NS_IMETHOD GetIsFrame(PRBool *aIsFrame) { return _to ## GetIsFrame(aIsFrame); } \
  NS_IMETHOD SetIsFrame(PRBool aIsFrame) { return _to ## SetIsFrame(aIsFrame); } \
  NS_IMETHOD GetDefaultCharacterSet(PRUnichar * *aDefaultCharacterSet) { return _to ## GetDefaultCharacterSet(aDefaultCharacterSet); } \
  NS_IMETHOD SetDefaultCharacterSet(const PRUnichar * aDefaultCharacterSet) { return _to ## SetDefaultCharacterSet(aDefaultCharacterSet); } \
  NS_IMETHOD GetForceCharacterSet(PRUnichar * *aForceCharacterSet) { return _to ## GetForceCharacterSet(aForceCharacterSet); } \
  NS_IMETHOD SetForceCharacterSet(const PRUnichar * aForceCharacterSet) { return _to ## SetForceCharacterSet(aForceCharacterSet); } \
  NS_IMETHOD GetHintCharacterSet(PRUnichar * *aHintCharacterSet) { return _to ## GetHintCharacterSet(aHintCharacterSet); } \
  NS_IMETHOD SetHintCharacterSet(const PRUnichar * aHintCharacterSet) { return _to ## SetHintCharacterSet(aHintCharacterSet); } \
  NS_IMETHOD GetHintCharacterSetSource(PRInt32 *aHintCharacterSetSource) { return _to ## GetHintCharacterSetSource(aHintCharacterSetSource); } \
  NS_IMETHOD SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource) { return _to ## SetHintCharacterSetSource(aHintCharacterSetSource); } \
  NS_IMETHOD SizeToContent(void) { return _to ## SizeToContent(); } 


#endif /* __gen_nsIMarkupDocumentViewer_h__ */
