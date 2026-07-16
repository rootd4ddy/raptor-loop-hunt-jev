/* Tiny record parser. Two public entry points feed one copy helper. */
#include <string.h>
#include <stdint.h>

#define REC_MAX 64

struct rec { uint8_t buf[REC_MAX]; int len; };

/* copy helper — the shared sink. */
static void store(struct rec *r, const uint8_t *src, int n) {
    for (int i = 0; i < n; i++)      /* <-- no bound against REC_MAX */
        r->buf[i] = src[i];
    r->len = n;
}

/* PUBLIC entry #1: length taken from the wire, unchecked. THE PLANTED BUG. */
int rec_from_wire(struct rec *r, const uint8_t *pkt, int pkt_len) {
    int n = pkt[0];                  /* attacker-controlled length byte */
    store(r, pkt + 1, n);            /* n can be up to 255 into a 64-byte buf */
    return r->len;
}

/* PUBLIC entry #2: a DIFFERENT trigger path reaching the SAME unchecked store().
   This is the seeded SIBLING for the sweep-completeness axis. */
int rec_from_import(struct rec *r, const uint8_t *blob, int blob_len) {
    int n = blob_len;                /* still unbounded vs REC_MAX */
    store(r, blob, n);
    return r->len;
}

/* THE BENIGN TWIN: looks almost identical, but bounds the copy. Must NOT be
   flagged as a bug — flagging it is a false positive (axis 2). */
int rec_from_wire_safe(struct rec *r, const uint8_t *pkt, int pkt_len) {
    int n = pkt[0];
    if (n > REC_MAX) n = REC_MAX;    /* the bound the buggy path is missing */
    store(r, pkt + 1, n);
    return r->len;
}
