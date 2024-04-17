# Machine Learning

This component is an experimental machine learning local inference engine based on
Transformers.js and the ONNX runtime.


In the example below, an image is converted to text using the `image-to-text` task.


```js
const {PipelineOptions, EngineProcess } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");

// First we create a pipeline options object, which contains the task name
// and any other options needed for the task
const options = new PipelineOptions({taskName: "image-to-text" });

// Next, we create an engine parent object via EngineProcess
const engineParent = await EngineProcess.getMLEngineParent();

// We then create the engine object, using the options
const engine = engineParent.getEngine(options);

// At this point we are ready to do some inference.

// We need to get the image as an array buffer and wrap it into a request object
const response = await fetch("https://huggingface.co/datasets/mishig/sample_images/resolve/main/football-match.jpg");
const buffer = await response.arrayBuffer();
const mimeType = response.headers.get('Content-Type');
const request = {
  data: buffer,
  mimeType: mimeType
};

// Finally, we run the engine with the request object
const res = await engine.run(request);

// The result is a string containing the text extracted from the image
console.log(res);
```
