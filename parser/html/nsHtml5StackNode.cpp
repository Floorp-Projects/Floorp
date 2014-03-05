/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2011 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit StackNode.java instead and regenerate.
 */

#define nsHtml5StackNode_cpp__

#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5Macros.h"
#include "nsIContentHandle.h"

#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5StackNode.h"

int32_t 
nsHtml5StackNode::getGroup()
{
  return flags & NS_HTML5ELEMENT_NAME_GROUP_MASK;
}

bool 
nsHtml5StackNode::isScoping()
{
  return (flags & NS_HTML5ELEMENT_NAME_SCOPING);
}

bool 
nsHtml5StackNode::isSpecial()
{
  return (flags & NS_HTML5ELEMENT_NAME_SPECIAL);
}

bool 
nsHtml5StackNode::isFosterParenting()
{
  return (flags & NS_HTML5ELEMENT_NAME_FOSTER_PARENTING);
}

bool 
nsHtml5StackNode::isHtmlIntegrationPoint()
{
  return (flags & NS_HTML5ELEMENT_NAME_HTML_INTEGRATION_POINT);
}


nsHtml5StackNode::nsHtml5StackNode(int32_t flags, int32_t ns, nsIAtom* name, nsIContentHandle* node, nsIAtom* popName, nsHtml5HtmlAttributes* attributes)
  : flags(flags),
    name(name),
    popName(popName),
    ns(ns),
    node(node),
    attributes(attributes),
    refcount(1)
{
  MOZ_COUNT_CTOR(nsHtml5StackNode);
}


nsHtml5StackNode::nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node)
  : flags(elementName->getFlags()),
    name(elementName->name),
    popName(elementName->name),
    ns(kNameSpaceID_XHTML),
    node(node),
    attributes(nullptr),
    refcount(1)
{
  MOZ_COUNT_CTOR(nsHtml5StackNode);
  MOZ_ASSERT(!elementName->isCustom(), "Don't use this constructor for custom elements.");
}


nsHtml5StackNode::nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node, nsHtml5HtmlAttributes* attributes)
  : flags(elementName->getFlags()),
    name(elementName->name),
    popName(elementName->name),
    ns(kNameSpaceID_XHTML),
    node(node),
    attributes(attributes),
    refcount(1)
{
  MOZ_COUNT_CTOR(nsHtml5StackNode);
  MOZ_ASSERT(!elementName->isCustom(), "Don't use this constructor for custom elements.");
}


nsHtml5StackNode::nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node, nsIAtom* popName)
  : flags(elementName->getFlags()),
    name(elementName->name),
    popName(popName),
    ns(kNameSpaceID_XHTML),
    node(node),
    attributes(nullptr),
    refcount(1)
{
  MOZ_COUNT_CTOR(nsHtml5StackNode);
}


nsHtml5StackNode::nsHtml5StackNode(nsHtml5ElementName* elementName, nsIAtom* popName, nsIContentHandle* node)
  : flags(prepareSvgFlags(elementName->getFlags())),
    name(elementName->name),
    popName(popName),
    ns(kNameSpaceID_SVG),
    node(node),
    attributes(nullptr),
    refcount(1)
{
  MOZ_COUNT_CTOR(nsHtml5StackNode);
}


nsHtml5StackNode::nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node, nsIAtom* popName, bool markAsIntegrationPoint)
  : flags(prepareMathFlags(elementName->getFlags(), markAsIntegrationPoint)),
    name(elementName->name),
    popName(popName),
    ns(kNameSpaceID_MathML),
    node(node),
    attributes(nullptr),
    refcount(1)
{
  MOZ_COUNT_CTOR(nsHtml5StackNode);
}

int32_t 
nsHtml5StackNode::prepareSvgFlags(int32_t flags)
{
  flags &= ~(NS_HTML5ELEMENT_NAME_FOSTER_PARENTING | NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  if ((flags & NS_HTML5ELEMENT_NAME_SCOPING_AS_SVG)) {
    flags |= (NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_HTML_INTEGRATION_POINT);
  }
  return flags;
}

int32_t 
nsHtml5StackNode::prepareMathFlags(int32_t flags, bool markAsIntegrationPoint)
{
  flags &= ~(NS_HTML5ELEMENT_NAME_FOSTER_PARENTING | NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_SPECIAL | NS_HTML5ELEMENT_NAME_OPTIONAL_END_TAG);
  if ((flags & NS_HTML5ELEMENT_NAME_SCOPING_AS_MATHML)) {
    flags |= (NS_HTML5ELEMENT_NAME_SCOPING | NS_HTML5ELEMENT_NAME_SPECIAL);
  }
  if (markAsIntegrationPoint) {
    flags |= NS_HTML5ELEMENT_NAME_HTML_INTEGRATION_POINT;
  }
  return flags;
}


nsHtml5StackNode::~nsHtml5StackNode()
{
  MOZ_COUNT_DTOR(nsHtml5StackNode);
  delete attributes;
}

void 
nsHtml5StackNode::dropAttributes()
{
  attributes = nullptr;
}

void 
nsHtml5StackNode::retain()
{
  refcount++;
}

void 
nsHtml5StackNode::release()
{
  refcount--;
  if (!refcount) {
    delete this;
  }
}

void
nsHtml5StackNode::initializeStatics()
{
}

void
nsHtml5StackNode::releaseStatics()
{
}


