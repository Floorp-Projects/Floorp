/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the shared types for the translations component. The intended use
 * is for defining types to be used in JSDoc. They are used in a form that the TypeScript
 * language server can read them, and provide code hints.
 */

/**
 * For Remote Settings, the JSON details about the attachment.
 */
export interface Attachment {
   // e.g. "2f7c0f7bbc...ca79f0850c4de",
  hash: string;
  // e.g. 5047568,
  size: string;
  // e.g. "lex.50.50.deen.s2t.bin",
  filename: string;
  // e.g. "main-workspace/translations-models/316ebb3a-0682-42cc-8e73-a3ba4bbb280f.bin",
  location: string;
  // e.g. "application/octet-stream"
  mimetype: string;
}

/**
 * The JSON that is synced from Remote Settings for the translation models.
 */
export interface TranslationModelRecord {
  // The full model name, e.g. "lex.50.50.deen.s2t.bin"
  name: string;
  // The BCP 47 language tag, e.g. "de"
  fromLang: string;
  // The BCP 47 language tag, e.g. "en"
  toLang: string;
  // The model record version number, used for handling future format changes. e.g. 1
  version: number;
  //
  fileType: string; // e.g. "lex"
  attachment: Attachment;
  // e.g. "0d4db293-a17c-4085-9bd8-e2e146c85000"
  id: string;
  // e.g. 1673023100578
  schema: number;
  // e.g. 1673455932527
  last_modified: string;
}

/**
 * The JSON that is synced from Remote Settings for the wasm binaries.
 */
export interface WasmRecord {
  // The name of the project, e.g. "bergamot-translator"
  name: string;
  // The human readable identifier for the release. e.g. "v0.4.4"
  release: string;
  // The commit hash for the project that generated the wasm.
  revision: string;
  // The license of the wasm, as a https://spdx.org/licenses/
  license: string;
}

/**
 * The following are the types that are provided by the Bergamot wasm library.
 *
 * See: https://github.com/mozilla/bergamot-translator/tree/main/wasm/bindings
 */
export namespace Bergamot {
  /**
   * The main module that is returned from bergamot-translator.js.
   */
  export interface ModuleExport {
    BlockingService: typeof BlockingService;
    AlignedMemoryList: typeof AlignedMemoryList;
    TranslationModel: typeof TranslationModel;
    AlignedMemory: typeof AlignedMemory;
    VectorResponseOptions: typeof VectorResponseOptions;
    VectorString: typeof VectorString;
  }

  /**
   * This class represents a C++ std::vector. The implementations will extend from it.
   */
  export class Vector<T> {
    size(): number;
    get(index: number): T;
    push_back(item: T);
  }

  export class VectorResponse extends Vector<Response> {}
  export class VectorString extends Vector<string> {}
  export class VectorResponseOptions extends Vector<ResponseOptions> {}
  export class AlignedMemoryList extends Vector<AlignedMemory> {}

  /**
   * A blocking (e.g. non-threaded) translation service, via Bergamot.
   */
  export class BlockingService {
    /**
     * Translate multiple messages in a single synchronous API call using a single model.
     */
    translate(
      translationModel,
      vectorSourceText: VectorString,
      vectorResponseOptions: VectorResponseOptions
    ): VectorResponse;

    /**
     * Translate by pivoting between two models
     *
     * For example to translate "fr" to "es", pivot using "en":
     *   "fr" to "en"
     *   "en" to "es"
     *
     * See https://github.com/mozilla/bergamot-translator/blob/5ae1b1ebb3fa9a3eabed8a64ca6798154bd486eb/src/translator/service.h#L80
     */
    translateViaPivoting(
      first: TranslationModel,
      second: TranslationModel,
      vectorSourceText: VectorString,
      vectorResponseOptions: VectorResponseOptions
    ): VectorResponse;
  }

  /**
   * The actual translation model, which is passed into the `BlockingService` methods.
   */
  export class TranslationModel {}

  /**
   * The models need to be placed in the wasm memory space. This object represents
   * aligned memory that was allocated on the wasm side of things. The memory contents
   * can be set via the getByteArrayView method and the Uint8Array.prototype.set method.
   */
  export class AlignedMemory {
    constructor(size: number, alignment: number);
    size(): number;
    getByteArrayView(): Uint8Array;
  }
  
  /**
   * The response from the translation. This definition isn't complete, but just
   * contains a subset of the available methods.
   *
   * See https://github.com/mozilla/bergamot-translator/blob/main/src/translator/response.h
   */
  export class Response {
    getOriginalText(): string;
    getTranslatedText(): string;
  }
  
  /**
   * The options to configure a translation response.
   *
   * See https://github.com/mozilla/bergamot-translator/blob/main/src/translator/response_options.h
   */
  export class ResponseOptions {
    // Include the quality estimations.
    qualityScores: boolean;
    // Include the alignments.
    alignment: boolean;
    // Remove HTML tags from text and insert it back into the output.
    html: boolean;
    // Whether to include sentenceMappings or not. Alignments require
    // sentenceMappings and are available irrespective of this option if
    // `alignment=true`.
    sentenceMappings: boolean
  }
}


/**
 * The client to interact with RemoteSettings.
 * See services/settings/RemoteSettingsClient.jsm
 */
interface RemoteSettingsClient {
  on: Function,
  get: Function,
  attachments: any,
}

/**
 * A single language model file.
 */
interface LanguageTranslationModelFile {
  buffer: ArrayBuffer,
  record: TranslationModelRecord,
}

/**
 * The files necessary to run the translations, these will be sent to the Bergamot
 * translation engine.
 */
interface LanguageTranslationModelFiles {
  // The machine learning language model.
  model: LanguageTranslationModelFile,
  // The lexical shortlist that limits possible output of the decoder and makes
  // inference faster.
  lex: LanguageTranslationModelFile,
  // A model that can generate a translation quality estimation.
  qualityModel?: LanguageTranslationModelFile,

  // There is either a single vocab file:
  vocab?: LanguageTranslationModelFile,

  // Or there are two:
  srcvocab?: LanguageTranslationModelFile,
  trgvocab?: LanguageTranslationModelFile,
};

/**
 * This is the type that is generated when the models are loaded into wasm aligned memory.
 */
type LanguageTranslationModelFilesAligned = {
  [K in keyof LanguageTranslationModelFiles]: AlignedMemory
};

/**
 * These are the files that are downloaded from Remote Settings that are necessary
 * to start the translations engine. These may not be available if running in tests,
 * and so the engine will be mocked.
 */
interface TranslationsEnginePayload {
  bergamotWasmArrayBuffer: ArrayBuffer,
  languageModelFiles: LanguageModelFiles[]
}

/**
 * These are the files that are downloaded from Remote Settings that are necessary
 * to start the language-identification engine. These may not be available if running
 * in tests.
 */
interface LanguageIdEnginePayload {
  wasmBuffer: ArrayBuffer,
  modelBuffer: ArrayBuffer,
}

/**
 * These are the values which a LanguageIdEngine returns after identifying a language.
 * These values are pre-determined when creating a mocked language-identification engine
 * and the mocked engine will always return those values.
 */
interface LanguageIdEngineMockedPayload {
  confidence: number,
  languageLabel: string,
}

/**
 * Nodes that are being translated are given priority according to their visibility.
 */
export type NodeVisibility = "in-viewport" | "out-of-viewport" | "hidden";

/**
 * Used to decide how to translate a page for full page translations.
 */
export interface LangTags {
  appLangTag: string,
  docLangTag: string,
}
