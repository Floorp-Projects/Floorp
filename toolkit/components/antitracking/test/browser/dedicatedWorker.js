self.onmessage = msg => {
  if (msg.data == "getCookies") {
    fetch("cookies.sjs")
      .then(response => response.text())
      .then(data => postMessage(data));
    return;
  }

  self.postMessage(msg.data);
};
