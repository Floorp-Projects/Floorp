import module from './async_test_module_circular_1.js';

export default {
  async test() {
    throw new Error("error thrown");
    return Promise.resolve()
  }
};
