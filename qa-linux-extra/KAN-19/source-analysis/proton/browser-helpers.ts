import UAParser from 'ua-parser-js';

import noop from '@proton/utils/noop';

import { Version } from './version';

const uaParser = new UAParser();
const ua = uaParser.getResult();

export const hasModulesSupport = () => {
    const script = document.createElement('script');
    return 'noModule' in script;
};

export const isFileSaverSupported = () => !!new Blob();

export const isWebglSupported = () => {
    try {
        const canvas = document.createElement('canvas');
        return Boolean(
            window.WebGLRenderingContext && (canvas.getContext('webgl') || canvas.getContext('experimental-webgl'))
        );
    } catch (e) {
        return false;
    }
};

export const textToClipboard = (text = '', target = document.body) => {
    const oldActiveElement = document.activeElement as HTMLElement;
    if (navigator.clipboard) {
        navigator.clipboard.writeText(text).catch(noop);
    } else {
        const dummy = document.createElement('textarea');
        target.appendChild(dummy);
        dummy.value = text;
        dummy.select();
        document.execCommand('copy');
        target.removeChild(dummy);
    }
    oldActiveElement?.focus?.();
};

export const copyDomToClipboard = async (element: HTMLElement) => {
    if (!element || !(element instanceof HTMLElement)) {
        console.error('Invalid element provided');
        return;
    }

    /** Try to use the Clipboard API if available */
    /*
     * Commenting the clipboard API solution for now because of 2 "issues"
     * 1- The current solution is copying HTML only. However, we would need to copy plaintext too for editors that are not supporting HTML
     * 2- When using the clipboard API, the content is sanitized, meaning that some parts of the content are dropped, such as classes
     */
    // if (navigator.clipboard && typeof navigator.clipboard.write === 'function') {
    //     const type = 'text/html';
    //     const blob = new Blob([element.innerHTML], { type });
    //     const data = [new ClipboardItem({ [type]: blob })];
    //     await navigator.clipboard.write(data);
    // } else {
    const activeElement = document.activeElement;

    // Create an off-screen container for the element's HTML content
    const tempContainer = document.createElement('div');
    tempContainer.style.position = 'absolute';
    tempContainer.style.left = '-9999px';
    tempContainer.innerHTML = element.innerHTML;

    document.body.appendChild(tempContainer);

    const selection = window.getSelection();
    if (!selection) {
        console.error('Failed to get selection');
        document.body.removeChild(tempContainer);
        return;
    }

    // Select the contents of the temporary container
    const range = document.createRange();
    range.selectNodeContents(tempContainer);

    selection.removeAllRanges();
    selection.addRange(range);

    // Copy the selected content to the clipboard
    try {
        document.execCommand('copy');
    } catch (err) {
        console.error('Failed to copy content', err);
    }

    // Clean up
    document.body.removeChild(tempContainer);
    selection.removeAllRanges();

    // Restore previous focus
    if (activeElement instanceof HTMLElement) {
        activeElement.focus();
    }
    // }
};

export const getOS = () => {
    const { name = 'other', version = '' } = ua.os;
    return { name, version };
};

export const isAndroid = () => {
    const { name } = getOS();
    return name.toLowerCase().includes('android');
};

export const isSafari = () => ua.browser.name === 'Safari' || ua.browser.name === 'Mobile Safari';
/**
 * True for any browser running on the WebKit engine, not only Safari: every iOS browser
 * (Chrome, Firefox, Edge...), in-app webviews (Gmail, Outlook...), DuckDuckGo, Orion, GNOME Web...
 */
export const isWebKit = () => ua.engine.name === 'WebKit';

export const isMinimumSafariVersion = (version: number) =>
    isSafari() && ua.browser.version && new Version(ua.browser.version).isGreaterThanOrEqual(version.toString());

export const isEdgeChromium = () => ua.browser.name === 'Edge' && ua.engine.name === 'Blink';
export const isBrave = () => ua.browser.name === 'Brave';
export const isFirefox = () => ua.browser.name === 'Firefox';
export const isMaybeTorLessThan11 = () => {
    const isMaybeTor =
        isFirefox() &&
        /\.0$/.test(ua.browser.version || '') && // The Firefox minor revision is omitted.
        Intl.DateTimeFormat().resolvedOptions().timeZone === 'UTC' && // The timezone is set to UTC
        !Object.prototype.hasOwnProperty.call(window, 'Components') && // It strips out Components
        navigator.plugins.length === 0; // 0 plugins are returned
    // Starting from tor browser 11, tor is based on firefox 91
    return isMaybeTor && !!ua.browser.major && +ua.browser.major < 91;
};
export const isChrome = () => ua.browser.name === 'Chrome';
export const isChromiumBased = () => 'chrome' in window;
export const isMac = () => ua.os.name === 'Mac OS';
export const isWindows = () => ua.os.name === 'Windows';
export const isArm = () => ua.cpu.architecture === 'arm64';
export const isLinux = () => ua.ua.match(/(L|l)inux/);
export const isDebianBased = () => !!ua.os.name && ['Ubuntu', 'Debian'].includes(ua.os.name);
export const isFedoraOrRedHatBased = () => !!ua.os.name && ['Fedora', 'Red Hat'].includes(ua.os.name);
export const getOs = () => ua.os;
export const getBrowser = () => ua.browser;
export const getDevice = () => ua.device;
export const isMobile = () => {
    const { type } = getDevice();
    return type === 'mobile';
};
export const isDesktop = () => {
    const { type } = getDevice();
    return !type;
};
export const getIsIframe = () => window.self !== window.top;

export const metaKey = isMac() ? '⌘' : 'Ctrl';
export const altKey = isMac() ? 'Option' : 'Alt';
export const shiftKey = 'Shift';

const getActiveXObject = (name: string) => {
    try {
        // @ts-ignore
        return new ActiveXObject(name);
    } catch (error: any) {
        return undefined;
    }
};

// See https://bugzilla.mozilla.org/show_bug.cgi?id=1918354
export const isFirefoxWithBrokenX25519Support = () =>
    isFirefox() && !!ua.browser.major && (+ua.browser.major === 130 || +ua.browser.major === 131);

export const isIos = () =>
    // https://racase.com.np/javascript-how-to-detect-if-device-is-ios/
    (/iPad|iPhone|iPod/.test(navigator.userAgent) && !(window as any).MSStream) ||
    ['iPad Simulator', 'iPhone Simulator', 'iPod Simulator', 'iPad', 'iPhone', 'iPod'].includes(navigator.platform) ||
    // iPad on iOS 13 detection
    (navigator.userAgent.includes('Mac') && 'ontouchend' in document);
export const isIpad = () => isSafari() && navigator.maxTouchPoints && navigator.maxTouchPoints > 2;
const hasAcrobatInstalled = () => !!(getActiveXObject('AcroPDF.PDF') || getActiveXObject('PDF.PdfCtrl'));
export const hasPDFSupport = () => {
    // mimeTypes is deprecated in favor of pdfViewerEnabled.
    return (
        (navigator.mimeTypes && 'application/pdf' in navigator.mimeTypes) ||
        // eslint-disable-next-line compat/compat
        navigator.pdfViewerEnabled ||
        (isFirefox() && isDesktop()) ||
        isIos() ||
        hasAcrobatInstalled()
    );
};
export const replaceUrl = (url = '') => document.location.replace(url);

/**
 * Open an URL inside a new tab/window and remove the referrer
 * @links { https://mathiasbynens.github.io/rel-noopener/}
 */
export const openNewTab = (url: string) => {
    const anchor = document.createElement('a');

    anchor.setAttribute('rel', 'noreferrer nofollow noopener');
    anchor.setAttribute('target', '_blank');
    anchor.href = url;

    return anchor.click();
};

// On safari < 14 the Version cookie is sent for index.html file but
// not sent for asset requests due to https://bugs.webkit.org/show_bug.cgi?id=171566
export const doesNotSupportEarlyAccessVersion = () => isSafari() && Number(ua.browser.major) < 14;

export async function detectStorageCapabilities(): Promise<{ isAccessible: boolean; hasIndexedDB: boolean }> {
    // Check for IndexedDB API. Every supported browser (see .browserslistrc) provides both `indexedDB` and `indexedDB.databases()`.
    // If either is missing, treat storage as inaccessible.
    const hasIndexedDB = 'indexedDB' in globalThis && globalThis.indexedDB != null;
    if (!hasIndexedDB || typeof indexedDB.databases !== 'function') {
        return { hasIndexedDB, isAccessible: false };
    }

    // Probe availability with databases() rather than opening a throwaway database: it exercises the
    // IndexedDB backend without creating anything, and rejects when storage is blocked.
    const isAccessible = await Promise.race([
        indexedDB.databases().then(
            () => true,
            () => false
        ),
        // Older Firefox versions (at least 140) are a bit
        // fussy with this call in case of blocked connections,
        // so this is to ensure we never get an unresolved Promise
        new Promise<boolean>((resolve) => setTimeout(() => resolve(false), 1_500)),
    ]);

    return { hasIndexedDB, isAccessible };
}

export const browserAPI = (globalThis as any)?.browser ?? (globalThis as any)?.chrome;
