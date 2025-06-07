# Noraneko Build Tools

This `tools/` directory contains all the tools for building and developing the Noraneko project.

## 📁 Directory Structure

```text
tools/
├── build/                      # Build System
│   ├── phases/                 # Build phases
│   │   ├── pre-build.ts        # Pre-Mozilla build processing
│   │   └── post-build.ts       # Post-Mozilla build processing
│   ├── tasks/                  # Individual build tasks
│   │   ├── inject/             # Code injection tasks
│   │   ├── git-patches/        # Git patch application
│   │   └── update/             # Update-related tasks
│   ├── index.ts                # Main build orchestrator
│   ├── defines.ts              # Path and constant definitions
│   ├── logger.ts               # Logging utilities
│   └── utils.ts                # Common utilities
│
└── dev/                        # Development Tools
    ├── launchDev/              # Development environment launch
    ├── prepareDev/             # Development preparation
    ├── cssUpdate/              # CSS update utilities
    └── index.ts                # Development tools entry point
```

## 🚀 Usage

### Build System

```bash
# Development build
deno run -A build.ts --dev

# Production build
deno run -A build.ts --production

# Development build (skip Mozilla build)
deno run -A build.ts --dev-skip-mozbuild
```

### Development Tools

```bash
# Start development server
deno run -A tools/dev/index.ts start

# Clean development environment
deno run -A tools/dev/index.ts clean

# Reset development environment
deno run -A tools/dev/index.ts reset
```

## 🔧 Key Improvements

### 1. **Clear Separation of Concerns**

- `build/`: All build-related functionality
- `dev/`: Development tools and utilities

### 2. **Modular Design**

- Phase-based build system
- Reusable tasks
- Independent utilities

### 3. **Enhanced Maintainability**

- Clear import paths
- Consistent error handling
- Comprehensive logging

### 4. **Improved Developer Experience**

- Intuitive CLI interface
- Help messages
- Better error reporting

## 🔄 Migration Guide

### Changes from Old `scripts/`

| Old Path              | New Path                    |
| --------------------- | --------------------------- |
| `scripts/build.ts`    | `tools/build/build.ts`      |
| `scripts/defines.ts`  | `tools/build/defines.ts`    |
| `scripts/inject/`     | `tools/build/tasks/inject/` |
| `scripts/launchDev/`  | `tools/dev/launchDev/`      |
| `scripts/prepareDev/` | `tools/dev/prepareDev/`     |

### Import Path Updates

```typescript
// Old
import { log } from "./scripts/logger.ts";

// New
import { log } from "./tools/build/logger.ts";
```

## 📚 Related Documentation

- [Build System Documentation](../docs/BUILD_SYSTEM_IMPROVEMENTS.md)
- [Development Guide](../docs/DEVELOPMENT_GUIDE.md)
- [Project Structure](../docs/PROJECT_STRUCTURE.md)

## 📝 Future Improvements

1. **Unified TypeScript Configuration**
2. **Test Coverage Addition**
3. **Enhanced CI/CD Integration**
4. **Documentation Expansion**
5. **Performance Optimization**
