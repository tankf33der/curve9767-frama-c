#include "inner.h"

/*
 * Operation on scalars (reference implementation).
 *
 * We represent a scalar as a sequence of 17 words of 15 bits each, in
 * little-endian order. Each word is held in a uint16_t; the top bit is
 * always zero. Scalars may use a slightly larger-than-necessary range.
 *
 * Montgomery multiplication is used: if sR = 2^255 mod n, then Montgomery
 * multiplication of a and b yields (a*b)/sR mod n. The "Montgomery
 * representation" of x is x*sR mod n. In general we do not keep scalars
 * in Montgomery representation, because we expect encoding/decoding
 * operations to be more frequent and multiplications (keeping values
 * in Montgomery representation is a gain when several multiplications are
 * performed successively).
 *
 * Each function has its own acceptable ranges for input values, and a
 * guaranteed output range. In general, we require scalar values at the
 * API level to be lower than 1.27*n.
 */

/*
 * Curve order, in base 2^15 (little-endian order).
 */
static const uint16_t order[] = {
	24177, 19022, 18073, 22927, 18879, 12156,  7504, 10559, 11571,
	26856, 15192, 22896, 14840, 31722,  2974,  9600,  3616
};

/*
 * sR2 = 2^510 mod n.
 */
static const uint16_t sR2[] = { 
	14755,  1449,  7175,  1324, 11384, 15866, 31249, 13920, 17944,
	 6728,  3858,  5900, 25302,   432,  5554, 29779,  1646
};

/*
 * sD = 2^503 mod n  (Montgomery representation of 2^248).
 */
static const uint16_t sD[] = {
	  167,  1579, 26634, 10886, 24646, 12845, 32322,  7660,  8304,
	12054, 20731,  3487, 26407,  9107, 22337,  7191,  1284
};

/*
 * n mod 2^15
 */
#define N0    24177

/*
 * -1/n mod 2^15
 */
#define N0I   23919

/* see curve9767.h */
const curve9767_scalar curve9767_scalar_zero = {
	{ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
};

/* see curve9767.h */
const curve9767_scalar curve9767_scalar_one = {
	{ { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
};

/*
 * Addition.
 * Input operands must be less than 1.56*n each.
 * Output is lower than 2^252, hence lower than 1.14*n.
 */
static void
scalar_add(uint16_t *c, const uint16_t *a, const uint16_t *b)
{
	/*
	 * Since a < 1.56*n and b < 1.56*n, we have a+b < 3.12*n.
	 * We subtract n repeatedly as long as the value is not
	 * lower than 2^252. Since 3.12*n < 2^252 + 2*n, two
	 * conditional subtractions are enough. Moreover, since
	 * 2^252 < 1.14*n, the result is in the appropriate range.
	 */
	uint16_t d[17];
	int i, j;
	uint32_t cc;

	cc = 0;
	for (i = 0; i < 17; i ++) {
		uint32_t w;

		w = a[i] + b[i] + cc;
		d[i] = (uint16_t)(w & 0x7FFF);
		cc = w >> 15;
	}

	/*
	 * No possible carry at this point (cc = 0). We now subtract n
	 * conditionally on d >= 2^252 (and we do it twice).
	 */
	for (j = 0; j < 2; j ++) {
		uint32_t m;

		/*
		 * d / (2^252) is equal to 0, 1 or 2 at this point. We
		 * set m to 0xFFFFFFFF is the value is 1 or 2, to
		 * 0x00000000 otherwise.
		 */
		m = (uint32_t)d[16] >> 12;
		m = -(-m >> 31);

		cc = 0;
		for (i = 0; i < 17; i ++) {
			uint32_t wd, wn;

			wd = d[i];
			wn = m & order[i];
			wd = wd - wn - cc;
			d[i] = (uint16_t)(wd & 0x7FFF);
			cc = wd >> 31;
		}
	}

	memcpy(c, d, sizeof d);
}

/*
 * Subtraction.
 * Input operand 'a' must be less than 2*n.
 * Input operand 'b' must be less than 2*n.
 * Output is lower than 'a'.
 */
static void
scalar_sub(uint16_t *c, const uint16_t *a, const uint16_t *b)
{
	/*
	 * We compute a-b, then add n repeatedly until the result is
	 * nonnegative. Since b < 2*n, two additions are sufficient.
	 */
	uint16_t d[17];
	int i, j;
	uint32_t cc;

	cc = 0;
	for (i = 0; i < 17; i ++) {
		uint32_t w;

		w = (uint32_t)a[i] - (uint32_t)b[i] - cc;
		d[i] = (uint16_t)(w & 0x7FFF);
		cc = w >> 31;
	}

	/*
	 * Condition on adding n is that the value is negative. This shows
	 * up as the top bit of the top word being 1.
	 */
	for (j = 0; j < 2; j ++) {
		uint32_t m;

		m = -((uint32_t)d[16] >> 14);
		cc = 0;
		for (i = 0; i < 17; i ++) {
			uint32_t wd, wn;

			wd = d[i];
			wn = m & order[i];
			wd = wd + wn + cc;
			d[i] = (uint16_t)(wd & 0x7FFF);
			cc = wd >> 15;
		}
	}

	memcpy(c, d, sizeof d);
}

/*
 * Normalize a scalar into the 0..n-1 range.
 * Input must be less than 2*n.
 *
 * Returned value is 1 if the source value was already in the 0..n-1 range,
 * 0 otherwise.
 */
static uint32_t
scalar_normalize(uint16_t *c, const uint16_t *a)
{
	uint16_t d[17];
	int i;
	uint32_t cc;

	/*
	 * We subtract n. If the result is nonnegative, we keep it;
	 * otherwise, we use the source value.
	 */
	cc = 0;
	for (i = 0; i < 17; i ++) {
		uint32_t w;

		w = (uint32_t)a[i] - (uint32_t)order[i] - cc;
		d[i] = (uint16_t)(w & 0x7FFF);
		cc = w >> 31;
	}

	/*
	 * If cc == 1, then the result was negative, and we must add
	 * back the modulus n. Otherwise, cc == 0.
	 */
	cc = -cc;
	for (i = 0; i < 17; i ++) {
		uint32_t wa, wd;

		wa = a[i];
		wd = d[i];
		c[i] = (uint16_t)(wd ^ (cc & (wa ^ wd)));
	}

	/*
	 * If the value was in the proper range, then the first subtraction
	 * yielded a negative value, and cc == -1; otherwise, the first
	 * subtraction did not yield a negative value, and cc == 0 here.
	 */
	return -cc;
}

/*
 * Decode bytes into a scalar value. At most 32 bytes will be read, and,
 * in the 32nd byte, only the low 4 bits are considered; the upper bits
 * and subsequent bytes are ignored (in effect, this truncates the value
 * modulo 2^252).
 *
 * Output is lower than 2^252, which is lower than 1.14*n.
 */
static void
scalar_decode_trunc(uint16_t *c, const void *src, size_t len)
{
	const uint8_t *buf;
	int i;
	size_t u;
	uint32_t acc;
	int acc_len;

	buf = src;
	i = 0;
	acc = 0;
	acc_len = 0;
	for (u = 0; u < len; u ++) {
		if (u == 31) {
			acc |= (uint32_t)(buf[31] & 0x0F) << 8;
			c[16] = (uint16_t)acc;
			return;
		} else {
			acc |= (uint32_t)buf[u] << acc_len;
			acc_len += 8;
			if (acc_len >= 15) {
				c[i ++] = (uint16_t)(acc & 0x7FFF);
				acc >>= 15;
				acc_len -= 15;
			}
		}
	}
	if (acc_len > 0) {
		c[i ++] = (uint16_t)acc;
	}
	while (i < 17) {
		c[i ++] = 0;
	}
}

/*
 * Perform Montgomery multiplication: c = (a*b)/sR.
 *
 * Input values must be lower than 1.27*n.
 * Output value is lower than 1.18*n.
 */
static void
scalar_mmul(uint16_t *c, const uint16_t *a, const uint16_t *b)
{
	int i, j;
	uint16_t d[17];
	uint32_t dh;

	memset(d, 0, sizeof d);
	dh = 0;
	for (i = 0; i < 17; i ++) {
		uint32_t f, g, t, cc;

		f = a[i];
		t = d[0] + f * b[0];
		g = (t * N0I) & 0x7FFF;
		cc = (t + g * N0) >> 15;
		for (j = 1; j < 17; j ++) {
			uint32_t h;

			/*
			 * If cc <= 2^16, then:
			 *   h <= 2^15-1 + 2*(2^15-1)^2 + 2^16
			 * Thus:
			 *   h < ((2^15)+1)*(2^16)
			 * Therefore, cc will be at most 2^16 on output.
			 */
			h = d[j] + f * b[j] + g * order[j] + cc;
			d[j - 1] = (uint16_t)(h & 0x7FFF);
			cc = h >> 15;
		}

		/*
		 * If dh <= 1, then dh+cc <= 1+2^16 < 2^17, and the new
		 * dh value can be only 0 or 1.
		 */
		dh += cc;
		d[16] = dh & 0x7FFF;
		dh >>= 15;
	}

	/*
	 * The loop above computed:
	 *   d = (a*b + k*n) / (2^255)
	 * with k < 2^255 (the successive values 'g' in the loop were the
	 * representation of 'k' in base 2^15). Since we assumed that,
	 * on input:
	 *   a < 1.27*n
	 *   b < 1.27*n
	 * it follows that:
	 *   d < (1.27*n)^2 + (2^255)*n) / (2^255)
	 * This latter bound is about 1.178*n, hence d < 1.18*n. This
	 * means that d is already in the proper range, and, in
	 * particular, that dh = 0.
	 */
	memcpy(c, d, sizeof d);
}

/* see curve9767.h */
uint32_t
curve9767_scalar_decode_strict(curve9767_scalar *s, const void *src, size_t len)
{
	/*
	 * Set dummy alignment word (to appease some sanitizing tools).
	 */
	s->v.w16[17] = 0;

	/*
	 * Decode the first 252 bits into a value.
	 */
	scalar_decode_trunc(s->v.w16, src, len);

	/*
	 * If the input length was at most 31 bytes, then the value is
	 * less than 2^248, hence necessarily correct. If it is 32 bytes
	 * or more, then we must verify that the value as decoded above is
	 * indeed lower than n, AND that all the ignored bits were zero.
	 */
	if (len >= 32) {
		const uint8_t *buf;
		size_t u;
		uint32_t r;

		buf = src;
		r = buf[31] >> 4;
		for (u = 32; u < len; u ++) {
			r |= buf[u];
		}
		r = (r - 1) >> 31;
		r &= scalar_normalize(s->v.w16, s->v.w16);
		return r;
	} else {
		return 1;
	}
}

/* see curve9767.h */
void
curve9767_scalar_decode_reduce(curve9767_scalar *s, const void *src, size_t len)
{
	/*
	 * Set dummy alignment word (to appease some sanitizing tools).
	 */
	s->v.w16[17] = 0;

	/*
	 * Principle: we decode the input by chunks of 31 bytes, in
	 * big-endian order (the bytes are in little-endian order, but we
	 * process the chunks from most to least significant). For each
	 * new chunk to process, we multiply the current value by 2^248,
	 * then add the chunk. Multiplication by 2^248 is done with a
	 * Montgomery multiplication by sD = 2^248*2^255 mod n.
	 */
	if (len <= 31) {
		scalar_decode_trunc(s->v.w16, src, len);
	} else {
		const uint8_t *buf;
		size_t u;

		buf = src;
		for (u = 0; u + 31 < len; u += 31);
		scalar_decode_trunc(s->v.w16, buf + u, len - u);

		/*
		 * At the entry of each iteration, we have s < 1.27*n.
		 * Montgomery multiplication returns a value less
		 * than 1.18*n, and decoding a 31-byte chunk yields
		 * a value less than 2^248 which is approximately 0.071*n.
		 * Therefore, the addition yields a value less than 1.27*n.
		 */
		while (u > 0) {
			uint16_t t[17];

			u -= 31;
			scalar_mmul(s->v.w16, s->v.w16, sD);
			scalar_decode_trunc(t, buf + u, 31);
			scalar_add(s->v.w16, s->v.w16, t);
		}
	}
}

/* see curve9767.h */
void
curve9767_scalar_encode(void *dst, const curve9767_scalar *s)
{
	uint16_t t[17];
	int i;
	unsigned acc;
	int acc_len;
	uint8_t *buf;
	size_t u;

	scalar_normalize(t, s->v.w16);
	buf = dst;
	u = 0;
	acc = 0;
	acc_len = 0;
	for (i = 0; i < 17; i ++) {
		acc |= (uint32_t)t[i] << acc_len;
		acc_len += 15;
		while (acc_len >= 8) {
			buf[u ++] = (uint8_t)acc;
			acc >>= 8;
			acc_len -= 8;
		}
	}
	buf[31] = (uint8_t)acc;
}

/* see curve9767.h */
int
curve9767_scalar_is_zero(const curve9767_scalar *s)
{
	uint16_t t[17];
	uint32_t r;
	int i;

	scalar_normalize(t, s->v.w16);
	r = 0;
	for (i = 0; i < 17; i ++) {
		r |= t[i];
	}
	return 1 - (-r >> 31);
}

/* see curve9767.h */
int
curve9767_scalar_eq(const curve9767_scalar *a, const curve9767_scalar *b)
{
	/*
	 * We cannot simply compare the two representations because they
	 * might need normalization.
	 */
	curve9767_scalar c;

	curve9767_scalar_sub(&c, a, b);
	return curve9767_scalar_is_zero(&c);
}

/* see curve9767.h */
void
curve9767_scalar_add(curve9767_scalar *c,
	const curve9767_scalar *a, const curve9767_scalar *b)
{
	/*
	 * Set dummy alignment word (to appease some sanitizing tools).
	 */
	c->v.w16[17] = 0;

	scalar_add(c->v.w16, a->v.w16, b->v.w16);
}

/* see curve9767.h */
void
curve9767_scalar_sub(curve9767_scalar *c,
	const curve9767_scalar *a, const curve9767_scalar *b)
{
	/*
	 * Set dummy alignment word (to appease some sanitizing tools).
	 */
	c->v.w16[17] = 0;

	scalar_sub(c->v.w16, a->v.w16, b->v.w16);
}

/* see curve9767.h */
void
curve9767_scalar_neg(curve9767_scalar *c, const curve9767_scalar *a)
{
	/*
	 * Set dummy alignment word (to appease some sanitizing tools).
	 */
	c->v.w16[17] = 0;

	scalar_sub(c->v.w16, curve9767_scalar_zero.v.w16, a->v.w16);
}

/* see curve9767.h */
void
curve9767_scalar_mul(curve9767_scalar *c,
	const curve9767_scalar *a, const curve9767_scalar *b)
{
	uint16_t t[17];

	/*
	 * Set dummy alignment word (to appease some sanitizing tools).
	 */
	c->v.w16[17] = 0;

	scalar_mmul(t, a->v.w16, sR2);
	scalar_mmul(c->v.w16, t, b->v.w16);
}

/* see curve9767.h */
void
curve9767_scalar_condcopy(curve9767_scalar *d,
	const curve9767_scalar *s, uint32_t ctl)
{
	int i;

	ctl = -ctl;
	for (i = 0; i < 9; i ++) {
		d->v.w32[i] ^= ctl & (d->v.w32[i] ^ s->v.w32[i]);
	}
}

/*
 * For lattice basis reduction, we use an other representation with
 * 30-bit limbs, in 32-bit words (top two bits of each word are always
 * 0). There are "small integers" (that fit on 5 limbs), and "large
 * integers" (that use 17 limbs). These values are signed; two's
 * complement is used for negative values.
 */

/*
 * Convert a value from 15-bit limbs to 30-bit limbs (small value). The
 * source value must contain at least 10 limbs; exactly five 30-bit
 * limbs are produced (source value is truncated).
 */
static void
to_small(uint32_t *d, const uint16_t *a)
{
	int i;

	for (i = 0; i < 5; i ++) {
		d[i] = (uint32_t)a[i << 1] + ((uint32_t)a[(i << 1) + 1] << 15);
	}
}

/*
 * Multiply two nonnegative integers in unsigned 15-bit limb
 * representation, without any modular reduction. Result is a large
 * integer. Source values must be less than or equal to the curve
 * order n.
 */
static void
mul15_to_large(uint32_t *d, const uint16_t *a, const uint16_t *b)
{
	uint16_t t[34];
	int i, j;

	memset(t, 0, sizeof t);
	for (i = 0; i < 17; i ++) {
		uint32_t f, cc;

		f = a[i];
		cc = 0;
		for (j = 0; j < 17; j ++) {
			cc += f * (uint32_t)b[j] + t[i + j];
			t[i + j] = (uint16_t)(cc & 0x7FFF);
			cc >>= 15;
		}
		t[i + 17] = (uint16_t)cc;
	}
	for (i = 0; i < 17; i ++) {
		d[i] = (uint32_t)t[i << 1] + ((uint32_t)t[(i << 1) + 1] << 15);
	}
}

/*
 * This macro defines a function with prototype:
 *
 *   static void name(uint32_t *a, const uint32_t *b, unsigned s)
 *
 * The 'op' parameter must be the + or the - operator. If 'op' is +,
 * then the function adds lshift(b,s) to a; otherwise, it subtracts
 * lshift(b,s) from a. Both values consist of 'size' limbs of 30 bits.
 * The result is silently truncated if it does not fit. All values of
 * s up to 74908 are supported; in practice, s will always be lower than
 * 505, and almost always be lower than 30.
 *
 * Note: '(x * 34953) >> 20' computes floor(x/30) for all x in 0..74908.
 */
#define DEF_OP_LSHIFT(name, op, size) \
static void \
name(uint32_t *a, const uint32_t *b, unsigned s) \
{ \
	uint32_t b2[(size)]; \
	uint32_t cc, ee; \
	int i; \
 \
	if (s >= 30) { \
		unsigned k; \
 \
		k = (unsigned)(((uint32_t)s * (uint32_t)34953) >> 20); \
		s -= k * 30; \
		if (k >= (size)) { \
			return; \
		} \
		memset(b2, 0, k * sizeof b2[0]); \
		memcpy(b2 + k, b, ((size) - k) * sizeof b2[0]); \
		b = b2; \
	} \
	cc = 0; \
	ee = 0; \
	for (i = 0; i < (size); i ++) { \
		uint32_t t, w; \
 \
		w = b[i]; \
		t = ((w << s) | ee) & 0x3FFFFFFF; \
		ee = w >> (30 - s); \
		t = a[i] op t op cc; \
		a[i] = t & 0x3FFFFFFF; \
		cc = t >> (31 - ((1 op 1) >> 1)); \
	} \
}

DEF_OP_LSHIFT(add_lshift_small, +, 5)
DEF_OP_LSHIFT(sub_lshift_small, -, 5)
DEF_OP_LSHIFT(add_lshift_large, +, 17)
DEF_OP_LSHIFT(sub_lshift_large, -, 17)

/*
 * Get the bit length of a "large" integer. The bit length is the minimal
 * size (in bits) of the binary representation of the value, in two's
 * complement, excluding the sign bit. Notably, both 0 and -1 have bit
 * length 0.
 */
static int
bitlength_large(const uint32_t *x)
{
	uint32_t smask;
	int i;

	smask = 0x3FFFFFFF & -(x[16] >> 29);
	for (i = 16; i >= 0; i --) {
		uint32_t w;

		w = x[i];
		if (w != smask) {
			w ^= smask;
			i = 30 * i + 1;
			if (w > 0xFFFF) {
				w >>= 16;
				i += 16;
			}
			if (w > 0xFF) {
				w >>= 8;
				i += 8;
			}
			if (w > 0x0F) {
				w >>= 4;
				i += 4;
			}
			if (w > 0x03) {
				w >>= 2;
				i += 2;
			}
			if (w > 0x01) {
				i ++;
			}
			return i;
		}
	}
	return 0;
}

/*
 * Encode a signed value (30-bit limbs) into bytes (little-endian signed
 * convention). There must be enough input limbs to cover all output
 * bytes.
 */
static void
encode_small(uint8_t *d, size_t d_len, const uint32_t *x)
{
	uint32_t acc;
	int acc_len;
	size_t u, v;

	acc = x[0];
	acc_len = 30;
	v = 1;
	for (u = 0; u < d_len; u ++) {
		if (acc_len < 8) {
			uint32_t w;

			w = x[v ++];
			d[u] = (uint8_t)(acc | (w << acc_len));
			acc = w >> (8 - acc_len);
			acc_len += 22;
		} else {
			d[u] = (uint8_t)acc;
			acc >>= 8;
			acc_len -= 8;
		}
	}
}

/*
 * Compare two large values: this function returns 1 if x is numerically
 * strictly lower than y, 0 otherwise. The two operands MUST be
 * nonnegative.
 */
static int
ult_large(const uint32_t *x, const uint32_t *y)
{
	uint32_t cc;
	int i;

	cc = 0;
	for (i = 0; i < 17; i ++) {
		cc = (uint32_t)(x[i] - y[i] - cc) >> 31;
	}
	return (int)cc;
}

/* see inner.h */
void
curve9767_inner_reduce_basis_vartime(
	uint8_t *c0, uint8_t *c1, const curve9767_scalar *b)
{
	uint16_t bw[17];
	uint32_t u0_tab[5], u1_tab[5], v0_tab[5], v1_tab[5];
	uint32_t nu_tab[17], nv_tab[17], sp[17];
	uint32_t *u0, *u1, *v0, *v1, *nu, *nv;

	/*
	 * 1 in "large" format. Also valid as "small" format (by
	 * using only the 9 first words).
	 */
	static const uint32_t one_large[17] = {
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	/*
	 * n^2 in "large" format.
	 */
	static const uint32_t order_squared_large[17] = {
		323300833, 275074172, 172260179,   3118812, 465268366,
		340941917, 888453332, 493731906, 660202842, 568099908,
		778829796, 396023345, 631383276, 208310379, 427453880,
		918983080,  13077574
	};

	/*
	 * We use pointers so that swaps are efficient.
	 */
	u0 = u0_tab;
	u1 = u1_tab;
	v0 = v0_tab;
	v1 = v1_tab;
	nu = nu_tab;
	nv = nv_tab;

	/*
	 * Init:
	 *   u = [n, 0]
	 *   v = [b, 1]
	 */
	scalar_normalize(bw, b->v.w16);
	to_small(u0, order);
	memset(u1, 0, 5 * sizeof(uint32_t));
	to_small(v0, bw);
	memcpy(v1, one_large, 5 * sizeof(uint32_t));

	/*
	 * nu = <u,u> = n^2
	 * nv = <v,v> = b^2 + 1
	 * sp = <u,v> = n*b
	 */
	memcpy(nu, order_squared_large, 17 * sizeof(uint32_t));
	mul15_to_large(nv, bw, bw);
	add_lshift_large(nv, one_large, 0);
	mul15_to_large(sp, order, bw);

	/*
	 * Main loop:
	 *  - If nu < nv, then swap(u,v) and swap(nu,nv).
	 *  - If bitlength(nv) <= 253, then success (v = [c0, c1]).
	 *  - Set s <- max(0, bitlength(sp) - bitlength(nv))
	 *  - If sp > 0, then:
	 *       u <- u - lshift(v, s)
	 *       nu <- nu + lshift(nv, 2*s) - lshift(sp, s+1)
	 *       sp <- sp - lshift(nv, s)
	 *    Else:
	 *       u <- u + lshift(v, s)
	 *       nu <- nu + lshift(nv, 2*s) + lshift(sp, s+1)
	 *       sp <- sp + lshift(nv, s)
	 */
	for (;;) {
		int s;

		if (ult_large(nu, nv)) {

#define SWAP(x, y)   do { \
		uint32_t *tmp = (x); \
		(x) = (y); \
		(y) = tmp; \
	} while (0)

			SWAP(u0, v0);
			SWAP(u1, v1);
			SWAP(nu, nv);

#undef SWAP

		}

		s = bitlength_large(nv);
		if (s <= 253) {
			encode_small(c0, 16, v0);
			encode_small(c1, 16, v1);
			return;
		}
		s = bitlength_large(sp) - s;
		if (s < 0) {
			s = 0;
		}
		if ((sp[16] >> 29) == 0) {
			sub_lshift_small(u0, v0, s);
			sub_lshift_small(u1, v1, s);
			add_lshift_large(nu, nv, 2 * s);
			sub_lshift_large(nu, sp, s + 1);
			sub_lshift_large(sp, nv, s);
		} else {
			add_lshift_small(u0, v0, s);
			add_lshift_small(u1, v1, s);
			add_lshift_large(nu, nv, 2 * s);
			add_lshift_large(nu, sp, s + 1);
			add_lshift_large(sp, nv, s);
		}
	}
}
