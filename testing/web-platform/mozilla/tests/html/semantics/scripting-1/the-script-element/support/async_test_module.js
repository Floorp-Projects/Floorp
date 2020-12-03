await 1;
await 2;
export default await Promise.resolve(42);

export const y = await 39;
export const x = await 'named';

// Bonus: this rejection is not unwrapped
if (false) {
  await Promise.reject(42);
}

