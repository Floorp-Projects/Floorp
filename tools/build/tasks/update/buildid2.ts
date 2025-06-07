export async function writeBuildid2(dir: string, buildid2: string) {
  const path_buildid2 = `${dir}/buildid2`;
  try {
    // Ensure directory exists
    await Deno.mkdir(dir, { recursive: true });

    // Force remove existing file if it exists to ensure clean overwrite
    try {
      await Deno.remove(path_buildid2);
      console.log(
        `Removed existing buildid2 file for overwrite: ${path_buildid2}`,
      );
    } catch (error: unknown) {
      // File doesn't exist, which is fine
      if (!(error instanceof Deno.errors.NotFound)) {
        const errorMessage = error instanceof Error
          ? error.message
          : String(error);
        console.warn(
          `Warning: Could not remove existing buildid2 file: ${errorMessage}`,
        );
      }
    }

    // Write the build ID
    await Deno.writeTextFile(path_buildid2, buildid2);
    console.log(`Successfully wrote build ID to: ${path_buildid2}`);
  } catch (error: unknown) {
    const errorMessage = error instanceof Error ? error.message : String(error);
    console.error(
      `Failed to write build ID to ${path_buildid2}:`,
      errorMessage,
    );
    throw error;
  }
}

export async function readBuildid2(dir: string): Promise<string | null> {
  const path_buildid2 = `${dir}/buildid2`;
  try {
    const buildid2 = await Deno.readTextFile(path_buildid2);
    return buildid2.trim();
  } catch (error: unknown) {
    if (error instanceof Deno.errors.NotFound) {
      return null;
    }
    const errorMessage = error instanceof Error ? error.message : String(error);
    console.warn(
      `Warning: Could not read buildid2 from ${path_buildid2}: ${errorMessage}`,
    );
    return null;
  }
}
