# Jev planner/control-plane integration

This fork integrates TypeSafe Jev as an optional **planner advisor** for `raptor-loop-hunt`. The integration is intentionally narrow: Jev helps choose and route bounded next actions, while the RAPTOR candidate ledger, evidence receipts, deterministic closure gate, and raw-source generate -> judge -> verify chain remain authoritative.

## Why Jev belongs here

Jev is a System One decision model: application state plus typed questions in, typed judgments and probabilities out. That maps well to the orchestration decisions RAPTOR repeatedly makes *after* it has already enumerated a bounded action set: which action to run next, whether a deterministic check should precede another reasoning call, and which *independent additive follow-ups* apply to a completed worker/explorer result.

Jev does **not** replace the creative generator, skeptical judge, live verifier, boundary scout, specialist reasoner, or ledger. It is the scheduler/router between those seats.

## Authority boundary (load-bearing)

Jev is **planner-only and non-dispositive**.

It may:
- recommend ordering among already-enumerated planner actions;
- score expected information gain;
- estimate duplication risk;
- recommend deterministic-first work;
- route a completed result into one or more independent additive follow-ups.

It may never:
- confirm or correct a finding;
- reject, duplicate, or mark a finding out of scope;
- downgrade severity;
- mark a component/cell covered;
- suppress a candidate, delivery vector, pending action, bug class, or coverage cell;
- mark code safe;
- close a RAPTOR pending action;
- satisfy any confirmation, rejection, downgrade, sweep, handoff, or conformance receipt.

Those transitions remain owned by `raptor-loop-ledger` and the existing evidence contract. A wrong Jev recommendation may waste scheduling effort, but must not be able to create a security false negative.

## Isolation and data minimization

`scripts/raptor-loop-jev` uses a strict allowlist projection. Do not serialize the agent's whole context into Jev state.

Allowed planner state is limited to bounded metadata such as run/round/altitude/bug-class labels, coverage counts, named pending actions, short current-run tried summaries, bounded action IDs/summaries/evidence gaps, and bounded completed-result summaries.

Explicitly excluded: raw source, secrets, credentials, API keys, PoC bytes, arbitrary target files, full command/tool transcripts, hidden reasoning, and unbounded model output.

This preserves RAPTOR's raw-source isolation: generator, judge, and verifier still reason from the evidence they are supposed to see, not from planner history or Jev conclusions.

## Modes

`RAPTOR_JEV_MODE` controls adoption:

| Mode | Effect |
|---|---|
| `off` | Baseline RAPTOR; no Jev call. |
| `shadow` | Call/log Jev, but do not change scheduling. **Default.** |
| `advisory` | Surface the recommendation to the orchestrator; the orchestrator still chooses. |
| `reorder` | Reserved active-routing mode. It remains fail-closed unless `RAPTOR_JEV_REORDER_ENABLE=1`; mandatory ledger/coverage work always wins. |

Do **not** promote from `shadow` just because calls succeed. Every decision must be correlated to what baseline RAPTOR actually chose and what happened afterward. Use `observe` events (below) and evaluate representative runs. Do not invent a universal confidence threshold; calibrate on the hunt workload. `reorder` is deliberately disabled by default even when the mode is requested.

## API configuration

The helper is pure-stdlib Python and calls TypeSafe's HTTP API directly, preserving the upstream repo's self-contained helper design.

Environment variables:
- `TYPESAFE_API_KEY` - TypeSafe API key.
- `TYPESAFE_BASE_URL` - optional; defaults to `https://api.typesafe.ai`.
- `TYPESAFE_DEFAULT_MODEL` - optional; defaults to `jev-latest`.
- `RAPTOR_JEV_MODE` - `off|shadow|advisory|reorder`.
- `RAPTOR_JEV_TIMEOUT` - timeout per HTTP attempt in seconds; default 10.
- `RAPTOR_JEV_REORDER_ENABLE` - explicit opt-in (`1|true|yes`) required before non-mandatory active reorder can occur.

The request target is `POST /v1/systemone` with bearer authorization. Missing credentials, timeouts, transient service failures, or malformed responses are **non-blocking**: the helper exits 4 and the hunt continues with baseline RAPTOR behavior. The API key is never written into a receipt.

## Decision 1: choose the next bounded action

First enumerate the candidate action set in ordinary RAPTOR code/model logic. Jev cannot invent an action that was not supplied. Keep the action set bounded (the helper caps it at 16).

Invoke:

    python3 scripts/raptor-loop-jev next-action \
      --state @planner-state.json \
      --receipt-log "$OUTPUT_DIR/planner/jev-decisions.jsonl"

The adapter intentionally keeps this request smaller than v1:
- `Choice`: best next action, with an explicit `none_of_these` outcome so Jev is never forced to pick a bad action;
- `Score`: expected information gain for each action;
- `Noul`: duplication risk for each action;
- `Noul`: deterministic-first for each action.

Executor-role and frontier-needed questions are **not fanned out over every action** in v2; the first real run showed that doing so consumed thousands of unnecessary input/output tokens. RAPTOR keeps its existing role selection until a narrower second-stage decision is justified by calibration.

Probabilities are planner signals, not permission to disposition a candidate.

## Decision 2: route a completed result

After a deterministic tool or subagent returns, provide only a bounded planner-visible summary and invoke:

    python3 scripts/raptor-loop-jev route-result \
      --state @result-routing-state.json \
      --receipt-log "$OUTPUT_DIR/planner/jev-decisions.jsonl"

The route dimensions are deliberately **independent Noul judgments**, not one `Choice`. Several may legitimately apply at once:
- `open_or_join_candidate`
- `attach_planner_evidence`
- `run_deterministic_check`
- `escalate_frontier`
- `needs_more_evidence`
- `possible_duplicate_review`

This is load-bearing: a result can require both a deterministic check **and** later frontier review while also being candidate-worthy. TypeSafe's own primitive guidance says to use one Noul per label when several labels may apply. v1 incorrectly forced these compatible routes through one Choice; v2 removes that information loss. Result-route reorder remains disabled until these signals are calibrated.

There is intentionally no `reject`, `confirm`, `safe`, `covered`, or severity decision. `possible_duplicate_review` merely schedules the ordinary same-engagement dedup check; it never drops the branch itself.

## Mandatory-work precedence

Before calling Jev, mark mechanically required work as `mandatory: true`: open ledger pending actions, coverage/completeness obligations, required independent reviews, conformance failures, dirty sweeps, handoff blockers, or other already-derived gates.

In `reorder` mode the helper deterministically selects the highest-priority mandatory action even if Jev recommends something else, and records `mandatory-work-precedence` as the override. For non-mandatory work, active selection remains disabled unless `RAPTOR_JEV_REORDER_ENABLE=1`. Jev therefore cannot starve a closure obligation or silently become active merely because someone changed the mode string.

## Planner receipts and closed-loop calibration

Every live decision now appends an auditable JSONL record to `<OUTPUT_DIR>/planner/jev-decisions.jsonl` containing:
- a stable `decision_id`;
- the **safe projected action/result context**, not merely opaque digests;
- state/request digests;
- decision kind/mode and typed probabilities;
- requested/used model and token usage;
- HTTP latency, attempt/retry count, status, and request id when supplied;
- mandatory-work overrides and hard authority guards.

API failures are logged as `decision_failure` events too, while RAPTOR continues with baseline scheduling.

Shadow mode is useful only if the recommendation is correlated with what RAPTOR actually did. After baseline scheduling/execution, append an outcome observation:

    python3 scripts/raptor-loop-jev observe \
      --decision-id J-... \
      --record '{"baseline_selected_action":"A2","executed_action":"A2","gate_closed":true,"deterministic_sufficient":false,"frontier_reasoning_used":true}' \
      --receipt-log "$OUTPUT_DIR/planner/jev-decisions.jsonl"

Supported observations include baseline/executed action, executed routes, result class, whether a gate closed or candidate opened, whether deterministic work sufficed, whether frontier reasoning was actually used, and bounded tool/agent ids/notes. Unknown fields are dropped.

This sidecar is **not** the candidate ledger and is not evidence for a disposition. It is evaluation data. Promotion decisions must be based on matched decision/outcome records, not Jev confidence alone.

## Failure asymmetry

Availability fails open; authority fails closed:
- missing TypeSafe key -> continue baseline RAPTOR;
- timeout/transient API error -> continue baseline RAPTOR;
- malformed Jev response -> continue baseline RAPTOR;
- Jev asks to skip mandatory work -> deterministic policy overrides it;
- Jev suggests a terminal security disposition -> unsupported vocabulary; ignore it.

Jev availability may affect efficiency, never the epistemic safety case of the hunt.

## Tests

Run:

    python3 scripts/test-raptor-loop-jev

The test is network-independent: it uses a local fake System One endpoint to cover state projection, typed question construction, missing-key fallback, modes, bearer handling without key leakage, receipt logging, mandatory-work precedence, transient retry, and the non-dispositive route vocabulary.
