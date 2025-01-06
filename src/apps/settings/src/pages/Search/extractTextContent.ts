import type { ReactNode } from "react";
import { renderToString } from "react-dom/server";

export async function extractTextContent(component: ReactNode): Promise<string> {
  try {
    const htmlString = renderToString(component as any);
    const tempDiv = document?.createElement('div') as HTMLDivElement;
    tempDiv.innerHTML = htmlString;
    return tempDiv.textContent || '';
  } catch (error) {
    console.error("Error extracting text content:", error);
    return '';
  }
}
