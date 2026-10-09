State: READY_FOR_REVIEW
Task ID: runtime-recovery-audit
Goal: Read-only audit of daemon runtime recovery failure paths and host-test gaps.
Branch/worktree: ai/laptop-2/runtime-recovery-audit / C:\Users\HP\Music\LnkizDen-laptop2-runtime-recovery-audit
Base commit: 75f84e4a10275a30807d7a2cd876c94c8e242c1b
Owned files: Read-only audit scope; write only this status handoff.
Latest commit: 16ce305a875cf49b3c97807578e2fde4389c99dd (audit handoff)
Last fetched default-branch head: 75f84e4a10275a30807d7a2cd876c94c8e242c1b
Last pushed UTC: 2026-10-09T21:02:34Z
Checks/results:
- Audited source/daemon/source/runtime_supervisor.cpp, daemon startup/shutdown, private-loader contract, readiness code, and host tests.
- Failure paths (verified): supervisor pthread_create return is ignored (source/daemon/source/main.cpp:173-174), so thread creation failure leaves no runtime recovery worker and is not reported. Private-loader outage recovery waits for two failed health checks, defers for a matching busy marker until the 150-second grace expires, then may SIGKILL the recorded loader; it needs external :9021, and failed launch/readiness attempts back off from 5 seconds to a 30-second cap (runtime_supervisor.cpp:42-48, 152-193, 246-303). Util recovery clears its ready marker, uploads through :9020, waits up to 15 seconds for a PID marker plus expected process name, and retries on failure (runtime_supervisor.cpp:201-218, 324-331).
- Edge case (verified from control flow): if CLOCK_MONOTONIC continuously fails while a valid busy marker persists, monotonic_seconds() returns 0, busy_since remains 0, and busy_timed_out never becomes true; the supervisor keeps deferring recovery (runtime_supervisor.cpp:81-86, 250-263).
- Inferred risk (not runtime-verified): a util upload can succeed at the transport layer but the process may not publish readiness within 15 seconds; the subsequent retry can send another util ELF while the first process is still initializing. The remote-send API confirms byte transfer, not exact launched PID (include/elfldr_remote.h:26-40; runtime_supervisor.cpp:201-218). Requires console/runtime validation before treating as a reproduced defect.
- Host coverage: no runtime-supervisor/recovery tests found in source/util/tests. test_ready covers ready-marker/PID file semantics (test_ready.c:79-110); test_payload_autostart covers bootstrapper autostart policy, not daemon restart behavior (test_payload_autostart.c:125-182). Neither exercises process-name validation, busy aging, SIGKILL, :9021 recovery, util restart, or retry backoff.
- Recommended isolated improvement: add a host-testable, side-effect-free supervisor policy seam with a fake clock and assert the existing two-check threshold, 150-second busy boundary, and 5-to-30-second capped backoff. Keep launch endpoints, retry behavior, and loader ownership unchanged; explicitly decide the CLOCK_MONOTONIC-failure policy before changing it.
Advice:
- Start with that deterministic host-test seam and preserve current recovery policy; it gives regression coverage without changing payload launch semantics.
- Separately decide and test what the supervisor should do when monotonic time is unavailable. Do not silently force recovery or kill a loader on clock failure; preserve the busy guard until an explicit safe bound/policy is agreed.
- Treat the delayed-util-readiness duplicate-launch concern as a hypothesis. Before changing retries, collect console evidence around the 15-second timeout; if confirmed, design an explicit instance/launch acknowledgement strategy and test that a retry cannot create a second util process.
- Validate recovery on hardware with :9021 absent/present, an unresponsive private loader, a busy launch past its grace, delayed util readiness, and stack shutdown during waits. Do not interpret host policy tests as proof of PS5 ptrace/socket/process behavior.
Risks/blockers: No implementation change made. Clock-failure handling and the inferred delayed-util-readiness duplicate-launch scenario need an owner decision/runtime evidence before any launch/retry behavior is changed.
Next action: Laptop 1 review this read-only report; no additional code work assigned.
Updated UTC: 2026-10-09T21:06:07Z
