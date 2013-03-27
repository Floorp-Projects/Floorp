function test()
{
  let data = [
    { value: "/tmp", result: "tmp" },
    { title: "foo", result: "foo" },
    { result: "No file selected." },
    { multiple: true, result: "No files selected." },
    { required: true, result: "Please select a file." }
  ];

  let doc = gBrowser.contentDocument;
  let tooltip = document.getElementById("aHTMLTooltip");

  for (let test of data) {
    let input = doc.createElement('input');
    doc.body.appendChild(input);
    input.type = 'file';
    if (test.title) {
      input.setAttribute('title', test.title);
    }
    if (test.value) {
      if (test.value == "/tmp" && navigator.platform.indexOf('Win') != -1) {
        test.value = "C:\\Temp";
        test.result = "Temp";
      }
      input.value = test.value;
    }
    if (test.multiple) {
      input.multiple = true;
    }
    if (test.required) {
      input.required = true;
    }

    ok(tooltip.fillInPageTooltip(input));
    is(tooltip.getAttribute('label'), test.result);
  }
}
