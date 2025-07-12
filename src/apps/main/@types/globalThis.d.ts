/// <reference path="../../../../@types/gecko/dom/index.d.ts" />

declare global {
  var gFloorp: {
    tabColor?: {
      setEnable: (value: boolean) => void;
    };
  };
  var gBrowser: any;
  var TabContextMenu: any;
  var SessionStore: any;
  var E10SUtils: any;
  var bmsLoadedURI: string;
  var floorpWebPanelWindow: boolean;
  var ZoomManager: any;
  var floorpBmsUserAgent: boolean | undefined | null;
}

export {};
