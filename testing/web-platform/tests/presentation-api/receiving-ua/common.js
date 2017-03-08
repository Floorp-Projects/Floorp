(window => {
  // Both a controlling side and a receiving one must share the same Stash ID to
  // transmit data from one to the other. On the other hand, due to polling mechanism
  // which cleans up a stash, stashes in both controller-to-receiver direction
  // and one for receiver-to-controller are necessary.
  window.stashIds = {
    toController: '0c382524-5738-4df0-837d-4f53ea8addc2',
    toReceiver: 'a9618cd1-ca2b-4155-b7f6-630dce953c44'
  }
})(window);