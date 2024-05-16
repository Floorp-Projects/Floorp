/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This import does not use Chromutils because the next version of the library
// will require an async import, which is not supported by importESModule,
// so we'll just add await here.
import {
  env,
  RawImage,
  AutoProcessor,
  AutoTokenizer,
  AutoModelForVision2Seq,
} from "chrome://global/content/ml/transformers-dev.js";

/**
 * Lazy initialization container.
 *
 * @type {object}
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(
  lazy,
  {
    arrayBufferToBlobURL: "chrome://global/content/ml/Utils.sys.mjs",
  },
  { global: "current" }
);

// Using a custom console, see https://bugzilla.mozilla.org/show_bug.cgi?id=1891789
let _logLevel = "Error";

function debug(...args) {
  if (["Debug", "Trace", "All"].includes(_logLevel)) {
    console.log("ML:", ...args); // eslint-disable-line no-console
  }
}

/**
 * Echo inference for testing purposes.
 *
 * @async
 * @param {object} request - The request object containing image data.
 * @param {object} _model - The model used for inference.
 * @param {object} _tokenizer - The tokenizer used for decoding.
 * @param {object} _processor - The processor used for preparing image data.
 * @returns {Promise<object>} The result object containing the processed text.
 */
async function echo(request, _model, _tokenizer, _processor) {
  return {
    metrics: {
      tokenizingTime: 0,
    },
    output: request.data,
  };
}

/**
 * Converts an image to text using a machine learning model.
 *
 * @async
 * @param {object} request - The request object containing image data.
 * @param {string} [request.url] - The URL of the image to process. If `url` is not provided, other fields are used.
 * @param {ArrayBuffer} [request.data] - The raw image data to process. Ignored if `url` is provided.
 * @param {number} [request.width] - The image width. Ignored if `url` is provided.
 * @param {number} [request.height] - The image height. Ignored if `url` is provided.
 * @param {number} [request.channels] - The image channels. Can be 1, 2, 3 or 4. Defaults to 4. Ignored if `url` is provided.
 * @param {object} model - The model used for inference.
 * @param {object} tokenizer - The tokenizer used for decoding.
 * @param {object} processor - The processor used for preparing image data.
 * @returns {Promise<object>} The result object containing the processed text.
 */
async function imageToText(request, model, tokenizer, processor) {
  let result = {
    metrics: {
      inferenceTime: 0,
      tokenizingTime: 0,
    },
  };
  let start = Date.now();
  let rawImage;

  if ("url" in request) {
    rawImage = await RawImage.fromURL(request.url);
  } else {
    rawImage = new RawImage(
      request.data,
      request.width,
      request.height,
      request.channels || 4
    );
  }

  debug("Image loaded in ", Date.now() - start);

  const { pixel_values } = await processor(rawImage);
  result.metrics.tokenizingTime += Date.now() - start;
  const toReturn = [];
  for (const batch of pixel_values) {
    batch.dims = [1, ...batch.dims];
    start = Date.now();
    const output = await model.generate(batch);
    result.metrics.inferenceTime += Date.now() - start;
    start = Date.now();
    const decoded = tokenizer
      .batch_decode(output, {
        skip_special_tokens: true,
      })
      .map(x => ({ generated_text: x.trim() }));
    result.metrics.tokenizingTime += Date.now() - start;
    toReturn.push(decoded);
  }
  debug("Inference done in ", Date.now() - start);
  result.output = toReturn[0][0].generated_text;
  return result;
}

/**
 * Configuration for engine. Each task has a configuration object that
 * gets merged at runtime with the options from PipelineOptions.
 *
 * When a key exists in both the default configuration and the options,
 * the value from the options is used.
 *
 * The configuration keys that are not exposed as options are all the
 * callables that are used in the pipeline:
 *
 * - modelClass
 * - tokenizerClass
 * - processorClass
 * - pipelineFunction
 *
 * @type {object}
 */
const ENGINE_CONFIGURATION = {
  "image-to-text": {
    modelId: "mozilla/distilvit",
    modelClass: AutoModelForVision2Seq,
    tokenizerId: "mozilla/distilvit",
    tokenizerClass: AutoTokenizer,
    processorId: "mozilla/distilvit",
    processorClass: AutoProcessor,
    pipelineFunction: imageToText,
  },
  echo: {
    modelId: null,
    modelClass: null,
    tokenizerId: null,
    tokenizerClass: null,
    processorId: null,
    processorClass: null,
    pipelineFunction: echo,
  },
};

/**
 * Represents a pipeline for processing machine learning tasks.
 */
export class Pipeline {
  #modelCache = null;
  #model = null;
  #tokenizer = null;
  #processor = null;
  #pipelineFunction = null;
  #taskName = null;
  #initTime = 0;
  #isReady = false;

  /**
   * Creates an instance of a Pipeline.
   *
   * @param {object} modelCache - Implements the Cache interface and used to get models
   * @param {object} config - The configuration options
   */
  constructor(modelCache, config) {
    let start = Date.now();
    this.#modelCache = modelCache;

    _logLevel = config.logLevel || "Error";
    // Setting up the Transformers.js environment
    // See https://huggingface.co/docs/transformers.js/api/env

    // Caching strategy.
    // Here we make sure that everytime transformers.js requires a file, it uses
    // modelCache, which transfers the request to the main thread and uses the
    // ModelHub that caches files into IndexDB.
    env.useBrowserCache = false;
    env.allowLocalModels = false;
    env.remoteHost = config.modelHubRootUrl;
    env.remotePathTemplate = config.modelHubUrlTemplate;
    env.useCustomCache = true;
    env.customCache = this.#modelCache;
    env.localModelPath = "/";

    // ONNX runtime - we set up the wasm runtime we got from RS for the ONNX backend to pick
    debug("Setting up ONNX backend");
    env.backends.onnx.wasm.wasmPaths = {};
    env.backends.onnx.wasm.wasmPaths[config.runtimeFilename] =
      lazy.arrayBufferToBlobURL(config.runtime);

    if (config.modelClass && config.modelId) {
      debug(`Loading model ${config.modelId} with class ${config.modelClass}`);
      this.#model = config.modelClass.from_pretrained(config.modelId);
    }
    if (config.tokenizerClass && config.tokenizerId) {
      debug(
        `Loading tokenizer ${config.tokenizerId} with class ${config.tokenizerClass}`
      );
      this.#tokenizer = config.tokenizerClass.from_pretrained(
        config.tokenizerId
      );
    }
    if (config.processorClass && config.processorId) {
      debug(
        `Loading processor ${config.processorId} with class ${config.processorClass}`
      );
      this.#processor = config.processorClass.from_pretrained(
        config.processorId
      );
    }
    this.#taskName = config.taskName;
    this.#pipelineFunction = config.pipelineFunction.bind(this);
    this.#initTime = Date.now() - start;
    debug("Pipeline initialized, took ", this.#initTime);
  }

  /**
   * Initializes the pipeline with given options.
   *
   * @static
   * @async
   * @param {object} modelCache - Implements the Cache interface and used to get models
   * @param {ArrayBuffer} runtime - The runtime wasm file.
   * @param {PipelineOptions} options - The options for initialization.
   * @returns {Promise<Pipeline>} The initialized pipeline instance.
   */
  static async initialize(modelCache, runtime, options) {
    const taskName = options.taskName;
    debug(`Initializing Pipeline for task ${taskName}`);

    if (!ENGINE_CONFIGURATION[taskName]) {
      throw new Error(`Task ${taskName} is not supported`);
    }

    // Loading the config defaults for the task
    let config = { ...ENGINE_CONFIGURATION[taskName] };
    config.runtime = runtime;

    // Overriding the defaults with the options
    options.applyToConfig(config);

    if (!config.pipelineFunction) {
      throw new Error("pipelineFunction is required for the pipeline");
    }
    return new Pipeline(modelCache, config);
  }

  /**
   * Runs the pipeline with the given request.
   *
   * @async
   * @param {T} request - The request object to be processed. The fields it may contain
   * depends on the task. See each pipeline function for more details.
   * @returns {Promise<object>} The result object from the pipeline execution.
   */
  async run(request) {
    debug("Running task: ", this.#taskName);
    // Calling all promises to ensure they are resolved before running the first pipeline
    if (!this.#isReady) {
      let start = Date.now();
      debug("Initializing model, tokenizer and processor");

      // deactive console.warn, see https://bugzilla.mozilla.org/show_bug.cgi?id=1891003
      const originalWarn = console.warn;
      console.warn = () => {};
      try {
        this.#model = await this.#model;
        this.#tokenizer = await this.#tokenizer;
        this.#processor = await this.#processor;
        this.#isReady = true;
      } catch (error) {
        debug("Error initializing pipeline", error);
        throw error;
      } finally {
        console.warn = originalWarn;
      }

      this.#initTime += Date.now() - start;
      debug("Pipeline is fully initialized, took ", this.#initTime);
    }

    let result = await this.#pipelineFunction(
      request,
      this.#model,
      this.#tokenizer,
      this.#processor
    );
    result.metrics.initTime = this.#initTime;
    return result;
  }
}
