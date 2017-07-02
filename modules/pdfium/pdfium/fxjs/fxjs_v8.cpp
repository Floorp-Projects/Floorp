// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/fxjs_v8.h"

#include <vector>

#include "core/fxcrt/fx_basic.h"

// Keep this consistent with the values defined in gin/public/context_holder.h
// (without actually requiring a dependency on gin itself for the standalone
// embedders of PDFIum). The value we want to use is:
//   kPerContextDataStartIndex + kEmbedderPDFium, which is 3.
static const unsigned int kPerContextDataIndex = 3u;
static unsigned int g_embedderDataSlot = 1u;
static v8::Isolate* g_isolate = nullptr;
static size_t g_isolate_ref_count = 0;
static FXJS_ArrayBufferAllocator* g_arrayBufferAllocator = nullptr;
static v8::Global<v8::ObjectTemplate>* g_DefaultGlobalObjectTemplate = nullptr;
static wchar_t kPerObjectDataTag[] = L"CFXJS_PerObjectData";

class CFXJS_PerObjectData {
 public:
  explicit CFXJS_PerObjectData(int nObjDefID)
      : m_ObjDefID(nObjDefID), m_pPrivate(nullptr) {}

  static void SetInObject(CFXJS_PerObjectData* pData,
                          v8::Local<v8::Object> pObj) {
    if (pObj->InternalFieldCount() == 2) {
      pObj->SetAlignedPointerInInternalField(0, pData);
      pObj->SetAlignedPointerInInternalField(
          1, static_cast<void*>(kPerObjectDataTag));
    }
  }

  static CFXJS_PerObjectData* GetFromObject(v8::Local<v8::Object> pObj) {
    if (pObj.IsEmpty() || pObj->InternalFieldCount() != 2 ||
        pObj->GetAlignedPointerFromInternalField(1) !=
            static_cast<void*>(kPerObjectDataTag)) {
      return nullptr;
    }
    return static_cast<CFXJS_PerObjectData*>(
        pObj->GetAlignedPointerFromInternalField(0));
  }

  const int m_ObjDefID;
  void* m_pPrivate;
};

class CFXJS_ObjDefinition {
 public:
  static int MaxID(v8::Isolate* pIsolate) {
    return FXJS_PerIsolateData::Get(pIsolate)->m_ObjectDefnArray.size();
  }

  static CFXJS_ObjDefinition* ForID(v8::Isolate* pIsolate, int id) {
    // Note: GetAt() halts if out-of-range even in release builds.
    return FXJS_PerIsolateData::Get(pIsolate)->m_ObjectDefnArray[id].get();
  }

  CFXJS_ObjDefinition(v8::Isolate* isolate,
                      const char* sObjName,
                      FXJSOBJTYPE eObjType,
                      CFXJS_Engine::Constructor pConstructor,
                      CFXJS_Engine::Destructor pDestructor)
      : m_ObjName(sObjName),
        m_ObjType(eObjType),
        m_pConstructor(pConstructor),
        m_pDestructor(pDestructor),
        m_pIsolate(isolate) {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(isolate);
    fun->InstanceTemplate()->SetInternalFieldCount(2);
    fun->SetCallHandler([](const v8::FunctionCallbackInfo<v8::Value>& info) {
      v8::Local<v8::Object> holder = info.Holder();
      ASSERT(holder->InternalFieldCount() == 2);
      holder->SetAlignedPointerInInternalField(0, nullptr);
      holder->SetAlignedPointerInInternalField(1, nullptr);
    });
    if (eObjType == FXJSOBJTYPE_GLOBAL) {
      fun->InstanceTemplate()->Set(
          v8::Symbol::GetToStringTag(isolate),
          v8::String::NewFromUtf8(isolate, "global", v8::NewStringType::kNormal)
              .ToLocalChecked());
    }
    m_FunctionTemplate.Reset(isolate, fun);

    v8::Local<v8::Signature> sig = v8::Signature::New(isolate, fun);
    m_Signature.Reset(isolate, sig);
  }

  int AssignID() {
    FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(m_pIsolate);
    pData->m_ObjectDefnArray.emplace_back(this);
    return pData->m_ObjectDefnArray.size() - 1;
  }

  v8::Local<v8::ObjectTemplate> GetInstanceTemplate() {
    v8::EscapableHandleScope scope(m_pIsolate);
    v8::Local<v8::FunctionTemplate> function =
        m_FunctionTemplate.Get(m_pIsolate);
    return scope.Escape(function->InstanceTemplate());
  }

  v8::Local<v8::Signature> GetSignature() {
    v8::EscapableHandleScope scope(m_pIsolate);
    return scope.Escape(m_Signature.Get(m_pIsolate));
  }

  const char* const m_ObjName;
  const FXJSOBJTYPE m_ObjType;
  const CFXJS_Engine::Constructor m_pConstructor;
  const CFXJS_Engine::Destructor m_pDestructor;

  v8::Isolate* m_pIsolate;
  v8::Global<v8::FunctionTemplate> m_FunctionTemplate;
  v8::Global<v8::Signature> m_Signature;
};

static v8::Local<v8::ObjectTemplate> GetGlobalObjectTemplate(
    v8::Isolate* pIsolate) {
  int maxID = CFXJS_ObjDefinition::MaxID(pIsolate);
  for (int i = 0; i < maxID; ++i) {
    CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(pIsolate, i);
    if (pObjDef->m_ObjType == FXJSOBJTYPE_GLOBAL)
      return pObjDef->GetInstanceTemplate();
  }
  if (!g_DefaultGlobalObjectTemplate) {
    v8::Local<v8::ObjectTemplate> hGlobalTemplate =
        v8::ObjectTemplate::New(pIsolate);
    hGlobalTemplate->Set(
        v8::Symbol::GetToStringTag(pIsolate),
        v8::String::NewFromUtf8(pIsolate, "global", v8::NewStringType::kNormal)
            .ToLocalChecked());
    g_DefaultGlobalObjectTemplate =
        new v8::Global<v8::ObjectTemplate>(pIsolate, hGlobalTemplate);
  }
  return g_DefaultGlobalObjectTemplate->Get(pIsolate);
}

void* FXJS_ArrayBufferAllocator::Allocate(size_t length) {
  return calloc(1, length);
}

void* FXJS_ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return malloc(length);
}

void FXJS_ArrayBufferAllocator::Free(void* data, size_t length) {
  free(data);
}

void V8TemplateMapTraits::Dispose(v8::Isolate* isolate,
                                  v8::Global<v8::Object> value,
                                  void* key) {
  v8::Local<v8::Object> obj = value.Get(isolate);
  if (obj.IsEmpty())
    return;
  CFXJS_Engine* pEngine = CFXJS_Engine::CurrentEngineFromIsolate(isolate);
  int id = pEngine->GetObjDefnID(obj);
  if (id == -1)
    return;
  CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(isolate, id);
  if (!pObjDef)
    return;
  if (pObjDef->m_pDestructor)
    pObjDef->m_pDestructor(pEngine, obj);
  CFXJS_Engine::FreeObjectPrivate(obj);
}

V8TemplateMapTraits::MapType* V8TemplateMapTraits::MapFromWeakCallbackInfo(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  V8TemplateMap* pMap =
      (FXJS_PerIsolateData::Get(data.GetIsolate()))->m_pDynamicObjsMap.get();
  return pMap ? &pMap->m_map : nullptr;
}

void FXJS_Initialize(unsigned int embedderDataSlot, v8::Isolate* pIsolate) {
  if (g_isolate) {
    ASSERT(g_embedderDataSlot == embedderDataSlot);
    ASSERT(g_isolate == pIsolate);
    return;
  }
  g_embedderDataSlot = embedderDataSlot;
  g_isolate = pIsolate;
}

void FXJS_Release() {
  ASSERT(!g_isolate || g_isolate_ref_count == 0);
  delete g_DefaultGlobalObjectTemplate;
  g_DefaultGlobalObjectTemplate = nullptr;
  g_isolate = nullptr;

  delete g_arrayBufferAllocator;
  g_arrayBufferAllocator = nullptr;
}

bool FXJS_GetIsolate(v8::Isolate** pResultIsolate) {
  if (g_isolate) {
    *pResultIsolate = g_isolate;
    return false;
  }
  // Provide backwards compatibility when no external isolate.
  if (!g_arrayBufferAllocator)
    g_arrayBufferAllocator = new FXJS_ArrayBufferAllocator();
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator = g_arrayBufferAllocator;
  *pResultIsolate = v8::Isolate::New(params);
  return true;
}

size_t FXJS_GlobalIsolateRefCount() {
  return g_isolate_ref_count;
}

V8TemplateMap::V8TemplateMap(v8::Isolate* isolate) : m_map(isolate) {}

V8TemplateMap::~V8TemplateMap() {}

void V8TemplateMap::set(void* key, v8::Local<v8::Object> handle) {
  ASSERT(!m_map.Contains(key));
  m_map.Set(key, handle);
}

FXJS_PerIsolateData::~FXJS_PerIsolateData() {}

// static
void FXJS_PerIsolateData::SetUp(v8::Isolate* pIsolate) {
  if (!pIsolate->GetData(g_embedderDataSlot))
    pIsolate->SetData(g_embedderDataSlot, new FXJS_PerIsolateData(pIsolate));
}

// static
FXJS_PerIsolateData* FXJS_PerIsolateData::Get(v8::Isolate* pIsolate) {
  return static_cast<FXJS_PerIsolateData*>(
      pIsolate->GetData(g_embedderDataSlot));
}

FXJS_PerIsolateData::FXJS_PerIsolateData(v8::Isolate* pIsolate)
    : m_pDynamicObjsMap(new V8TemplateMap(pIsolate)) {}

CFXJS_Engine::CFXJS_Engine() : m_isolate(nullptr) {}

CFXJS_Engine::CFXJS_Engine(v8::Isolate* pIsolate) : m_isolate(pIsolate) {}

CFXJS_Engine::~CFXJS_Engine() {
  m_V8PersistentContext.Reset();
}

// static
CFXJS_Engine* CFXJS_Engine::CurrentEngineFromIsolate(v8::Isolate* pIsolate) {
  return static_cast<CFXJS_Engine*>(
      pIsolate->GetCurrentContext()->GetAlignedPointerFromEmbedderData(
          kPerContextDataIndex));
}

// static
int CFXJS_Engine::GetObjDefnID(v8::Local<v8::Object> pObj) {
  CFXJS_PerObjectData* pData = CFXJS_PerObjectData::GetFromObject(pObj);
  return pData ? pData->m_ObjDefID : -1;
}

// static
void CFXJS_Engine::FreeObjectPrivate(void* pPerObjectData) {
  delete static_cast<CFXJS_PerObjectData*>(pPerObjectData);
}

// static
void CFXJS_Engine::FreeObjectPrivate(v8::Local<v8::Object> pObj) {
  CFXJS_PerObjectData* pData = CFXJS_PerObjectData::GetFromObject(pObj);
  pObj->SetAlignedPointerInInternalField(0, nullptr);
  pObj->SetAlignedPointerInInternalField(1, nullptr);
  delete pData;
}

int CFXJS_Engine::DefineObj(const char* sObjName,
                            FXJSOBJTYPE eObjType,
                            CFXJS_Engine::Constructor pConstructor,
                            CFXJS_Engine::Destructor pDestructor) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  FXJS_PerIsolateData::SetUp(m_isolate);
  CFXJS_ObjDefinition* pObjDef = new CFXJS_ObjDefinition(
      m_isolate, sObjName, eObjType, pConstructor, pDestructor);
  return pObjDef->AssignID();
}

void CFXJS_Engine::DefineObjMethod(int nObjDefnID,
                                   const char* sMethodName,
                                   v8::FunctionCallback pMethodCall) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(
      m_isolate, pMethodCall, v8::Local<v8::Value>(), pObjDef->GetSignature());
  fun->RemovePrototype();
  pObjDef->GetInstanceTemplate()->Set(
      v8::String::NewFromUtf8(m_isolate, sMethodName,
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      fun, v8::ReadOnly);
}

void CFXJS_Engine::DefineObjProperty(int nObjDefnID,
                                     const char* sPropName,
                                     v8::AccessorGetterCallback pPropGet,
                                     v8::AccessorSetterCallback pPropPut) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  pObjDef->GetInstanceTemplate()->SetAccessor(
      v8::String::NewFromUtf8(m_isolate, sPropName, v8::NewStringType::kNormal)
          .ToLocalChecked(),
      pPropGet, pPropPut);
}

void CFXJS_Engine::DefineObjAllProperties(
    int nObjDefnID,
    v8::NamedPropertyQueryCallback pPropQurey,
    v8::NamedPropertyGetterCallback pPropGet,
    v8::NamedPropertySetterCallback pPropPut,
    v8::NamedPropertyDeleterCallback pPropDel) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  pObjDef->GetInstanceTemplate()->SetNamedPropertyHandler(pPropGet, pPropPut,
                                                          pPropQurey, pPropDel);
}

void CFXJS_Engine::DefineObjConst(int nObjDefnID,
                                  const char* sConstName,
                                  v8::Local<v8::Value> pDefault) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  pObjDef->GetInstanceTemplate()->Set(m_isolate, sConstName, pDefault);
}

void CFXJS_Engine::DefineGlobalMethod(const char* sMethodName,
                                      v8::FunctionCallback pMethodCall) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(m_isolate, pMethodCall);
  fun->RemovePrototype();
  GetGlobalObjectTemplate(m_isolate)->Set(
      v8::String::NewFromUtf8(m_isolate, sMethodName,
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      fun, v8::ReadOnly);
}

void CFXJS_Engine::DefineGlobalConst(const wchar_t* sConstName,
                                     v8::FunctionCallback pConstGetter) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFX_ByteString bsConst = FX_UTF8Encode(CFX_WideStringC(sConstName));
  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(m_isolate, pConstGetter);
  fun->RemovePrototype();
  GetGlobalObjectTemplate(m_isolate)->SetAccessorProperty(
      v8::String::NewFromUtf8(m_isolate, bsConst.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      fun);
}

void CFXJS_Engine::InitializeEngine() {
  if (m_isolate == g_isolate)
    ++g_isolate_ref_count;

  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);

  // This has to happen before we call GetGlobalObjectTemplate because that
  // method gets the PerIsolateData from m_isolate.
  FXJS_PerIsolateData::SetUp(m_isolate);

  v8::Local<v8::Context> v8Context =
      v8::Context::New(m_isolate, nullptr, GetGlobalObjectTemplate(m_isolate));
  v8::Context::Scope context_scope(v8Context);

  v8Context->SetAlignedPointerInEmbedderData(kPerContextDataIndex, this);

  int maxID = CFXJS_ObjDefinition::MaxID(m_isolate);
  m_StaticObjects.resize(maxID + 1);
  for (int i = 0; i < maxID; ++i) {
    CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(m_isolate, i);
    if (pObjDef->m_ObjType == FXJSOBJTYPE_GLOBAL) {
      CFXJS_PerObjectData::SetInObject(new CFXJS_PerObjectData(i),
                                       v8Context->Global()
                                           ->GetPrototype()
                                           ->ToObject(v8Context)
                                           .ToLocalChecked());
      if (pObjDef->m_pConstructor) {
        pObjDef->m_pConstructor(this, v8Context->Global()
                                          ->GetPrototype()
                                          ->ToObject(v8Context)
                                          .ToLocalChecked());
      }
    } else if (pObjDef->m_ObjType == FXJSOBJTYPE_STATIC) {
      v8::Local<v8::String> pObjName =
          v8::String::NewFromUtf8(m_isolate, pObjDef->m_ObjName,
                                  v8::NewStringType::kNormal,
                                  strlen(pObjDef->m_ObjName))
              .ToLocalChecked();

      v8::Local<v8::Object> obj = NewFxDynamicObj(i, true);
      v8Context->Global()->Set(v8Context, pObjName, obj).FromJust();
      m_StaticObjects[i] = new v8::Global<v8::Object>(m_isolate, obj);
    }
  }
  m_V8PersistentContext.Reset(m_isolate, v8Context);
}

void CFXJS_Engine::ReleaseEngine() {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_V8PersistentContext);
  v8::Context::Scope context_scope(context);

  FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(m_isolate);
  if (!pData)
    return;

  m_ConstArrays.clear();

  int maxID = CFXJS_ObjDefinition::MaxID(m_isolate);
  for (int i = 0; i < maxID; ++i) {
    CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(m_isolate, i);
    v8::Local<v8::Object> pObj;
    if (pObjDef->m_ObjType == FXJSOBJTYPE_GLOBAL) {
      pObj =
          context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
    } else if (m_StaticObjects[i] && !m_StaticObjects[i]->IsEmpty()) {
      pObj = v8::Local<v8::Object>::New(m_isolate, *m_StaticObjects[i]);
      delete m_StaticObjects[i];
      m_StaticObjects[i] = nullptr;
    }

    if (!pObj.IsEmpty()) {
      if (pObjDef->m_pDestructor)
        pObjDef->m_pDestructor(this, pObj);
      FreeObjectPrivate(pObj);
    }
  }

  m_V8PersistentContext.Reset();

  if (m_isolate == g_isolate && --g_isolate_ref_count > 0)
    return;

  delete pData;
  m_isolate->SetData(g_embedderDataSlot, nullptr);
}

int CFXJS_Engine::Execute(const CFX_WideString& script, FXJSErr* pError) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::TryCatch try_catch(m_isolate);
  CFX_ByteString bsScript = script.UTF8Encode();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  v8::Local<v8::Script> compiled_script;
  if (!v8::Script::Compile(context,
                           v8::String::NewFromUtf8(m_isolate, bsScript.c_str(),
                                                   v8::NewStringType::kNormal,
                                                   bsScript.GetLength())
                               .ToLocalChecked())
           .ToLocal(&compiled_script)) {
    v8::String::Utf8Value error(try_catch.Exception());
    // TODO(tsepez): return error via pError->message.
    return -1;
  }

  v8::Local<v8::Value> result;
  if (!compiled_script->Run(context).ToLocal(&result)) {
    v8::String::Utf8Value error(try_catch.Exception());
    // TODO(tsepez): return error via pError->message.
    return -1;
  }
  return 0;
}

v8::Local<v8::Object> CFXJS_Engine::NewFxDynamicObj(int nObjDefnID,
                                                    bool bStatic) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  if (nObjDefnID == -1) {
    v8::Local<v8::ObjectTemplate> objTempl = v8::ObjectTemplate::New(m_isolate);
    v8::Local<v8::Object> obj;
    if (!objTempl->NewInstance(context).ToLocal(&obj))
      return v8::Local<v8::Object>();
    return obj;
  }

  FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(m_isolate);
  if (!pData)
    return v8::Local<v8::Object>();

  if (nObjDefnID < 0 || nObjDefnID >= CFXJS_ObjDefinition::MaxID(m_isolate))
    return v8::Local<v8::Object>();

  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  v8::Local<v8::Object> obj;
  if (!pObjDef->GetInstanceTemplate()->NewInstance(context).ToLocal(&obj))
    return v8::Local<v8::Object>();

  CFXJS_PerObjectData* pObjData = new CFXJS_PerObjectData(nObjDefnID);
  CFXJS_PerObjectData::SetInObject(pObjData, obj);
  if (pObjDef->m_pConstructor)
    pObjDef->m_pConstructor(this, obj);

  if (!bStatic && FXJS_PerIsolateData::Get(m_isolate)->m_pDynamicObjsMap)
    FXJS_PerIsolateData::Get(m_isolate)->m_pDynamicObjsMap->set(pObjData, obj);

  return obj;
}

v8::Local<v8::Object> CFXJS_Engine::GetThisObj() {
  v8::Isolate::Scope isolate_scope(m_isolate);
  if (!FXJS_PerIsolateData::Get(m_isolate))
    return v8::Local<v8::Object>();

  // Return the global object.
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
}

void CFXJS_Engine::Error(const CFX_WideString& message) {
  // Conversion from pdfium's wchar_t wide-strings to v8's uint16_t
  // wide-strings isn't handled by v8, so use UTF8 as a common
  // intermediate format.
  CFX_ByteString utf8_message = message.UTF8Encode();
  m_isolate->ThrowException(v8::String::NewFromUtf8(m_isolate,
                                                    utf8_message.c_str(),
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked());
}

void CFXJS_Engine::SetObjectPrivate(v8::Local<v8::Object> pObj, void* p) {
  CFXJS_PerObjectData* pPerObjectData =
      CFXJS_PerObjectData::GetFromObject(pObj);
  if (!pPerObjectData)
    return;
  pPerObjectData->m_pPrivate = p;
}

void* CFXJS_Engine::GetObjectPrivate(v8::Local<v8::Object> pObj) {
  CFXJS_PerObjectData* pData = CFXJS_PerObjectData::GetFromObject(pObj);
  if (!pData && !pObj.IsEmpty()) {
    // It could be a global proxy object.
    v8::Local<v8::Value> v = pObj->GetPrototype();
    v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
    if (v->IsObject()) {
      pData = CFXJS_PerObjectData::GetFromObject(
          v->ToObject(context).ToLocalChecked());
    }
  }
  return pData ? pData->m_pPrivate : nullptr;
}

v8::Local<v8::Value> CFXJS_Engine::GetObjectProperty(
    v8::Local<v8::Object> pObj,
    const CFX_WideString& wsPropertyName) {
  if (pObj.IsEmpty())
    return v8::Local<v8::Value>();
  v8::Local<v8::Value> val;
  if (!pObj->Get(m_isolate->GetCurrentContext(),
                 NewString(wsPropertyName.AsStringC()))
           .ToLocal(&val))
    return v8::Local<v8::Value>();
  return val;
}

std::vector<CFX_WideString> CFXJS_Engine::GetObjectPropertyNames(
    v8::Local<v8::Object> pObj) {
  if (pObj.IsEmpty())
    return std::vector<CFX_WideString>();

  v8::Local<v8::Array> val;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  if (!pObj->GetPropertyNames(context).ToLocal(&val))
    return std::vector<CFX_WideString>();

  std::vector<CFX_WideString> result;
  for (uint32_t i = 0; i < val->Length(); ++i) {
    result.push_back(ToWideString(val->Get(context, i).ToLocalChecked()));
  }

  return result;
}

void CFXJS_Engine::PutObjectProperty(v8::Local<v8::Object> pObj,
                                     const CFX_WideString& wsPropertyName,
                                     v8::Local<v8::Value> pPut) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(),
            NewString(wsPropertyName.AsStringC()), pPut)
      .FromJust();
}


v8::Local<v8::Array> CFXJS_Engine::NewArray() {
  return v8::Array::New(m_isolate);
}

unsigned CFXJS_Engine::PutArrayElement(v8::Local<v8::Array> pArray,
                                       unsigned index,
                                       v8::Local<v8::Value> pValue) {
  if (pArray.IsEmpty())
    return 0;
  if (pArray->Set(m_isolate->GetCurrentContext(), index, pValue).IsNothing())
    return 0;
  return 1;
}

v8::Local<v8::Value> CFXJS_Engine::GetArrayElement(v8::Local<v8::Array> pArray,
                                                   unsigned index) {
  if (pArray.IsEmpty())
    return v8::Local<v8::Value>();
  v8::Local<v8::Value> val;
  if (!pArray->Get(m_isolate->GetCurrentContext(), index).ToLocal(&val))
    return v8::Local<v8::Value>();
  return val;
}

unsigned CFXJS_Engine::GetArrayLength(v8::Local<v8::Array> pArray) {
  if (pArray.IsEmpty())
    return 0;
  return pArray->Length();
}

v8::Local<v8::Context> CFXJS_Engine::NewLocalContext() {
  return v8::Local<v8::Context>::New(m_isolate, m_V8PersistentContext);
}

v8::Local<v8::Context> CFXJS_Engine::GetPersistentContext() {
  return m_V8PersistentContext.Get(m_isolate);
}

v8::Local<v8::Value> CFXJS_Engine::NewNumber(int number) {
  return v8::Int32::New(m_isolate, number);
}

v8::Local<v8::Value> CFXJS_Engine::NewNumber(double number) {
  return v8::Number::New(m_isolate, number);
}

v8::Local<v8::Value> CFXJS_Engine::NewNumber(float number) {
  return v8::Number::New(m_isolate, (float)number);
}

v8::Local<v8::Value> CFXJS_Engine::NewBoolean(bool b) {
  return v8::Boolean::New(m_isolate, b);
}

v8::Local<v8::Value> CFXJS_Engine::NewString(const CFX_ByteStringC& str) {
  v8::Isolate* pIsolate = m_isolate ? m_isolate : v8::Isolate::GetCurrent();
  return v8::String::NewFromUtf8(pIsolate, str.c_str(),
                                 v8::NewStringType::kNormal, str.GetLength())
      .ToLocalChecked();
}

v8::Local<v8::Value> CFXJS_Engine::NewString(const CFX_WideStringC& str) {
  return NewString(FX_UTF8Encode(str).AsStringC());
}

v8::Local<v8::Value> CFXJS_Engine::NewNull() {
  return v8::Local<v8::Value>();
}

v8::Local<v8::Date> CFXJS_Engine::NewDate(double d) {
  return v8::Date::New(m_isolate->GetCurrentContext(), d)
      .ToLocalChecked()
      .As<v8::Date>();
}

int CFXJS_Engine::ToInt32(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return 0;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToInt32(context).ToLocalChecked()->Value();
}

bool CFXJS_Engine::ToBoolean(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return false;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToBoolean(context).ToLocalChecked()->Value();
}

double CFXJS_Engine::ToDouble(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return 0.0;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToNumber(context).ToLocalChecked()->Value();
}

CFX_WideString CFXJS_Engine::ToWideString(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return CFX_WideString();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  v8::String::Utf8Value s(pValue->ToString(context).ToLocalChecked());
  return CFX_WideString::FromUTF8(CFX_ByteStringC(*s, s.length()));
}

v8::Local<v8::Object> CFXJS_Engine::ToObject(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty() || !pValue->IsObject())
    return v8::Local<v8::Object>();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToObject(context).ToLocalChecked();
}

v8::Local<v8::Array> CFXJS_Engine::ToArray(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty() || !pValue->IsArray())
    return v8::Local<v8::Array>();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return v8::Local<v8::Array>::Cast(pValue->ToObject(context).ToLocalChecked());
}

void CFXJS_Engine::SetConstArray(const CFX_WideString& name,
                                 v8::Local<v8::Array> array) {
  m_ConstArrays[name] = v8::Global<v8::Array>(GetIsolate(), array);
}

v8::Local<v8::Array> CFXJS_Engine::GetConstArray(const CFX_WideString& name) {
  return v8::Local<v8::Array>::New(GetIsolate(), m_ConstArrays[name]);
}
