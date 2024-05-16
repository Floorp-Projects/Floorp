Machine Learning
================

This component is an experimental machine learning local inference engine based on
Transformers.js and the ONNX runtime.

In the example below, an image is converted to text using the `image-to-text` task.


.. code-block:: javascript

  const {PipelineOptions, EngineProcess } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");

  // First we create a pipeline options object, which contains the task name
  // and any other options needed for the task
  const options = new PipelineOptions({taskName: "image-to-text" });

  // Next, we create an engine parent object via EngineProcess
  const engineParent = await EngineProcess.getMLEngineParent();

  // We then create the engine object, using the options
  const engine = engineParent.getEngine(options);

  // Preparing a request
  const request = {url: "https://huggingface.co/datasets/mishig/sample_images/resolve/main/football-match.jpg"};

  // At this point we are ready to do some inference.
  const res = await engine.run(request);

  // The result is a string containing the text extracted from the image
  console.log(res);


Supported Inference Tasks
:::::::::::::::::::::::::

The following tasks are supported by the machine learning engine:

.. js:autofunction:: imageToText
