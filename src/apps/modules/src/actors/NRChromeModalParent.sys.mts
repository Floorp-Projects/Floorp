export class NRChromeModalParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "ChromeModal:Open": {
        console.debug("NRChromeModalParent received message: ChromeModal:Open");
      }
    }
  }
}
