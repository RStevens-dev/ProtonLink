# GitHub Pages deployment

## Required repository setting

Open https://github.com/RStevens-dev/ProtonLink/settings/pages and select:

- Source: **Deploy from a branch**
- Branch: **main**
- Folder: **/ (root)**

Save the setting. GitHub's native Pages service publishes the branch. The root index redirects to ./simulator/; the simulator imports its JavaScript modules directly from that directory. The root .nojekyll file disables Jekyll processing.

## Regression testing

The existing .github/workflows/pages.yml is now test-only, named Simulator regression tests. It runs on main pushes, pull requests and manual dispatch. It checks the shared regression suite, module syntax, root redirect and local module references. Native branch publishing is independent of this workflow and is not gated by its result. Check tests before merging changes. No custom upload artifact, deployment API, OIDC credential or Pages write permission remains.

## Rationale

Several November 26, 2026 runs passed tests but failed in custom artifact discovery or Pages publishing. The last direct API deployment (run 45760633876) created a deployment but received deployment_failed. Branch publishing removes the custom machinery. It does not guarantee GitHub service availability. If publishing fails, inspect the native pages build and deployment workflow and verify the source setting.

Public URL: https://rstevens-dev.github.io/ProtonLink/

The entire public branch may be served by Pages; do not commit private credentials or personal configuration. The simulator remains offline-only. The final branding image is optional and loaded from assets/branding/ProtonLink.png when present.
