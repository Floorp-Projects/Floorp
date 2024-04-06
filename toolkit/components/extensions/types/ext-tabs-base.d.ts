// @ts-nocheck

declare namespace tabs_base {

declare function getUserContextIdForCookieStoreId(extension: any, cookieStoreId: any, isPrivateBrowsing: any): any;
declare var DefaultMap: any;
declare var DefaultWeakMap: any;
declare var ExtensionError: any;
declare var parseMatchPatterns: any;
declare var defineLazyGetter: any;
/**
 * The platform-specific type of native tab objects, which are wrapped by
 * TabBase instances.
 *
 * @typedef {object | XULElement} NativeTab
 */
/**
 * @typedef {object} MutedInfo
 * @property {boolean} muted
 *        True if the tab is currently muted, false otherwise.
 * @property {string} [reason]
 *        The reason the tab is muted. Either "user", if the tab was muted by a
 *        user, or "extension", if it was muted by an extension.
 * @property {string} [extensionId]
 *        If the tab was muted by an extension, contains the internal ID of that
 *        extension.
 */
/**
 * A platform-independent base class for extension-specific wrappers around
 * native tab objects.
 *
 * @param {Extension} extension
 *        The extension object for which this wrapper is being created. Used to
 *        determine permissions for access to certain properties and
 *        functionality.
 * @param {NativeTab} nativeTab
 *        The native tab object which is being wrapped. The type of this object
 *        varies by platform.
 * @param {integer} id
 *        The numeric ID of this tab object. This ID should be the same for
 *        every extension, and for the lifetime of the tab.
 */
declare class TabBase {
    constructor(extension: any, nativeTab: any, id: any);
    extension: any;
    tabManager: any;
    id: any;
    nativeTab: any;
    activeTabWindowID: any;
    /**
     * Capture the visible area of this tab, and return the result as a data: URI.
     *
     * @param {BaseContext} context
     *        The extension context for which to perform the capture.
     * @param {number} zoom
     *        The current zoom for the page.
     * @param {object} [options]
     *        The options with which to perform the capture.
     * @param {string} [options.format = "png"]
     *        The image format in which to encode the captured data. May be one of
     *        "png" or "jpeg".
     * @param {integer} [options.quality = 92]
     *        The quality at which to encode the captured image data, ranging from
     *        0 to 100. Has no effect for the "png" format.
     * @param {DOMRectInit} [options.rect]
     *        Area of the document to render, in CSS pixels, relative to the page.
     *        If null, the currently visible viewport is rendered.
     * @param {number} [options.scale]
     *        The scale to render at, defaults to devicePixelRatio.
     * @returns {Promise<string>}
     */
    capture(context: BaseContext, zoom: number, options?: {
        format?: string;
        quality?: integer;
        rect?: DOMRectInit;
        scale?: number;
    }): Promise<string>;
    /**
     * @property {integer | null} innerWindowID
     *        The last known innerWindowID loaded into this tab's docShell. This
     *        property must remain in sync with the last known values of
     *        properties such as `url` and `title`. Any operations on the content
     *        of an out-of-process tab will automatically fail if the
     *        innerWindowID of the tab when the message is received does not match
     *        the value of this property when the message was sent.
     *        @readonly
     */
    readonly get innerWindowID(): any;
    /**
     * @property {boolean} hasTabPermission
     *        Returns true if the extension has permission to access restricted
     *        properties of this tab, such as `url`, `title`, and `favIconUrl`.
     *        @readonly
     */
    readonly get hasTabPermission(): any;
    /**
     * @property {boolean} hasActiveTabPermission
     *        Returns true if the extension has the "activeTab" permission, and
     *        has been granted access to this tab due to a user executing an
     *        extension action.
     *
     *        If true, the extension may load scripts and CSS into this tab, and
     *        access restricted properties, such as its `url`.
     *        @readonly
     */
    readonly get hasActiveTabPermission(): boolean;
    /**
     * @property {boolean} matchesHostPermission
     *        Returns true if the extensions host permissions match the current tab url.
     *        @readonly
     */
    readonly get matchesHostPermission(): any;
    /**
     * @property {boolean} incognito
     *        Returns true if this is a private browsing tab, false otherwise.
     *        @readonly
     */
    readonly get _incognito(): any;
    /**
     * @property {string} _url
     *        Returns the current URL of this tab. Does not do any permission
     *        checks.
     *        @readonly
     */
    readonly get _url(): any;
    /**
     * @property {string | null} url
     *        Returns the current URL of this tab if the extension has permission
     *        to read it, or null otherwise.
     *        @readonly
     */
    readonly get url(): any;
    /**
     * @property {nsIURI} _uri
     *        Returns the current URI of this tab.
     *        @readonly
     */
    readonly get _uri(): any;
    /**
     * @property {string} _title
     *        Returns the current title of this tab. Does not do any permission
     *        checks.
     *        @readonly
     */
    readonly get _title(): any;
    /**
     * @property {nsIURI | null} title
     *        Returns the current title of this tab if the extension has permission
     *        to read it, or null otherwise.
     *        @readonly
     */
    readonly get title(): any;
    /**
     * @property {string} _favIconUrl
     *        Returns the current favicon URL of this tab. Does not do any permission
     *        checks.
     *        @readonly
     *        @abstract
     */
    readonly get _favIconUrl(): void;
    /**
     * @property {nsIURI | null} faviconUrl
     *        Returns the current faviron URL of this tab if the extension has permission
     *        to read it, or null otherwise.
     *        @readonly
     */
    readonly get favIconUrl(): void;
    /**
     * @property {integer} lastAccessed
     *        Returns the last time the tab was accessed as the number of
     *        milliseconds since epoch.
     *        @readonly
     *        @abstract
     */
    readonly get lastAccessed(): void;
    /**
     * @property {boolean} audible
     *        Returns true if the tab is currently playing audio, false otherwise.
     *        @readonly
     *        @abstract
     */
    readonly get audible(): void;
    /**
     * @property {boolean} autoDiscardable
     *        Returns true if the tab can be discarded on memory pressure, false otherwise.
     *        @readonly
     *        @abstract
     */
    readonly get autoDiscardable(): void;
    /**
     * @property {XULElement} browser
     *        Returns the XUL browser for the given tab.
     *        @readonly
     *        @abstract
     */
    readonly get browser(): XULBrowserElement;
    /**
     * @property {BrowsingContext} browsingContext
     *        Returns the BrowsingContext for the given tab.
     *        @readonly
     */
    readonly get browsingContext(): any;
    /**
     * @property {FrameLoader} frameLoader
     *        Returns the frameloader for the given tab.
     *        @readonly
     */
    readonly get frameLoader(): void;
    /**
     * @property {string} cookieStoreId
     *        Returns the cookie store identifier for the given tab.
     *        @readonly
     *        @abstract
     */
    readonly get cookieStoreId(): void;
    /**
     * @property {integer} openerTabId
     *        Returns the ID of the tab which opened this one.
     *        @readonly
     */
    readonly get openerTabId(): any;
    /**
     * @property {integer} discarded
     *        Returns true if the tab is discarded.
     *        @readonly
     *        @abstract
     */
    readonly get discarded(): void;
    /**
     * @property {integer} height
     *        Returns the pixel height of the visible area of the tab.
     *        @readonly
     *        @abstract
     */
    readonly get height(): void;
    /**
     * @property {integer} hidden
     *        Returns true if the tab is hidden.
     *        @readonly
     *        @abstract
     */
    readonly get hidden(): void;
    /**
     * @property {integer} index
     *        Returns the index of the tab in its window's tab list.
     *        @readonly
     *        @abstract
     */
    readonly get index(): void;
    /**
     * @property {MutedInfo} mutedInfo
     *        Returns information about the tab's current audio muting status.
     *        @readonly
     *        @abstract
     */
    readonly get mutedInfo(): void;
    /**
     * @property {SharingState} sharingState
     *        Returns object with tab sharingState.
     *        @readonly
     *        @abstract
     */
    readonly get sharingState(): void;
    /**
     * @property {boolean} pinned
     *        Returns true if the tab is pinned, false otherwise.
     *        @readonly
     *        @abstract
     */
    readonly get pinned(): void;
    /**
     * @property {boolean} active
     *        Returns true if the tab is the currently-selected tab, false
     *        otherwise.
     *        @readonly
     *        @abstract
     */
    readonly get active(): void;
    /**
     * @property {boolean} highlighted
     *        Returns true if the tab is highlighted.
     *        @readonly
     *        @abstract
     */
    readonly get highlighted(): void;
    /**
     * @property {string} status
     *        Returns the current loading status of the tab. May be either
     *        "loading" or "complete".
     *        @readonly
     *        @abstract
     */
    readonly get status(): void;
    /**
     * @property {integer} height
     *        Returns the pixel height of the visible area of the tab.
     *        @readonly
     *        @abstract
     */
    readonly get width(): void;
    /**
     * @property {DOMWindow} window
     *        Returns the browser window to which the tab belongs.
     *        @readonly
     *        @abstract
     */
    readonly get window(): void;
    /**
     * @property {integer} window
     *        Returns the numeric ID of the browser window to which the tab belongs.
     *        @readonly
     *        @abstract
     */
    readonly get windowId(): void;
    /**
     * @property {boolean} attention
     *          Returns true if the tab is drawing attention.
     *          @readonly
     *          @abstract
     */
    readonly get attention(): void;
    /**
     * @property {boolean} isArticle
     *        Returns true if the document in the tab can be rendered in reader
     *        mode.
     *        @readonly
     *        @abstract
     */
    readonly get isArticle(): void;
    /**
     * @property {boolean} isInReaderMode
     *        Returns true if the document in the tab is being rendered in reader
     *        mode.
     *        @readonly
     *        @abstract
     */
    readonly get isInReaderMode(): void;
    /**
     * @property {integer} successorTabId
     *        @readonly
     *        @abstract
     */
    readonly get successorTabId(): void;
    /**
     * Returns true if this tab matches the the given query info object. Omitted
     * or null have no effect on the match.
     *
     * @param {object} queryInfo
     *        The query info against which to match.
     * @param {boolean} [queryInfo.active]
     *        Matches against the exact value of the tab's `active` attribute.
     * @param {boolean} [queryInfo.audible]
     *        Matches against the exact value of the tab's `audible` attribute.
     * @param {boolean} [queryInfo.autoDiscardable]
     *        Matches against the exact value of the tab's `autoDiscardable` attribute.
     * @param {string} [queryInfo.cookieStoreId]
     *        Matches against the exact value of the tab's `cookieStoreId` attribute.
     * @param {boolean} [queryInfo.discarded]
     *        Matches against the exact value of the tab's `discarded` attribute.
     * @param {boolean} [queryInfo.hidden]
     *        Matches against the exact value of the tab's `hidden` attribute.
     * @param {boolean} [queryInfo.highlighted]
     *        Matches against the exact value of the tab's `highlighted` attribute.
     * @param {integer} [queryInfo.index]
     *        Matches against the exact value of the tab's `index` attribute.
     * @param {boolean} [queryInfo.muted]
     *        Matches against the exact value of the tab's `mutedInfo.muted` attribute.
     * @param {boolean} [queryInfo.pinned]
     *        Matches against the exact value of the tab's `pinned` attribute.
     * @param {string} [queryInfo.status]
     *        Matches against the exact value of the tab's `status` attribute.
     * @param {string} [queryInfo.title]
     *        Matches against the exact value of the tab's `title` attribute.
     * @param {string|boolean } [queryInfo.screen]
     *        Matches against the exact value of the tab's `sharingState.screen` attribute, or use true to match any screen sharing tab.
     * @param {boolean} [queryInfo.camera]
     *        Matches against the exact value of the tab's `sharingState.camera` attribute.
     * @param {boolean} [queryInfo.microphone]
     *        Matches against the exact value of the tab's `sharingState.microphone` attribute.
     *
     *        Note: Per specification, this should perform a pattern match, rather
     *        than an exact value match, and will do so in the future.
     * @param {MatchPattern} [queryInfo.url]
     *        Requires the tab's URL to match the given MatchPattern object.
     *
     * @returns {boolean}
     *        True if the tab matches the query.
     */
    matches(queryInfo: {
        active?: boolean;
        audible?: boolean;
        autoDiscardable?: boolean;
        cookieStoreId?: string;
        discarded?: boolean;
        hidden?: boolean;
        highlighted?: boolean;
        index?: integer;
        muted?: boolean;
        pinned?: boolean;
        status?: string;
        title?: string;
        screen?: string | boolean;
        camera?: boolean;
        microphone?: boolean;
        url?: MatchPattern;
    }): boolean;
    /**
     * Converts this tab object to a JSON-compatible object containing the values
     * of its properties which the extension is permitted to access, in the format
     * required to be returned by WebExtension APIs.
     *
     * @param {object} [fallbackTabSize]
     *        A geometry data if the lazy geometry data for this tab hasn't been
     *        initialized yet.
     * @returns {object}
     */
    convert(fallbackTabSize?: object): object;
    /**
     * Query each content process hosting subframes of the tab, return results.
     *
     * @param {string} message
     * @param {object} options
     *        These options are also sent to the message handler in the
     *        `ExtensionContentChild`.
     * @param {number[]} options.frameIds
     *        When omitted, all frames will be queried.
     * @param {boolean} options.returnResultsWithFrameIds
     * @returns {Promise[]}
     */
    queryContent(message: string, options: {
        frameIds: number[];
        returnResultsWithFrameIds: boolean;
    }): Promise<any>[];
    /**
     * Inserts a script or stylesheet in the given tab, and returns a promise
     * which resolves when the operation has completed.
     *
     * @param {BaseContext} context
     *        The extension context for which to perform the injection.
     * @param {InjectDetails} details
     *        The InjectDetails object, specifying what to inject, where, and
     *        when.
     * @param {string} kind
     *        The kind of data being injected. Either "script" or "css".
     * @param {string} method
     *        The name of the method which was called to trigger the injection.
     *        Used to generate appropriate error messages on failure.
     *
     * @returns {Promise}
     *        Resolves to the result of the execution, once it has completed.
     * @private
     */
    private _execute;
    /**
     * Executes a script in the tab's content window, and returns a Promise which
     * resolves to the result of the evaluation, or rejects to the value of any
     * error the injection generates.
     *
     * @param {BaseContext} context
     *        The extension context for which to inject the script.
     * @param {InjectDetails} details
     *        The InjectDetails object, specifying what to inject, where, and
     *        when.
     *
     * @returns {Promise}
     *        Resolves to the result of the evaluation of the given script, once
     *        it has completed, or rejects with any error the evaluation
     *        generates.
     */
    executeScript(context: BaseContext, details: InjectDetails): Promise<any>;
    /**
     * Injects CSS into the tab's content window, and returns a Promise which
     * resolves when the injection is complete.
     *
     * @param {BaseContext} context
     *        The extension context for which to inject the script.
     * @param {InjectDetails} details
     *        The InjectDetails object, specifying what to inject, and where.
     *
     * @returns {Promise}
     *        Resolves when the injection has completed.
     */
    insertCSS(context: BaseContext, details: InjectDetails): Promise<any>;
    /**
     * Removes CSS which was previously into the tab's content window via
     * `insertCSS`, and returns a Promise which resolves when the operation is
     * complete.
     *
     * @param {BaseContext} context
     *        The extension context for which to remove the CSS.
     * @param {InjectDetails} details
     *        The InjectDetails object, specifying what to remove, and from where.
     *
     * @returns {Promise}
     *        Resolves when the operation has completed.
     */
    removeCSS(context: BaseContext, details: InjectDetails): Promise<any>;
}
declare const WINDOW_ID_NONE: -1;
declare const WINDOW_ID_CURRENT: -2;
/**
 * A platform-independent base class for extension-specific wrappers around
 * native browser windows
 *
 * @param {Extension} extension
 *        The extension object for which this wrapper is being created.
 * @param {DOMWindow} window
 *        The browser DOM window which is being wrapped.
 * @param {integer} id
 *        The numeric ID of this DOM window object. This ID should be the same for
 *        every extension, and for the lifetime of the window.
 */
declare class WindowBase {
    /**
     * Returns the window state of the given window.
     *
     * @param {DOMWindow} window
     *        The window for which to return a state.
     *
     * @returns {string}
     *        The window's state. One of "normal", "minimized", "maximized",
     *        "fullscreen", or "docked".
     * @static
     * @abstract
     */
    static getState(window: DOMWindow): string;
    constructor(extension: any, window: any, id: any);
    extension: any;
    window: any;
    id: any;
    /**
     * @property {nsIAppWindow} appWindow
     *        The nsIAppWindow object for this browser window.
     *        @readonly
     */
    readonly get appWindow(): any;
    /**
     * Returns true if this window is the current window for the given extension
     * context, false otherwise.
     *
     * @param {BaseContext} context
     *        The extension context for which to perform the check.
     *
     * @returns {boolean}
     */
    isCurrentFor(context: BaseContext): boolean;
    /**
     * @property {string} type
     *        The type of the window, as defined by the WebExtension API. May be
     *        either "normal" or "popup".
     *        @readonly
     */
    readonly get type(): "popup" | "normal";
    /**
     * Converts this window object to a JSON-compatible object which may be
     * returned to an extension, in the format required to be returned by
     * WebExtension APIs.
     *
     * @param {object} [getInfo]
     *        An optional object, the properties of which determine what data is
     *        available on the result object.
     * @param {boolean} [getInfo.populate]
     *        Of true, the result object will contain a `tabs` property,
     *        containing an array of converted Tab objects, one for each tab in
     *        the window.
     *
     * @returns {object}
     */
    convert(getInfo?: {
        populate?: boolean;
    }): object;
    /**
     * Returns true if this window matches the the given query info object. Omitted
     * or null have no effect on the match.
     *
     * @param {object} queryInfo
     *        The query info against which to match.
     * @param {boolean} [queryInfo.currentWindow]
     *        Matches against against the return value of `isCurrentFor()` for the
     *        given context.
     * @param {boolean} [queryInfo.lastFocusedWindow]
     *        Matches against the exact value of the window's `isLastFocused` attribute.
     * @param {boolean} [queryInfo.windowId]
     *        Matches against the exact value of the window's ID, taking into
     *        account the special WINDOW_ID_CURRENT value.
     * @param {string} [queryInfo.windowType]
     *        Matches against the exact value of the window's `type` attribute.
     * @param {BaseContext} context
     *        The extension context for which the matching is being performed.
     *        Used to determine the current window for relevant properties.
     *
     * @returns {boolean}
     *        True if the window matches the query.
     */
    matches(queryInfo: {
        currentWindow?: boolean;
        lastFocusedWindow?: boolean;
        windowId?: boolean;
        windowType?: string;
    }, context: BaseContext): boolean;
    /**
     * @property {boolean} focused
     *        Returns true if the browser window is currently focused.
     *        @readonly
     *        @abstract
     */
    readonly get focused(): void;
    /**
     * @property {integer} top
     *        Returns the pixel offset of the top of the window from the top of
     *        the screen.
     *        @readonly
     *        @abstract
     */
    readonly get top(): void;
    /**
     * @property {integer} left
     *        Returns the pixel offset of the left of the window from the left of
     *        the screen.
     *        @readonly
     *        @abstract
     */
    readonly get left(): void;
    /**
     * @property {integer} width
     *        Returns the pixel width of the window.
     *        @readonly
     *        @abstract
     */
    readonly get width(): void;
    /**
     * @property {integer} height
     *        Returns the pixel height of the window.
     *        @readonly
     *        @abstract
     */
    readonly get height(): void;
    /**
     * @property {boolean} incognito
     *        Returns true if this is a private browsing window, false otherwise.
     *        @readonly
     *        @abstract
     */
    readonly get incognito(): void;
    /**
     * @property {boolean} alwaysOnTop
     *        Returns true if this window is constrained to always remain above
     *        other windows.
     *        @readonly
     *        @abstract
     */
    readonly get alwaysOnTop(): void;
    /**
     * @property {boolean} isLastFocused
     *        Returns true if this is the browser window which most recently had
     *        focus.
     *        @readonly
     *        @abstract
     */
    readonly get isLastFocused(): void;
    set state(state: void);
    /**
     * @property {string} state
     *        Returns or sets the current state of this window, as determined by
     *        `getState()`.
     *        @abstract
     */
    get state(): void;
    /**
     * @property {nsIURI | null} title
     *        Returns the current title of this window if the extension has permission
     *        to read it, or null otherwise.
     *        @readonly
     */
    readonly get title(): any;
    /**
     * Returns an iterator of TabBase objects for each tab in this window.
     *
     * @returns {Iterator<TabBase>}
     */
    getTabs(): Iterator<TabBase>;
    /**
     * Returns an iterator of TabBase objects for each highlighted tab in this window.
     *
     * @returns {Iterator<TabBase>}
     */
    getHighlightedTabs(): Iterator<TabBase>;
    /**
     * @property {TabBase} The window's currently active tab.
     */
    get activeTab(): void;
    /**
     * Returns the window's tab at the specified index.
     *
     * @param {integer} index
     *        The index of the desired tab.
     *
     * @returns {TabBase|undefined}
     */
    getTabAtIndex(index: integer): TabBase | undefined;
}
/**
 * The parameter type of "tab-attached" events, which are emitted when a
 * pre-existing tab is attached to a new window.
 *
 * @typedef {object} TabAttachedEvent
 * @property {NativeTab} tab
 *        The native tab object in the window to which the tab is being
 *        attached. This may be a different object than was used to represent
 *        the tab in the old window.
 * @property {integer} tabId
 *        The ID of the tab being attached.
 * @property {integer} newWindowId
 *        The ID of the window to which the tab is being attached.
 * @property {integer} newPosition
 *        The position of the tab in the tab list of the new window.
 */
/**
 * The parameter type of "tab-detached" events, which are emitted when a
 * pre-existing tab is detached from a window, in order to be attached to a new
 * window.
 *
 * @typedef {object} TabDetachedEvent
 * @property {NativeTab} tab
 *        The native tab object in the window from which the tab is being
 *        detached. This may be a different object than will be used to
 *        represent the tab in the new window.
 * @property {NativeTab} adoptedBy
 *        The native tab object in the window to which the tab will be attached,
 *        and is adopting the contents of this tab. This may be a different
 *        object than the tab in the previous window.
 * @property {integer} tabId
 *        The ID of the tab being detached.
 * @property {integer} oldWindowId
 *        The ID of the window from which the tab is being detached.
 * @property {integer} oldPosition
 *        The position of the tab in the tab list of the window from which it is
 *        being detached.
 */
/**
 * The parameter type of "tab-created" events, which are emitted when a
 * new tab is created.
 *
 * @typedef {object} TabCreatedEvent
 * @property {NativeTab} tab
 *        The native tab object for the tab which is being created.
 */
/**
 * The parameter type of "tab-removed" events, which are emitted when a
 * tab is removed and destroyed.
 *
 * @typedef {object} TabRemovedEvent
 * @property {NativeTab} tab
 *        The native tab object for the tab which is being removed.
 * @property {integer} tabId
 *        The ID of the tab being removed.
 * @property {integer} windowId
 *        The ID of the window from which the tab is being removed.
 * @property {boolean} isWindowClosing
 *        True if the tab is being removed because the window is closing.
 */
/**
 * An object containing basic, extension-independent information about the window
 * and tab that a XUL <browser> belongs to.
 *
 * @typedef {object} BrowserData
 * @property {integer} tabId
 *        The numeric ID of the tab that a <browser> belongs to, or -1 if it
 *        does not belong to a tab.
 * @property {integer} windowId
 *        The numeric ID of the browser window that a <browser> belongs to, or -1
 *        if it does not belong to a browser window.
 */
/**
 * A platform-independent base class for the platform-specific TabTracker
 * classes, which track the opening and closing of tabs, and manage the mapping
 * of them between numeric IDs and native tab objects.
 *
 * Instances of this class are EventEmitters which emit the following events,
 * each with an argument of the given type:
 *
 * - "tab-attached" {@link TabAttacheEvent}
 * - "tab-detached" {@link TabDetachedEvent}
 * - "tab-created" {@link TabCreatedEvent}
 * - "tab-removed" {@link TabRemovedEvent}
 */
declare class TabTrackerBase {
    on(...args: any[]): any;
    /**
     * Called to initialize the tab tracking listeners the first time that an
     * event listener is added.
     *
     * @protected
     * @abstract
     */
    protected init(): void;
    /**
     * Returns the numeric ID for the given native tab.
     *
     * @param {NativeTab} nativeTab
     *        The native tab for which to return an ID.
     *
     * @returns {integer}
     *        The tab's numeric ID.
     * @abstract
     */
    getId(nativeTab: NativeTab): integer;
    /**
     * Returns the native tab with the given numeric ID.
     *
     * @param {integer} tabId
     *        The numeric ID of the tab to return.
     * @param {*} default_
     *        The value to return if no tab exists with the given ID.
     *
     * @returns {NativeTab}
     * @throws {ExtensionError}
     *       If no tab exists with the given ID and a default return value is not
     *       provided.
     * @abstract
     */
    getTab(tabId: integer, default_?: any): NativeTab;
    /**
     * Returns basic information about the tab and window that the given browser
     * belongs to.
     *
     * @param {XULElement} browser
     *        The XUL browser element for which to return data.
     *
     * @returns {BrowserData}
     * @abstract
     */
    getBrowserData(browser: XULElement): BrowserData;
    /**
     * @property {NativeTab} activeTab
     *        Returns the native tab object for the active tab in the
     *        most-recently focused window, or null if no live tabs currently
     *        exist.
     *        @abstract
     */
    get activeTab(): void;
}
/**
 * A browser progress listener instance which calls a given listener function
 * whenever the status of the given browser changes.
 *
 * @param {function(object): void} listener
 *        A function to be called whenever the status of a tab's top-level
 *        browser. It is passed an object with a `browser` property pointing to
 *        the XUL browser, and a `status` property with a string description of
 *        the browser's status.
 * @private
 */
declare class StatusListener {
    constructor(listener: any);
    listener: any;
    onStateChange(browser: any, webProgress: any, request: any, stateFlags: any, statusCode: any): void;
    onLocationChange(browser: any, webProgress: any, request: any, locationURI: any, flags: any): void;
}
/**
 * A platform-independent base class for the platform-specific WindowTracker
 * classes, which track the opening and closing of windows, and manage the
 * mapping of them between numeric IDs and native tab objects.
 */
declare class WindowTrackerBase {
    /**
     * A private method which is called whenever a new browser window is opened,
     * and adds the necessary listeners to it.
     *
     * @param {DOMWindow} window
     *        The window being opened.
     * @private
     */
    private _handleWindowOpened;
    _openListeners: Set<any>;
    _closeListeners: Set<any>;
    _listeners: any;
    _statusListeners: any;
    _windowIds: any;
    isBrowserWindow(window: any): boolean;
    /**
     * Returns an iterator for all currently active browser windows.
     *
     * @param {boolean} [includeInomplete = false]
     *        If true, include browser windows which are not yet fully loaded.
     *        Otherwise, only include windows which are.
     *
     * @returns {Iterator<DOMWindow>}
     */
    browserWindows(includeIncomplete?: boolean): Iterator<DOMWindow>;
    /**
     * @property {DOMWindow|null} topWindow
     *        The currently active, or topmost, browser window, or null if no
     *        browser window is currently open.
     *        @readonly
     */
    readonly get topWindow(): any;
    /**
     * @property {DOMWindow|null} topWindow
     *        The currently active, or topmost, browser window that is not
     *        private browsing, or null if no browser window is currently open.
     *        @readonly
     */
    readonly get topNonPBWindow(): any;
    /**
     * Returns the top window accessible by the extension.
     *
     * @param {BaseContext} context
     *        The extension context for which to return the current window.
     *
     * @returns {DOMWindow|null}
     */
    getTopWindow(context: BaseContext): DOMWindow | null;
    /**
     * Returns the numeric ID for the given browser window.
     *
     * @param {DOMWindow} window
     *        The DOM window for which to return an ID.
     *
     * @returns {integer}
     *        The window's numeric ID.
     */
    getId(window: DOMWindow): integer;
    /**
     * Returns the browser window to which the given context belongs, or the top
     * browser window if the context does not belong to a browser window.
     *
     * @param {BaseContext} context
     *        The extension context for which to return the current window.
     *
     * @returns {DOMWindow|null}
     */
    getCurrentWindow(context: BaseContext): DOMWindow | null;
    /**
     * Returns the browser window with the given ID.
     *
     * @param {integer} id
     *        The ID of the window to return.
     * @param {BaseContext} context
     *        The extension context for which the matching is being performed.
     *        Used to determine the current window for relevant properties.
     * @param {boolean} [strict = true]
     *        If false, undefined will be returned instead of throwing an error
     *        in case no window exists with the given ID.
     *
     * @returns {DOMWindow|undefined}
     * @throws {ExtensionError}
     *        If no window exists with the given ID and `strict` is true.
     */
    getWindow(id: integer, context: BaseContext, strict?: boolean): DOMWindow | undefined;
    /**
     * @property {boolean} _haveListeners
     *        Returns true if any window open or close listeners are currently
     *        registered.
     * @private
     */
    private get _haveListeners();
    /**
     * Register the given listener function to be called whenever a new browser
     * window is opened.
     *
     * @param {function(DOMWindow): void} listener
     *        The listener function to register.
     */
    addOpenListener(listener: (arg0: DOMWindow) => void): void;
    /**
     * Unregister a listener function registered in a previous addOpenListener
     * call.
     *
     * @param {function(DOMWindow): void} listener
     *        The listener function to unregister.
     */
    removeOpenListener(listener: (arg0: DOMWindow) => void): void;
    /**
     * Register the given listener function to be called whenever a browser
     * window is closed.
     *
     * @param {function(DOMWindow): void} listener
     *        The listener function to register.
     */
    addCloseListener(listener: (arg0: DOMWindow) => void): void;
    /**
     * Unregister a listener function registered in a previous addCloseListener
     * call.
     *
     * @param {function(DOMWindow): void} listener
     *        The listener function to unregister.
     */
    removeCloseListener(listener: (arg0: DOMWindow) => void): void;
    /**
     * Handles load events for recently-opened windows, and adds additional
     * listeners which may only be safely added when the window is fully loaded.
     *
     * @param {Event} event
     *        A DOM event to handle.
     * @private
     */
    private handleEvent;
    /**
     * Observes "domwindowopened" and "domwindowclosed" events, notifies the
     * appropriate listeners, and adds necessary additional listeners to the new
     * windows.
     *
     * @param {DOMWindow} window
     *        A DOM window.
     * @param {string} topic
     *        The topic being observed.
     * @private
     */
    private observe;
    /**
     * Add an event listener to be called whenever the given DOM event is received
     * at the top level of any browser window.
     *
     * @param {string} type
     *        The type of event to listen for. May be any valid DOM event name, or
     *        one of the following special cases:
     *
     *        - "progress": Adds a tab progress listener to every browser window.
     *        - "status": Adds a StatusListener to every tab of every browser
     *           window.
     *        - "domwindowopened": Acts as an alias for addOpenListener.
     *        - "domwindowclosed": Acts as an alias for addCloseListener.
     * @param {Function | object} listener
     *        The listener to invoke in response to the given events.
     *
     * @returns {undefined}
     */
    addListener(type: string, listener: Function | object): undefined;
    /**
     * Removes an event listener previously registered via an addListener call.
     *
     * @param {string} type
     *        The type of event to stop listening for.
     * @param {Function | object} listener
     *        The listener to remove.
     *
     * @returns {undefined}
     */
    removeListener(type: string, listener: Function | object): undefined;
    /**
     * Adds a listener for the given event to the given window.
     *
     * @param {DOMWindow} window
     *        The browser window to which to add the listener.
     * @param {string} eventType
     *        The type of DOM event to listen for, or "progress" to add a tab
     *        progress listener.
     * @param {Function | object} listener
     *        The listener to add.
     * @private
     */
    private _addWindowListener;
    /**
     * Adds a tab progress listener to the given browser window.
     *
     * @param {DOMWindow} window
     *        The browser window to which to add the listener.
     * @param {object} listener
     *        The tab progress listener to add.
     * @abstract
     */
    addProgressListener(window: DOMWindow, listener: object): void;
    /**
     * Removes a tab progress listener from the given browser window.
     *
     * @param {DOMWindow} window
     *        The browser window from which to remove the listener.
     * @param {object} listener
     *        The tab progress listener to remove.
     * @abstract
     */
    removeProgressListener(window: DOMWindow, listener: object): void;
}
/**
 * Manages native tabs, their wrappers, and their dynamic permissions for a
 * particular extension.
 *
 * @param {Extension} extension
 *        The extension for which to manage tabs.
 */
declare class TabManagerBase {
    constructor(extension: any);
    extension: any;
    _tabs: any;
    /**
     * If the extension has requested activeTab permission, grant it those
     * permissions for the current inner window in the given native tab.
     *
     * @param {NativeTab} nativeTab
     *        The native tab for which to grant permissions.
     */
    addActiveTabPermission(nativeTab: NativeTab): void;
    /**
     * Revoke the extension's activeTab permissions for the current inner window
     * of the given native tab.
     *
     * @param {NativeTab} nativeTab
     *        The native tab for which to revoke permissions.
     */
    revokeActiveTabPermission(nativeTab: NativeTab): void;
    /**
     * Returns true if the extension has requested activeTab permission, and has
     * been granted permissions for the current inner window if this tab.
     *
     * @param {NativeTab} nativeTab
     *        The native tab for which to check permissions.
     * @returns {boolean}
     *        True if the extension has activeTab permissions for this tab.
     */
    hasActiveTabPermission(nativeTab: NativeTab): boolean;
    /**
     * Activate MV3 content scripts if the extension has activeTab or an
     * (ungranted) host permission.
     *
     * @param {NativeTab} nativeTab
     */
    activateScripts(nativeTab: NativeTab): void;
    /**
     * Returns true if the extension has permissions to access restricted
     * properties of the given native tab. In practice, this means that it has
     * either requested the "tabs" permission or has activeTab permissions for the
     * given tab.
     *
     * NOTE: Never use this method on an object that is not a native tab
     * for the current platform: this method implicitly generates a wrapper
     * for the passed nativeTab parameter and the platform-specific tabTracker
     * instance is likely to store it in a map which is cleared only when the
     * tab is closed (and so, if nativeTab is not a real native tab, it will
     * never be cleared from the platform-specific tabTracker instance),
     * See Bug 1458918 for a rationale.
     *
     * @param {NativeTab} nativeTab
     *        The native tab for which to check permissions.
     * @returns {boolean}
     *        True if the extension has permissions for this tab.
     */
    hasTabPermission(nativeTab: NativeTab): boolean;
    /**
     * Returns this extension's TabBase wrapper for the given native tab. This
     * method will always return the same wrapper object for any given native tab.
     *
     * @param {NativeTab} nativeTab
     *        The tab for which to return a wrapper.
     *
     * @returns {TabBase|undefined}
     *        The wrapper for this tab.
     */
    getWrapper(nativeTab: NativeTab): TabBase | undefined;
    /**
     * Determines access using extension context.
     *
     * @param {NativeTab} nativeTab
     *        The tab to check access on.
     * @returns {boolean}
     *        True if the extension has permissions for this tab.
     * @protected
     * @abstract
     */
    protected canAccessTab(nativeTab: NativeTab): boolean;
    /**
     * Converts the given native tab to a JSON-compatible object, in the format
     * required to be returned by WebExtension APIs, which may be safely passed to
     * extension code.
     *
     * @param {NativeTab} nativeTab
     *        The native tab to convert.
     * @param {object} [fallbackTabSize]
     *        A geometry data if the lazy geometry data for this tab hasn't been
     *        initialized yet.
     *
     * @returns {object}
     */
    convert(nativeTab: NativeTab, fallbackTabSize?: object): object;
    /**
     * Returns an iterator of TabBase objects which match the given query info.
     *
     * @param {object | null} [queryInfo = null]
     *        An object containing properties on which to filter. May contain any
     *        properties which are recognized by {@link TabBase#matches} or
     *        {@link WindowBase#matches}. Unknown properties will be ignored.
     * @param {BaseContext|null} [context = null]
     *        The extension context for which the matching is being performed.
     *        Used to determine the current window for relevant properties.
     *
     * @returns {Iterator<TabBase>}
     */
    query(queryInfo?: object | null, context?: BaseContext | null): Iterator<TabBase>;
    /**
     * Returns a TabBase wrapper for the tab with the given ID.
     *
     * @param {integer} tabId
     *        The ID of the tab for which to return a wrapper.
     *
     * @returns {TabBase}
     * @throws {ExtensionError}
     *        If no tab exists with the given ID.
     * @abstract
     */
    get(tabId: integer): TabBase;
    /**
     * Returns a new TabBase instance wrapping the given native tab.
     *
     * @param {NativeTab} nativeTab
     *        The native tab for which to return a wrapper.
     *
     * @returns {TabBase}
     * @protected
     * @abstract
     */
    protected wrapTab(nativeTab: NativeTab): TabBase;
}
/**
 * Manages native browser windows and their wrappers for a particular extension.
 *
 * @param {Extension} extension
 *        The extension for which to manage windows.
 */
declare class WindowManagerBase {
    constructor(extension: any);
    extension: any;
    _windows: any;
    /**
     * Converts the given browser window to a JSON-compatible object, in the
     * format required to be returned by WebExtension APIs, which may be safely
     * passed to extension code.
     *
     * @param {DOMWindow} window
     *        The browser window to convert.
     * @param {*} args
     *        Additional arguments to be passed to {@link WindowBase#convert}.
     *
     * @returns {object}
     */
    convert(window: DOMWindow, ...args: any): object;
    /**
     * Returns this extension's WindowBase wrapper for the given browser window.
     * This method will always return the same wrapper object for any given
     * browser window.
     *
     * @param {DOMWindow} window
     *        The browser window for which to return a wrapper.
     *
     * @returns {WindowBase|undefined}
     *        The wrapper for this tab.
     */
    getWrapper(window: DOMWindow): WindowBase | undefined;
    /**
     * Returns whether this window can be accessed by the extension in the given
     * context.
     *
     * @param {DOMWindow} window
     *        The browser window that is being tested
     * @param {BaseContext|null} context
     *        The extension context for which this test is being performed.
     * @returns {boolean}
     */
    canAccessWindow(window: DOMWindow, context: BaseContext | null): boolean;
    /**
     * Returns an iterator of WindowBase objects which match the given query info.
     *
     * @param {object | null} [queryInfo = null]
     *        An object containing properties on which to filter. May contain any
     *        properties which are recognized by {@link WindowBase#matches}.
     *        Unknown properties will be ignored.
     * @param {BaseContext|null} [context = null]
     *        The extension context for which the matching is being performed.
     *        Used to determine the current window for relevant properties.
     *
     * @returns {Iterator<WindowBase>}
     */
    query(queryInfo?: object | null, context?: BaseContext | null): Iterator<WindowBase>;
    /**
     * Returns a WindowBase wrapper for the browser window with the given ID.
     *
     * @param {integer} windowId
     *        The ID of the browser window for which to return a wrapper.
     * @param {BaseContext} context
     *        The extension context for which the matching is being performed.
     *        Used to determine the current window for relevant properties.
     *
     * @returns {WindowBase}
     * @throws {ExtensionError}
     *        If no window exists with the given ID.
     * @abstract
     */
    get(windowId: integer, context: BaseContext): WindowBase;
    /**
     * Returns an iterator of WindowBase wrappers for each currently existing
     * browser window.
     *
     * @returns {Iterator<WindowBase>}
     * @abstract
     */
    getAll(): Iterator<WindowBase>;
    /**
     * Returns a new WindowBase instance wrapping the given browser window.
     *
     * @param {DOMWindow} window
     *        The browser window for which to return a wrapper.
     *
     * @returns {WindowBase}
     * @protected
     * @abstract
     */
    protected wrapWindow(window: DOMWindow): WindowBase;
}
/**
 * The platform-specific type of native tab objects, which are wrapped by
 * TabBase instances.
 */
type NativeTab = object | XULElement;
type MutedInfo = {
    /**
     *        True if the tab is currently muted, false otherwise.
     */
    muted: boolean;
    /**
     * The reason the tab is muted. Either "user", if the tab was muted by a
     * user, or "extension", if it was muted by an extension.
     */
    reason?: string;
    /**
     * If the tab was muted by an extension, contains the internal ID of that
     * extension.
     */
    extensionId?: string;
};
/**
 * The parameter type of "tab-attached" events, which are emitted when a
 * pre-existing tab is attached to a new window.
 */
type TabAttachedEvent = {
    /**
     *        The native tab object in the window to which the tab is being
     *        attached. This may be a different object than was used to represent
     *        the tab in the old window.
     */
    tab: NativeTab;
    /**
     *        The ID of the tab being attached.
     */
    tabId: integer;
    /**
     *        The ID of the window to which the tab is being attached.
     */
    newWindowId: integer;
    /**
     *        The position of the tab in the tab list of the new window.
     */
    newPosition: integer;
};
/**
 * The parameter type of "tab-detached" events, which are emitted when a
 * pre-existing tab is detached from a window, in order to be attached to a new
 * window.
 */
type TabDetachedEvent = {
    /**
     *        The native tab object in the window from which the tab is being
     *        detached. This may be a different object than will be used to
     *        represent the tab in the new window.
     */
    tab: NativeTab;
    /**
     *        The native tab object in the window to which the tab will be attached,
     *        and is adopting the contents of this tab. This may be a different
     *        object than the tab in the previous window.
     */
    adoptedBy: NativeTab;
    /**
     *        The ID of the tab being detached.
     */
    tabId: integer;
    /**
     *        The ID of the window from which the tab is being detached.
     */
    oldWindowId: integer;
    /**
     *        The position of the tab in the tab list of the window from which it is
     *        being detached.
     */
    oldPosition: integer;
};
/**
 * The parameter type of "tab-created" events, which are emitted when a
 * new tab is created.
 */
type TabCreatedEvent = {
    /**
     *        The native tab object for the tab which is being created.
     */
    tab: NativeTab;
};
/**
 * The parameter type of "tab-removed" events, which are emitted when a
 * tab is removed and destroyed.
 */
type TabRemovedEvent = {
    /**
     *        The native tab object for the tab which is being removed.
     */
    tab: NativeTab;
    /**
     *        The ID of the tab being removed.
     */
    tabId: integer;
    /**
     *        The ID of the window from which the tab is being removed.
     */
    windowId: integer;
    /**
     *        True if the tab is being removed because the window is closing.
     */
    isWindowClosing: boolean;
};
/**
 * An object containing basic, extension-independent information about the window
 * and tab that a XUL <browser> belongs to.
 */
type BrowserData = {
    /**
     *        The numeric ID of the tab that a <browser> belongs to, or -1 if it
     *        does not belong to a tab.
     */
    tabId: integer;
    /**
     *        The numeric ID of the browser window that a <browser> belongs to, or -1
     *        if it does not belong to a browser window.
     */
    windowId: integer;
};

}  // namespace tabs_base

declare global {
    type TabTrackerBase = tabs_base.TabTrackerBase;
    type TabManagerBase = tabs_base.TabManagerBase;
    type TabBase = tabs_base.TabBase;
    type WindowTrackerBase = tabs_base.WindowTrackerBase;
    type WindowManagerBase = tabs_base.WindowManagerBase;
    type WindowBase = tabs_base.WindowBase;
    type getUserContextIdForCookieStoreId = tabs_base.getUserContextIdForCookieStoreId;
    type NativeTab = tabs_base.NativeTab;
}

export {};
