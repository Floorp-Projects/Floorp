export class NRRestartBrowserParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "RestartBrowser:Restart": {
        let result = false;
        try {
          Services.startup.quit(
            Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart,
          );
          result = true;
        } catch (error) {
          console.error("Failed to restart browser:", error);
        }

        this.sendAsyncMessage("RestartBrowser:Result", result);
      }
    }
  }
}
