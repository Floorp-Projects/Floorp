/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHtml5TreeOperation.h"
#include "nsContentUtils.h"
#include "nsNodeUtils.h"
#include "nsAttrName.h"
#include "nsHtml5TreeBuilder.h"
#include "nsIDOMMutationEvent.h"
#include "mozAutoDocUpdate.h"
#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsContentCreatorFunctions.h"
#include "nsIScriptElement.h"
#include "nsIDTD.h"
#include "nsTraceRefcnt.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIDOMDocumentType.h"

nsHtml5TreeOperation::nsHtml5TreeOperation()
 : mOpCode(eTreeOpAppend)
{
  MOZ_COUNT_CTOR(nsHtml5TreeOperation);
}

nsHtml5TreeOperation::~nsHtml5TreeOperation()
{
  MOZ_COUNT_DTOR(nsHtml5TreeOperation);
  switch(mOpCode) {
    case eTreeOpAddAttributes:
      delete mTwo.attributes;
      break;
    case eTreeOpCreateElement:
      delete mThree.attributes;
      break;
    case eTreeOpCreateDoctype:
      delete mTwo.stringPair;
      break;
    case eTreeOpCreateTextNode:
    case eTreeOpCreateComment:
      delete[] mTwo.unicharPtr;
      break;
    default: // keep the compiler happy
      break;
  }
}

nsresult
nsHtml5TreeOperation::Perform(nsHtml5TreeOpExecutor* aBuilder)
{
  nsresult rv = NS_OK;
  switch(mOpCode) {
    case eTreeOpAppend: {
      nsIContent* node = *(mOne.node);
      nsIContent* parent = *(mTwo.node);
      aBuilder->PostPendingAppendNotification(parent, node);
      rv = parent->AppendChildTo(node, PR_FALSE);
      return rv;
    }
    case eTreeOpDetach: {
      nsIContent* node = *(mOne.node);
      aBuilder->FlushPendingAppendNotifications();
      nsIContent* parent = node->GetParent();
      if (parent) {
        PRUint32 pos = parent->IndexOf(node);
        NS_ASSERTION((pos >= 0), "Element not found as child of its parent");
        rv = parent->RemoveChildAt(pos, PR_TRUE, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      return rv;
    }
    case eTreeOpAppendChildrenToNewParent: {
      nsIContent* node = *(mOne.node);
      nsIContent* parent = *(mTwo.node);
      aBuilder->FlushPendingAppendNotifications();
      PRUint32 childCount = parent->GetChildCount();
      PRBool didAppend = PR_FALSE;
      while (node->GetChildCount()) {
        nsCOMPtr<nsIContent> child = node->GetChildAt(0);
        rv = node->RemoveChildAt(0, PR_TRUE, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = parent->AppendChildTo(child, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
        didAppend = PR_TRUE;
      }
      if (didAppend) {
        nsNodeUtils::ContentAppended(parent, childCount);
      }
      return rv;
    }
    case eTreeOpFosterParent: {
      nsIContent* node = *(mOne.node);
      nsIContent* parent = *(mTwo.node);
      nsIContent* table = *(mThree.node);
      nsIContent* foster = table->GetParent();
      if (foster && foster->IsNodeOfType(nsINode::eELEMENT)) {
        aBuilder->FlushPendingAppendNotifications();
        PRUint32 pos = foster->IndexOf(table);
        rv = foster->InsertChildAt(node, pos, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
        nsNodeUtils::ContentInserted(foster, node, pos);
      } else {
        aBuilder->PostPendingAppendNotification(parent, node);
        rv = parent->AppendChildTo(node, PR_FALSE);
      }
      return rv;
    }
    case eTreeOpAppendToDocument: {
      nsIContent* node = *(mOne.node);
      aBuilder->FlushPendingAppendNotifications();
      nsIDocument* doc = aBuilder->GetDocument();
      PRUint32 childCount = doc->GetChildCount();
      rv = doc->AppendChildTo(node, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
      nsNodeUtils::ContentInserted(doc, node, childCount);
      return rv;
    }
    case eTreeOpAddAttributes: {
      nsIContent* node = *(mOne.node);
      nsHtml5HtmlAttributes* attributes = mTwo.attributes;

      nsIDocument* document = node->GetCurrentDoc();

      PRInt32 len = attributes->getLength();
      for (PRInt32 i = 0; i < len; ++i) {
        // prefix doesn't need regetting. it is always null or a static atom
        // local name is never null
        nsCOMPtr<nsIAtom> localName = Reget(attributes->getLocalName(i));
        PRInt32 nsuri = attributes->getURI(i);
        if (!node->HasAttr(nsuri, localName)) {

          // the manual notification code is based on nsGenericElement
          
          PRUint32 stateMask = PRUint32(node->IntrinsicState());
          nsNodeUtils::AttributeWillChange(node, 
                                           nsuri,
                                           localName,
                                           static_cast<PRUint8>(nsIDOMMutationEvent::ADDITION));

          // prefix doesn't need regetting. it is always null or a static atom
          // local name is never null
          node->SetAttr(nsuri, localName, attributes->getPrefix(i), *(attributes->getValue(i)), PR_FALSE);
          // XXX what to do with nsresult?
          
          if (document || node->HasFlag(NODE_FORCE_XBL_BINDINGS)) {
            nsIDocument* ownerDoc = node->GetOwnerDoc();
            if (ownerDoc) {
              nsRefPtr<nsXBLBinding> binding =
                ownerDoc->BindingManager()->GetBinding(node);
              if (binding) {
                binding->AttributeChanged(localName, nsuri, PR_FALSE, PR_FALSE);
              }
            }
          }
          
          stateMask ^= PRUint32(node->IntrinsicState());
          if (stateMask && document) {
            MOZ_AUTO_DOC_UPDATE(document, UPDATE_CONTENT_STATE, PR_TRUE);
            document->ContentStatesChanged(node, nsnull, stateMask);
          }
          nsNodeUtils::AttributeChanged(node, 
                                        nsuri, 
                                        localName, 
                                        static_cast<PRUint8>(nsIDOMMutationEvent::ADDITION),
                                        stateMask);
        }
      }
      
      return rv;
    }
    case eTreeOpCreateElement: {
      nsIContent** target = mOne.node;
      PRInt32 ns = mInt;
      nsCOMPtr<nsIAtom> name = Reget(mTwo.atom);
      nsHtml5HtmlAttributes* attributes = mThree.attributes;
      
      nsCOMPtr<nsIContent> newContent;
      nsCOMPtr<nsINodeInfo> nodeInfo = aBuilder->GetNodeInfoManager()->GetNodeInfo(name, nsnull, ns);
      NS_ASSERTION(nodeInfo, "Got null nodeinfo.");
      NS_NewElement(getter_AddRefs(newContent), nodeInfo->NamespaceID(), nodeInfo, PR_TRUE);
      NS_ASSERTION(newContent, "Element creation created null pointer.");

      aBuilder->HoldElement(*target = newContent);      

      if (!attributes) {
        return rv;
      }

      PRInt32 len = attributes->getLength();
      for (PRInt32 i = 0; i < len; ++i) {
        // prefix doesn't need regetting. it is always null or a static atom
        // local name is never null
        nsCOMPtr<nsIAtom> localName = Reget(attributes->getLocalName(i));
        newContent->SetAttr(attributes->getURI(i), localName, attributes->getPrefix(i), *(attributes->getValue(i)), PR_FALSE);
        // XXX what to do with nsresult?
      }
      if (ns != kNameSpaceID_MathML && (name == nsHtml5Atoms::style || (ns == kNameSpaceID_XHTML && name == nsHtml5Atoms::link))) {
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(newContent));
        if (ssle) {
          ssle->InitStyleLinkElement(PR_FALSE);
          ssle->SetEnableUpdates(PR_FALSE);
        }
      }
      return rv;
    }
    case eTreeOpSetFormElement: {
      nsIContent* node = *(mOne.node);
      nsIContent* parent = *(mTwo.node);
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(node));
      NS_ASSERTION(formControl, "Form-associated element did not implement nsIFormControl.");
      nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(parent));
      NS_ASSERTION(formElement, "The form element doesn't implement nsIDOMHTMLFormElement.");
      if (formControl) { // avoid crashing on <output>
        formControl->SetForm(formElement);
      }
      return rv;
    }
    case eTreeOpCreateTextNode: {
      nsIContent** target = mOne.node;
      PRUnichar* buffer = mTwo.unicharPtr;
      PRInt32 length = mInt;
      
      nsCOMPtr<nsIContent> text;
      NS_NewTextNode(getter_AddRefs(text), aBuilder->GetNodeInfoManager());
      // XXX nsresult and comment null check?
      text->SetText(buffer, length, PR_FALSE);
      // XXX nsresult
      
      aBuilder->HoldNonElement(*target = text);
      return rv;
    }
    case eTreeOpCreateComment: {
      nsIContent** target = mOne.node;
      PRUnichar* buffer = mTwo.unicharPtr;
      PRInt32 length = mInt;
      
      nsCOMPtr<nsIContent> comment;
      NS_NewCommentNode(getter_AddRefs(comment), aBuilder->GetNodeInfoManager());
      // XXX nsresult and comment null check?
      comment->SetText(buffer, length, PR_FALSE);
      // XXX nsresult
      
      aBuilder->HoldNonElement(*target = comment);
      return rv;
    }
    case eTreeOpCreateDoctype: {
      nsCOMPtr<nsIAtom> name = Reget(mOne.atom);
      nsHtml5TreeOperationStringPair* pair = mTwo.stringPair;
      nsString publicId;
      nsString systemId;
      pair->Get(publicId, systemId);
      nsIContent** target = mThree.node;
      
      // Adapted from nsXMLContentSink
      // Create a new doctype node
      nsCOMPtr<nsIDOMDocumentType> docType;
      nsAutoString voidString;
      voidString.SetIsVoid(PR_TRUE);
      NS_NewDOMDocumentType(getter_AddRefs(docType),
                            aBuilder->GetNodeInfoManager(),
                            nsnull,
                            name,
                            nsnull,
                            nsnull,
                            publicId,
                            systemId,
                            voidString);
      NS_ASSERTION(docType, "Doctype creation failed.");
      nsCOMPtr<nsIContent> asContent = do_QueryInterface(docType);
      aBuilder->HoldNonElement(*target = asContent);      
      return rv;
    }
    case eTreeOpRunScript: {
      nsIContent* node = *(mOne.node);
      aBuilder->SetScriptElement(node);
      return rv;
    }
    case eTreeOpDoneAddingChildren: {
      nsIContent* node = *(mOne.node);
      node->DoneAddingChildren(aBuilder->HaveNotified(node));
      return rv;
    }
    case eTreeOpDoneCreatingElement: {
      nsIContent* node = *(mOne.node);
      node->DoneCreatingElement();
      return rv;
    }
    case eTreeOpUpdateStyleSheet: {
      nsIContent* node = *(mOne.node);
      aBuilder->UpdateStyleSheet(node);
      return rv;
    }
    case eTreeOpProcessBase: {
      nsIContent* node = *(mOne.node);
      rv = aBuilder->ProcessBASETag(node);
      return rv;
    }
    case eTreeOpProcessMeta: {
      nsIContent* node = *(mOne.node);
      rv = aBuilder->ProcessMETATag(node);
      return rv;
    }
    case eTreeOpProcessOfflineManifest: {
      nsIContent* node = *(mOne.node);
      aBuilder->ProcessOfflineManifest(node);
      return rv;
    }
    case eTreeOpMarkMalformedIfScript: {
      nsIContent* node = *(mOne.node);
      nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(node);
      if (sele) {
        // Make sure to serialize this script correctly, for nice round tripping.
        sele->SetIsMalformed();
      }
      return rv;
    }
    case eTreeOpStartLayout: {
      aBuilder->StartLayout(); // this causes a flush anyway
      return rv;
    }
    case eTreeOpDocumentMode: {
      aBuilder->DocumentMode(mOne.mode);
      return rv;
    }
    default: {
      NS_NOTREACHED("Bogus tree op");
    }
  }
  return rv; // keep compiler happy
}
