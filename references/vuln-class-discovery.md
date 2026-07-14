# Vulnerability-Class Discovery Methodology

Leaf-level reference for `raptor-loop-hunt`. The loop tells you the *search procedure* (altitudes,
generate→judge, ledger, stop condition). This file supplies, per **vulnerability class**, the
stack-agnostic *discovery method* — how to recognize, enumerate, and confirm instances of that class
in **any** language or framework. It is deliberately **not** a catalog of product/framework/version
signatures (those rot, are version-specific, and duplicate Semgrep + `/sca`). Product names, if they
appear at all, are illustrations of a *mechanism*, never the substance.

Load the relevant class when its bug-class lens is active. Every discovered instance still has to
clear the finding contract (attacker-controlled entry → dangerous sink, root cause, concrete attack,
severity) and independent live-verify. These are discovery seeds, not a checklist to pad a report.

Scope note: RAPTOR's research focus is RCE / auth-bypass / data-integrity classes. Pure
availability / resource-exhaustion (algorithmic-complexity, decompression bombs, entity-expansion
DoS) is out of scope by default — noted where a class has such a facet, not hunted as a headline.

---

## The engine — every class has the same five parts

Discovery of any class reduces to the same shape. Fill these five slots for the class in hand:

1. **Source class** — the untrusted origins, defined by the **trust boundary crossed**, not by an API
   name. Always includes the *second-order* source: data stored earlier from an untrusted origin and
   trusted on the way out, and trust-crossing responses from another component. (For a few classes the
   trigger is not attacker-*input* but attacker-*influenced state or timing* — the slot is labelled
   accordingly rather than forced.)
2. **Sink class** — defined by the **capability/mechanism** the operation grants, not by a memorized
   function list. You recognize a sink by what it *does* (parses a sub-language, acts on a
   principal's resource, renders into a context, reconstructs an object, frees memory), so the method
   survives new APIs.
3. **Violated invariant** — the **security property** that must hold and is broken, stated as a
   property, not as a prescribed fix. This *is* the root-cause template for the finding:
   *"`<unit>` does not `<preserve property>`, allowing `<consequence>`."* Specific controls
   (parameterization, encoding, constant-time compare) are *ways to achieve* the property, cited as
   illustration — the invariant is the property itself.
4. **Enumeration strategy** — how to derive the **complete** sink set from *this* codebase's own
   inventory/map (not a fixed list), then trace each candidate back to a source class. Completeness
   here is what separates a real audit from a hot-spot sample.
5. **Confirmation oracle** — the class-appropriate proof that turns a hypothesis into a finding
   (differential behavior, authz-trace, sanitizer, invariant-assert). A bare crash or "it looks
   wrong" is not an oracle.

**Reachability rule (all classes).** A "not reachable / gated / needs non-default config" claim that
would *kill* a candidate carries the finding's own proof burden — verify it against the observable
build + default config first.

**Two-axis severity under partial visibility (all classes).** When the only barrier between a
candidate and exploitation is a layer you cannot observe (a server behind client code, a runtime you
can't run, a service out of scope), rate **two** numbers, never one collapsed number:
*confirmed severity* (what the artifacts in hand prove) and *potential severity* (the worst case
**if** the unobserved assumption is insecure). Carry the finding as **`needs-live-validation`** with
the potential severity and an exact, safe test (request/command + expected vulnerable-vs-safe
response). Do **not** collapse potential severity to Low merely because the confirming layer is
unseen — and do **not** presume reachability either; potential severity is explicitly conditional.
This is the single most common false-negative in refute-mode, so it is a rule, not a preference.

---

## Injection into a sub-interpreter (SQL, shell, template, expression, LDAP, XPath, ORM-raw)

- **Source class:** any value crossing inward — request line/params/body/headers/cookies, uploaded or
  imported content, values previously *stored* from untrusted input (second-order), decoded blobs.
- **Sink class (by mechanism):** any site where input is composed into a string that a **second
  interpreter** then parses for *structure*. Recognition test: "is another language parsed here?" —
  independent of the specific call.
- **Violated invariant:** untrusted input never alters the **grammar/structure** the sub-interpreter
  parses — it stays in the data plane, not the control plane. Binding as inert data (parameterization)
  or allowlisting the structural tokens *achieves* this reliably; ad-hoc escaping achieves it only if
  provably correct for that exact grammar and context, so treat blocklist filtering as bypassable
  until shown otherwise.
- **Enumeration:** from the inventory, list every sub-language the system embeds; for each, enumerate
  **every** construction site and trace inputs to a source class.
- **Variant / second-order:** a bound value later re-composed inside a stored proc or dynamic
  fragment (second-order); a **structural** token (identifier / sort key / operator / CLI flag /
  option) that is input-controlled and therefore cannot be parameterized (must allowlist); one
  ingress sanitized while a sibling ingress (header/cookie/env/config) reaches the same sink; a value
  that reaches a **second** interpreter after the first (decoded → evaluated).
- **Oracle:** differential — a structure-breaking input observably changes interpreter behavior
  (error / timing / result set / side effect) while an equivalent inert input does not; or a trace
  proving the input reaches the structural position unbound.

## Output-context injection (XSS / response-splitting / log & header injection)

- **Source class:** as above, plus any value *reflected* into a response or *rendered* from storage.
- **Sink class:** any point where input is emitted into an **output context** that a downstream
  parser (browser DOM, HTTP framing, log processor) interprets — HTML body, attribute, URL, JS
  string, CSS, response header/status line, log record.
- **Violated invariant:** untrusted input cannot introduce **unintended structure or parser-control**
  in the output context it lands in. Context-correct encoding is the usual control; a safe structured
  builder, a proven sanitizer, or a scheme/character allowlist can also satisfy it — encoding for the
  *wrong* context (an HTML-escaped value dropped into a JS string, `href`, or event handler) does not.
- **Enumeration:** enumerate every sink by output context, not by template; for each, identify the
  context at the point of interpolation and check the control matches that context. Include DOM sinks
  fed by client-only sources (fragment, referrer, name, postMessage) the server never sees.
- **Variant:** stored (rendered later elsewhere), context-mismatch (right escape, wrong context),
  sanitize-once then re-render/normalize, and mutation via a second pass.
- **Oracle:** a context-appropriate breakout payload observably changes the parsed structure
  (script executes / header splits / log record forges), not merely "unescaped-looking."

## Path & resource confinement (filesystem traversal, and any input-selected resource id)

- **Source class:** any request-supplied path segment, filename, key, or resource identifier.
- **Sink class:** any operation that resolves that input to a **resource location** and then reads /
  writes / deletes / includes it.
- **Violated invariant:** the resolved target stays within the **intended scope**, enforced
  race-safely **at the point of use** — not merely string-checked before resolution. Applies to any
  input-selected resource, not only filesystem paths. Canonicalize-then-contain is one control and is
  still TOCTOU-prone across symlink/rename; the property is scope-confinement that holds at use time.
- **Enumeration:** enumerate every resource-resolution site; for each, confirm confinement holds after
  canonicalization *and* at use, and re-check the *write* paths (upload/export/include) where escape
  becomes code-adjacent.
- **Variant:** encoded/double-encoded/`....//`/NUL-truncated traversal; absolute-path override;
  delivery difference (blocked in the URL path but literal `../` in a query/body param); symlink
  TOCTOU between check and use.
- **Oracle:** a differential proving a crafted input resolves *outside* the intended scope (reads a
  control file / writes an out-of-scope target), not a static "concatenation looks unsafe."

## Access control — object-level (IDOR) and function-level

- **Source class:** any request-supplied **selector** of a resource or capability — object id, key,
  path, filter, role/field name, tenant discriminator.
- **Sink class:** any operation acting on a resource *selected by* that input where authorization
  must be re-derived from the **authenticated principal**, not the input.
- **Violated invariant:** on **every** exposed path, the principal is authorized for the specific
  (action, resource, tenant, context) — however that authorization is implemented (query predicate,
  policy check, guard). Broken iff any reachable exposure performs the operation on the input-selected
  object without that authorization.
- **Enumeration:** the unit is **(resource × operation × exposure)**. Enumerate every operation on
  every resource across *all* transports (REST, GraphQL field/mutation, bulk/export, admin mirror,
  internal/replication path) — the bug hides in the second exposure that forgot the check.
- **Variant:** function-level escalation (a lower role reaching a higher-privilege verb); horizontal
  access to peers' objects; guessable ids aid discovery but unguessable ids are **not** an
  authorization control.
- **Oracle:** authz-trace / differential — principal A performs the operation on principal B's
  resource; or code proof the authorization is absent on a reachable path. Server-enforced ⇒ if the
  enforcing layer is unseen, `needs-live-validation`.

## Authentication, session & token integrity

- **Source class:** any credential, token, session reference, or account-recovery selector under
  attacker influence.
- **Sink class:** the code that **decides identity or grants a session/reset** — signature/secret
  verification, credential comparison, token issuance/validation, reset/OTP flows.
- **Violated invariant:** identity or authority is established only by a check the attacker cannot
  **forge or replay**, appropriate to the credential's purpose and trust model — the signature/secret
  is actually verified, freshness/audience/binding constraints suited to that token are enforced, and
  account recovery is bound server-side to the real principal. Broken iff any step trusts
  attacker-supplied data (e.g. a client-chosen user id, a "skip verification" flag, an unverified
  decode, an accepted signatureless algorithm or key-confusion — the last two are illustrations).
- **Enumeration:** enumerate every path that yields an authenticated session or a privileged token,
  including the *recovery* paths (reset, OTP, magic link, SSO assertion), which are the softest.
- **Variant:** verify-vs-decode confusion; algorithm/key confusion; missing freshness/audience
  validation; recovery flow trusting a client-supplied identity or skip-flag; missing rate limit
  enabling brute-force / enumeration.
- **Oracle:** forge/replay that yields a valid session or acts as another principal; or a trace
  showing identity derived from unverified input. Server-enforced ⇒ `needs-live-validation` by
  default in an authorized-owner context.

## Unsafe deserialization / object injection

- **Source class:** any byte stream or structured blob the attacker can influence — request body,
  cookie, cache/queue entry, imported file, restore-from-dump.
- **Sink class:** any construct that **reconstructs typed objects** from those bytes (native object
  deserializers, polymorphic/typed decoders, config loaders that instantiate by type).
- **Violated invariant:** reconstruction cannot reach a **dangerous construction, callback, or
  gadget**, nor violate a data invariant the rest of the code assumes. Attacker-controlled *type
  selection* matters only insofar as it reaches such a capability — it is not the bug by itself.
- **Enumeration:** enumerate every deserialization site; cross the **subsystem's own custom-format
  loader** (a component's payload parser often runs *outside* the generic input sanitizer) — a
  proven-dirty cross-vein.
- **Variant:** polymorphic-typing enabled vs. off; a safe decoder on one path and a rich/typed one on
  a sibling path; nested/second-stage decode.
- **Oracle:** a trace to an actual construction/gadget/callback capability, or a differential
  demonstrating attacker-chosen behavior — not "deserialization exists." Where the ecosystem only
  permits data-shaped decoding, downgrade and say so: the risk is logic error / panic, not gadget RCE.

## Server-side request forgery (SSRF) & trust-boundary request forgery

- **Source class:** any attacker-influenced URL, host, or address reaching an outbound request —
  webhooks, URL preview, media/import fetch, PDF/HTML render, SSO metadata.
- **Sink class:** any outbound network client whose destination is input-derived.
- **Violated invariant:** the request destination stays within the **intended destination policy /
  trust zone at the moment of connection**, after redirect and name resolution. Internal addressing
  is not inherently the bug — reaching an **unintended** trust zone or capability is. Broken iff the
  destination can be steered outside policy or re-resolved after the check.
- **Enumeration:** enumerate every outbound-request site with an input-derived destination; check the
  policy survives redirects, DNS rebinding, and alternate address encodings (IPv6, decimal, metadata
  hostnames).
- **Variant:** redirect-following bypass; DNS-rebinding / TOCTOU on the resolved address; blind SSRF
  (no body but observable side effect); protocol smuggling (`file:`, `gopher:`).
- **Oracle:** a request observably issued to a destination the policy forbids / an unintended internal
  capability reached (out-of-band callback / metadata read / timing), not "the URL is user-controlled."

## XML / external-entity & markup-parser abuse (XXE)

- **Source class:** any attacker-supplied XML or structured markup reaching a parser — API bodies,
  SOAP/SAML assertions, office/PDF/SVG documents, config or import files.
- **Sink class:** a document parser configured to resolve **external entities, DTDs, or includes**.
- **Violated invariant:** the parser does not resolve **attacker-directed external references**
  (entities, DTDs, `xinclude`, schema locations). Broken iff an attacker-supplied reference is
  fetched or inlined.
- **Enumeration:** enumerate every markup/document parser and its entity/DTD/external-reference
  settings; include indirect parsers (SSO, document converters, feed/import handlers).
- **Variant:** local-file disclosure via a SYSTEM entity; SSRF via an external entity; parameter
  entities and out-of-band exfiltration. (Billion-laughs entity-expansion is a DoS facet — out of
  scope per the scope note; hunt the file-read / SSRF facets.)
- **Oracle:** a differential proving an external reference resolved — attacker-chosen file content
  returned or an out-of-band callback fired.

## Cross-site request forgery & open redirect (browser-driven request integrity)

- **Source class:** a victim's authenticated browser induced to issue a request (CSRF), or a
  user-controlled redirect/forward target (open redirect).
- **Sink class:** a state-changing operation authenticated by an **ambient credential** (cookie)
  without intent proof; or a redirect/forward whose destination is input-derived.
- **Violated invariant:** a state-changing request carries **proof of user intent that cannot be
  forged cross-site** (unpredictable token, `SameSite`, or a validated Origin/Referer); a redirect
  target is confined to an **allowlisted destination**. Broken iff a cross-site page can drive the
  state change, or the redirect can be pointed off-allowlist.
- **Enumeration:** enumerate every state-changing operation relying on ambient auth, and every
  redirect/forward with an input-derived destination.
- **Variant:** login-CSRF; method-override / nested-JSON content-type bypass; redirect used to leak a
  token or credential, or as a phishing / OAuth-flow pivot.
- **Oracle:** a cross-site-forged request effects the state change; or a redirect observably reaches
  an attacker-chosen destination.

## Unsafe file / content ingestion (upload, import, archive extraction)

- **Source class:** uploaded or imported files, archive members, and referenced external content.
- **Sink class:** storage, parsing, extraction, or serving of that content.
- **Violated invariant:** ingested content is confined to **inert data in a path-contained,
  non-executable location**, and no archive member can escape the extraction root. Broken iff
  attacker-chosen type/content lands in an executable or interpreted location, or an archive member's
  path/symlink escapes containment (Zip-Slip).
- **Enumeration:** enumerate every upload/import/extract site; for each, check type/content
  validation, the storage location's execution/interpretation semantics, and archive-member path
  containment.
- **Variant:** extension/MIME confusion; active content (SVG/HTML/polyglot) served same-origin;
  archive path-traversal or symlink members; overwrite of a sensitive existing path.
- **Oracle:** a crafted upload/extraction observably lands in an executable/interpreted or out-of-root
  location — not "an upload endpoint exists."

## Unsafe data binding / object mutation (mass assignment, prototype pollution)

- **Source class:** attacker-controlled key/value maps bound into objects — request bodies, query
  maps, merged config, stored maps replayed later (second-order).
- **Sink class:** any bulk-bind / merge / recursive-assign / deep-set that sets fields by an
  **input-chosen name**.
- **Violated invariant:** input may set only an **explicitly permitted set of fields** on the target;
  it must not choose the *attribute/field name*. Broken iff input can name a privileged field, a
  shared base/prototype attribute, or internal state.
- **Enumeration:** enumerate every bulk-bind / merge / deep-set from an untrusted map; distinguish
  shallow copies (bounded) from recursive merges/deep-sets (the pollution-capable sinks).
- **Variant:** mass-assign a privileged field (role/owner/flag); pollute a shared prototype/base
  influencing *later, unrelated* logic; second-order via a stored map re-merged downstream.
- **Oracle:** a request observably sets a field it must not, or pollutes shared state that changes a
  later code path — not "a merge exists."

## Cryptographic misuse

- **Trigger / context** (often *not* attacker-input-driven): wherever confidentiality, integrity, or
  authenticity is *supposed* to be provided — establish the **intended security goal first** (a
  checksum is not a security control).
- **Sink class:** the primitive selection and its usage (hash/cipher/mode/KDF, IV/nonce/salt
  handling, RNG source, comparison).
- **Violated invariant:** the primitive and its parameters **achieve the stated security goal under
  the threat model**. Adequacy is judged against the goal — illustratively: collision/preimage
  resistance where integrity is claimed; a unique nonce/IV and an authenticated mode for
  confidentiality+integrity; a slow, salted, memory-hard function for password storage; a CSPRNG for
  anything that must be unpredictable; constant-time comparison where a timing signal on a secret is
  attacker-observable.
- **Enumeration:** enumerate crypto-relevant sites and tag each with its security goal, then test the
  invariant for that goal (avoid flagging non-security uses of a "weak" primitive).
- **Variant:** nonce/IV reuse under a fixed key; predictable seed for tokens/ids/reset codes;
  fast-digest password hashing; non-constant-time secret comparison (timing oracle).
- **Oracle:** demonstrate the property fails (forgeable MAC, predictable token, exploitable timing
  signal) or a trace establishing the primitive/goal mismatch.

## Memory & lifetime safety (native code)

- **Source class:** attacker-controlled sizes, lengths, indices, counts, and byte content reaching
  allocation, indexing, copying, or arithmetic.
- **Sink class:** memory operations — bounds/index math, copies, allocation sizing, free/use, integer
  arithmetic feeding any of these.
- **Violated invariant:** every access stays within its object's **bounds and lifetime**, and
  size/index arithmetic cannot overflow/underflow or narrow into a wrong value. Broken iff attacker
  input can drive an out-of-bounds access, a use-after-free/double-free, or an integer flaw that
  becomes one.
- **Enumeration:** at the function altitude, enumerate every size/length computation and every
  copy/index; when one memory bug is confirmed in a file, treat the **file as proven-dirty** and
  sweep its other trigger paths (every command/opcode/ingest channel, and its own (de)serialization
  loader) before closing it.
- **Variant:** unsigned underflow into a large length; narrowing on assignment; off-by-one at a
  boundary; the same sink reached by semantically distinct trigger paths (test each arm, incl.
  against a patched build).
- **Oracle:** a **sanitizer** signal (ASAN/UBSAN) on a controlled input tied to the violated
  invariant — RAPTOR's preferred oracle — or an equivalent bounds/lifetime proof (deterministic
  debugger repro, controlled-corruption evidence). A bare crash is characterization-pending, not a
  proven primitive.

## Logic, state & concurrency (TOCTOU, replay, quota, business rules)

- **Source class:** any request that changes shared or business state, plus the values, ordering, and
  timing the attacker controls.
- **Sink class:** check-then-act sequences on shared resources, and multi-step flows whose steps can
  be reordered / replayed / parallelized (balance, quota, stock, one-time token, pricing, workflow).
- **Violated invariant:** the flow's **application-specific security invariant** (value bounds,
  ordering, single-use, quota, pricing, state-machine legality) is preserved under
  attacker-controlled values, ordering, retries, and concurrency. Atomic single-use (TOCTOU/replay)
  is one such invariant, not the whole class.
- **Enumeration:** enumerate state-changing operations on shared/limited resources and multi-step
  flows; for each, ask whether the invariant survives concurrency, replay, reordering, and
  out-of-range values.
- **Variant:** concurrent double-spend / coupon reuse; replay of a state-changing request; integer
  over/underflow or narrowing in a quantity/price/length computation; missing rate limit on an
  enumerable or costly action.
- **Oracle:** a race/replay/out-of-order sequence that observably violates the invariant (double
  effect, negative balance, bypassed limit, illegal state), or a trace establishing the non-atomic
  window.

## Secret & credential exposure (provenance → exposure → liveness)

The method here is **reasoning**, not pattern-matching — a prefix regex or entropy score is at most a
*recall aid* delegated to the deterministic layer (the Semgrep secret pack). On the five-slot engine:

- **Source class:** credential-shaped values wherever they appear — source, config, `.env`, CI/CD
  definitions, Dockerfiles, IaC, logs, client bundles.
- **Sink class:** any boundary a *live* secret must not cross.
- **Violated invariant (three judgments):**
  - **Provenance** — is the value **originated in code** (a committed literal) or merely *referenced*
    from env/vault/secrets-manager? A reference is the safe pattern and not a finding on its own.
  - **Exposure** — does a **real** secret cross a boundary it must not: committed to VCS (and thus in
    history), logged, echoed to CI output, baked into an image layer, shipped in a client bundle, or
    sent in cleartext? Vault provenance does **not** make later exposure safe.
  - **Liveness** — is it a **usable** credential, or a placeholder / example / already-rotated value?
  - Reportable = **(originated-in-code OR exposed) AND live**. For a live committed secret, note that
    history retains it and **rotation**, not deletion, is the remediation.
- **Enumeration:** scan *all* files including config/CI/IaC (not just source); for each credential-
  shaped hit, run the three judgments rather than trusting the pattern match.
- **Oracle:** provenance + exposure evidence from the artifacts (and, where authorized, a liveness
  probe). A `.env` pattern merely missing from ignore rules, with no secret yet committed, is a
  hardening note — not a finding.

---

## How this wires into the loop

- One class ⇄ one bug-class lens per round; sweep them across the altitudes (whole → file →
  functionality → function). The **enumeration strategy** for each class is what makes a round's
  coverage *provable* — log the enumerated sink set and which cells you traced in `TRIED.md`; an
  unexamined sink is `UNCOVERED`, never `unaffected`. A class **absent** from this file is never
  enumerated at all, so keep the class list complete for the target's surface.
- The **violated invariant** line is the root-cause half of the finding contract; the **oracle** is
  what moves a survivor from `needs-live-validation` to `confirmed`.
- Keep this file generic. Concrete signatures live in the deterministic layer (Semgrep rules, `/sca`)
  and in the model's own knowledge; baking product/version specifics in here is exactly the rot this
  reframe removed.
