"""Deploy the exact Pages artifact using GitHub's Pages REST API and job OIDC.
Avoids the separate artifact-service listing used by deploy-pages@v4.
"""
import json
import os
import sys
import time
import urllib.request
import urllib.error

api = os.environ["GITHUB_API_URL"].rstrip("/")
repo = os.environ["GITHUB_REPOSITORY"]
run = os.environ["GITHUB_RUN_ID"]
artifact = os.environ["ARTIFACT_ID"]
name = os.environ["ARTIFACT_NAME"]
token = os.environ["GH_TOKEN"]
base = f"{api}/repos/{repo}"
if not artifact.isdecimal():
    sys.exit("Upload did not supply a numeric artifact ID.")

def request(url, auth, method="GET", data=None):
    headers = {
        "Authorization": "Bearer " + auth,
        "Accept": "application/vnd.github+json",
        "User-Agent": "ProtonLink-Pages",
    }
    if data is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(
        url, headers=headers, method=method,
        data=json.dumps(data).encode() if data is not None else None,
    )
    with urllib.request.urlopen(req, timeout=25) as response:
        return json.load(response)

# Validate exact ID against this workflow run, rather than accepting any artifact.
for attempt in range(24):
    try:
        info = request(f"{base}/actions/artifacts/{artifact}", token)
        if (str(info.get("id")) == artifact and info.get("name") == name
                and not info.get("expired")
                and str(info.get("workflow_run", {}).get("id")) == run):
            break
    except (urllib.error.URLError, TimeoutError) as error:
        print("Artifact metadata not yet available:", type(error).__name__, flush=True)
    time.sleep(5)
else:
    sys.exit("Uploaded artifact could not be verified for this workflow run.")
print(f"Verified artifact {artifact} ({name}) for run {run}", flush=True)

# The deployment job supplies its own short-lived OIDC credential.
# Never print tokens, payloads, or raw error bodies.
oidc = request(os.environ["ACTIONS_ID_TOKEN_REQUEST_URL"],
               os.environ["ACTIONS_ID_TOKEN_REQUEST_TOKEN"])["value"]
print("::add-mask::" + oidc, flush=True)
try:
    deployment = request(base + "/pages/deployments", token, "POST", {
        "artifact_id": int(artifact),
        "pages_build_version": os.environ["GITHUB_SHA"],
        "oidc_token": oidc,
    })
except urllib.error.HTTPError as error:
    sys.exit(f"Pages deployment creation failed: HTTP {error.code}. Check Pages source and permissions.")
deployment_id = deployment.get("id") or deployment.get("status_url", "").rstrip("/").split("/")[-1]
if not deployment_id:
    sys.exit("Pages did not return a deployment ID.")
print(f"Created Pages deployment {deployment_id}", flush=True)
status_url = f"{base}/pages/deployments/{deployment_id}"
terminal = {"deployment_failed", "deployment_perms_error", "deployment_content_failed",
            "deployment_cancelled", "deployment_lost"}
deadline = time.monotonic() + 540
while time.monotonic() < deadline:
    time.sleep(5)
    try:
        result = request(status_url, token)
    except (urllib.error.URLError, TimeoutError) as error:
        print("Temporary status query error:", type(error).__name__, flush=True)
        continue
    status = result.get("status")
    print("Pages status:", status, flush=True)
    if status == "succeed":
        url = deployment.get("page_url") or result.get("page_url")
        if url:
            with open(os.environ["GITHUB_OUTPUT"], "a") as output:
                output.write(f"page_url={url}\n")
        sys.exit(0)
    if status in terminal:
        sys.exit(f"Pages returned terminal status {status}")
sys.exit("Pages deployment did not report success within the polling deadline.")
