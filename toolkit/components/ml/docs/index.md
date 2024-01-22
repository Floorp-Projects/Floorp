# Machine Learning

This component is an experimental machine learning local inference engine. Currently there is no inference engine actually integrated yet.

Here is an example of the API:

```js
// The engine process manages the life cycle of the engine. It runs in its own process.
// Models can consume large amounts of memory, and this helps encapsulate it at the
// operating system level.
const EngineProcess = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");

// The MLEngineParent is a JSActor that can communicate with the engine process.
const mlEngineParent = await EngineProcess.getMLEngineParent();


/**
 * When implementing a model, there should be a class that provides a `getModel` function
 * that is responsible for providing the `ArrayBuffer` of the model. Typically this
 * download is managed by RemoteSettings.
 */
class SummarizerModel {
  /**
   * @returns {ArrayBuffer}
   */
  static getModel() { ... }
}

// An engine can be created using a unique name for the engine, and the function
// to get the model. This class handles the life cycle of the engine.
const summarizer = mlEngineParent.getEngine(
  "summarizer",
  SummarizerModel.getModel
);

// In order to run the model, use the `run` method. This will initiate the engine if
// it is needed, and return the result. The messaging to the engine process happens
// through a MessagePort.
const result = await summarizer.run("A sentence that can be summarized.")

// The engine can be explicitly terminated, or it will be destroyed through an idle
// timeout when not in use, as the memory requirements for models can be quite large.
summarizer.terminate();
```
