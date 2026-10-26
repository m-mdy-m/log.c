# Security Policy

## Reporting a vulnerability

Please do not report security vulnerabilities in a public issue or pull request.

Use GitHub's **Report a vulnerability** option in the repository's **Security** tab when it is available. If private vulnerability reporting is not enabled, contact the maintainer through the private contact method listed on the maintainer's GitHub profile. Include the affected version, the impact, steps to reproduce, and any relevant logs or proof of concept. Do not include real secrets or users' private data.

Please allow time for the maintainer to investigate before publicly disclosing the issue. This project is maintained on a best-effort basis; a response or a particular fix date cannot be guaranteed.

## Supported versions

Security fixes are intended for the latest published release. The development branch may receive fixes before the next release. Older releases are not guaranteed to receive backports; users should update to the latest release when one is available.

## Safe logging

The library writes the messages provided by the application. It cannot know whether a message contains a password, access token, private key, session cookie, or personal data. Applications should redact such values before logging them.
