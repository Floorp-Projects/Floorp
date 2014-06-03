/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShimInterfaceInfo.h"

#include "nsIDOMAnimationEvent.h"
#include "nsIDOMAttr.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMClientRect.h"
#include "nsIDOMClientRectList.h"
#include "nsIDOMClipboardEvent.h"
#include "nsIDOMCloseEvent.h"
#include "nsIDOMCommandEvent.h"
#include "nsIDOMComment.h"
#include "nsIDOMCompositionEvent.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSValueList.h"
#include "nsIDOMCustomEvent.h"
#include "nsIDOMDataContainerEvent.h"
#ifdef MOZ_WEBRTC
#include "nsIDOMDataChannel.h"
#endif
#include "nsIDOMDataTransfer.h"
#include "nsIDOMDeviceOrientationEvent.h"
#include "nsIDOMDeviceStorage.h"
#include "nsIDOMDeviceStorageChangeEvent.h"
#include "nsIDOMDOMCursor.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMDOMTransactionEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMDragEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementReplaceEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMFileList.h"
#include "nsIDOMFileReader.h"
#include "nsIDOMFocusEvent.h"
#include "nsIDOMFormData.h"
#include "nsIDOMGeoPositionError.h"
#include "nsIDOMHashChangeEvent.h"
#include "nsIDOMHistory.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLAudioElement.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMHTMLHeadElement.h"
#include "nsIDOMHTMLHeadingElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLIElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMHTMLMenuItemElement.h"
#include "nsIDOMHTMLMetaElement.h"
#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMHTMLParagraphElement.h"
#include "nsIDOMHTMLPreElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsIDOMHTMLStyleElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLTitleElement.h"
#include "nsIDOMHTMLUListElement.h"
#include "nsIDOMHTMLVideoElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMediaError.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMMediaStream.h"
#include "nsIDOMMessageEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMMouseScrollEvent.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDOMMozApplicationEvent.h"
#include "nsIDOMMozMmsEvent.h"
#include "nsIDOMMozNamedAttrMap.h"
#include "nsIDOMMozSettingsEvent.h"
#include "nsIDOMMozSmsEvent.h"
#ifdef MOZ_B2G_RIL
#include "nsIDOMMozVoicemailEvent.h"
#endif
#ifdef MOZ_WIDGET_GONK
#include "nsIDOMMozWifiConnectionInfoEvent.h"
#include "nsIDOMMozWifiStatusChangeEvent.h"
#include "nsIDOMMozWifiP2pStatusChangeEvent.h"
#endif
#include "nsIDOMNode.h"
#include "nsIDOMNodeIterator.h"
#include "nsIDOMNotifyPaintEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDOMPageTransitionEvent.h"
#include "nsIDOMPaintRequest.h"
#include "nsIDOMParser.h"
#include "nsIDOMPopStateEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMRange.h"
#include "nsIDOMRecordErrorEvent.h"
#include "nsIDOMRect.h"
#include "nsIDOMScreen.h"
#include "nsIDOMScrollAreaEvent.h"
#include "nsIDOMSerializer.h"
#include "nsIDOMSimpleGestureEvent.h"
#include "nsIDOMSmartCardEvent.h"
#ifdef MOZ_WEBSPEECH
#include "nsIDOMSpeechRecognitionEvent.h"
#include "nsIDOMSpeechSynthesisEvent.h"
#endif // MOZ_WEBSPEECH
#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMStyleRuleChangeEvent.h"
#include "nsIDOMStyleSheetApplicableStateChangeEvent.h"
#include "nsIDOMStyleSheetChangeEvent.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMText.h"
#include "nsIDOMTimeEvent.h"
#include "nsIDOMTimeRanges.h"
#include "nsIDOMTouchEvent.h"
#include "nsIDOMTransitionEvent.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMValidityState.h"
#include "nsIDOMWheelEvent.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsISelection.h"
#include "nsIXMLHttpRequest.h"

#include "mozilla/dom/AnimationEventBinding.h"
#include "mozilla/dom/AttrBinding.h"
#include "mozilla/dom/BeforeUnloadEventBinding.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CDATASectionBinding.h"
#include "mozilla/dom/CharacterDataBinding.h"
#include "mozilla/dom/DOMRectBinding.h"
#include "mozilla/dom/DOMRectListBinding.h"
#include "mozilla/dom/ClipboardEventBinding.h"
#include "mozilla/dom/CloseEventBinding.h"
#include "mozilla/dom/CommandEventBinding.h"
#include "mozilla/dom/CommentBinding.h"
#include "mozilla/dom/CompositionEventBinding.h"
#include "mozilla/dom/CSSPrimitiveValueBinding.h"
#include "mozilla/dom/CSSStyleDeclarationBinding.h"
#include "mozilla/dom/CSSStyleSheetBinding.h"
#include "mozilla/dom/CSSValueBinding.h"
#include "mozilla/dom/CSSValueListBinding.h"
#include "mozilla/dom/CustomEventBinding.h"
#ifdef MOZ_WEBRTC
#include "mozilla/dom/DataChannelBinding.h"
#endif
#include "mozilla/dom/DataContainerEventBinding.h"
#include "mozilla/dom/DataTransferBinding.h"
#include "mozilla/dom/DeviceOrientationEventBinding.h"
#include "mozilla/dom/DeviceStorageBinding.h"
#include "mozilla/dom/DeviceStorageChangeEventBinding.h"
#include "mozilla/dom/DOMCursorBinding.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/DOMParserBinding.h"
#include "mozilla/dom/DOMRequestBinding.h"
#include "mozilla/dom/DOMTransactionEventBinding.h"
#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/dom/DocumentFragmentBinding.h"
#include "mozilla/dom/DocumentTypeBinding.h"
#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/dom/DragEventBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/ElementReplaceEventBinding.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/FileReaderBinding.h"
#include "mozilla/dom/FocusEventBinding.h"
#include "mozilla/dom/FormDataBinding.h"
#include "mozilla/dom/HashChangeEventBinding.h"
#include "mozilla/dom/HistoryBinding.h"
#include "mozilla/dom/HTMLAnchorElementBinding.h"
#include "mozilla/dom/HTMLAppletElementBinding.h"
#include "mozilla/dom/HTMLAreaElementBinding.h"
#include "mozilla/dom/HTMLAudioElementBinding.h"
#include "mozilla/dom/HTMLBRElementBinding.h"
#include "mozilla/dom/HTMLBaseElementBinding.h"
#include "mozilla/dom/HTMLBodyElementBinding.h"
#include "mozilla/dom/HTMLButtonElementBinding.h"
#include "mozilla/dom/HTMLCanvasElementBinding.h"
#include "mozilla/dom/HTMLCollectionBinding.h"
#include "mozilla/dom/HTMLDirectoryElementBinding.h"
#include "mozilla/dom/HTMLDivElementBinding.h"
#include "mozilla/dom/HTMLDocumentBinding.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/HTMLFieldSetElementBinding.h"
#include "mozilla/dom/HTMLFormElementBinding.h"
#include "mozilla/dom/HTMLFrameElementBinding.h"
#include "mozilla/dom/HTMLFrameSetElementBinding.h"
#include "mozilla/dom/HTMLHRElementBinding.h"
#include "mozilla/dom/HTMLHeadElementBinding.h"
#include "mozilla/dom/HTMLHeadingElementBinding.h"
#include "mozilla/dom/HTMLHtmlElementBinding.h"
#include "mozilla/dom/HTMLIFrameElementBinding.h"
#include "mozilla/dom/HTMLImageElementBinding.h"
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "mozilla/dom/HTMLLIElementBinding.h"
#include "mozilla/dom/HTMLLabelElementBinding.h"
#include "mozilla/dom/HTMLLinkElementBinding.h"
#include "mozilla/dom/HTMLMapElementBinding.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "mozilla/dom/HTMLMenuElementBinding.h"
#include "mozilla/dom/HTMLMenuItemElementBinding.h"
#include "mozilla/dom/HTMLMetaElementBinding.h"
#include "mozilla/dom/HTMLOListElementBinding.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLOptGroupElementBinding.h"
#include "mozilla/dom/HTMLOptionElementBinding.h"
#include "mozilla/dom/HTMLOptionsCollectionBinding.h"
#include "mozilla/dom/HTMLParagraphElementBinding.h"
#include "mozilla/dom/HTMLPreElementBinding.h"
#include "mozilla/dom/HTMLQuoteElementBinding.h"
#include "mozilla/dom/HTMLScriptElementBinding.h"
#include "mozilla/dom/HTMLSelectElementBinding.h"
#include "mozilla/dom/HTMLSourceElementBinding.h"
#include "mozilla/dom/HTMLStyleElementBinding.h"
#include "mozilla/dom/HTMLTableCaptionElementBinding.h"
#include "mozilla/dom/HTMLTableCellElementBinding.h"
#include "mozilla/dom/HTMLTableElementBinding.h"
#include "mozilla/dom/HTMLTextAreaElementBinding.h"
#include "mozilla/dom/HTMLTitleElementBinding.h"
#include "mozilla/dom/HTMLUListElementBinding.h"
#include "mozilla/dom/HTMLVideoElementBinding.h"
#include "mozilla/dom/KeyEventBinding.h"
#include "mozilla/dom/LocalMediaStreamBinding.h"
#include "mozilla/dom/MediaErrorBinding.h"
#include "mozilla/dom/MediaListBinding.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/MouseScrollEventBinding.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/MozApplicationEventBinding.h"
#include "mozilla/dom/MozMmsEventBinding.h"
#include "mozilla/dom/MozNamedAttrMapBinding.h"
#include "mozilla/dom/MozSettingsEventBinding.h"
#include "mozilla/dom/MozSmsEventBinding.h"
#ifdef MOZ_B2G_RIL
#include "mozilla/dom/MozVoicemailEventBinding.h"
#endif
#ifdef MOZ_WIDGET_GONK
#include "mozilla/dom/MozWifiConnectionInfoEventBinding.h"
#include "mozilla/dom/MozWifiStatusChangeEventBinding.h"
#include "mozilla/dom/MozWifiP2pStatusChangeEventBinding.h"
#endif
#include "mozilla/dom/NodeIteratorBinding.h"
#include "mozilla/dom/NodeBinding.h"
#include "mozilla/dom/NotifyPaintEventBinding.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/OfflineResourceListBinding.h"
#include "mozilla/dom/PageTransitionEventBinding.h"
#include "mozilla/dom/PaintRequestBinding.h"
#include "mozilla/dom/PopStateEventBinding.h"
#include "mozilla/dom/PopupBlockedEventBinding.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "mozilla/dom/ProcessingInstructionBinding.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/RecordErrorEventBinding.h"
#include "mozilla/dom/RectBinding.h"
#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/dom/SelectionBinding.h"
#include "mozilla/dom/ScrollAreaEventBinding.h"
#include "mozilla/dom/SimpleGestureEventBinding.h"
#include "mozilla/dom/SmartCardEventBinding.h"
#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechRecognitionEventBinding.h"
#include "mozilla/dom/SpeechSynthesisEventBinding.h"
#endif // MOZ_WEBSPEECH
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/StyleSheetBinding.h"
#include "mozilla/dom/StyleSheetListBinding.h"
#include "mozilla/dom/StyleRuleChangeEventBinding.h"
#include "mozilla/dom/StyleSheetApplicableStateChangeEventBinding.h"
#include "mozilla/dom/StyleSheetChangeEventBinding.h"
#include "mozilla/dom/SVGElementBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/TextBinding.h"
#include "mozilla/dom/TimeEventBinding.h"
#include "mozilla/dom/TimeRangesBinding.h"
#include "mozilla/dom/TransitionEventBinding.h"
#include "mozilla/dom/TreeWalkerBinding.h"
#include "mozilla/dom/UIEventBinding.h"
#include "mozilla/dom/ValidityStateBinding.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "mozilla/dom/XMLDocumentBinding.h"
#include "mozilla/dom/XMLHttpRequestEventTargetBinding.h"
#include "mozilla/dom/XMLHttpRequestUploadBinding.h"
#include "mozilla/dom/XMLSerializerBinding.h"
#include "mozilla/dom/XPathEvaluatorBinding.h"
#include "mozilla/dom/XULCommandEventBinding.h"
#include "mozilla/dom/XULDocumentBinding.h"
#include "mozilla/dom/XULElementBinding.h"

using namespace mozilla;

struct ComponentsInterfaceShimEntry {
  const char *geckoName;
  nsIID iid;
  const mozilla::dom::NativePropertyHooks* nativePropHooks;
};

#define DEFINE_SHIM_WITH_CUSTOM_INTERFACE(geckoName, domName) \
  { #geckoName, NS_GET_IID(geckoName), \
     mozilla::dom::domName ## Binding::sNativePropertyHooks }
#define DEFINE_SHIM(name) \
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOM ## name, name)

/**
 * These shim entries allow us to make old XPIDL interfaces implementing DOM
 * APIs as non-scriptable in order to save some runtime memory on Firefox OS,
 * without breaking the entries under Components.interfaces which might both
 * be used by our code and add-ons.  Specifically, the shim entries provide
 * the following:
 *
 * * Components.interfaces.nsIFoo entries.  These entries basically work
 *   almost exactly as the usual ones that you would get through the
 *   XPIDL machinery.  Specifically, they have the right name, they reflect
 *   the right IID, and they will work properly when passed to QueryInterface.
 *
 * * Components.interfaces.nsIFoo.CONSTANT values.  These entries will have
 *   the right name and the right value for most integer types.  Note that
 *   support for non-numerical constants is untested and will probably not
 *   work out of the box.
 *
 * FAQ:
 * * When should I add an entry to the list here?
 *   Only if you're making an XPIDL interfaces which has a corresponding
 *   WebIDL interface non-scriptable.
 * * When should I remove an entry from this list?
 *   If you are completely removing an XPIDL interface from the code base.  If
 *   you forget to do so, the compiler will remind you.
 * * How should I add an entry to the list here?
 *   First, make sure that the XPIDL interface in question is non-scriptable
 *   and also has a corresponding WebIDL interface.  Then, add two include
 *   entries above, one for the XPIDL interface and one for the WebIDL
 *   interface, and add a shim entry below.  If the name of the XPIDL
 *   interface only has an "nsIDOM" prefix prepended to the WebIDL name, you
 *   can use the DEFINE_SHIM macro and pass in the name of the WebIDL
 *   interface.  Otherwise, use DEFINE_SHIM_WITH_CUSTOM_INTERFACE.
 */

const ComponentsInterfaceShimEntry kComponentsInterfaceShimMap[] =
{
  DEFINE_SHIM(AnimationEvent),
  DEFINE_SHIM(Attr),
  DEFINE_SHIM(BeforeUnloadEvent),
  DEFINE_SHIM(CanvasRenderingContext2D),
  DEFINE_SHIM(CDATASection),
  DEFINE_SHIM(CharacterData),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMClientRect, DOMRectReadOnly),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMClientRectList, DOMRectList),
  DEFINE_SHIM(ClipboardEvent),
  DEFINE_SHIM(CloseEvent),
  DEFINE_SHIM(CommandEvent),
  DEFINE_SHIM(Comment),
  DEFINE_SHIM(CompositionEvent),
  DEFINE_SHIM(CSSPrimitiveValue),
  DEFINE_SHIM(CSSStyleDeclaration),
  DEFINE_SHIM(CSSStyleSheet),
  DEFINE_SHIM(CSSValue),
  DEFINE_SHIM(CSSValueList),
  DEFINE_SHIM(CustomEvent),
#ifdef MOZ_WEBRTC
  DEFINE_SHIM(DataChannel),
#endif
  DEFINE_SHIM(DataContainerEvent),
  DEFINE_SHIM(DataTransfer),
  DEFINE_SHIM(DeviceOrientationEvent),
  DEFINE_SHIM(DeviceStorage),
  DEFINE_SHIM(DeviceStorageChangeEvent),
  DEFINE_SHIM(DOMCursor),
  DEFINE_SHIM(DOMException),
  DEFINE_SHIM(DOMRequest),
  DEFINE_SHIM(DOMTransactionEvent),
  DEFINE_SHIM(Document),
  DEFINE_SHIM(DocumentFragment),
  DEFINE_SHIM(DocumentType),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMDocumentXBL, Document),
  DEFINE_SHIM(DragEvent),
  DEFINE_SHIM(Element),
  DEFINE_SHIM(ElementReplaceEvent),
  DEFINE_SHIM(Event),
  DEFINE_SHIM(EventTarget),
  DEFINE_SHIM(FileReader),
  DEFINE_SHIM(FileList),
  DEFINE_SHIM(FocusEvent),
  DEFINE_SHIM(FormData),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMGeoPositionError, PositionError),
  DEFINE_SHIM(HashChangeEvent),
  DEFINE_SHIM(History),
  DEFINE_SHIM(HTMLAnchorElement),
  DEFINE_SHIM(HTMLAppletElement),
  DEFINE_SHIM(HTMLAreaElement),
  DEFINE_SHIM(HTMLAudioElement),
  DEFINE_SHIM(HTMLBRElement),
  DEFINE_SHIM(HTMLBaseElement),
  DEFINE_SHIM(HTMLBodyElement),
  DEFINE_SHIM(HTMLButtonElement),
  DEFINE_SHIM(HTMLCanvasElement),
  DEFINE_SHIM(HTMLCollection),
  DEFINE_SHIM(HTMLDirectoryElement),
  DEFINE_SHIM(HTMLDivElement),
  DEFINE_SHIM(HTMLDocument),
  DEFINE_SHIM(HTMLElement),
  DEFINE_SHIM(HTMLEmbedElement),
  DEFINE_SHIM(HTMLFieldSetElement),
  DEFINE_SHIM(HTMLFormElement),
  DEFINE_SHIM(HTMLFrameElement),
  DEFINE_SHIM(HTMLFrameSetElement),
  DEFINE_SHIM(HTMLHRElement),
  DEFINE_SHIM(HTMLHeadElement),
  DEFINE_SHIM(HTMLHeadingElement),
  DEFINE_SHIM(HTMLHtmlElement),
  DEFINE_SHIM(HTMLIFrameElement),
  DEFINE_SHIM(HTMLImageElement),
  DEFINE_SHIM(HTMLInputElement),
  DEFINE_SHIM(HTMLLIElement),
  DEFINE_SHIM(HTMLLabelElement),
  DEFINE_SHIM(HTMLLinkElement),
  DEFINE_SHIM(HTMLMapElement),
  DEFINE_SHIM(HTMLMediaElement),
  DEFINE_SHIM(HTMLMenuElement),
  DEFINE_SHIM(HTMLMenuItemElement),
  DEFINE_SHIM(HTMLMetaElement),
  DEFINE_SHIM(HTMLOListElement),
  DEFINE_SHIM(HTMLObjectElement),
  DEFINE_SHIM(HTMLOptGroupElement),
  DEFINE_SHIM(HTMLOptionElement),
  DEFINE_SHIM(HTMLOptionsCollection),
  DEFINE_SHIM(HTMLParagraphElement),
  DEFINE_SHIM(HTMLPreElement),
  DEFINE_SHIM(HTMLQuoteElement),
  DEFINE_SHIM(HTMLScriptElement),
  DEFINE_SHIM(HTMLSelectElement),
  DEFINE_SHIM(HTMLSourceElement),
  DEFINE_SHIM(HTMLStyleElement),
  DEFINE_SHIM(HTMLTableCaptionElement),
  DEFINE_SHIM(HTMLTableCellElement),
  DEFINE_SHIM(HTMLTableElement),
  DEFINE_SHIM(HTMLTextAreaElement),
  DEFINE_SHIM(HTMLTitleElement),
  DEFINE_SHIM(HTMLUListElement),
  DEFINE_SHIM(HTMLVideoElement),
  DEFINE_SHIM(KeyEvent),
  DEFINE_SHIM(LocalMediaStream),
  DEFINE_SHIM(MediaError),
  DEFINE_SHIM(MediaList),
  DEFINE_SHIM(MediaStream),
  DEFINE_SHIM(MessageEvent),
  DEFINE_SHIM(MouseEvent),
  DEFINE_SHIM(MouseScrollEvent),
  DEFINE_SHIM(MutationEvent),
  DEFINE_SHIM(MozApplicationEvent),
  DEFINE_SHIM(MozMmsEvent),
  DEFINE_SHIM(MozNamedAttrMap),
  DEFINE_SHIM(MozSettingsEvent),
  DEFINE_SHIM(MozSmsEvent),
#ifdef MOZ_B2G_RIL
  DEFINE_SHIM(MozVoicemailEvent),
#endif
#ifdef MOZ_WIDGET_GONK
  DEFINE_SHIM(MozWifiConnectionInfoEvent),
  DEFINE_SHIM(MozWifiP2pStatusChangeEvent),
  DEFINE_SHIM(MozWifiStatusChangeEvent),
#endif
  DEFINE_SHIM(NodeIterator),
  DEFINE_SHIM(Node),
  DEFINE_SHIM(NotifyPaintEvent),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMNSEvent, Event),
  DEFINE_SHIM(OfflineResourceList),
  DEFINE_SHIM(PageTransitionEvent),
  DEFINE_SHIM(PaintRequest),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMParser, DOMParser),
  DEFINE_SHIM(PopStateEvent),
  DEFINE_SHIM(PopupBlockedEvent),
  DEFINE_SHIM(ProcessingInstruction),
  DEFINE_SHIM(Range),
  DEFINE_SHIM(RecordErrorEvent),
  DEFINE_SHIM(Rect),
  DEFINE_SHIM(Screen),
  DEFINE_SHIM(ScrollAreaEvent),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMSerializer, XMLSerializer),
  DEFINE_SHIM(SimpleGestureEvent),
  DEFINE_SHIM(SmartCardEvent),
#ifdef MOZ_WEBSPEECH
  DEFINE_SHIM(SpeechRecognitionEvent),
  DEFINE_SHIM(SpeechSynthesisEvent),
#endif // MOZ_WEBSPEECH
  DEFINE_SHIM(StyleSheet),
  DEFINE_SHIM(StyleSheetList),
  DEFINE_SHIM(StyleRuleChangeEvent),
  DEFINE_SHIM(StyleSheetApplicableStateChangeEvent),
  DEFINE_SHIM(StyleSheetChangeEvent),
  DEFINE_SHIM(SVGElement),
  DEFINE_SHIM(SVGLength),
  DEFINE_SHIM(Text),
  DEFINE_SHIM(TimeEvent),
  DEFINE_SHIM(TimeRanges),
  DEFINE_SHIM(TransitionEvent),
  DEFINE_SHIM(TreeWalker),
  DEFINE_SHIM(UIEvent),
  DEFINE_SHIM(ValidityState),
  DEFINE_SHIM(WheelEvent),
  DEFINE_SHIM(XMLDocument),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIXMLHttpRequestEventTarget, XMLHttpRequestEventTarget),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIXMLHttpRequestUpload, XMLHttpRequestUpload),
  DEFINE_SHIM(XPathEvaluator),
  DEFINE_SHIM(XULCommandEvent),
  DEFINE_SHIM(XULDocument),
  DEFINE_SHIM(XULElement),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsISelection, Selection),
};

#undef DEFINE_SHIM
#undef DEFINE_SHIM_WITH_CUSTOM_INTERFACE

NS_IMPL_ISUPPORTS(ShimInterfaceInfo, nsISupports, nsIInterfaceInfo)

already_AddRefed<ShimInterfaceInfo>
ShimInterfaceInfo::MaybeConstruct(const char* aName, JSContext* cx)
{
    nsRefPtr<ShimInterfaceInfo> info;
    for (uint32_t i = 0; i < ArrayLength(kComponentsInterfaceShimMap); ++i) {
        if (!strcmp(aName, kComponentsInterfaceShimMap[i].geckoName)) {
            const ComponentsInterfaceShimEntry& shimEntry =
                kComponentsInterfaceShimMap[i];
            info = new ShimInterfaceInfo(shimEntry.iid,
                                         shimEntry.geckoName,
                                         shimEntry.nativePropHooks);
            break;
        }
    }
    return info.forget();
}

ShimInterfaceInfo::ShimInterfaceInfo(const nsIID& aIID,
                                     const char* aName,
                                     const mozilla::dom::NativePropertyHooks* aNativePropHooks)
    : mIID(aIID)
    , mName(aName)
    , mNativePropHooks(aNativePropHooks)
{
}

NS_IMETHODIMP
ShimInterfaceInfo::GetName(char** aName)
{
    *aName = ToNewCString(mName);
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetInterfaceIID(nsIID** aIID)
{
    *aIID = static_cast<nsIID*> (nsMemory::Clone(&mIID, sizeof(mIID)));
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsScriptable(bool* aRetVal)
{
    // This class should pretend that the interface is scriptable because
    // that's what nsJSIID assumes.
    *aRetVal = true;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsBuiltinClass(bool* aRetVal)
{
    *aRetVal = true;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetParent(nsIInterfaceInfo** aParent)
{
    *aParent = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetMethodCount(uint16_t* aCount)
{
    // Pretend we don't have any methods.
    *aCount = 0;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetConstantCount(uint16_t* aCount)
{
    // We assume that we never have interfaces with more than UINT16_MAX
    // constants defined on them.
    uint16_t count = 0;

    // NOTE: The structure of this loop must be kept in sync with the loop
    // in GetConstant.
    const mozilla::dom::NativePropertyHooks* propHooks = mNativePropHooks;
    do {
        const mozilla::dom::NativeProperties* props[] = {
            propHooks->mNativeProperties.regular,
            propHooks->mNativeProperties.chromeOnly
        };
        for (size_t i = 0; i < ArrayLength(props); ++i) {
            auto prop = props[i];
            if (prop && prop->constants) {
                for (auto cs = prop->constants->specs; cs->name; ++cs) {
                    // We have found one constant here.  We explicitly do not
                    // bother calling isEnabled() here because it's OK to define
                    // potentially extra constants on these shim interfaces.
                    ++count;
                }
            }
        }
    } while ((propHooks = propHooks->mProtoHooks));
    *aCount = count;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetMethodInfo(uint16_t aIndex, const nsXPTMethodInfo** aInfo)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetMethodInfoForName(const char* aName, uint16_t* aIndex, const nsXPTMethodInfo** aInfo)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetConstant(uint16_t aIndex, JS::MutableHandleValue aConstant,
                               char** aName)
{
    // We assume that we never have interfaces with more than UINT16_MAX
    // constants defined on them.
    uint16_t index = 0;

    // NOTE: The structure of this loop must be kept in sync with the loop
    // in GetConstantCount.
    const mozilla::dom::NativePropertyHooks* propHooks = mNativePropHooks;
    do {
        const mozilla::dom::NativeProperties* props[] = {
            propHooks->mNativeProperties.regular,
            propHooks->mNativeProperties.chromeOnly
        };
        for (size_t i = 0; i < ArrayLength(props); ++i) {
            auto prop = props[i];
            if (prop && prop->constants) {
                for (auto cs = prop->constants->specs; cs->name; ++cs) {
                    // We have found one constant here.  We explicitly do not
                    // bother calling isEnabled() here because it's OK to define
                    // potentially extra constants on these shim interfaces.
                    if (index == aIndex) {
                        aConstant.set(cs->value);
                        *aName = ToNewCString(nsDependentCString(cs->name));
                        return NS_OK;
                    }
                    ++index;
                }
            }
        }
    } while ((propHooks = propHooks->mProtoHooks));

    // aIndex was bigger than the number of constants we have.
    return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetInfoForParam(uint16_t aIndex, const nsXPTParamInfo* aParam, nsIInterfaceInfo** aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetIIDForParam(uint16_t aIndex, const nsXPTParamInfo* aParam, nsIID** aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetTypeForParam(uint16_t aInex, const nsXPTParamInfo* aParam, uint16_t aDimension, nsXPTType* aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetSizeIsArgNumberForParam(uint16_t aInex, const nsXPTParamInfo* aParam, uint16_t aDimension, uint8_t* aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetInterfaceIsArgNumberForParam(uint16_t aInex, const nsXPTParamInfo* aParam, uint8_t* aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsIID(const nsIID* aIID, bool* aRetVal)
{
    *aRetVal = mIID.Equals(*aIID);
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetNameShared(const char** aName)
{
    *aName = mName.get();
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetIIDShared(const nsIID** aIID)
{
    *aIID = &mIID;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsFunction(bool* aRetVal)
{
    *aRetVal = false;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::HasAncestor(const nsIID* aIID, bool* aRetVal)
{
    *aRetVal = false;
    return NS_OK;
}

NS_IMETHODIMP_(nsresult)
ShimInterfaceInfo::GetIIDForParamNoAlloc(uint16_t aIndex, const nsXPTParamInfo* aInfo, nsIID* aIID)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}
