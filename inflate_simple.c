
/* no input validation, no bounds check, 2-3x slower than optimized inflate */

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

enum {
	CodeBits  = 16,  /* max number of bits in a code + 1 */
	Nlit      = 256, /* number of lit codes */
	Nlen      = 29,  /* number of len codes */
	Nlitlen   = Nlit + Nlen + 3, /* litlen codes + block end + 2 unused */
    Ndist     = 30,  /* number of distance codes */
	Nclen     = 19   /* number of code length codes */
};

typedef struct {
	ushort count[CodeBits]; /* code length -> count */
	ushort symbol[Nlitlen]; /* symbols ordered by code length */
} Huff;

typedef struct {
	uchar *src;
	uchar *dst;

	uint bits;
	uint nbits;

 	Huff lhuff; /* dynamic lit/len huffman code tree */
	Huff dhuff; /* dynamic distance huffman code tree */
} Stream;


static Huff lhuff; /* fixed lit/len huffman code tree */
static Huff dhuff; /* fixed distance huffman code tree */

  /* base offset tables */
static ushort lenbase[Nlen];
static ushort distbase[Ndist];
 
/* extra bits tables */
static uchar lenbits[Nlen] = {
	0,  0,  0,  0,  0,  0,  0,  0,  1,  1,
	1,  1,  2,  2,  2,  2,  3,  3,  3,  3,
	4,  4,  4,  4,  5,  5,  5,  5,  0
};
static uchar distbits[Ndist] = {
	0,  0,  0,  0,  1,  1,  2,  2,  3,  3,
	4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
 	9,  9, 10, 10, 11, 11, 12, 12, 13, 13
};

 /* ordering of code lengths */
static uchar clenorder[Nclen] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static void init_base_tables(void) {
	uint base;
	int i;

	for (base = 3, i = 0; i < Nlen; i++) {
		lenbase[i] = base;
		base += 1 << lenbits[i];
	}
	lenbase[Nlen-1]--; /* deflate bug */
	for (base = 1, i = 0; i < Ndist; i++) {
		distbase[i] = base;
		base += 1 << distbits[i];
	}
}

static void init_fixed_huffs(void) {
	int i;

	lhuff.count[7] = 24;
	lhuff.count[8] = 152;
	lhuff.count[9] = 112;
	for (i = 0; i < 24; i++)
		lhuff.symbol[i] = 256 + i;
	for (i = 0; i < 144; i++)
		lhuff.symbol[24 + i] = i;
	for (i = 0; i < 8; i++)
		lhuff.symbol[24 + 144 + i] = 280 + i;
	for (i = 0; i < 112; i++)
		lhuff.symbol[24 + 144 + 8 + i] = 144 + i;
	dhuff.count[5] = Ndist;
	for (i = 0; i < Ndist; i++)
 		dhuff.symbol[i] = i;
}

/* build huffman code tree from code lengths */
static void build_huff(Huff *h, const uchar *lens, int n) {
	int offs[CodeBits];
	int i, sum;

	/* count code lengths and calc first code (offs) for each length */
	for (i = 0; i < CodeBits; i++)
 		h->count[i] = 0;
	for (i = 0; i < n; i++)
		h->count[lens[i]]++;
	h->count[0] = 0;
 	for (sum = 0, i = 0; i < CodeBits; i++) {
		offs[i] = sum;
		sum += h->count[i];
	}
	/* sort symbols by code length */
	for (i = 0; i < n; i++)
		if (lens[i])
			h->symbol[offs[lens[i]]++] = i;
}

 /* get one bit from stream */
static uint getbit(Stream *s) {
	uint bit;
 
 	if (!s->nbits--) {
		s->bits = *s->src++;
		s->nbits = 7;
 	}
	bit = s->bits & 1;
	s->bits >>= 1;
	return bit;
}

/* get n bits from stream */
static uint getbits(Stream *s, int n) {
	uint bits = 0;
	int i;

	for (i = 0; i < n; i++)
		bits |= getbit(s) << i;
 	return bits;
}

/* decode a symbol from stream with huffman code tree */
static uint decode_symbol(Stream *s, Huff *h) {
	int sum = 0, cur = 0;
	ushort *count = h->count + 1;
 
	for (;;) {
		cur |= getbit(s);
		sum += *count;
 		cur -= *count;
 		if (cur < 0)
			break;
		cur <<= 1;
		count++;
	}
	return h->symbol[sum + cur];
}

 /* decode dynamic huffman code trees from stream */
static void decode_huffs(Stream *s) {
	Huff chuff;
	uchar lens[Nlitlen+Ndist];
	uint nlit, ndist, nclen;
	uint i;

	nlit = 257 + getbits(s, 5);
	ndist = 1 + getbits(s, 5);
	nclen = 4 + getbits(s, 4);
 	/* build code length code tree */
 	for (i = 0; i < Nclen; i++)
		lens[i] = 0;
 	for (i = 0; i < nclen; i++)
		lens[clenorder[i]] = getbits(s, 3);
 	build_huff(&chuff, lens, Nclen);
	/* decode code lengths for the dynamic code tree */
	for (i = 0; i < nlit + ndist; ) {
		uint sym = decode_symbol(s, &chuff);
 		uint len;
		uchar c;

		if (sym < 16) {
			lens[i++] = sym;
 		} else if (sym == 16) {
			/* copy previous code length 3-6 times */
			c = lens[i - 1];
			for (len = 3 + getbits(s, 2); len; len--)
				lens[i++] = c;
		} else if (sym == 17) {
 			/* repeat 0 for 3-10 times */
			for (len = 3 + getbits(s, 3); len; len--)
				lens[i++] = 0;
		} else if (sym == 18) {
			/* repeat 0 for 11-138 times */
 			for (len = 11 + getbits(s, 7); len; len--)
				lens[i++] = 0;
		}
	}
 	/* build dynamic huffman code tree */
	build_huff(&s->lhuff, lens, nlit);
	build_huff(&s->dhuff, lens + nlit, ndist);
}

 /* decode a block of data from stream with huffman code trees */
static void decode_block(Stream *s, Huff *lhuff, Huff *dhuff) {
	uint sym;

	for (;;) {
 		sym = decode_symbol(s, lhuff);
  	if (sym == 256)
			return;
		if (sym < 256)
			*s->dst++ = sym;
		else {
			uint len, dist;

			sym -= 257;
			len = lenbase[sym] + getbits(s, lenbits[sym]);
			sym = decode_symbol(s, dhuff);
			dist = distbase[sym] + getbits(s, distbits[sym]);
			/* copy match */
			while (len--) {
				*s->dst = *(s->dst - dist);
				s->dst++;
			}
		}
	}
}

static void inflate_uncompressed_block(Stream *s) 
{
	uint len;
 
 	s->nbits = 0; /* start block on a byte boundary */
 	len = (s->src[1] << 8) | s->src[0];
	s->src += 4;
	while (len--)
 		*s->dst++ = *s->src++;
}
 

static void inflate_fixed_block(Stream *s) {
	decode_block(s, &lhuff, &dhuff);
}

static void inflate_dynamic_block(Stream *s) {
	decode_huffs(s);
	decode_block(s, &s->lhuff, &s->dhuff);
}

 
/* extern */

 /* inflate stream from src to dst, return end pointer */
void *inflate(void *dst, void *src)
{
	Stream s;
	uint final;

 	/* initialize global (static) data */
 	init_base_tables();
	init_fixed_huffs();

	s.src = src;
	s.dst = dst;
	s.nbits = 0;
	do {
		final = getbit(&s);
	switch (getbits(&s, 2)) {
	    case 0:
			inflate_uncompressed_block(&s);
			break;
		case 1:
 			inflate_fixed_block(&s);
			break;
		case 2:
			inflate_dynamic_block(&s);
			break;
		}
 	} while (!final);
	return s.dst;
}


 /* simple test */

#include <stdlib.h>
#include <stdio.h>

void *readall(FILE *in) 
{
	uint len = 1 << 22;
	void *buf;

	buf = malloc(len);
	fread(buf, 1, len, in);
	fclose(in);
	return buf;
}

int main(void) 
{
	uint len = 1 << 24;
	uchar *src, *dst;

	src = readall(stdin);
	dst = malloc(len);
	len = (uchar *)inflate(dst, src) - dst;
	fprintf(stderr, "decompressed %u bytes\n", len);
 	fwrite(dst, 1, len, stdout);
	free(dst);
	free(src);
	return 0;
}