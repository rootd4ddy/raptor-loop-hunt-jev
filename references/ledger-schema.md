# Candidate ledger — schema & the transition gate (`raptor-loop-ledger`)

The **engagement-scoped** enforcement layer that makes a disposition a *state transition the
orchestrator certifies*, not a sentence a reasoner emits (SKILL.md, "Disposition receipts"). It is
the disposition-time counterpart to the cross-engagement KB (`references/kb-schema.md`): the ledger
refuses an unproven transition *this run*; `raptor-loop-kb synthesize` folds the durable outcome
across runs. Storage is a run-dir sidecar, **engagement-scoped like `TRIED.md`** — a fresh clone /
new release starts a fresh ledger.

Design principle (gpt-5.6-sol cross-model review): **models propose claims and actions; only the
orchestrator and the execution harness certify completion.** A field a model could fabricate — the
cross-vendor verdict, the oracle result — is populated from an actual recorded job, never from the
reasoner's own text. A weak generator/judge seat states a discipline in prose and skips it; the gate
turns each discipline into a receipt the transition *requires*.

## Storage

```
<OUTPUT_DIR>/ledger/
  ledger.json      # candidates + receipts + pending_actions + cross_vendor_jobs (atomic, locked, fsynced)
  .ledger.lock     # flock file (all writers serialize)
```

Hardening mirrors the KB helper: symlinked path components / `ledger.json` / lock refused;
non-owned root refused; temp-file + `os.replace` + directory `fsync`; lock + inbox `O_NOFOLLOW`;
corrupt JSON refused; every field capped; every durable collection bounded. Pure stdlib.

## The candidate lifecycle

Every generated candidate gets an immutable id at `add` (`C-<sha(finding_sig|component|path)>`),
state `open`. States: `open`, `needs-live-validation` (open, re-drivable), and the terminal set
`confirmed` / `corrected` / `rejected` / `duplicate` / `out-of-scope`. **No candidate leaves the
ledger** — a refused transition leaves it `open`, never dropped. Full `state_history` is kept.

## Transition receipts (the gate; no receipt → no transition)

`raptor-loop-ledger transition --candidate C --to STATE --receipt JSON`. Exit `3` = REFUSED
(candidate stays put, the gap is enqueued as a PENDING action). Required receipts:

- **confirmed / corrected — confirmation receipt:** `oracle_class` ∈ {sanitizer, differential,
  authorization, state-change, disclosure, execution, dos, timing, crypto-failure,
  parser-differential, policy-bypass, race-invariant, other}; `replay_command`; `input_hash`;
  `expected_predicate` (a machine-checkable signal — "looks exploitable" is rejected);
  `observed_artifacts` (≥1). **Crash-inflation guard:** `oracle_class=dos` (a bare crash) cannot
  confirm a memory-safety *exploitability* claim — use a sanitizer-class receipt, or file the DoS
  impact. A confirmed memory-safety bug auto-enqueues a `dirty_sweep`.
- **rejected — rejection receipt:** `rejection_reason` ∈ {unreachable, non-exploitable,
  expected-behavior, duplicate, out-of-scope}; a `counter_hypothesis`; a `vector_matrix` where each
  row is `{vector, status}` and `tested` → `command_or_fixture` + `observed`, `not-applicable` →
  `rationale` (a bare list proves nothing; `enc` is a transform dimension, not a vector); for
  `unreachable`, a `gating` block (`build_digest`, `config_digest`, `route_or_symbol_evidence`); and
  `independent_review.job_id` referencing a **recorded** cross-vendor job whose `verdict` is
  `uphold` (an overturn / inconclusive / unknown job REFUSES the kill).
- **needs-live-validation — validation receipt:** `safe_test` (exact request/command + expected
  vulnerable-vs-safe response) and `potential_severity`. First-class, not a soft reject;
  auto-enqueues a `live_validation`.
- **material downgrade — `material_downgrade_receipt`:** a *severity* downgrade of a severe finding is
  a partial rejection and carries the rejection burden. It is **DERIVED, never a caller-selected
  label**: fires when the candidate's append-only `potential_hwm` ∈ {high, critical} AND the target is
  an effective-final Low/hardening (`corrected` with `effective_severity ≤ low`, or
  `duplicate`/`out-of-scope` without a `canonical_finding` that preserves it). The high-water-mark is
  retained, so `High→Medium→Low` still trips. `rejected` (own gate), `confirmed` (oracle-gated), and
  `needs-live`/`open` (kept alive) are exempt. The receipt needs `negated_hypothesis`,
  `consequence_trace`, a bounded `trigger_paths[]` (each `{path_id, status ∈
  tested|static-cleared|needs-live|needs-review, observed|evidence}` — a `needs-live`/`needs-review`
  path is NOT cleared, so it refuses), `gating` digests, and `independent_review.job_id` referencing a
  recorded cross-vendor job whose `verdict` is **`concur_downgrade`** (an
  `overturn`/`inconclusive`/`uphold` refuses). Miss any → transition fails, candidate stays put, an
  `incomplete_material_downgrade → complete_material_downgrade_bundle` action is enqueued.

## Cross-vendor jobs (the trusted channel)

`raptor-loop-ledger cross-vendor --job '{"model":..,"target":..,"verdict":"uphold|overturn|inconclusive|concur_downgrade","evidence_hash":..}'`
records an orchestrator-dispatched review as `J-<hash>`. A rejection receipt cites a job whose verdict is
`uphold`; a **material-downgrade** receipt cites one whose verdict is `concur_downgrade`. Either way the
reasoner cannot invent the verdict — it lives in a separate recorded step the orchestrator (not the
judged reasoner) writes.

## PENDING actions (the follow-up that cannot vanish)

Auto-enqueued from *events*, never from a model remembering: `memory_bug_confirmed → dirty_sweep`,
`server_dependency → live_validation`, `incomplete_rejection → complete_rejection_bundle`,
`incomplete_material_downgrade → complete_material_downgrade_bundle`, and `severe_sink_hypothesis →
enumerate_trigger_paths`. The last is **sink-keyed** (id `S-<hash(sink)>`), shared by every candidate
reaching that sink and carrying a `candidates[]` list, so the sink's ingress set is enumerated once
across findings — closing it needs an `enumeration_recipe` (route/reference/dispatcher/loader searches,
not a hand-picked file list) + a `trigger_paths[]` inventory, which is what stops a re-sweep from
silently omitting the files that reach the sink. A `dirty_sweep` closes only with a valid sweep receipt
(`enumeration_recipe` + `discovered_sites` + `site_set_hash` — a grep-only sweep with no hash is
refused). `file-close --file F` is REFUSED while a `dirty_sweep` for a candidate under `F` is open;
`summary.report_blocking` is true while any pending action is open, gating the report.

## CLI

- `init --ledger DIR --target DIR` — create, stamp identity.
- `add --ledger DIR --target DIR (--record JSON | -)` — register candidate (idempotent by signature).
- `cross-vendor --ledger DIR (--job JSON | -)` — record a dispatched review job.
- `transition --ledger DIR --target DIR --candidate C --to STATE (--receipt JSON | -)` — the gate.
- `pending-complete --ledger DIR --action A (--receipt JSON | -)` — close a pending action.
- `file-close --ledger DIR --file PATH` — refused while a sweep is open.
- `summary --ledger DIR` — report-ready one-line refs, open pendings, `report_blocking`.
- `conformance --ledger DIR [--target D --inventory F --covered F --omitted F --claims F]` — the
  deterministic **closure gate**: machine set-diffs (inventory − covered − omitted; terminal-state
  candidates missing their receipt; dirty-confirmed with no sweep; **a severe (High/Critical
  high-water-mark) finding sitting at effective-Low with no material-downgrade receipt** — catches a
  direct-edit / stair-step / alias-disposition bypass of the transition gate; open pendings; receipts
  whose `certified_commit ≠ HEAD`; a claims manifest that contradicts ledger state). Exit 3 = any diff
  non-empty. This is the "verify claims, not just re-read your own work" pass — coverage, rejection, and
  downgrade claims are checked, not only `confirmed` ones, because those are the claims that *suppress* work.

Self-test: `python3 scripts/test-raptor-loop-ledger` (44 assertions; no network, no RAPTOR imports).
Eval battery + fixtures: `eval/README.md` (the 11-axis trap suite; deterministic axes are these tests,
live-model axes are orchestrator-driven).

## Why this is safe / what it is not

The ledger only ever *refuses* an unproven transition; it never suppresses a candidate (a refusal
leaves it `open` for the next round) and holds no cross-engagement state. It enforces **structural
integrity** — a rejection cannot cite a nonexistent or non-uphold cross-vendor job, a confirmation
cannot pass without a machine predicate — not cryptographic non-repudiation: the orchestrator is
trusted to record true job verdicts. The value is moving the fabricable fields out of the reasoner's
freeform text into separate, checkable, re-runnable steps.
