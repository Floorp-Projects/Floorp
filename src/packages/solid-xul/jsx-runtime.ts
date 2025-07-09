import type { JSX as SolidJSX } from "solid-js";

declare module "solid-js" {
  namespace JSX {
    type XULElementBase =
      & Omit<SolidJSX.HTMLAttributes<HTMLElement>, "oncommand">
      & {
        flex?: `${number}`;
        "data-l10n-id"?: string;
        align?: "center";
        oncommand?: string | (() => void);
      };
    interface XULBrowserElement extends XULElementBase {
      contextmenu?: string;
      message?: string;
      messagemanagergroup?: string;
      type?: "content";
      remote?: `${boolean}`;
      maychangeremoteness?: `${boolean}`;
      initiallyactive?: string;

      autocompletepopup?: string;
      src?: string;
      disablefullscreen?: `${boolean}`;
      disablehistory?: `${boolean}`;
      nodefaultsrc?: string;
      tooltip?: string;
      xmlns?: string;
      autoscroll?: `${boolean}`;
      disableglobalhistory?: `${boolean}`;
      initialBrowsingContextGroupId?: `${number}`;
      usercontextid?: `${number}`;
      changeuseragent?: `${boolean}`;
      context?: string;
    }

    interface XULMenuListElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      oncommand?: string | (() => void);
      onCommand?: () => void;
      value?: string;
    }

    interface XULMenuitemElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      type?: "checkbox";
      checked?: boolean;
      disabled?: boolean;
      oncommand?: string | (() => void);
      onCommand?: () => void;
      onMouseDown?: (event: MouseEvent) => void;
      value?: string;
    }

    interface XULRichListItem extends XULElementBase {
      value?: string;
      helpTopic?: string;
    }

    interface XULPopupSetElement extends XULElementBase {
      onpopupshowing?: string | (() => void);
    }

    interface XULMenuPopupElement extends XULElementBase {
      position?:
        | "after_start"
        | "bottomright topright"
        | "end_before"
        | "bottomleft topleft"
        | "overlap";
      onpopupshowing?: string;
      onpopuphiding?: string;
      onPopupShowing?: (e: Event) => void;
      onPopupHiding?: (e: Event) => void;
    }

    interface XULPanelElement extends XULElementBase {
      type?: "arrow";
      position?:
        | "after_start"
        | "end_before"
        | "bottomleft topleft"
        | "bottomright topright"
        | "overlap";
      onPopupShowing?: () => void;
      onPopupHiding?: () => void;
      onpopupshowing?: string;
    }

    interface XULMenuElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      onpopupshowing?: string | ((event: Event) => void);
    }

    interface XULBoxElement extends XULElementBase {
      pack?: string;
      orient?: "horizontal" | "vertical";
      popup?: string;
      clicktoscroll?: boolean;
    }

    interface XULToolbarButtonElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      oncommand?: string | (() => void);
      onCommand?: () => void;
      context?: string;
      image?: string;
      hidden?: boolean;
      closemenu?: "none" | "all" | "current" | "parent";
    }

    interface XULButtonElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      oncommand?: string | (() => void);
      onCommand?: () => void;
      context?: string;
      image?: string;
    }

    interface XULImageElement extends XULElementBase {
      src?: string;
      width?: string;
      height?: string;
    }

    interface XULTabElement extends XULElementBase {
      onwheel?: (event: WheelEvent) => void;
    }

    interface IntrinsicElements {
      "xul:arrowscrollbox": XULElementBase;
      "xul:browser": XULBrowserElement;
      "xul:button": XULButtonElement;
      "xul:menuitem": XULMenuitemElement;
      "xul:window": XULElementBase;
      "xul:linkset": XULElementBase;
      "xul:popupset": XULPopupSetElement;
      "xul:tooltip": XULElementBase;
      "xul:toolbaritem": XULElementBase & {
        role?: string;
        ariaLabel?: string;
        ariaLevel?: number;
        orient?: "vertical" | "horizontal";
        smoothscroll?: boolean;
        flatList?: boolean;
        tooltip?: string;
        context?: string;
      };
      "xul:tab": XULTabElement;
      "xul:panel": XULPanelElement;
      "xul:panelview": XULPanelElement;
      "xul:menupopup": XULMenuPopupElement;
      "xul:menulist": XULMenuListElement;
      "xul:vbox": XULBoxElement;
      "xul:hbox": XULBoxElement;
      "xul:box": XULElementBase & {
        context?: string;
      };
      "xul:toolbar": {
        id?: string;
        toolbarname?: string;
        customizable?: string;
        mode?: string;
        context?: string;
        accesskey?: string;
        style?: string;
        class?: string;
        children: Element;
      };
      "xul:toolbarbutton": XULToolbarButtonElement;
      "xul:toolbarseparator": XULElementBase;
      "xul:spacer": XULElementBase;
      "xul:splitter": XULElementBase;
      "xul:menuseparator": XULElementBase;
      "xul:menu": XULMenuElement;
      "xul:keyset": {
        id?: string;
        children: Element;
      };
      "xul:key": {
        id?: string;
        "data-l10n-id"?: string;
        "data-l10n-attrs"?: string;
        modifiers?: string;
        keycode?: string;
        key?: string;
        command: string;
      };
      "xul:commandset": {
        id?: string;
        children: Element;
      };
      "xul:command": {
        id: string;
        oncommand: string | (() => void);
      };
      "xul:description": XULElementBase;
      "xul:checkbox": XULElementBase;
      "xul:richlistitem": XULRichListItem;
      "xul:image": XULImageElement;
      "xul:label": XULElementBase;
    }

    interface Directives {
      dndzone: Accessor<unknown[]>;
    }
  }
}
