# raptor-loop-hunt

A Claude Code **skill** implementing the RAPTOR *autonomous, looping, multi-altitude*
security vulnerability hunt — the "Karpathy auto-research" methodology that finds far more
bugs than a single-pass scan by generating candidates, adversarially judging them, and
looping across altitudes until coverage converges.

It is the default approach whenever you point Claude at source code (a repo, service, app,
module, or directory) and want vulnerabilities found — "audit this", "find every bug",
"security-review it", "find anything exploitable".

## Layout

| Path | Purpose |
|------|---------|
| `SKILL.md` | The skill itself — methodology, altitudes, generate→judge→verify loop, severity rubric. |
| `references/vuln-class-discovery.md` | Leaf-level per-class search procedure (source → sink → oracle → variants). |
| `references/kb-schema.md` | Schema + storage for the monotonic-scrutiny cross-run knowledge base. |
| `references/ledger-schema.md` | Schema + the disposition-time transition gate (candidate ledger + evidence receipts). |
| `scripts/raptor-loop-kb` | Deterministic cross-run KB helper (can only ever *raise* hunt scrutiny, never lower it). |
| `scripts/raptor-loop-ledger` | Engagement-scoped candidate state machine — certifies each disposition transition against an evidence receipt (confirmations, rejections, sweeps, PENDING actions). |
| `scripts/test-raptor-loop-ledger` | Self-contained test for the ledger + conformance gate (30 assertions; no network, no RAPTOR imports). |
| `scripts/raptor-loop-exec` | Execution-auth broker — a live-PoC command runs only under a typed capability grant (least-privilege sandbox plan; `--exec` via RAPTOR's `core.sandbox` when reachable, else refuse). |
| `scripts/test-raptor-loop-exec` | Self-contained test for the broker (11 assertions; authority compliance). |
| `eval/` | The 10-axis trap battery (recall, FP-resistance, false-rejection, counterfeit-evidence, sweep, pending, coverage-honesty, authority, staleness, ablation) — deterministic axes are the tests, live axes are orchestrator-driven fixtures. |

## Install

Drop the directory into your Claude Code skills folder:

```bash
git clone <this-repo-url> ~/.claude/skills/raptor-loop-hunt
```

Claude Code discovers it automatically on the next session. Invoke it with `/raptor-loop-hunt`
or just describe an audit task and let the skill trigger.

## Collaborating

This repo *is* a live skill working copy. After a teammate pushes changes, pull them in place:

```bash
cd ~/.claude/skills/raptor-loop-hunt && git pull
```

## Scope

For defensive security research, education, and authorized penetration testing only.
