export class NRRestartBrowserChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRRestartBrowserChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://")
    ) {
      console.debug("NRRestartBrowser 5183 ! or Chrome Page!");
      Cu.exportFunction(this.NRRestartBrowser.bind(this), window, {
        defineAs: "NRRestartBrowser",
      });
    }
  }

  NRRestartBrowser(callback: (result: boolean) => void = () => {}) {
    const promise = new Promise<boolean>((resolve, _reject) => {
      this.resolveRestartBrowser = resolve;
    });
    this.sendAsyncMessage("RestartBrowser:Restart");
    promise.then((result) => callback(result));
  }

  resolveRestartBrowser: ((result: boolean) => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "RestartBrowser:Result": {
        this.resolveRestartBrowser?.(message.data);
        this.resolveRestartBrowser = null;
        break;
      }
    }
  }
}
