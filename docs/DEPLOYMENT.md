# GitHub Pages deployment

The custom workflow tests all simulator modules before packaging the static site. In repository Settings → Pages, choose **GitHub Actions** for the source. Branch publishing is a fallback only: the root index redirects to simulator/ and does not use the custom Pages artifact.

The deployment job explicitly grants actions: read, pages: write, and id-token: write. It uploads an attempt-specific artifact, checks for visibility through the Actions REST API with the same job token, and waits up to approximately 90 seconds before failing with an actionable message. The deploy action receives the exact same artifact name. This avoids relying on immediate artifact indexing and avoids selecting an earlier attempt's artifact.

On November 25, 2026, runs 45747544831 attempts 1 and 2 passed the test job and uploaded an artifact, but deploy-pages reported zero artifacts. The second artifact subsequently appeared in the Actions REST API. Missing explicit actions: read and indexing delay were plausible causes; the original logs do not establish which was decisive. The workflow changes address both. If Pages still rejects deployment, verify the repository source is GitHub Actions and inspect the deploy job logs. Do not disable tests to bypass deployment.
