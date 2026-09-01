# Fresh-operator handoff and intervention ledger

This is the owner-delivery extension to the technical finding ledger. It catches a different
failure class from clean reset: the machine may be clean while the researcher still carries tacit
knowledge about retries, timing, missing dependencies, privileged setup, disabled controls, or
manual repairs.

The extension keeps two outcomes separate:

- `report_blocking` concerns technical candidate and evidence closure.
- `delivery_blocking` concerns whether a fresh operator can reproduce the packaged PoC.

A technically `confirmed` finding stays confirmed when handoff fails. The package is simply not
owner-ready until its independent replay succeeds. Conversely, a polished handoff does not upgrade
a weak technical claim.

## When the gate is required

Use the extension for any PoC, lab, reproduction bundle, or remediation retest package intended for
an application owner or another team. Exploratory hunts should still record interventions as they
happen, but they need not run the final handoff until a deliverable exists.

For an owner-facing delivery, run conformance with `--require-handoff`. Without that option,
technical conformance remains backward-compatible and independent of delivery readiness.

## Record every intervention

An intervention is any step that was not part of the intended procedure, including a retry pattern,
manual state repair, dependency install, remembered path or port, privilege change, disabled
mitigation, target patch, harness patch, or undocumented configuration change.

```bash
scripts/raptor-loop-ledger intervention \
  --ledger "$OUTPUT_DIR/ledger" \
  --target "$TARGET" \
  --record '{
    "stage": "setup",
    "unexpected_condition": "the required parser package was absent",
    "intervention": "installed the package manually before rerunning",
    "candidate_id": "C-optional",
    "classification": "documentation-defect",
    "documented_before_run": false,
    "required_for_success": true,
    "changed_target_semantics": false,
    "changed_security_controls": false,
    "operator_knowledge_required": true,
    "evidence": ["handoff/setup.log:12"]
  }'
```

`classification` is one of:

- `target-defect`
- `harness-defect`
- `documentation-defect`
- `environment-mismatch`
- `operator-error`
- `unresolved`

Interventions are immutable observations. Resolve one with a separate receipt:

```bash
scripts/raptor-loop-ledger intervention-resolve \
  --ledger "$OUTPUT_DIR/ledger" \
  --target "$TARGET" \
  --intervention I-... \
  --record '{
    "resolution": "documented",
    "evidence": ["README.md:18"],
    "instructions_artifact": "README.md#sha256:..."
  }'
```

Resolution receipts are append-only. If the state later changes—for example, a documented lab
limitation is eliminated—record a new resolution. It becomes the current resolution without deleting
the earlier evidence, and it forces another cold replay.

Resolution vocabulary and its required binding field:

| Resolution | Required field | Meaning |
|---|---|---|
| `documented` | `instructions_artifact` | The required step is now in the operator procedure. |
| `eliminated` | `verification_artifact` | The package no longer needs the workaround. |
| `matches-claimed-deployment` | `deployment_evidence` | The intervention reflects the real claimed deployment rather than a research-only alteration. |
| `documented-lab-boundary` | `limitations_artifact` | A deliberate lab-only semantic difference is explicitly disclosed. |
| `accepted-blocker` | `blocker` | The issue remains unresolved and delivery stays blocked. |

A required undocumented intervention blocks delivery until it is documented, eliminated, or bound
to the claimed environment. An intervention that changes target semantics or security controls is
stricter:

- An `exact-target` claim accepts only `eliminated` or `matches-claimed-deployment`.
- A `declared-lab` claim may also accept `documented-lab-boundary`.

Simply documenting a disabled mitigation does not turn a modified target into exact-target proof.

## G5b: fresh-operator handoff review

Give an uninvolved reviewer only the immutable package, its declared prerequisites, and its
instructions. Use a new environment without the author's warm caches, shell history, saved
credentials, helper scripts, or remembered recovery steps. The author may observe but must not
coach.

Record the attempt whether it succeeds or fails:

```bash
scripts/raptor-loop-ledger handoff-review \
  --ledger "$OUTPUT_DIR/ledger" \
  --target "$TARGET" \
  --record '{
    "reviewer": "fresh-reviewer-job-17",
    "reviewer_independent": true,
    "package_digest": "sha256:...",
    "environment_digest": "sha256:...",
    "instructions_artifact": "README.md#sha256:...",
    "author_assistance": false,
    "claim_scope": "exact-target",
    "target_equivalence": "exact-target",
    "success_marker": {
      "expected": "marker file contains RAPTOR_OK",
      "observed": "marker file contains RAPTOR_OK",
      "passed": true
    },
    "negative_control": {
      "expected": "patched/control build does not create the marker",
      "observed": "marker absent",
      "passed": true
    },
    "observed_artifacts": ["handoff/run.log#sha256:..."]
  }'
```

`claim_scope` is `exact-target` or `declared-lab`. `target_equivalence` is `exact-target`,
`declared-lab`, `modified-target`, or `unknown`.

The ledger derives, rather than trusts, `delivery_readiness`:

- `ready`: independent, uncoached replay; exact marker and negative control pass; target equivalence
  matches the claim; no blocking intervention remains.
- `needs_documentation`: author help or a required undocumented operator step remains.
- `needs_environment_fix`: a required harness/environment workaround remains.
- `blocked`: no review, failed marker/control, target-equivalence mismatch, unresolved semantic or
  security-control change, accepted blocker, or structurally invalid stored receipt.

A newly recorded intervention or resolution invalidates an older review so the changed package or
instructions must be replayed. The summary and conformance
commands always recompute readiness from the latest handoff plus the complete current intervention
set.

## Owner-delivery closure

```bash
scripts/raptor-loop-ledger summary --ledger "$OUTPUT_DIR/ledger"

scripts/raptor-loop-ledger conformance \
  --ledger "$OUTPUT_DIR/ledger" \
  --target "$TARGET" \
  --inventory "$OUTPUT_DIR/inventory.txt" \
  --covered "$OUTPUT_DIR/covered.txt" \
  --omitted "$OUTPUT_DIR/omitted.txt" \
  --claims "$OUTPUT_DIR/claims.json" \
  --require-handoff
```

`--require-handoff` adds the `fresh_operator_handoff` conformance check and refuses owner-ready
delivery unless the derived state is `ready`. On Git targets, it also binds the latest handoff
receipt to the current commit so a package replayed before the target changed cannot satisfy the
gate.

The owner report should show the technical verdict and delivery readiness as separate lines. Never
rename a handoff failure into a technical refutation, and never hide a technically unresolved
candidate behind a successful handoff.
