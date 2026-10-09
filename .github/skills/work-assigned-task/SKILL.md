---
name: work-assigned-task
description: Check laptop 2's coordination inbox and execute only its current assigned task.
disable-model-invocation: true
---
Read `docs/AI_COORDINATION.md`, `.ai-coordination/LAPTOP2_INBOX.md`, `.ai-coordination/LAPTOP1_STATUS.md`, and `.ai-coordination/LAPTOP2_STATUS.md`.

If a live remote-session message assigned a task, follow it. Otherwise, act only if the inbox state is `ASSIGNED`. If there is no current assignment, report `IDLE` and make no project changes.

Before editing, verify the task ID, base commit, allowed files, acceptance criteria, validation commands, and stop condition. Update only `.ai-coordination/LAPTOP2_STATUS.md` for your status. Do not change the inbox or Laptop 1 status. Do not take another task while your current task is active.

Work in a separate worktree on your unique branch `ai/laptop-2/<task-id>` from the assigned base. Keep within scope and run the requested checks. At each coherent milestone whose relevant checks pass, inspect the staged diff, stage only your owned paths, commit with `ai(laptop-2/<task-id>): <summary>`, and push your task branch. Fetch laptop 1's branch at meaningful milestones, but do not merge it into your active worktree unless laptop 1 confirms a dependency and new base. This prompt authorizes checkpoint commits and pushes to your task branch only. Never use `git add .`, force-push, push to the default branch, merge, or deploy. If a push is rejected, stop and report rather than overwrite work.

Report the task ID, branch, base/final commit hashes, changed files, exact check results, and risks. If you discover a shared dependency or out-of-scope requirement, set your state to `BLOCKED` and ask Laptop 1 for a decision.