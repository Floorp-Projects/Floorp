import { Browser } from '../api/Browser.js';
import { Product } from '../common/Product.js';
import { BrowserLaunchArgumentOptions, ChromeReleaseChannel, PuppeteerNodeLaunchOptions } from './LaunchOptions.js';
import { PuppeteerNode } from './PuppeteerNode.js';
/**
 * Describes a launcher - a class that is able to create and launch a browser instance.
 *
 * @public
 */
export declare class ProductLauncher {
    #private;
    /**
     * @internal
     */
    puppeteer: PuppeteerNode;
    /**
     * @internal
     */
    protected actualBrowserRevision?: string;
    /**
     * @internal
     */
    constructor(puppeteer: PuppeteerNode, product: Product);
    get product(): Product;
    launch(object: PuppeteerNodeLaunchOptions): Promise<Browser>;
    executablePath(channel?: ChromeReleaseChannel): string;
    defaultArgs(object: BrowserLaunchArgumentOptions): string[];
    /**
     * Set only for Firefox, after the launcher resolves the `latest` revision to
     * the actual revision.
     * @internal
     */
    getActualBrowserRevision(): string | undefined;
    /**
     * @internal
     */
    protected getProfilePath(): string;
    /**
     * @internal
     */
    protected resolveExecutablePath(): string;
}
//# sourceMappingURL=ProductLauncher.d.ts.map