// @ts-nocheck

export namespace XPCShellContentUtils {
    const currentScope: any;
    const fetchScopes: Map<any, any>;
    function initCommon(scope: any): void;
    function init(scope: any): void;
    function initMochitest(scope: any): void;
    function ensureInitialized(scope: any): void;
    /**
     * Creates a new HttpServer for testing, and begins listening on the
     * specified port. Automatically shuts down the server when the test
     * unit ends.
     *
     * @param {object} [options = {}]
     *        The options object.
     * @param {integer} [options.port = -1]
     *        The port to listen on. If omitted, listen on a random
     *        port. The latter is the preferred behavior.
     * @param {sequence<string>?} [options.hosts = null]
     *        A set of hosts to accept connections to. Support for this is
     *        implemented using a proxy filter.
     *
     * @returns {HttpServer}
     *        The HTTP server instance.
     */
    function createHttpServer({ port, hosts }?: {
        port?: number;
        hosts?: sequence<string>;
    }): HttpServer;

    var remoteContentScripts: boolean;
    type ContentPage = ContentPage;

    function registerJSON(server: any, path: any, obj: any): void;
    function fetch(origin: any, url: any, options: any): Promise<any>;
    /**
     * Loads a content page into a hidden docShell.
     *
     * @param {string} url
     *        The URL to load.
     * @param {object} [options = {}]
     * @param {ExtensionWrapper} [options.extension]
     *        If passed, load the URL as an extension page for the given
     *        extension.
     * @param {boolean} [options.remote]
     *        If true, load the URL in a content process. If false, load
     *        it in the parent process.
     * @param {boolean} [options.remoteSubframes]
     *        If true, load cross-origin frames in separate content processes.
     *        This is ignored if |options.remote| is false.
     * @param {string} [options.redirectUrl]
     *        An optional URL that the initial page is expected to
     *        redirect to.
     *
     * @returns {ContentPage}
     */
    function loadContentPage(url: string, { extension, remote, remoteSubframes, redirectUrl, privateBrowsing, userContextId, }?: {
        extension?: any;
        remote?: boolean;
        remoteSubframes?: boolean;
        redirectUrl?: string;
    }): ContentPage;
}
declare class ContentPage {
    constructor(remote?: any, remoteSubframes?: any, extension?: any, privateBrowsing?: boolean, userContextId?: any);
    remote: any;
    remoteSubframes: any;
    extension: any;
    privateBrowsing: boolean;
    userContextId: any;
    browserReady: Promise<Element>;
    _initBrowser(): Promise<Element>;
    windowlessBrowser: any;
    browser: Element;
    get browsingContext(): any;
    get SpecialPowers(): any;
    loadFrameScript(func: any): void;
    addFrameScriptHelper(func: any): void;
    didChangeBrowserRemoteness(event: any): void;
    loadURL(url: any, redirectUrl?: any): Promise<any>;
    fetch(...args: any[]): Promise<any>;
    spawn(params: any, task: any): any;
    legacySpawn(params: any, task: any): any;
    close(): Promise<void>;
}
export {};
