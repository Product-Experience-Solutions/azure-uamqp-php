# Centralize the extension and package version

Status: ready-for-review
Updated: 2026-09-08
Repository / scope: azure-uamqp-php; native build, Debian packaging, release CI, maintainer documentation

Maintain this plan according to `.ai/PLANS.md` and `AGENTS.md`.

## Purpose / Big Picture

The maintainer should edit one version file before a release. PHP's reported extension version,
the Debian control metadata, and the package filename must agree with that value. A release tag
must match it, and packaging must detect an old binary instead of assigning it a newer label.

## Scope and Constraints

Keep the existing extension name, PHP dependencies, and package contents. Preserve the pending
AMQP decoder changes and planning documentation. Do not publish, tag, commit, or install into
PXH as part of this refactor. Use `0.2.4` initially because it is the current package version and
latest local release tag; the hard-coded native version is behind at `0.2.3`.

## Progress

- [x] (2026-09-08) Inspected native registration, Makefile, CMake, packaging, setup, and release CI.
- [x] (2026-09-08) Added `VERSION`, a validating reader, Make/CMake integration, and package checks.
- [x] (2026-09-08) Added release-tag validation and permanent real-artifact regression coverage.
- [x] (2026-09-08) Passed isolated Docker builds, native tests, package checks, and CMake configuration;
  reviewed the implementation twice and documented the release workflow in `docs/RELEASING.md`.

## Next Action / Handoff

Ready for maintainer review. For the next release, edit only `VERSION` and follow
`docs/RELEASING.md`, including a matching release tag. No commit, tag, publication, installation
into PXH, or change to a running application was performed. The initial value remains `0.2.4`;
the existing `v0.2.4` tag was not moved or recreated.

## Surprises & Discoveries

Before this change, `main.cpp` used `0.2.3`; `packaging/build-deb.sh` assigned `v0.2.4`
unconditionally and also had a `0.2.4` fallback, so its apparent environment/tag override was ineffective. The latest local
tag was `v0.2.4`. The package script searched for existing binaries without checking their version.

The Makefile has pending changes from the decoder fix. Preserve those when adding a dependency
on `VERSION` so incremental builds rebuild native version metadata. `setup.sh` delegates the
extension build to Make; the CMake file is a secondary standalone target.

The existing development image supplies CMake `3.22.1`, below the repository's pre-existing
`4.1.2` minimum. The secondary configuration was checked with the official portable CMake
`4.1.2` distribution inside a disposable container, without changing the host or Dockerfile.

## Decision Log

- Decision: Use a plain `VERSION` file containing a stable `MAJOR.MINOR.PATCH` value without `v`.
  Rationale: One easily edited value can drive C++, shell packaging, and CI without a runtime config file.
  Date: 2026-09-08.
- Decision: Initialize to `0.2.4` and require release tags to equal `v` plus the file value.
  Rationale: Keep the current package release identity and prevent future drift.
  Date: 2026-09-08.
- Decision: Load the selected `.so` with PHP before writing package output.
  Rationale: Reading the current version file alone could label an old compiled binary with a new version.
  Date: 2026-09-08.

## Outcomes & Retrospective

`VERSION` now drives PHP's reported native version, Debian metadata and filenames, and both
build configurations. The tag workflow rejects mismatches before building. Package-only
environment overrides are no longer used. Packaging fails before writing output when the
selected binary does not load or reports a different version.

Real Docker artifact tests passed for `0.2.4` and the temporary fixture version `42.7.9`,
including changing only `VERSION` between builds. The native decoder tests also passed during
the initial artifact build. Two source/diff reviews found no remaining actionable issue in
the versioning changes. Existing decoder work and unrelated untracked files were preserved.
No broker round trip was rerun because this refactor changes build metadata, not message behavior.

## Context and Orientation

`main.cpp` registers `uamqpphpbinding` through PHP-CPP. `Makefile` compiles root C++ sources;
`setup.sh` uses it after preparing dependencies. `CMakeLists.txt` lists the same sources for
a separate target. `packaging/build-deb.sh` locates a binary and writes the package metadata.
`.github/workflows/build-deb.yml` builds and uploads packages for `v*` tags. The development
image has PHP, PHP-CPP, uAMQP, and Debian packaging tools needed for isolated regression tests.

## Plan of Work and Milestones

First, make the native version a compile-time value read from `VERSION`, with validation shared
by build and packaging. Make must rebuild the metadata when that file changes; CMake must
observe the same input. Second, validate tags before expensive release builds, and check the
selected binary's PHP-reported version before writing a package. Third, test the real artifacts
before and after changing only `VERSION`, document the maintainer steps, and review the final diff.

## Concrete Steps

Work from the extension repository root. Use the existing Docker development image with the
source mounted read-only and copy only build inputs into a temporary test directory. The
regression script compiles, loads, and packages the extension, changes the fixture version,
rejects the stale binary, and rebuilds/packages successfully. From the repository root, the
validation used the equivalent of:

```bash
docker run --rm --network none \
  --mount "type=bind,src=$PWD,dst=/review-source,readonly" \
  --workdir /review-source --entrypoint /bin/bash azure-uamqp-php-phuamqp:latest \
  -lc 'bash tests/versioning-test.sh'
```

The existing image provides the installed PHP and native dependencies; the test compiles
the current checkout, not the source snapshot baked into the image. A complete image rebuild
was not performed for this metadata refactor. Maintainers can rerun the same permanent check
using `docker compose run --rm --no-deps phuamqp make test-versioning`.

Shell syntax was checked in the same read-only Docker environment using
`bash -n scripts/version.sh packaging/build-deb.sh tests/versioning-test.sh`.

For CMake, a separate disposable container downloaded and extracted the official
`https://github.com/Kitware/CMake/releases/download/v4.1.2/cmake-4.1.2-linux-x86_64.tar.gz`
archive under `/tmp`, then ran:

```bash
/tmp/cmake-4.1.2-linux-x86_64/bin/cmake -S /review-source -B /tmp/cmake-review
grep -F CMAKE_PROJECT_VERSION:STATIC /tmp/cmake-review/CMakeCache.txt
grep -F PHUAMQP_VERSION /tmp/cmake-review/CMakeFiles/php_uamqp.dir/flags.make
grep -F /review-source/VERSION /tmp/cmake-review/CMakeFiles/Makefile.cmake
```

## Validation and Acceptance

Verify a valid version, missing or malformed values, matching and mismatched release tags,
`phpversion('uamqpphpbinding')`, `dpkg-deb` metadata, and package filenames. Confirm an incremental
build after only a version-file change updates the native result. Confirm the package script
rejects a mismatching compiled version. Run existing focused native tests, shell syntax checks,
and final whitespace checks. Broker behavior is unchanged; an emulator round trip is not required
for metadata changes.

## Idempotence and Recovery

Run artifact tests in a `mktemp` directory inside a disposable container. Cleanup targets only
that directory. Do not replace installed modules or alter existing containers. No release action
is required for validation.

## Artifacts and Notes

Source baseline: `514ccb2`, with the pre-existing decoder and documentation changes retained.
The real artifact test exited successfully and reported:

```text
PASS: Native extension reads VERSION
PASS: Debian metadata matches VERSION
PASS: Packaged extension reports the package version
PASS: rejected does not match VERSION
PASS: Changing only VERSION triggers an incremental rebuild
PASS: Debian metadata matches VERSION
PASS: Packaged extension reports the package version
Versioning regression tests passed.
```

Additional passing cases cover matching/mismatching tags, incorrect reader arguments, missing
and malformed version files, and attempts to override the version via legacy environment or
Make command-line variables. Artifacts were extracted and loaded with PHP to check their actual
embedded versions, then removed with the disposable test environment.

CMake configuration exited successfully, producing:

```text
CMAKE_PROJECT_VERSION:STATIC=0.2.4
CXX_DEFINES = -DPHUAMQP_VERSION=\"0.2.4\"
  "/review-source/VERSION"
```

This verifies CMake's configured value, compiler definition, and regeneration dependency, not
the standalone executable's complete build. Shell syntax checks, `git diff --check`, whitespace
checks on new files, and local documentation-target checks passed. Keep binaries and packages
out of source control.

## Interfaces and Dependencies

No PHP method signatures or library dependencies change. The version reader must treat `VERSION`
as data and validate before its value enters compiler flags or package paths. Release CI uses the
same reader to compare the tag. Existing package layout and dependency bundling remain intact.
