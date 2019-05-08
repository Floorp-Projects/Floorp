async_test(t => {
  const frame = document.createElement("iframe");
  frame.onload = t.step_func_done(() => {
    frame.onload = null;
    const doc = frame.contentDocument;

    // Each open() call should reset the DOM for the document, because even
    // though there is a live parser, that parser's script insertion level is 0.
    for (var i = 0; i < 5; ++i) {
      doc.open();
      doc.write("hello");
    }
    doc.close()
    assert_equals(doc.documentElement.textContent, "hello");
  });
  t.add_cleanup(() => frame.remove());
  document.body.appendChild(frame);
}, "open() should reset the DOM even if there's an active parser, if that parser's script insertion level is 0");
