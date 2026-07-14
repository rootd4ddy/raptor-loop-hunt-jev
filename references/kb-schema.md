# Cross-run Knowledge Base — schema & storage (`raptor-loop-kb`)

The KB is a **monotonic-scrutiny** cross-engagement layer above the `TRIED.md`/`FINDINGS.md` ledger. It can
only ever RAISE hunt effort — it stores no coverage and no "safe" signal, does no deprioritization, and never
excludes a candidate. All state is owned by the deterministic helper `scripts/raptor-loop-kb`
(`identity` / `append` / `reflect` / `synthesize` / `load`).

## Storage location, keying, hardening

- **Active `/project` (preferred):** `<project.output_dir>/kb/` — RAPTOR-owned, **outside the scanned target
  tree**. The helper **refuses** a KB root that resolves inside the target (`assert_safe_kb_root`).
- **No project:** `out/kb/<target_path_id>/`, `target_path_id = sha256(realpath(target))[:16]` (a directory
  key, not a freshness signal — freshness is the full identity below).
- **Hardening (helper-enforced):** every KB path component lstat-checked for symlinks (no follow); symlinked
  `kb_root`/`kb.json`/`learnings.jsonl`/`.kb.lock` refused; KB-inside-target refused; ownership checked;
  `kb.json` written temp-file + `os.replace` + directory `fsync`; lock + inbox opened `O_NOFOLLOW`; corrupt or
  structurally-invalid `kb.json` refused (never silently accepted); reads byte-capped; every durable
  collection bounded; `learnings.jsonl` appended only via the locked `append` subcommand and rotated aside
  under the lock before folding (no append/clear race).

```
<kb_root>/
  kb.json                  # durable monotonic KB — single source of truth (snake_case)
  learnings.jsonl          # inbox — append-only via the locked `append`; rotated+folded by synthesize
  .fold.<pid>.<ms>.jsonl   # transient fold-file (rotated inbox); re-ingested on crash
  .kb.lock                 # flock file (all writers serialize)
```

## Target identity (freshness; drift dependencies)

Computed fresh every run (`raptor-loop-kb identity`): `target_path_id`, `vcs_commit`, `dirty_tree_digest`
(content-based: `git diff HEAD` + untracked file contents), `inventory_digest` (fresh component list),
`build_config_digest` + `dependency_digest` (recursive — nested poms/lockfiles), `release_label`,
`methodology_version`. A prior rejection stores the identity **under which it was adjudicated**; drift stales
it on ANY divergence (new commit / new module / changed lockfile / changed dirty content), plus wall-clock
(`EXPIRY_DAYS=90`) and `methodology_version`. Aggressive-by-design: "stale" means *more* rechecking, which is
safe under monotonicity. Drift is recomputed at BOTH synthesize and `load` (load never trusts the stored
status), fail-conservative.

## `learnings.jsonl` — the inbox (append via the locked `append`; rotated by synthesize)

```jsonc
// STEERING — written by the orchestrator (typed; validated against the fresh inventory)
{"kind":"finding_outcome","run_id":"<run id>","commit":"<sha>","component":"modules/report",
 "disposition":"confirmed|corrected|rejected|needs_live_validation","finding_sig":"<stable id>",
 "path":"modules/report/filter.py","entry_point":"GET /report?q","sink":"db.execute",
 "violated_invariant":"unsanitized q reaches SQL","bug_class":"CWE-89",
 "delivery_vectors_tested":["query","body","enc"],"cross_vendor_result":"gpt-5.6 concurred reject",
 "evidence_spans":[{"role":"source|caller|route|mitigation","file":"modules/report/filter.py",
                    "start_line":1,"end_line":3}]}
// confirmed/corrected -> component ever_dirty (hunt-first). rejected -> a prior ONLY with the full bundle
// (path-under-root, entry_point, sink, violated_invariant, >=1 delivery vector, >=1 typed evidence span,
// cross_vendor_result), else -> open_item. needs_live_validation -> open_item.
// There is NO `attempt`/coverage kind and NO `solid` kind — the KB never lowers scrutiny.

// TELEMETRY — written by `reflect` (display-only; NEVER steers state)
{"kind":"telemetry","run_id":"…","insight_type":"tool_error|gave_up","display_only":true,"detail":"<capped>"}
{"kind":"run_signal","run_id":"…","terminated_by":"budget_exceeded","display_only":true, …}
```

**Stable finding signature.** `finding_sig` is authored over *canonical relative path + function/symbol +
entry point + sink + violated invariant + bug class + delivery vector*. Used only to dedup durable records; it
never excludes a candidate and a shared signature never suppresses a changed-code variant.

## `kb.json` — the durable monotonic KB

```jsonc
{
  "schema_version": 3, "identity": {…8 fields…}, "runs_folded": ["hunt-1"],   // runs_folded = audit only
  "components": {                                  // ONLY scrutiny-raising flags; NO coverage
    "modules/report": {"ever_dirty": true},        // from a confirmed/corrected finding -> hunt FIRST
    "modules/auth":   {"has_prior_rejection": true}},  // -> recheck annotation
  "vuln_classes": {"CWE-89": {"watch_components": ["modules/report"]}},
  "prior_rejections": [                            // SCOPED PRIORS — annotation source, never suppression
    {"kind":"prior_rejection","finding_sig":"…","component":"modules/report","path":"…","entry_point":"…",
     "sink":"…","violated_invariant":"…","bug_class":"CWE-89","delivery_vectors_tested":["query"],
     "cross_vendor_result":"…",
     "evidence_spans":[{"role":"source","file":"…","start_line":1,"end_line":3,"sha":"…"}],
     "adjudicated_identity":{…8 fields…},"synthesized_at":1752…,"methodology_version":"kb-v3",
     "drift_status":"fresh|stale"}],
  "open_items": [ … needs_live + incomplete rejections — re-driven, never suppressed … ],
  "telemetry": [ … bounded display-only ring … ]
}
```

There is **no coverage field and no `solid_by_design`**. Current-run coverage is always computed fresh from
`TRIED.md`; the KB never contributes to it.

## Helper CLI

- `raptor-loop-kb identity --target DIR` → target-identity JSON.
- `raptor-loop-kb append --kb KB --target DIR [--record JSON | --file F | -]` → locked JSONL append to the
  inbox (validates JSON). Both telemetry and steering appends route through here.
- `raptor-loop-kb reflect RUN_DIR --kb KB --target DIR [--max-chars N]` → mines `RUN_DIR/trajectories/*/
  trajectory.json` into display-only telemetry, locked-appends them. Never full-reads a transcript.
- `raptor-loop-kb synthesize --kb KB --target DIR --inventory FILE --run-id R` → rotate+fold (crash-tolerant,
  incremental), schema-validate, drift-decay, atomic write, clear inbox.
- `raptor-loop-kb load --kb KB --target DIR --inventory FILE` → planner-only payload (priority-ordered, every
  `current_state:"uncovered"`; recheck annotations with drift recomputed here). Refuses unsafe KB paths on
  every subcommand.

## Security contract (why this is safe)

Every signal the KB can emit only RAISES scrutiny: mark a component hunt-first, or annotate "recheck this and
every delivery vector." It stores no coverage and no "safe" signal, so it **cannot semantically suppress,
cover, or clear a candidate**. Given the fresh inventory is accepted in full (the helper fails closed on any
inventory cap/invalid entry) and the completeness gate is honoured, the KB changes only *scheduling* and adds
*recheck* work. Only structured, locally-recomputed facts steer state (component identity from fresh
enumeration; disposition from typed schema-validated records; evidence spans resolved under the canonical
target root); free text (trajectory `final_summary` / "gave-up" / tool-error prose) is display-only telemetry
that never reaches a state transition. The payload is planner/orchestrator-context only — never fed to the
independent generator, judge, or live-verifier.
