function testSparseReturningTrailKeepsInitialDirection(): void {
  const { recognizer, shapeDb } = createRecognizer([
    { pattern: ["up", "down"], action: "up-down-action" },
    { pattern: ["down", "up"], action: "down-up-action" },
  ]);
  const result = recognize(
    recognizer,
    [{ x: 0, y: 0 }, { x: 0, y: -160 }, { x: 0, y: 16 }],
    0.73,
    shapeDb,
    20,
  );
  assertEquals(
    result?.patternName,
    "up-down",
    "returning beyond the start must not reverse the configured action",
  );
}
