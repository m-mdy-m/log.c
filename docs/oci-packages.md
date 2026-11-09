# GitHub Container Registry packages

The project publishes two complementary release channels:

- [GitHub Releases](https://github.com/m-mdy-m/log.c/releases) contains seven platform SDK archives, two source archives, SHA-256 sidecars, and `SHA256SUMS.txt`.
- GHCR contains the same seven native SDK archives as versioned OCI package artifacts.

The GHCR package is `ghcr.io/m-mdy-m/m-mdy-m-log`. It is an OCI package, not a runnable container image. Use ORAS to pull it; do not use `docker pull` for these archives.

## Published targets

| Target | OCI tag suffix | Archive | OCI layer media type |
|---|---|---|---|
| Linux x86-64 | `linux-x86_64` | TAR.GZ | `application/vnd.pipeline.package.layer.v1.tar+gzip` |
| Linux ARM64 | `linux-aarch64` | TAR.GZ | `application/vnd.pipeline.package.layer.v1.tar+gzip` |
| macOS Intel | `macos-x86_64` | TAR.GZ | `application/vnd.pipeline.package.layer.v1.tar+gzip` |
| macOS Apple silicon | `macos-arm64` | TAR.GZ | `application/vnd.pipeline.package.layer.v1.tar+gzip` |
| Windows x86-64 UCRT64 | `windows-x86_64-ucrt64` | ZIP | `application/zip` |
| FreeBSD x86-64 | `freebsd-x86_64` | TAR.GZ | `application/vnd.pipeline.package.layer.v1.tar+gzip` |
| OpenBSD x86-64 | `openbsd-x86_64` | TAR.GZ | `application/vnd.pipeline.package.layer.v1.tar+gzip` |

For version `0.1.1`, the Linux x86-64 package tag is:

~~~text
ghcr.io/m-mdy-m/m-mdy-m-log:0.1.1-linux-x86_64
~~~

## Pull a package

Install ORAS 1.3.5 or a compatible version, then pull the archive:

~~~sh
oras pull ghcr.io/m-mdy-m/m-mdy-m-log:0.1.1-linux-x86_64
~~~

The Windows package is a ZIP archive; the other targets are TAR.GZ archives. Select the target that matches the consuming operating system, CPU architecture, and ABI. The Windows build targets MinGW-w64 UCRT64, not MSVC.

For private packages, authenticate first:

~~~sh
oras login ghcr.io
~~~

For reproducible consumption, prefer the immutable manifest reference `ghcr.io/m-mdy-m/m-mdy-m-log@sha256:...` recorded in the publication evidence rather than relying on a mutable tag.

## Integrity and provenance

The release workflow uses the exact platform SDK archives produced by the build jobs and downloaded for the GitHub Release. It does not rebuild them for GHCR. For every platform, it:

1. Creates a Pipeline package descriptor with component ID, package version, source repository and commit, OS, architecture, ABI, archive media type, byte size, and SHA-256.
2. Verifies the source archive against the descriptor.
3. Stages a private copy and verifies the staged bytes again.
4. Pushes the package to GHCR using ORAS.
5. Creates a publication receipt containing the immutable OCI manifest digest.
6. Pulls the package back by immutable digest and verifies the downloaded archive against the original descriptor.
7. Uploads the descriptor, receipt, pulled bytes, and checksums as the `ghcr-publication-evidence` GitHub Actions artifact.

The GitHub Release and GHCR publication jobs both depend on the complete seven-platform build matrix but are independent of one another. A GHCR publication failure does not prevent GitHub Release assets from being published, and a GitHub Release publication failure does not suppress GHCR publication.

The first publication may require a one-time visibility review in GitHub Packages. Set the package to Public if unauthenticated downloads are intended; otherwise consumers need a GitHub token with package read access.
