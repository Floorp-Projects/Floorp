/**
 * Copyright 2018 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

module.exports.addTests = function({testRunner, expect}) {
  const {describe, xdescribe, fdescribe} = testRunner;
  const {it, fit, xit, it_fails_ffox} = testRunner;
  const {beforeAll, beforeEach, afterAll, afterEach} = testRunner;

  describe('Page.Events.Dialog', function() {
    it('should fire', async({page, server}) => {
      let type, defaultValue, message;
      page.on('dialog', dialog => {
        type = dialog.type();
        defaultValue = dialog.defaultValue();
        message = dialog.message();
        dialog.accept();
      });
      await page.evaluate(() => alert('yo'));
      expect(type).toBe('alert');
      expect(defaultValue).toBe('');
      expect(message).toBe('yo');
    });
    it('should allow accepting prompts', async({page, server}) => {
      let type, defaultValue, message;
      page.on('dialog', dialog => {
        type = dialog.type();
        defaultValue = dialog.defaultValue();
        message = dialog.message();
        dialog.accept('answer!');
      });
      const result = await page.evaluate(() => prompt('question?', 'yes.'));
      expect(type).toBe('prompt');
      expect(defaultValue).toBe('yes.');
      expect(message).toBe('question?');
      expect(result).toBe('answer!');
    });
    it('should dismiss the prompt', async({page, server}) => {
      page.on('dialog', dialog => {
        dialog.dismiss();
      });
      const result = await page.evaluate(() => prompt('question?'));
      expect(result).toBe(null);
    });
  });
};
