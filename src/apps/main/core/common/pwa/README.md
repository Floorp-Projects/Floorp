# PWA

PWA stands for Progressive Web App, which is a technology that allows web pages available in modern browsers to be used like applications.

## What is PWA on Floorp?

Floorp realizes PWA by refactoring Firefox's code, utilizing the SSB (Site Specific Browser) feature that Firefox provided.

For example, ManifestProcesser.ts is written with reference to Firefox code, and functions with the same name exist.

## Architecture Overview

The PWA feature consists mainly of two parts:

1. **Frontend part** (`src/apps/main/core/common/pwa/`)

   - UI elements (SsbPageAction, SsbPanelView)
   - PWA window management
   - High-level service API

2. **Backend part** (`src/apps/modules/src/modules/pwa/`)
   - Command line integration
   - Data storage
   - OS integration features

## Main Components

- **PwaService**: Central service class. Integrates other components and provides high-level API
- **SiteSpecificBrowserManager**: Responsible for PWA installation and management
- **SsbRunner**: Handles launching PWA windows
- **DataManager**: Manages information of installed PWAs
- **ManifestProcesser**: Extracts and processes PWA information from web manifests

## OS Integration

Currently, the following integration features are available on the Windows platform:

- Pinning to the taskbar
- Launching in dedicated windows
- Icon integration

## Directory Structure

```plaintext
src/apps/main/core/common/pwa/
├── config.ts             # Configuration options
├── dataStore.ts          # Data persistence
├── default-pref.ts       # Default settings
├── iconProcesser.ts      # Icon processing
├── imageTools.ts         # Image processing utilities
├── index.ts              # Main entry point
├── manifestProcesser.ts  # Manifest processing
├── pwa-window.tsx        # PWA window UI
├── pwaService.ts         # Central service
├── SsbPageAction.tsx     # URL bar PWA action
├── SsbPanelView.tsx      # PWA management panel
├── ssbManager.ts         # SSB management
├── ssbRunner.ts          # SSB execution
└── type.ts               # Type definitions

src/apps/modules/src/modules/pwa/
├── DataStore.sys.mts           # System-level data store
├── SsbCommandLineHandler.sys.mts # Command line integration
├── ImageTools.sys.mts          # Image processing utilities
├── supports/                   # OS-specific support features
└── type.ts                     # Type definitions
```

## Developer Information

To extend the PWA functionality, you mainly need to understand the following files:

- `pwaService.ts`: Adding/changing service APIs
- `ssbManager.ts`: Extending installation and management features
- `manifestProcesser.ts`: Customizing manifest processing
- `SsbPageAction.tsx`/`SsbPanelView.tsx`: Customizing UI elements

To add support for a new platform, implement it in the `supports/` directory.
