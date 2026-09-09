# Releasing the extension

The repository-root [VERSION](../VERSION) file is the single source of the extension's release
version. Use a stable `MAJOR.MINOR.PATCH` value without a `v` prefix. PHP's reported extension
version, the Debian package's `Version` field, and its filename all use this value.

## Prepare a release

1. Edit `VERSION` to the next release number. No version edit in C++ or packaging scripts is needed.
2. Build and verify from the extension repository root:

   ```bash
   docker compose build phuamqp
   docker compose run --rm --no-deps phuamqp make test test-versioning
   docker compose run --rm --no-deps phuamqp bash /workspace/quick-check.sh
   ```

3. Run the broker tests described in [AGENTS.md](../AGENTS.md) when the release changes message
   processing or connection behavior.
4. Review and commit the release changes. Create and push a tag named `v` followed by the exact
   value in `VERSION`. The tag must point to the commit containing that version and its changes.

The tag workflow checks the version before building, then produces and uploads the Debian
package. The tag identifies the release; it does not override the version file. PHP and PHP-CPP
dependency versions remain separate settings.

## Build a package locally

After preparing the development image, rebuild the mounted checkout and package it:

```bash
docker compose run --rm --no-deps phuamqp bash -lc 'make && bash packaging/build-deb.sh'
```

The package appears in the repository root. The package script loads the selected binary with
PHP and checks its version before packaging. Rebuild if it reports a version different from
`VERSION`. Package-only environment overrides are not used.

For builds without Docker, follow [INSTALL.md](INSTALL.md). Make reads `VERSION` when compiling
`main.cpp` and rebuilds that object when the file changes. The secondary CMake configuration
reads the same version and reconfigures when it changes.

An application must load the newly installed binary to report the new version; restart existing
PHP workers after installation as appropriate. Check `php --ri uamqpphpbinding` in the target
runtime to verify what it actually loads.
