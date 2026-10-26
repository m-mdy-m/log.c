# Release guide

This document describes how to verify and publish a release of `m-mdy-m/log.c`. Building a package locally does not publish it. The GitHub Release workflow starts only after a version tag is pushed, and Docker Hub publication additionally requires credentials.

## 1. Before a release

1. Review `CHANGELOG.md` and add the entry for the version you intend to publish.
2. Keep `CMakeLists.txt`, `Makefile`, and release package metadata on the same version.
3. Read [C ABI reference](abi.md) and decide whether the ABI version should change.
4. Run the gates from the [developer guide](development.md): `make clean check`, CMake/CTest, and extended installed-consumer tests.
5. Inspect `git diff --check`, the full diff, generated package contents, the license, and `NOTICE`.
6. Commit the changes locally with your own signing key and verify the signature before pushing.

Example local signed commit:

```sh
git add -A
git diff --cached --check
git diff --cached
git -c commit.gpgsign=true commit -S -m "docs: rewrite user and ABI guides"
git show --show-signature --stat HEAD
```

These commands create a local commit only. They do not push a branch or tag.

## 2. Create a version tag and GitHub Release

The release workflow expects a tag like `v0.1.0` to match the version declared by the project and a matching `CHANGELOG.md` section. **Do not create or push the tag until the commit you intend to release is signed, reviewed, and green in CI.**

When you choose to publish:

```sh
git tag -s v0.1.0 -m "m-mdy-m/log.c v0.1.0"
git tag -v v0.1.0
# Push only when you intentionally want GitHub Actions to publish the release:
# git push origin v0.1.0
```

The final push is intentionally commented out in this example. The workflow performs builds and tests for the configured platform matrix, creates source archives and checksums, then attaches the SDK archives to a GitHub Release. If any required platform build fails, release publication should be blocked.

Current release assets are configured for:

| Asset target | Package format |
|---|---|
| Source | ZIP and TAR.GZ |
| Linux x86-64 / ARM64 | TAR.GZ SDK archives |
| macOS Intel / Apple silicon | TAR.GZ SDK archives |
| Windows x86-64 MinGW-w64 UCRT64 | ZIP SDK archive |
| FreeBSD x86-64 | TAR.GZ SDK archive |
| OpenBSD x86-64 | TAR.GZ SDK archive |
| Checksums | `SHA256SUMS.txt` plus per-asset sidecars |

A prebuilt library must match the consumer's operating system, CPU architecture, and C runtime. The Windows asset is for MinGW-w64 UCRT64, not MSVC.

## 3. Docker Hub SDK image

The Docker image is an SDK/build environment, not a long-running log service. It is intended to let an application build with the logger's public header and libraries.

To enable release publishing, create a Docker Hub repository for the desired name, then add these GitHub Actions repository secrets:

- `DOCKERHUB_USERNAME`: Docker Hub namespace or username.
- `DOCKERHUB_TOKEN`: access token permitted to push the image.

The release workflow publishes multi-platform `linux/amd64` and `linux/arm64` images when both secrets exist. The tag is derived from the Git tag, and stable releases also update `latest`. If credentials are missing, Docker publication is skipped; it does not prevent you from using local Docker builds.

After the image is published, a consumer can use it as a build environment:

```sh
docker pull <dockerhub-username>/log.c:latest
docker run --rm -it -v "$PWD:/workspace" <dockerhub-username>/log.c:latest
```

The example name is a placeholder. Use the actual namespace configured in the repository's workflow.

## 4. Verify release downloads

After downloading an archive, check its digest against the matching line in `SHA256SUMS.txt`. On Linux:

```sh
sha256sum -c SHA256SUMS.txt
```

On macOS, calculate the file hash with `shasum -a 256`. On Windows, PowerShell can calculate it with:

```powershell
Get-FileHash .\m-mdy-m-log-v0.1.0-windows-x86_64-ucrt64.zip -Algorithm SHA256
```

Compare the resulting digest to the trusted checksum published for the same release. A checksum downloaded from the same untrusted source as a changed archive cannot independently prove the source is trustworthy.

## 5. Local package checks

You can create source packages without publishing them:

```sh
make package
```

On Linux x86-64, create the SDK package with:

```sh
make package-linux
```

Both package targets run `make check` first. Inspect the archives with `tar -tzf` or `unzip -l` and verify the header, docs, examples, license and attribution are present. Local package creation does not create a GitHub Release and does not push to Docker Hub.
