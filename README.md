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
| `scripts/raptor-loop-kb` | Deterministic cross-run KB helper (can only ever *raise* hunt scrutiny, never lower it). |

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
