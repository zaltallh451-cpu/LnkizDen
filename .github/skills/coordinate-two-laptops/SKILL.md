---
name: coordinate-two-laptops
description: Coordinate a bounded task with laptop 2 while continuing independent implementation on laptop 1.
disable-model-invocation: true
---
Read `docs/AI_COORDINATION.md`, `.ai-coordination/LAPTOP1_STATUS.md`, `.ai-coordination/LAPTOP2_STATUS.md`, and `.ai-coordination/LAPTOP2_INBOX.md`.

Use the user's current request as the goal. Check actual remote Agent Host session state before deciding whether laptop 2 is available. Treat unknown, disconnected, running, or waiting-for-input as busy. Do not infer availability from a stale status file.

If and only if there is a useful independent subtask within the user's request, assign it using the task gate in the protocol: outcome, why parallel, exact file ownership, base commit, acceptance criteria, validation commands, and stop condition. Use a separate worktree and unique branch `ai/laptop-2/<task-id>`, and request an explicit report. Otherwise, do not invent a task.

Continue implementing a different, non-overlapping part of the user's request yourself while laptop 2 works, on your own branch `ai/laptop-1/<task-id>`. After each coherent milestone passes relevant checks, inspect the staged diff, stage only owned paths, commit with a descriptive message, and push your task branch. Fetch laptop 2's branch at meaningful milestones and record the hashes. This prompt authorizes those task-branch checkpoint commits/pushes only. Never use `git add .`, force-push, push to the default branch, or auto-merge. Review and validate the worker's result before integration.

If remote-agent tools are unavailable, prepare a Git-only inbox handoff and state plainly that laptop 2 must be prompted to fetch it; do not describe that fallback as automatic.