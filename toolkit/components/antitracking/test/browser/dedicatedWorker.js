self.onmessage = msg => {
  self.postMessage(msg.data);
};
