# JSON design tokens
## Background
The benefit of storing design tokens with a platform-agnostic format such as JSON is that it can be converted or translated into other languages or tools (e.g CSS, Swift, Kotlin, Figma).

## Quick start
`design-tokens.json` holds our source of truth for design tokens in `mozilla-central` under the [design-system](https://searchfox.org/mozilla-central/source/toolkit/themes/shared/design-system) folder in `toolkit/themes/shared`. The CSS design token files in that folder come from the JSON file. If you need to modify a design token file, you should be editing the JSON.

In order for us to be able to define design tokens in one place (the JSON file) and allow all platforms to consume design tokens in their specific format, we use a build system called [Style Dictionary](https://amzn.github.io/style-dictionary/#/).

Here's how to build design tokens for desktop:

```sh
$ ./mach buildtokens
```

If successful, you should see Style Dictionary building all of our tokens files within the `design-system` folder. Otherwise, Style Dictionary can also generate helpful errors to help you debug.

At the end, we're capable of transforming JSON notation into CSS:

```json
{
 "color": {
    "blue": {
      "05": {
        "value": "#deeafc"
      },
      "30": {
        "value": "#73a7f3"
      },
      "50": {
        "value": "#0060df"
      },
      "60": {
        "value": "#0250bb"
      },
      "70": {
        "value": "#054096"
      },
      "80": {
        "value": "#003070"
      }
    },
  },
}
```

```css
--color-blue-05: #deeafc;
--color-blue-30: #73a7f3;
--color-blue-50: #0060df;
--color-blue-60: #0250bb;
--color-blue-70: #054096;
--color-blue-80: #003070;
```

Neat!

## Testing

We have basic tests in place to ensure that our built CSS files stay up to date
and that new tokens are properly categorized. These tests will fail if the JSON
is modified but the command to build the tokens CSS files doesn't run, or if the
tokens CSS files are modified directly without changing the JSON.

Our tests are Node-based to allow us to install and work with the
`style-dictionary` library, and follow a format that has been used previously
for Devtools and New Tab tests.

You can run the tests locally using the following command:

```sh
$ ./mach npm test --prefix=toolkit/themes/shared/design-system
```

## JSON format deep dive

Style Dictionary works by ingesting JSON files with tokens data and performing various platform-specific transformations to output token files formatted for different languages. The library has certain quirks and limitations that we had to take into consideration when coming up with a JSON format for representing our tokens. The following is a bit of a "how to" guide for reading and adding to [`design-tokens.json`](https://searchfox.org/mozilla-central/source/toolkit/themes/shared/design-system/design-tokens.json) for anyone who needs to consume our tokens or add new tokens.

### Naming

When going from token name to JSON, each place we would insert a hyphen, underscore, or case change in a variable name translates to a layer of nesting in our JSON. That means a token with the CSS variable name `--border-radius-circle` would be represented as:

```json
"border": {
  "radius": {
    "circle": {
      "value": "9999px"
    }
  }
}
```

The `value` key is required as this is the indicator [Style Dictionary looks for](https://amzn.github.io/style-dictionary/#/architecture?id=_4a-transform-the-tokens) to know what sections of the JSON to parse into distinct tokens. It gets omitted from the final variable name.

For simple cases `value` will be either a string or a number, but when we have to take media queries or themes into account `value` will be an object. Examples of these cases will be explained in more detail below.

### `@base`

When reading through our JSON you will see many instances where we have used an `@base` key, always appearing right before a `value` key indicating that everything up to that point should be parsed as a token. For example our `--font-weight` CSS token is represented like this in our JSON:

```json
"font": {
  "weight": {
    "@base": {
      "value": "normal"
    }
  }
}
```
This is a workaround for a [known](https://github.com/amzn/style-dictionary/issues/119) [limitation](https://github.com/amzn/style-dictionary/issues/366) of Style Dictionary where it doesn't support nested token names that appear after a `value` key. If we want to have both `--font-weight` and `--font-weight-bold` CSS tokens they need to be represented as distinct objects with their own `value`s:

```json
"font": {
  "weight": {
    "@base": {
      "value": "normal"
    },
    "bold": {
      "value": 600
    }
  }
}
```
`@base` is a special indicator we chose to enable Style Dictionary to parse our tokens correctly while still generating our desired variable names. Much like `value`, it ultimately [gets removed](https://searchfox.org/mozilla-central/rev/159929cd10b8fba135c72a497d815ab2dd5a521c/toolkit/themes/shared/design-system/tokens-config.js#94-96) from the final token name.

You will also see many places where `@base` is referenced in a value definition. This is a bit of a gotcha - even though `@base` isn't part of the token name, we still need to include it when using Style Dictionary's syntax for [variable references/aliases](https://amzn.github.io/style-dictionary/#/tokens?id=referencing-aliasing). That means the following CSS:

```css
--input-text-min-height: var(--button-min-height);
```

Will look like this in our JSON:

```json
"input": {
  "text": {
    "min": {
      "height": {
        "value": "{button.min.height.@base}"
      }
    }
  }
}
```

### High Contrast Mode (HCM) media queries

We need to be able to change our token values to support both the `prefers-contrast` and `forced-colors` [media queries](https://firefox-source-docs.mozilla.org/accessible/HCMMediaQueries.html). We employ `prefersContrast` and `forcedColors` keys nested in a token's `value` key to indicate that we need to generate a media query entry for that token. For example this JSON:

```json
"border": {
  "color": {
    "interactive": {
      "@base": {
        "value": {
          "prefersContrast": "{text.color.@base}",
          "forcedColors": "ButtonText",
        }
      }
    },
  }
}
```
results in the following CSS:

```css
/* tokens-shared.css */

@layer tokens-prefers-contrast {
  @media (prefers-contrast) {
    :root,
    :host(.anonymous-content-host) {
      /** Border **/
      --border-color-interactive: var(--text-color);
    }
  }
}

@layer tokens-forced-colors {
  @media (forced-colors) {
    :root,
    :host(.anonymous-content-host) {
      /** Border **/
      --border-color-interactive: ButtonText;
    }
  }
}
```

### Theming

Our JSON supports two dimensions of theming designed around the needs of the Firefox desktop client; light and dark themes, and brand and platform themes.

#### Light and dark themes

We employ `light` and `dark` keys in our `value` objects to indicate when a given token should have different values in our light and dark themes:

```json
"background": {
  "color": {
    "box": {
      "value": {
        "light": "{color.white}",
        "dark": "{color.gray.80}",
      }
    },
  }
}
```

The above JSON communicates that `--background-color-box` should have the value of `var(--color-white)` in our light theme and `var(--color-gray-80)` in our dark theme.

If a token has the same value for both the light and dark themes it will either have a string or number as its `value` or it will use a `default` key:

```json
"button": {
  "background": {
    "color": {
      "disabled": {
        "value": {
          "default": "{button.background.color.@base}",
        }
      }
    }
  }
}
```

The above JSON indicates that `--button-background-color-disabled` will have the value of `var(--button-background-color)` regardless of theme.

#### Brand and platform themes

The Firefox desktop client consists of [two distinct surfaces](https://acorn.firefox.com/latest/resources/browser-anatomy/desktop-ZaxCgqkt#section-anatomy-fd); "the chrome," or the UI of the browser application that surrounds web pages, and the web content itself which is often referred to as "in-content." Firefox UI development spans both surfaces since our `about:` pages are "in-content" pages. In our design system we distinguish between these surfaces by using the terminology of `platform` vs `brand`. Chrome specific token values live in [`tokens-platform.css`](https://searchfox.org/mozilla-central/source/toolkit/themes/shared/design-system/tokens-platform.css) and in-content specific token values live in [`tokens-brand.css`](https://searchfox.org/mozilla-central/source/toolkit/themes/shared/design-system/tokens-brand.css).

We use `platform` and `brand` keys in our JSON to indicate when a token has a surface-specific value. For example this token definition:

```json
"text": {
  "color": {
    "@base": {
      "value": {
        "brand": {
          "light": "{color.gray.100}",
          "dark": "{color.gray.05}"
        },
        "platform": {
          "default": "currentColor"
        }
      }
    },
  }
}
```
communicates that `--text-color` should have the value `currentColor` in `tokens-platform.css` for chrome surfaces, and the value `light-dark(var(--color-gray-100), var(--color-gray-05))` in `tokens-brand.css` for in-content pages. The resulting CSS spans multiple files:

```css
/* tokens-platform.css */
@layer tokens-foundation {
  :root,
  :host(.anonymous-content-host) {
    /** Text **/
    --text-color: currentColor;
  }
}
```

```css
/* tokens-brand.css */
@layer tokens-foundation {
  :root,
  :host(.anonymous-content-host) {
    /** Text **/
    --text-color: light-dark(var(--color-gray-100), var(--color-gray-05));
  }
}
```

### Adding new tokens

In order to add a new token you will need to make changes to `design-tokens.json` - any files generated based on our JSON token definitions should never be modified directly. You should be able to work backwards from the variable name in whatever language you're working with to figure out the correct JSON structure.

Once you've made your changes, you can generate the new tokens CSS files by running `./mach buildtokens`.

If you encounter errors or get unexpected results when running the build command it may be because:

* You forgot to nest everything in a `value` key and consequently Style Dictionary is not parsing your tokens correctly, or;
* You omitted `@base` when referencing another token, or;
* You are referencing another token that you haven't defined in the JSON yet - Style Dictionary should give descriptive error messages in this case.

If you need any help feel free to reach out in #firefox-reusable-components on Slack or #reusable-components on Matrix.
