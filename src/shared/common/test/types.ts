export interface ServerFunctions {
  hi(name: string): string;
  /**
   * @param name image name to save
   * @param image base64-encoded png data
   */
  saveImage(name: string, image: string): void;
}

export interface ClientFunctions {
  hey(name: string): string;
}
