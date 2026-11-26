# GitHub Pages deployment

The custom workflow tests all simulator modules before packaging the static site. In repository Settings → Pages, choose **GitHub Actions** for the source. Branch publishing is a fallback only: the root index redirects to simulator/ and does not use the custom Pages artifact.

The deployment job explicitly grants actions: read, pages: write, and id-token: write. It uploads an attempt-specific artifact, checks for visibility through the Actions REST API with the same job token, and waits up to approximately 90 seconds before failing with an actionable message. The deploy action receives the exact same artifact name. This avoids relying on immediate artifact indexing and avoids selecting an earlier attempt's artifact.

On November 25, 2026, runs 45747544831 attempts 1 and 2 passed the test job and uploaded an artifact, but deploy-pages reported zero artifacts. The second artifact subsequently appeared in the Actions REST API. Missing explicit actions: read and indexing delay were plausible causes; the original logs do not establish which was decisive. The workflow changes address both. If Pages still rejects deployment, verify the repository source is GitHub Actions and inspect the deploy job logs. Do not disable tests to bypass deployment.


## Direct artifact-ID deployment

Two November 26 builds passed tests and uploaded valid artifacts, but `actions/deploy-pages@v4` returned zero artifacts from its separate Actions artifact service even after the REST API confirmed visibility. The workflow now uses the upload action's `artifact_id` output, verifies the ID/name/workflow run through the REST API, obtains the job OIDC token and creates the Pages deployment directly using the same REST payload as the official action. It polls until success or a terminal failure. No application code changes. Pages must be configured for GitHub Actions. Credentials remain job-scoped and masked; the workflow fails if deployment is not verified. See https://github.com/actions/deploy-pages/blob/v4/src/internal/api-client.js and https://github.com/actions/deploy-pages/blob/v4/src/internal/deployment.js.
