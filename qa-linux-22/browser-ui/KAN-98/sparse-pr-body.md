A sparse right-down gesture can retain its corner but skip every point in the initial 10–50px direction-sampling band. The fallback searches backwards from the gesture endpoint, incorrectly classifies the initial movement as diagonal, and rejects the matching L-shaped gesture.

Search forwards for the first significant movement instead. Preserve the existing averaged sampling path, thresholds and shape-confidence checks. Add regression cases using the two recorded failed trails, rotated L variants with initial jitter, and a diagonal two-point trail whose missing corner must not be guessed.

Related to #2244 / KAN-98. This is independent of Draft #3031's returning-distance gate and does not close the issue. Physical input and Arch/Sway/Wayland remain unverified.

Validation: Debian13.6/Xvfb/DBus, official Floorp12.21.1 / Gecko159.0.1. The full recognizer test module executes in Floorp chrome without replaced module boundaries: 63 cases pass after the change; both new sparse cases fail against the baseline recognizer. Explicit W3C replay of the recorded three-point and five-point paths fails in the unmodified product in all six trials, while dense-input controls pass3/3. A separate product copy with only the matching fallback loop changed passes all six sparse trials and all three dense controls. This diagnostic copy is not a full production build. Targeted lint and diff checks pass; full native CI/production build were not run locally. Browser protections remain enabled.

Base: 984509d2053be7bae9027133bda7259cd706b208.
