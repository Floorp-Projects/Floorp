import { Browser } from '../api/Browser.js';
import { BrowserLaunchArgumentOptions, ChromeReleaseChannel, PuppeteerNodeLaunchOptions } from './LaunchOptions.js';
import { ProductLauncher } from './ProductLauncher.js';
import { PuppeteerNode } from './PuppeteerNode.js';
/**
 * @internal
 */
export declare class ChromeLauncher extends ProductLauncher {
    #private;
    constructor(puppeteer: PuppeteerNode);
    launch(options?: PuppeteerNodeLaunchOptions): Promise<Browser>;
    defaultArgs(options?: BrowserLaunchArgumentOptions): string[];
    executablePath(channel?: ChromeReleaseChannel): string;
}
//# sourceMappingURL=ChromeLauncher.d.ts.map