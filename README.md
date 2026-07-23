# raptor-loop-hunt

An autonomous, looping, multi-altitude **security vulnerability hunt** for a codebase, packaged as a
Claude Code **skill**. It replaces the model's default "single pass, summarize, stop" behaviour with an
explicit search procedure: traverse every altitude, generate candidates, adversarially verify them
*from raw source*, run isolated parallel reasoners, and keep a persistent ledger so each loop is
net-new coverage instead of rediscovery. In practice it finds far more real bugs than a one-shot scan —
the "Karpathy auto-research" methodology.

It triggers whenever you point Claude at source code and want vulnerabilities found — "audit this",
"find every bug", "security-review it", "find anything exploitable".

## Install

**As a plugin — no clone needed.** Add the marketplace and install; Claude Code fetches it for you:

```
/plugin marketplace add dinosn/raptor-loop-hunt
/plugin install raptor-loop-hunt@raptor
```

Then `/reload-plugins` (or restart). The skill auto-triggers on audit requests, or invoke it directly.

**Or drop it in as a personal skill** — clone into your Claude Code skills directory and it is
discovered automatically on the next session:

```bash
git clone https://github.com/dinosn/raptor-loop-hunt ~/.claude/skills/raptor-loop-hunt
```

## Use

Invoke it explicitly:

```
/raptor-loop-hunt
```

…or just describe an audit task ("audit ./src for security", "find every exploitable bug in this
service") and the skill triggers on its own. Point it at a repo, a service, a module, or a directory.

## What it does

- **Multi-altitude generate → judge → verify loop.** Candidates are generated at every altitude
  (whole-system down to a single function), then *adversarially* judged and verified from raw source —
  never from the generator's own summary.
- **Isolated parallel reasoners** so independent findings don't contaminate one another.
- **Monotonic-scrutiny knowledge base** (`raptor-loop-kb`) that can only ever *raise* scrutiny across
  runs, never lower it — each loop is net-new coverage rather than rediscovery.
- **Disposition ledger** (`raptor-loop-ledger`) — an engagement-scoped state machine that certifies
  every disposition transition (confirm / reject / downgrade / sweep) against an evidence receipt, so a
  real bug can't be quietly rejected and a severe finding can't be laundered down to Low.
- **Execution-auth broker** (`raptor-loop-exec`) — a live-PoC command runs only under a typed,
  least-privilege capability grant.
- **Eval trap battery** (`eval/`) — 11 deterministic and orchestrator-driven axes (recall,
  FP-resistance, false-rejection, counterfeit-evidence, coverage-honesty, severity-downgrade
  integrity, …).

**The prompt is the program.** In an agentic system the model's "algorithm" is whatever you tell it to
be; this skill specifies that algorithm. The quality comes from the structure, not from any single
clever instruction.

## Layout

| Path | Purpose |
|------|---------|
| `SKILL.md` | The skill itself — methodology, altitudes, generate→judge→verify loop, severity rubric. |
| `references/vuln-class-discovery.md` | Leaf-level per-class search procedure (source → sink → oracle → variants). |
| `references/kb-schema.md` | Schema + storage for the monotonic-scrutiny cross-run knowledge base. |
| `references/ledger-schema.md` | Schema + the disposition-time transition gate (candidate ledger + evidence receipts). |
| `scripts/raptor-loop-kb` | Deterministic cross-run KB helper (can only ever raise hunt scrutiny). |
| `scripts/raptor-loop-ledger` | Candidate state machine — certifies each disposition against an evidence receipt. |
| `scripts/raptor-loop-exec` | Execution-auth broker — least-privilege sandbox plan for live PoCs. |
| `scripts/test-*` | Self-contained tests (no network, no external imports). |
| `eval/` | The 11-axis trap battery. |

The scripts are pure-stdlib Python and self-contained; the execution broker will optionally use a host
RAPTOR checkout's sandbox when one is present (`RAPTOR_DIR`), and refuses to run live commands
otherwise.

## Track record

**So far: 200+ verified vulnerabilities across 40+ real-world codebases (Dec 2025 – Jul 2026).**

Run against C network daemons, message brokers, routing stacks, web apps, language runtimes, and media
libraries, the skill has surfaced original, verified vulnerabilities across most of them. Findings are
listed by title and class only; **exploit mechanics and PoCs are withheld**, and outcomes are stated at
their true strength (a proven DoS is not called an RCE; unweaponized primitives are labelled as such).
All have been reported to or communicated with the respective vendor / maintainer.

### Original findings

| Target | Highest verified outcome | Class | Status |
|--------|--------------------------|-------|--------|
| memcached | pre-auth heap buffer overflow, default config, client-reachable | memory-corruption | Fixed upstream, finder credited |
| Immich | unauth restore endpoint → OS-command-exec RCE (container-root) + broken-authorization / IDOR cluster | authz / command-exec | Disclosed |
| RustDesk (client + server) | malicious-host file-transfer path-traversal → RCE on controller; secure-channel / persistent-ID-hijack primitives | path-traversal / crypto | Reported |
| Samba 4.25 (master) | 2× pre-auth memory-corruption crash (DNS TSIG underflow; LDAP type-confusion) — DoS-grade | memory-corruption | Reported |
| Apache Kafka (trunk / 4.0.0) | pre-auth secret / file disclosure + static-init RCE-class primitive | authz / code-load | Reported |
| FRR (master) | pre-auth EIGRP authentication bypass + ASAN-proven FPM stack overflow | auth-bypass / memory-corruption | Reported |
| Redis (unstable / 8.8.0) | pre-auth AUTH/ACL bypass + RCE-grade primitives (not weaponized to full shell) | auth-bypass / memory | Reported |
| Linux XDP zero-copy | OOB over-copy in `xdp_build_skb_from_zc`; root RCE demonstrated on an instrumented rig | memory-corruption | Reported |
| Apache ActiveMQ 5.19.6 | authenticated RCE + multiple authz-bypass / IDOR primitives (incl. a pre-auth connection-kill) | authz / deser | Reported |
| LimeSurvey 7.0.1 | unauth time-based SQL injection + zip-slip → webroot RCE | injection / path-traversal | Reported |
| Apache Axis 1.x / 2 | unauth deserialization RCE (lab-proven) | deserialization | Reported |
| Apache Tomcat 12 (dev) | unauth blind SSRF / DNS-exfil (Medium) | SSRF | Reported |
| Quagga 1.2.3 / 1.2.4 bgpd | pre-auth stack OOB write + use-after-free (DoS-confirmed) | memory-corruption | Reported |
| llama.cpp | pre-auth process-crash DoS (RCE escalation hypothesized, not proven) | memory-safety | Reported |
| Fastjson 1.2.83 | RCE (lab-proven) | deserialization | Public |
| Pillow 12.3.0 | blind-calibration run: rediscovered a real heap-OOB write + 4 net-new ASan-confirmed bugs | memory-corruption | Reported |

**Adversarial rigor, not just recall.** The judge/verify half of the loop is built to *refute*, so it
also kills bad findings: it disproved a fabricated pre-auth RCE exploit package live, and corrected an
over-stated public "ASLR bypass" down to what actually reproduces.

### Public CVEs reproduced / validated

Independent labs, PoCs, and patch analysis — validations of already-public issues, not original
discoveries:

| ID | Component | Class | Notes |
|----|-----------|-------|-------|
| CVE-2026-42588 / -42253 | Apache ActiveMQ | Jolokia → XBean RCE; header-injection / XSS | reproduced end-to-end |
| CVE-2026-15583 | Grafana MCP (`mcp-grafana` ≤ 0.17.1) | confused-deputy SA-token exfil | reproduced; fixed 0.17.2 |
| CVE-2026-20230 | Cisco CUCM 14 / 15 | pre-auth SSRF → Axis 1.4 → JSP webshell | reproduced (in-the-wild chain) |
| CVE-2026-4282 | Keycloak < 26.5.7 | PAR `request_uri` → forged OAuth code → admin token | lab A/B validated |
| CVE-2024-21182 | Oracle WebLogic 12.2.1.4 / 14.1.1 | T3 / IIOP JNDI-injection RCE | reproduced; public lab repo |
| CVE-2025-13673 | Tutor LMS (WordPress) ≤ 3.9.7 | unauth SQLi via `coupon_code` | UNION exploit + Docker lab |
| MDEV-39493 (ZDI-CAN-30333/4) | MariaDB ≤ 11.4.10 | FILE-priv bypass via CTE `INTO OUTFILE` | reproduced; fixed 11.4.11 |
| CVE-2026-6479 | PostgreSQL | pre-auth SSL/GSS unbounded recursion (DoS) | reproduced (needs `-O0` build) |
| ZDI-CAN-30607 | dnsmasq | `find_soa()` out-of-bounds read | ASan-confirmed + 22-site variant sweep |
| CVE-2026-4176 | Perl `Compress::Raw::Zlib` ≤ 2.219 | `crc32_combine` DoS | reproduced; reachability assessed low |
| CVE-2026-27654 | nginx (WebDAV COPY/MOVE) | heap overflow | reproduced; fixed 1.29.7 |
| CVE-2026-42945 (Rift) | nginx | primitive-recovery chain | partial; corrected the public ASLR-bypass framing |
| CVE-2026-35273 | Oracle PeopleSoft (PSEMHUB) | unauth SSRF → XXE → webshell | analysis-only (JAR patch-diff) |

## Scope

For defensive security research, education, and authorized penetration testing only.

---

> Oh — and RAPTOR is required. Obviously. It's right there in the name. 🦖
> Go feed the bird → [github.com/gadievron/raptor](https://github.com/gadievron/raptor)
