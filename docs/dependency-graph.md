# GitHub Dependency Graph and dependents

GitHub's **Used by** area is derived from dependency information submitted by
repositories that consume this library. It is not a switch that can be enabled
from the library repository alone.

This repository provides a reusable workflow that lets a consumer submit an
explicit dependency snapshot for `m-mdy-m/log.c`. The snapshot uses the
Package URL `pkg:github/m-mdy-m/log.c@VERSION` and the caller's own
`GITHUB_TOKEN`; no personal access token or secret needs to be created.

## 1. Enable Dependency Graph in the consumer

In each consuming repository:

1. Open **Settings → Advanced Security** (under **Security**).
2. Enable **Dependency graph**.
3. Ensure GitHub Actions is enabled.
4. Allow the workflow to request **Contents: read and write**.

Public repositories commonly have Dependency Graph enabled by default, but
verify the setting rather than assuming it.

## 2. Add a caller workflow

Create `.github/workflows/dependency-submission.yml` in the consumer:

```yaml
name: Dependency graph

on:
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: write

jobs:
  log-c-dependency:
    uses: m-mdy-m/log.c/.github/workflows/dependency-submission.yml@master
    with:
      dependency-version: "0.1.0"
      manifest-path: "deps.lock"
```

Replace `main` with the consumer's default branch if necessary. Set
`dependency-version` to the exact version or release the consumer pins. Set
`manifest-path` to the file that actually records the dependency; it is
metadata for traceability and is not downloaded by the reusable workflow.

The called workflow submits a snapshot for the caller repository, not for
`m-mdy-m/log.c`. It therefore belongs in each consumer repository. The caller
must grant `contents: write`; the reusable workflow cannot elevate the
caller's permissions.

## 3. Verify the result

After the workflow succeeds, open the consumer repository's
**Insights → Dependency graph** and inspect its dependencies. GitHub processes
submitted snapshots asynchronously, so the graph may take a short time to
update.

The library's dependents are visible from its Dependency Graph / Dependents
view when GitHub recognizes the submitted package URL. A submission does not
guarantee that GitHub will display a prominent **Used by** count on the
repository overview; that presentation is controlled by GitHub.

## Important limitations

- C and CMake are not currently listed as native static-analysis ecosystems in
  GitHub's supported package ecosystem table. This workflow uses the
  Dependency Submission API rather than pretending that a CMake or Makefile is
  a supported lockfile.
- A successful submission adds dependency data to the consumer's graph. It
  does not publish a binary package, create a GitHub Release, or install the
  logger.
- Submit only dependencies that the consumer actually uses, and keep the
  submitted version synchronized with its pinned source revision.
- If GitHub does not resolve the `pkg:github` identifier to this repository in
  its Dependents view, the consumer's graph submission is still useful
  provenance, but a package registry integration may be needed for a stronger
  package-to-repository association.

## References

- [How GitHub recognizes dependencies](https://docs.github.com/en/code-security/concepts/supply-chain-security/dependency-graph-data)
- [Dependency submission API](https://docs.github.com/en/rest/dependency-graph/dependency-submission)
- [Supported package ecosystems](https://docs.github.com/en/code-security/reference/supply-chain-security/dependency-graph-supported-package-ecosystems)
