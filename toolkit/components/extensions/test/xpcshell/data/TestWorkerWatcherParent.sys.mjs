export class TestWorkerWatcherParent extends JSProcessActorParent {
  constructor() {
    super();
    // This is set by the test helper that does use these process actors.
    this.eventEmitter = null;
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "Test:WorkerSpawned":
        this.eventEmitter?.emit("worker-spawned", msg.data);
        break;
      case "Test:WorkerTerminated":
        this.eventEmitter?.emit("worker-terminated", msg.data);
        break;
      default:
        throw new Error(`Unexpected message received: ${msg.name}`);
    }
  }
}
