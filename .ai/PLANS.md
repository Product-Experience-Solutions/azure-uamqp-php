# Azure uAMQP PHP Execution Plans (ExecPlans)

An execution plan records the outcome, implementation approach, progress, and evidence for a long or complex task. Write it so another agent or maintainer can resume with only this repository and the plan, without access to the original conversation.

This file defines the planning convention and template. Individual task plans live under `.ai/projects/`. Follow the repository's [AGENTS.md](../AGENTS.md); a plan does not grant permission to commit, publish a release, install into an external application, or expand the requested scope.

## When to use a plan

Create or update an ExecPlan for several dependent milestones, substantial unknowns, significant refactoring, changes across the native and PHP layers, build or packaging changes with compatibility implications, or work likely to span sessions. Use one whenever the user asks for a persistent plan. Small, straightforward edits do not need a separate planning file.

Inspect the relevant code and existing notes first. Continue an existing plan for the same task instead of creating competing versions. A request to analyze or plan does not by itself authorize implementation. During authorized implementation, continue through milestones with meaningful progress updates; do not introduce a new approval gate at each milestone. Do not commit without explicit user approval.

## Where plans live

Use `.ai/projects/<project>/tasks/<descriptive-task-name>.md`, with lowercase, hyphen-separated names. For example, a future message-decoding task could use `.ai/projects/message-decoding/tasks/decode-binary-message-bodies.md`. Each task has one authoritative plan containing its progress and decisions.

Use `.ai/projects/<project>/README.md` for shared project context and links when needed. Supporting design or maintainer notes can sit beside that overview. Create directories only when they have useful content. See [the documentation map](README.md) for the full layout.

Retain completed plans and their outcomes. When a task directory becomes crowded, completed plans can move to `.ai/projects/<project>/archive/`; update incoming links and relative links inside the moved files. Promote reusable knowledge into maintained documentation or a relevant skill before archiving. Archived plans are historical evidence, not current instructions.

These paths are relative to the extension repository, including when it is checked out inside PXH. Plans must also work in a standalone checkout. If work spans PXH and the extension, identify the two repository roots and their separate changes and validation explicitly.

## Status and resuming work

Start each plan with its status, last-updated date, and affected repository or components. Use `draft`, `active`, `blocked`, `ready-for-review`, or `complete`. A blocked plan names the missing input or external prerequisite. `Ready-for-review` means implementation and available checks are done but acceptance is pending. Mark complete when the acceptance criteria and any required user confirmation are satisfied.

Update progress, decisions, evidence, and the next concrete action at milestones, changes of direction, and handoffs. Split partially completed steps into completed and remaining work. Separate confirmed observations from hypotheses, and distinguish a proposed check from a check that actually ran.

When resuming, inspect the current diff and relevant source before trusting the recorded state. Reconcile changes made since the last update. Record source revisions, dependency versions, PHP version, build artifacts, installed module paths, and running-process state when relevant. A rebuilt library and a running application's loaded extension can differ.

## Writing the plan

Read this file in full before authoring an ExecPlan and use the skeleton below. Explain the user's desired outcome before implementation details. State scope, accepted decisions, assumptions, and observable acceptance criteria. Give repository-relative paths and name the relevant classes, methods, scripts, and build targets.

Use ordinary Markdown without an outer code fence. Prefer concise prose, use checkboxes for progress, and include exact commands with their working directory. Summarize essential context in the plan; links to source code and maintained documentation provide supporting detail, not a substitute for the problem statement. Define unfamiliar terms where they matter.

Organize work into independently verifiable milestones. Each milestone should state its result, dependencies, intended edits, and how to verify it. Research uncertain library behavior before relying on it. A focused prototype can resolve an uncertainty; record its evidence and whether it should become permanent coverage or be discarded.

Keep `Progress`, `Next Action / Handoff`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` current. Record important decisions with a concise rationale and date, and revise affected milestones when the approach changes. Keep unresolved questions explicit rather than inventing answers.

## Extension-specific concerns

Trace behavior from the PHP entry point through the native wrapper and the uAMQP API. Search for existing helpers before adding similar code. For PHP-visible changes, inspect the declarations and registrations in `main.cpp` alongside the class implementation and relevant examples.

For C and C++ changes, explain ownership of handles and buffers, their lifetimes across callbacks, cleanup on failure, and exception propagation when relevant. Preserve explicit byte lengths for binary data. Distinguish AMQP body sections from the types of values inside them, and protocol payloads from diagnostic text.

For build or packaging changes, use `setup.sh`, `Makefile`, and the workflows as the source of truth. Inspect the actual compiler flags and dependency versions rather than assuming the standalone CMake target is the extension build path. Keep source changes, compilation, installation, module loading, and a running consumer's behavior separate in the evidence.

Keep credentials and real queue payloads out of plans. Use synthetic messages and sanitized logs. Record enough of a reproduction to repeat it without depending on a temporary file or a live production queue. Raw logs and generated binaries belong outside `.ai/`.

## Validation and acceptance

Choose validation for the changed behavior and follow [AGENTS.md](../AGENTS.md). Run native and PHP checks inside the extension's Docker environment. From the extension repository root, the existing build, focused test, and module-load checks are:

```bash
docker compose build phuamqp
docker compose run --rm --no-deps phuamqp make test
docker compose run --rm --no-deps phuamqp bash /workspace/quick-check.sh
```

For broker-dependent changes, follow [.github/workflows/e2e.yml](../.github/workflows/e2e.yml) and [scripts/run-e2e.sh](../scripts/run-e2e.sh). Record the emulator configuration and test prerequisites, including the SQL password and EULA settings supplied through the local test environment; do not record real credentials. Inspect existing containers before starting the stack and describe how to restore their prior state after testing.

A helper test does not by itself demonstrate that the PHP-visible receive path uses the helper correctly. A regression plan should exercise the failing public behavior with the relevant AMQP representation and check compatibility with existing behavior. Record what the broker test actually covers; a Data-body round trip does not establish Value-body coverage.

For each relevant check, record the command, environment, actual result, and a short evidence excerpt. Describe expected results before execution and replace guesses with observed results afterward. Record checks that could not run and their remaining prerequisites. A successful compile or module-load check alone does not prove a message-processing fix.

Review the implementation and final diff for correctness, reuse, compatibility, and unintended changes. If code changes during review, repeat the affected checks. For documentation-only work, check accuracy, local links, and whitespace; rebuilding the extension is unnecessary unless executable behavior also changes.

## Skeleton of an ExecPlan

Copy the following skeleton into the task file as ordinary Markdown. Replace its placeholders, and keep a brief explanation for sections that do not apply.

    # <Short, action-oriented task title>

    Status: draft
    Updated: <YYYY-MM-DD>
    Repository / scope: <repository and affected components>

    Maintain this plan according to `.ai/PLANS.md` and the applicable `AGENTS.md` instructions.

    ## Purpose / Big Picture

    Explain the current problem, the resulting user-visible behavior, and how to observe success.

    ## Scope and Constraints

    Record the requested work, exclusions, compatibility requirements, accepted decisions,
    unresolved assumptions, and any actions requiring additional authorization.

    ## Progress

    - [ ] <First verifiable milestone>
    - [ ] <Implementation and relevant regression coverage>
    - [ ] <Review, validation, and handoff>

    Add dated evidence when marking a step complete. Split partially completed steps.

    ## Next Action / Handoff

    State the next concrete action, relevant files, current build or test state, and any
    missing prerequisite. Keep this usable by someone starting a new session.

    ## Surprises & Discoveries

    Record confirmed observations with evidence. Label hypotheses and how to verify them.

    ## Decision Log

    Record each important decision, its rationale, and date. Explain changes of direction.

    ## Outcomes & Retrospective

    Compare results against acceptance criteria. Record remaining work and useful lessons.

    ## Context and Orientation

    Name the relevant files, PHP methods, native classes, library APIs, and current behavior.
    Describe the versions and runtime environment that matter to this task.

    ## Plan of Work and Milestones

    For each milestone, describe its outcome, dependencies, intended edits, and verification.
    Include a research or prototype milestone when evidence is needed to choose an approach.

    ## Concrete Steps

    Give exact commands and working directories. State prerequisites and expected output.

    ## Validation and Acceptance

    Define specific inputs and observable outputs. Cover the relevant PHP-visible path and
    compatibility cases. Record actual checks and any validation that remains unavailable.

    ## Idempotence and Recovery

    Explain which operations can safely be repeated, how to recover from partial failures,
    and how to restore containers or other test resources changed by this task.

    ## Artifacts and Notes

    Include short sanitized evidence and reproduction commands. Identify generated artifacts
    and installed versions separately from source revisions.

    ## Interfaces and Dependencies

    Specify relevant PHP API changes, native handle ownership, callback lifetimes, and library
    or packaging dependencies. Base paths and signatures on inspected source code.
