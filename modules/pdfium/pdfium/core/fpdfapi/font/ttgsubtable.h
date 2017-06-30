// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_TTGSUBTABLE_H_
#define CORE_FPDFAPI_FONT_TTGSUBTABLE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "core/fxcrt/fx_basic.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/fx_freetype.h"

class CFX_GlyphMap {
 public:
  CFX_GlyphMap();
  ~CFX_GlyphMap();

  void SetAt(int key, int value);
  bool Lookup(int key, int& value);

 protected:
  CFX_BinaryBuf m_Buffer;
};

class CFX_CTTGSUBTable {
 public:
  CFX_CTTGSUBTable();
  explicit CFX_CTTGSUBTable(FT_Bytes gsub);
  virtual ~CFX_CTTGSUBTable();

  bool IsOk() const;
  bool LoadGSUBTable(FT_Bytes gsub);
  bool GetVerticalGlyph(uint32_t glyphnum, uint32_t* vglyphnum);

 private:
  struct tt_gsub_header {
    uint32_t Version;
    uint16_t ScriptList;
    uint16_t FeatureList;
    uint16_t LookupList;
  };

  struct TLangSys {
    TLangSys();
    ~TLangSys();

    uint16_t LookupOrder;
    uint16_t ReqFeatureIndex;
    std::vector<uint16_t> FeatureIndices;

   private:
    TLangSys(const TLangSys&) = delete;
    TLangSys& operator=(const TLangSys&) = delete;
  };

  struct TLangSysRecord {
    TLangSysRecord() : LangSysTag(0) {}

    uint32_t LangSysTag;
    TLangSys LangSys;

   private:
    TLangSysRecord(const TLangSysRecord&) = delete;
    TLangSysRecord& operator=(const TLangSysRecord&) = delete;
  };

  struct TScript {
    TScript();
    ~TScript();

    uint16_t DefaultLangSys;
    std::vector<TLangSysRecord> LangSysRecords;

   private:
    TScript(const TScript&) = delete;
    TScript& operator=(const TScript&) = delete;
  };

  struct TScriptRecord {
    TScriptRecord() : ScriptTag(0) {}

    uint32_t ScriptTag;
    TScript Script;

   private:
    TScriptRecord(const TScriptRecord&) = delete;
    TScriptRecord& operator=(const TScriptRecord&) = delete;
  };

  struct TScriptList {
    TScriptList();
    ~TScriptList();

    std::vector<TScriptRecord> ScriptRecords;

   private:
    TScriptList(const TScriptList&) = delete;
    TScriptList& operator=(const TScriptList&) = delete;
  };

  struct TFeature {
    TFeature();
    ~TFeature();

    uint16_t FeatureParams;
    std::vector<uint16_t> LookupListIndices;

   private:
    TFeature(const TFeature&) = delete;
    TFeature& operator=(const TFeature&) = delete;
  };

  struct TFeatureRecord {
    TFeatureRecord() : FeatureTag(0) {}

    uint32_t FeatureTag;
    TFeature Feature;

   private:
    TFeatureRecord(const TFeatureRecord&) = delete;
    TFeatureRecord& operator=(const TFeatureRecord&) = delete;
  };

  struct TFeatureList {
    TFeatureList();
    ~TFeatureList();

    std::vector<TFeatureRecord> FeatureRecords;

   private:
    TFeatureList(const TFeatureList&) = delete;
    TFeatureList& operator=(const TFeatureList&) = delete;
  };

  enum TLookupFlag {
    LOOKUPFLAG_RightToLeft = 0x0001,
    LOOKUPFLAG_IgnoreBaseGlyphs = 0x0002,
    LOOKUPFLAG_IgnoreLigatures = 0x0004,
    LOOKUPFLAG_IgnoreMarks = 0x0008,
    LOOKUPFLAG_Reserved = 0x00F0,
    LOOKUPFLAG_MarkAttachmentType = 0xFF00,
  };

  struct TCoverageFormatBase {
    TCoverageFormatBase() : CoverageFormat(0) {}
    explicit TCoverageFormatBase(uint16_t format) : CoverageFormat(format) {}
    virtual ~TCoverageFormatBase() {}

    uint16_t CoverageFormat;
    CFX_GlyphMap m_glyphMap;

   private:
    TCoverageFormatBase(const TCoverageFormatBase&);
    TCoverageFormatBase& operator=(const TCoverageFormatBase&);
  };

  struct TCoverageFormat1 : public TCoverageFormatBase {
    TCoverageFormat1();
    ~TCoverageFormat1() override;

    std::vector<uint16_t> GlyphArray;

   private:
    TCoverageFormat1(const TCoverageFormat1&) = delete;
    TCoverageFormat1& operator=(const TCoverageFormat1&) = delete;
  };

  struct TRangeRecord {
    TRangeRecord();

    friend bool operator>(const TRangeRecord& r1, const TRangeRecord& r2) {
      return r1.Start > r2.Start;
    }

    uint16_t Start;
    uint16_t End;
    uint16_t StartCoverageIndex;

   private:
    TRangeRecord(const TRangeRecord&) = delete;
  };

  struct TCoverageFormat2 : public TCoverageFormatBase {
    TCoverageFormat2();
    ~TCoverageFormat2() override;

    std::vector<TRangeRecord> RangeRecords;

   private:
    TCoverageFormat2(const TCoverageFormat2&) = delete;
    TCoverageFormat2& operator=(const TCoverageFormat2&) = delete;
  };

  struct TDevice {
    TDevice() : StartSize(0), EndSize(0), DeltaFormat(0) {}

    uint16_t StartSize;
    uint16_t EndSize;
    uint16_t DeltaFormat;

   private:
    TDevice(const TDevice&) = delete;
    TDevice& operator=(const TDevice&) = delete;
  };

  struct TSubTableBase {
    TSubTableBase() : SubstFormat(0) {}
    explicit TSubTableBase(uint16_t format) : SubstFormat(format) {}
    virtual ~TSubTableBase() {}

    uint16_t SubstFormat;

   private:
    TSubTableBase(const TSubTableBase&) = delete;
    TSubTableBase& operator=(const TSubTableBase&) = delete;
  };

  struct TSingleSubstFormat1 : public TSubTableBase {
    TSingleSubstFormat1();
    ~TSingleSubstFormat1() override;

    std::unique_ptr<TCoverageFormatBase> Coverage;
    int16_t DeltaGlyphID;

   private:
    TSingleSubstFormat1(const TSingleSubstFormat1&) = delete;
    TSingleSubstFormat1& operator=(const TSingleSubstFormat1&) = delete;
  };

  struct TSingleSubstFormat2 : public TSubTableBase {
    TSingleSubstFormat2();
    ~TSingleSubstFormat2() override;

    std::unique_ptr<TCoverageFormatBase> Coverage;
    std::vector<uint16_t> Substitutes;

   private:
    TSingleSubstFormat2(const TSingleSubstFormat2&) = delete;
    TSingleSubstFormat2& operator=(const TSingleSubstFormat2&) = delete;
  };

  struct TLookup {
    TLookup();
    ~TLookup();

    uint16_t LookupType;
    uint16_t LookupFlag;
    std::vector<std::unique_ptr<TSubTableBase>> SubTables;

   private:
    TLookup(const TLookup&) = delete;
    TLookup& operator=(const TLookup&) = delete;
  };

  struct TLookupList {
    TLookupList();
    ~TLookupList();

    std::vector<TLookup> Lookups;

   private:
    TLookupList(const TLookupList&) = delete;
    TLookupList& operator=(const TLookupList&) = delete;
  };

  bool Parse(FT_Bytes scriptlist, FT_Bytes featurelist, FT_Bytes lookuplist);
  void ParseScriptList(FT_Bytes raw, TScriptList* rec);
  void ParseScript(FT_Bytes raw, TScript* rec);
  void ParseLangSys(FT_Bytes raw, TLangSys* rec);
  void ParseFeatureList(FT_Bytes raw, TFeatureList* rec);
  void ParseFeature(FT_Bytes raw, TFeature* rec);
  void ParseLookupList(FT_Bytes raw, TLookupList* rec);
  void ParseLookup(FT_Bytes raw, TLookup* rec);
  TCoverageFormatBase* ParseCoverage(FT_Bytes raw);
  void ParseCoverageFormat1(FT_Bytes raw, TCoverageFormat1* rec);
  void ParseCoverageFormat2(FT_Bytes raw, TCoverageFormat2* rec);
  void ParseSingleSubst(FT_Bytes raw, std::unique_ptr<TSubTableBase>* rec);
  void ParseSingleSubstFormat1(FT_Bytes raw, TSingleSubstFormat1* rec);
  void ParseSingleSubstFormat2(FT_Bytes raw, TSingleSubstFormat2* rec);

  bool GetVerticalGlyphSub(uint32_t glyphnum,
                           uint32_t* vglyphnum,
                           TFeature* Feature);
  bool GetVerticalGlyphSub2(uint32_t glyphnum,
                            uint32_t* vglyphnum,
                            TLookup* Lookup);
  int GetCoverageIndex(TCoverageFormatBase* Coverage, uint32_t g) const;

  uint8_t GetUInt8(FT_Bytes& p) const;
  int16_t GetInt16(FT_Bytes& p) const;
  uint16_t GetUInt16(FT_Bytes& p) const;
  int32_t GetInt32(FT_Bytes& p) const;
  uint32_t GetUInt32(FT_Bytes& p) const;

  std::set<uint32_t> m_featureSet;
  bool m_bFeautureMapLoad;
  bool loaded;
  tt_gsub_header header;
  TScriptList ScriptList;
  TFeatureList FeatureList;
  TLookupList LookupList;
};

#endif  // CORE_FPDFAPI_FONT_TTGSUBTABLE_H_
