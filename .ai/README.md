# Agent documentation map

Start with this repository's [AGENTS.md](../AGENTS.md). This directory holds agent reference material and persistent task plans for the Azure uAMQP PHP extension, including when the repository is checked out independently of PXH.

## Organization

Use the following structure for new work. Project, archive, and skill directories are conventions to create when needed; they do not all exist yet.

```text
AGENTS.md                          Repository rules and entry points
.ai/
  README.md                        This documentation map
  PLANS.md                         Planning rules and task-plan skeleton
  <topic>-context.md               Shared agent reference material, if needed
  projects/
    <project>/
      README.md                    Project overview and links
      <topic>.md                   Shared design or maintainer notes
      tasks/
        <task>.md                  One living execution plan per task
      archive/
        <completed-task>.md        Completed plans and outcomes
.agents/
  skills/
    <skill>/SKILL.md               Reusable procedures, when needed
docs/                              Maintained user and maintainer documentation
tests/                             Permanent native regression tests
scripts/                           PHP tests, end-to-end flow, and development tools
debugging/                         Standalone diagnostic programs
```

`AGENTS.md` explains how to work in this repository. [PLANS.md](PLANS.md) explains how to plan substantial tasks. Individual plans record the task's goal, decisions, progress, evidence, and next action. A skill describes a procedure reusable across many tasks, not the state of a particular task.

## Existing reference material

| Resource | Read when |
|----------|-----------|
| [PLANS.md](PLANS.md) | Planning or continuing a long or complex task |
| [Repository overview](../README.md) | Understanding what the extension provides |
| [Installation guide](../docs/INSTALL.md) | Building, configuring, installing, or debugging the extension |
| [Release guide](../docs/RELEASING.md) and [VERSION](../VERSION) | Updating and verifying the extension and package version |
| [setup.sh](../setup.sh) and [Makefile](../Makefile) | Checking the authoritative build and link commands |
| [main.cpp](../main.cpp) | Changing PHP-visible class or method registrations |
| [docker-compose.yml](../docker-compose.yml) | Preparing the development container and broker emulator |
| [End-to-end workflow](../.github/workflows/e2e.yml) and [runner](../scripts/run-e2e.sh) | Checking the CI validation flow and emulator prerequisites |
| [Module smoke check](../quick-check.sh) | Verifying that PHP can load the installed module |
| [Package builder](../packaging/build-deb.sh) and [dependency list](../packaging/dependencies.txt) | Changing Debian packaging or runtime dependencies |
| [Diagnostic consumer](../debugging/subscription_consumer.cpp) | Investigating AMQP traffic and subscription behavior |

## Keeping context useful

Use descriptive task filenames and keep one authoritative plan per task under `.ai/projects/<project>/tasks/`. Maintain it across milestones and sessions, following [PLANS.md](PLANS.md). Retain completed plans and their outcomes; use the project's `archive/` directory when useful.

Keep durable API, setup, and usage documentation in `docs/`; link to it from plans instead of copying it. Add shared context files only when they explain something not already documented. Keep source code, permanent tests, and executable tools in their existing locations, and keep generated binaries and large logs outside `.ai/`.

Load only context relevant to the current task. Keep links inside this repository where possible so a standalone checkout remains usable. For work that also touches PXH, name the repositories and record their plans, diffs, and verification separately.
