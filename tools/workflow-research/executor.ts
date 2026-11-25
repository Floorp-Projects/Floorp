// SPDX-License-Identifier: MPL-2.0

import type {
  ExecutionResult,
  MissingSource,
  OracleSpec,
  Workflow,
} from "./types.ts";

export interface FixtureElement {
  fingerprint: string;
  text: string;
  input?: boolean;
  revealsText?: string;
}

export interface FixturePage {
  url: string;
  title: string;
  baseText: string;
  elements: FixtureElement[];
}

export interface MiniBrowserOptions {
  pages: FixturePage[];
  initialUrl: string;
  workspaceIds: string[];
  initialWorkspaceId: string | null;
}

interface MiniBrowserState {
  url: string;
  activeWorkspaceId: string | null;
  revealedText: Map<string, string[]>;
  inputValues: Map<string, string>;
  extracted: Record<string, string>;
}

function failure(
  message: string,
  missingSource: MissingSource,
  state: MiniBrowserState,
): ExecutionResult {
  return {
    success: false,
    failureKind: "runtime",
    message,
    missingSource,
    extracted: { ...state.extracted },
    finalUrl: state.url,
    activeWorkspaceId: state.activeWorkspaceId,
  };
}

export class MiniBrowser {
  private readonly pages: Map<string, FixturePage>;
  private readonly workspaceIds: ReadonlySet<string>;
  private readonly state: MiniBrowserState;

  constructor(options: MiniBrowserOptions) {
    this.pages = new Map(options.pages.map((page) => [page.url, page]));
    this.workspaceIds = new Set(options.workspaceIds);
    this.state = {
      url: options.initialUrl,
      activeWorkspaceId: options.initialWorkspaceId,
      revealedText: new Map(),
      inputValues: new Map(),
      extracted: {},
    };
  }

  private page(): FixturePage | null {
    return this.pages.get(this.state.url) ?? null;
  }

  private visibleText(): string {
    const page = this.page();
    if (!page) return "";
    return [page.baseText, ...(this.state.revealedText.get(page.url) ?? [])]
      .join("\n");
  }

  private element(fingerprint: string): FixtureElement | null {
    const page = this.page();
    return page?.elements.find((entry) => entry.fingerprint === fingerprint) ??
      null;
  }

  navigate(url: string): ExecutionResult | null {
    if (!this.pages.has(url)) {
      return failure(
        `navigate target is unknown: ${url}`,
        "api-schema",
        this.state,
      );
    }
    this.state.url = url;
    return null;
  }

  click(fingerprint: string): ExecutionResult | null {
    const element = this.element(fingerprint);
    if (!element) {
      return failure(
        `element fingerprint not found: ${fingerprint}`,
        "dom-structure",
        this.state,
      );
    }
    if (element.revealsText) {
      const revealed = this.state.revealedText.get(this.state.url) ?? [];
      if (!revealed.includes(element.revealsText)) {
        revealed.push(element.revealsText);
      }
      this.state.revealedText.set(this.state.url, revealed);
    }
    return null;
  }

  fill(fingerprint: string, value: string): ExecutionResult | null {
    const element = this.element(fingerprint);
    if (!element || element.input !== true) {
      return failure(
        `input fingerprint not found: ${fingerprint}`,
        "dom-structure",
        this.state,
      );
    }
    this.state.inputValues.set(fingerprint, value);
    return null;
  }

  waitForText(text: string): ExecutionResult | null {
    if (!this.visibleText().includes(text)) {
      return failure(`text did not appear: ${text}`, "page-text", this.state);
    }
    return null;
  }

  extractText(fingerprint: string, as: string): ExecutionResult | null {
    const element = this.element(fingerprint);
    if (!element) {
      return failure(
        `extract fingerprint not found: ${fingerprint}`,
        "dom-structure",
        this.state,
      );
    }
    if (!this.visibleText().includes(element.text)) {
      return failure(
        `extract target is not visible: ${fingerprint}`,
        "page-text",
        this.state,
      );
    }
    this.state.extracted[as] = element.text;
    return null;
  }

  assertText(text: string): ExecutionResult | null {
    if (!this.visibleText().includes(text)) {
      return failure(
        `assertion text is absent: ${text}`,
        "page-text",
        this.state,
      );
    }
    return null;
  }

  switchWorkspace(workspaceId: string): ExecutionResult | null {
    if (!this.workspaceIds.has(workspaceId)) {
      return failure(
        `workspace not found: ${workspaceId}`,
        "browser-state",
        this.state,
      );
    }
    this.state.activeWorkspaceId = workspaceId;
    return null;
  }

  finish(oracle: OracleSpec): ExecutionResult {
    const base: Omit<
      ExecutionResult,
      "success" | "failureKind" | "message" | "missingSource"
    > = {
      extracted: { ...this.state.extracted },
      finalUrl: this.state.url,
      activeWorkspaceId: this.state.activeWorkspaceId,
    };
    let ok = false;
    let message = "oracle mismatch";
    let missingSource: MissingSource = "page-text";

    if (oracle.type === "extraction") {
      const value = this.state.extracted[oracle.as] ?? "";
      ok = value.includes(oracle.includes);
      message = ok
        ? "success"
        : `extracted ${oracle.as} did not include ${oracle.includes}`;
    } else if (oracle.type === "workspace") {
      ok = this.state.activeWorkspaceId === oracle.workspaceId;
      missingSource = "browser-state";
      message = ok
        ? "success"
        : `active workspace ${
          String(this.state.activeWorkspaceId)
        } != ${oracle.workspaceId}`;
    } else {
      ok = this.visibleText().includes(oracle.includes);
      message = ok ? "success" : `page text did not include ${oracle.includes}`;
    }

    return {
      success: ok,
      failureKind: ok ? "none" : "oracle",
      message,
      missingSource: ok ? "none" : missingSource,
      ...base,
    };
  }
}

export function executeWorkflow(
  workflow: Workflow,
  browser: MiniBrowser,
  oracle: OracleSpec,
): ExecutionResult {
  for (const step of workflow.steps) {
    let error: ExecutionResult | null = null;
    if (step.action === "tabs.create" || step.action === "tabs.navigate") {
      error = browser.navigate(step.url);
    } else if (step.action === "page.click") {
      error = browser.click(step.fingerprint);
    } else if (step.action === "page.fill") {
      error = browser.fill(step.fingerprint, step.value);
    } else if (step.action === "page.waitForText") {
      error = browser.waitForText(step.text);
    } else if (step.action === "page.extractText") {
      error = browser.extractText(step.fingerprint, step.as);
    } else if (step.action === "page.assertText") {
      error = browser.assertText(step.text);
    } else if (step.action === "workspace.switch") {
      error = browser.switchWorkspace(step.workspaceId);
    }
    if (error) return error;
  }
  return browser.finish(oracle);
}
