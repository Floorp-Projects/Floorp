// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_XML_H_
#define CORE_FXCRT_FX_XML_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_basic.h"

class CXML_AttrItem {
 public:
  bool Matches(const CFX_ByteString& space, const CFX_ByteString& name) const;

  CFX_ByteString m_QSpaceName;
  CFX_ByteString m_AttrName;
  CFX_WideString m_Value;
};

class CXML_AttrMap {
 public:
  CXML_AttrMap();
  ~CXML_AttrMap();

  const CFX_WideString* Lookup(const CFX_ByteString& space,
                               const CFX_ByteString& name) const;
  int GetSize() const;
  CXML_AttrItem& GetAt(int index) const;

  void SetAt(const CFX_ByteString& space,
             const CFX_ByteString& name,
             const CFX_WideString& value);

  std::unique_ptr<std::vector<CXML_AttrItem>> m_pMap;
};

class CXML_Content {
 public:
  CXML_Content() : m_bCDATA(false), m_Content() {}
  void Set(bool bCDATA, const CFX_WideStringC& content) {
    m_bCDATA = bCDATA;
    m_Content = content;
  }

  bool m_bCDATA;
  CFX_WideString m_Content;
};

class CXML_Element {
 public:
  enum ChildType { Invalid, Element, Content };

  static std::unique_ptr<CXML_Element> Parse(const void* pBuffer, size_t size);

  CXML_Element(const CXML_Element* pParent,
               const CFX_ByteStringC& qSpace,
               const CFX_ByteStringC& tagname);
  ~CXML_Element();

  void Empty();
  CFX_ByteString GetTagName(bool bQualified = false) const;
  CFX_ByteString GetNamespace(bool bQualified = false) const;
  CFX_ByteString GetNamespaceURI(const CFX_ByteString& qName) const;
  const CXML_Element* GetParent() const { return m_pParent; }
  uint32_t CountAttrs() const { return m_AttrMap.GetSize(); }
  void GetAttrByIndex(int index,
                      CFX_ByteString& space,
                      CFX_ByteString& name,
                      CFX_WideString& value) const;
  bool HasAttr(const CFX_ByteStringC& qName) const;
  bool GetAttrValue(const CFX_ByteStringC& name,
                    CFX_WideString& attribute) const;
  CFX_WideString GetAttrValue(const CFX_ByteStringC& name) const {
    CFX_WideString attr;
    GetAttrValue(name, attr);
    return attr;
  }

  bool GetAttrValue(const CFX_ByteStringC& space,
                    const CFX_ByteStringC& name,
                    CFX_WideString& attribute) const;
  CFX_WideString GetAttrValue(const CFX_ByteStringC& space,
                              const CFX_ByteStringC& name) const {
    CFX_WideString attr;
    GetAttrValue(space, name, attr);
    return attr;
  }

  bool GetAttrInteger(const CFX_ByteStringC& name, int& attribute) const;
  int GetAttrInteger(const CFX_ByteStringC& name) const {
    int attr = 0;
    GetAttrInteger(name, attr);
    return attr;
  }

  bool GetAttrInteger(const CFX_ByteStringC& space,
                      const CFX_ByteStringC& name,
                      int& attribute) const;
  int GetAttrInteger(const CFX_ByteStringC& space,
                     const CFX_ByteStringC& name) const {
    int attr = 0;
    GetAttrInteger(space, name, attr);
    return attr;
  }

  bool GetAttrFloat(const CFX_ByteStringC& name, FX_FLOAT& attribute) const;
  FX_FLOAT GetAttrFloat(const CFX_ByteStringC& name) const {
    FX_FLOAT attr = 0;
    GetAttrFloat(name, attr);
    return attr;
  }

  bool GetAttrFloat(const CFX_ByteStringC& space,
                    const CFX_ByteStringC& name,
                    FX_FLOAT& attribute) const;
  FX_FLOAT GetAttrFloat(const CFX_ByteStringC& space,
                        const CFX_ByteStringC& name) const {
    FX_FLOAT attr = 0;
    GetAttrFloat(space, name, attr);
    return attr;
  }

  uint32_t CountChildren() const { return m_Children.size(); }
  ChildType GetChildType(uint32_t index) const;
  CFX_WideString GetContent(uint32_t index) const;
  CXML_Element* GetElement(uint32_t index) const;
  CXML_Element* GetElement(const CFX_ByteStringC& space,
                           const CFX_ByteStringC& tag) const {
    return GetElement(space, tag, 0);
  }

  uint32_t CountElements(const CFX_ByteStringC& space,
                         const CFX_ByteStringC& tag) const;
  CXML_Element* GetElement(const CFX_ByteStringC& space,
                           const CFX_ByteStringC& tag,
                           int index) const;

  uint32_t FindElement(CXML_Element* pChild) const;
  void SetTag(const CFX_ByteStringC& qTagName);
  void RemoveChildren();
  void RemoveChild(uint32_t index);

 protected:
  struct ChildRecord {
    ChildType type;
    void* child;  // CXML_Element and CXML_Content lack a common ancestor.
  };

  const CXML_Element* const m_pParent;
  CFX_ByteString m_QSpaceName;
  CFX_ByteString m_TagName;
  CXML_AttrMap m_AttrMap;
  std::vector<ChildRecord> m_Children;

  friend class CXML_Parser;
  friend class CXML_Composer;
};

#endif  // CORE_FXCRT_FX_XML_H_
