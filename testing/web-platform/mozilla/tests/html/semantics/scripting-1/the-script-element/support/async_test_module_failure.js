export default 42;

export const named = 'named';

var rejection = Promise.reject(TypeError('I reject this!'));
await rejection;
