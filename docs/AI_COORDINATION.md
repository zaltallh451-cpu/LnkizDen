# Two-Laptop Copilot Workflow

This protocol coordinates two Copilot agents working from separate laptop clones of this repository. Laptop 1 is the coordinator and an active contributor. Laptop 2 is a specialist worker that takes explicit assignments only. The laptops may use different GitHub and Copilot accounts, provided both GitHub accounts can access the repository.

## What Counts as Shared State

Use this authority order if information disagrees:

1. The user's latest instruction.
2. Live Agent Host session state and messages, when remote-agent tools are available.
3. Fetched Git branches, commits, and pull requests.
4. Files under `.ai-coordination/`.

Status notes are summaries, not proof that a Copilot session is idle. A note can be stale, a laptop can be offline, and a Git push does not wake a chat session.

## Ground Rules

- The user's request is the source of truth. Do not invent unrelated backlog tasks just to keep an agent busy.
- Laptop 1 must keep working on an independent part of the requested task while Laptop 2 works. Delegation is not a reason for the coordinator to wait.
- Delegate only tasks that can make progress independently. State the outcome, allowed files or boundaries, acceptance criteria, validation commands, and relevant behavior to preserve.
- Keep code changes on separate branches or isolated worktrees. Avoid assigning both agents the same files, shared interfaces, generated files, or build configuration at the same time.
- When either two-laptop prompt is explicitly used, the user authorizes checkpoint commits and pushes to that task's own branch only. Review the worker's diff and test results before integrating. Never force-push, push to the default branch, or merge automatically.
- Do not put passwords, tokens, private keys, or other secrets in these files, commits, or prompts.
- Do not assign vague work such as "improve the project" or invent backlog tasks just to keep an agent busy.
- Treat unknown, disconnected, running, and waiting-for-input remote sessions as busy. If availability is ambiguous, ask the user or continue locally.
- Never have both agents edit the same working directory at once. A finished branch is not integrated until laptop 1 reviews it.

## Files and Ownership

- `.ai-coordination/LAPTOP2_INBOX.md` is written only by Laptop 1. It contains the current assignment for Laptop 2. Laptop 2 reads it and does not edit it.
- `.ai-coordination/LAPTOP1_STATUS.md` is written only by Laptop 1.
- `.ai-coordination/LAPTOP2_STATUS.md` is written only by Laptop 2.
- Keep each status file short: current state, task, branch, latest commit, validation, and next action. Replace the current fields instead of appending a long transcript.

## Laptop 1: Coordinate and Contribute

At the start of a user task:

1. Read both status files and the inbox. Run `git fetch origin --prune`, record the latest default-branch commit as the base, and inspect the other agent's published task branches before relying on notes or commits.
2. If remote-agent tools are available, inspect connected hosts and live session states. Reuse a matching active session; do not create duplicate work.
3. Identify a useful subtask only within the user's request. Split work only if both tasks can proceed from a stated base commit without a shared design decision or overlapping files. Tests, read-only investigation, or review are good assignments when implementation overlaps.
4. If no independent task exists, do not manufacture one. Continue alone, or request a read-only review that cannot conflict with implementation.
5. Give laptop 2 a task ID, observable result, allowed files, acceptance criteria, validation commands, base commit, and explicit exclusions. Create a separate worktree on `ai/laptop-2/<task-id>`.
6. Start or message the remote session, then immediately implement a different, non-overlapping part yourself on `ai/laptop-1/<task-id>`. State file ownership in both prompts; do not wait idly.
7. Check progress at meaningful milestones, not in a tight polling loop. If the worker finds a shared dependency, pause affected work and decide which agent owns it.
8. Ask for an explicit report with task ID, branch, base/final commit hashes, changed files, check results, and unresolved risks. A remote session's final response may not be forwarded automatically.
9. Review the full diff and validation. Integrate one branch at a time, resolve conflicts deliberately, and rerun relevant checks on the combined result.

If host/session state is unavailable or ambiguous, ask the user or continue locally; do not infer that laptop 2 is idle from a status note alone.

## Laptop 2: Execute and Report

Before starting work, read this protocol, the inbox, and both status files; fetch the latest refs. A live remote-session assignment takes priority. Otherwise, act only on a current inbox entry whose state is `ASSIGNED`. If none exists, report `IDLE` and make no project changes.

For each assigned task:

1. Confirm the task ID, base commit, branch/worktree, file ownership, acceptance criteria, validation, and stop condition. If any are missing or conflict with other work, report `BLOCKED`; do not guess.
2. Set your status to `ACCEPTED`, then `WORKING` when editing starts. Implement and validate only the assignment.
3. Update `LAPTOP2_STATUS.md` with branch, base/final commit, changed files, exact check results, blockers/risks, next action, and UTC update time.
4. After each coherent milestone passes its relevant checks, inspect the staged diff, stage only owned paths, commit with `ai(laptop-2/<task-id>): <summary>`, and push that branch. Never use `git add .`, force-push, or push to the default branch. If push is rejected, fetch and stop to reconcile rather than overwrite remote work.
5. Fetch laptop 1's branch at meaningful milestones. Read its commits without merging them into your active worktree unless the task depends on them and laptop 1 confirms a new base.
6. Report the result, branch, hashes, changed files, and check results to laptop 1. Set `IDLE` only after the task is reported and no other assignment is active. Do not merge or deploy.

## File-Based Handoff Format

Use a compact assignment in `LAPTOP2_INBOX.md`:

```text
State: ASSIGNED
Task ID: <short unique id>
Goal: <observable result>
Why parallel: <why this can proceed independently>
Owned files: <files laptop 2 may change>
Files/behavior reserved for laptop 1: <overlap to avoid>
Base commit: <shared starting commit>
Acceptance criteria: <conditions that demonstrate completion>
Validation commands: <commands/checks>
Stop condition: <when to pause and ask>
Remote session or branch: <where the work is happening>
Laptop 1 branch: <branch laptop 1 is using>
Created UTC: <timestamp>
```

Inbox states: `NONE`, `ASSIGNED`, `CANCELLED`, `CLOSED`. Worker states: `IDLE`, `ACCEPTED`, `WORKING`, `BLOCKED`, `READY_FOR_REVIEW`. Laptop 1 alone edits the inbox; each laptop edits only its own status file. Replace the current fields rather than appending a transcript. Laptop 2 sets `BLOCKED` with the decision needed if stopped; laptop 1 changes or closes the inbox after resolving it.

## Checkpoint Sync

When either two-laptop prompt is explicitly used, the user authorizes automatic checkpoint commits and pushes to each agent's unique task branch: `ai/laptop-1/<task-id>` and `ai/laptop-2/<task-id>`. This authorization is limited to those branches and tasks. It does not authorize force-push, default-branch push, merge, deploy, or external changes. If repository policy or permissions block a push, report the blocker; never bypass it.

At task start and meaningful milestones, both agents fetch `origin` and inspect the other's published branch. A push makes commits visible after the other side fetches, but fetch alone does not merge or apply code to the other worktree. Laptop 1 reviews and integrates branches one at a time; the default branch becomes the shared baseline only after that review and integration.

## Limits of Git-Only Coordination

Separate clones do not share chat history, files, or live status. A commit becomes visible only after it is pushed and the other side fetches it. Without remote-agent tools, the inbox is asynchronous: laptop 2 must be prompted to fetch/check it, and laptop 1 must fetch laptop 2's branch to read its status and commits. Git-only coordination cannot start, wake, or monitor an idle Copilot chat.