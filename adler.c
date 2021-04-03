#include "flate.h"

enum {
	AdlerBase = 65521, /* largest 16bit prime */
	AdlerN    = 5552   /* max iters before 32bit overflow */
 };

uint adler32(uchar *p, int n, uint adler) {
	uint s1 = adler & 0xffff;
	uint s2 = (adler >> 16) & 0xffff;
	uchar *ep;
	int k;

	for (; n >= 16; n -= k) {
		k = n < AdlerN ? n : AdlerN;
		k &= ~0xf;
		for (ep = p + k; p < ep; p += 16) {
			s1 += p[0];
			s2 += s1;
			s1 += p[1];
			s2 += s1;
			s1 += p[2];
			s2 += s1;
 			s1 += p[3];
			s2 += s1;
 			s1 += p[4];
			s2 += s1;
			s1 += p[5];
			s2 += s1;
			s1 += p[6];
			s2 += s1;
			s1 += p[7];
 			s2 += s1;
			s1 += p[8];
			s2 += s1;
			s1 += p[9];
			s2 += s1;
			s1 += p[10];
 			s2 += s1;
			s1 += p[11];
   			s2 += s1;
			s1 += p[12];
 			s2 += s1;
			s1 += p[13];
			s2 += s1;
			s1 += p[14];
 			s2 += s1;
			s1 += p[15];
			s2 += s1;
		}
		s1 %= AdlerBase;
 		s2 %= AdlerBase;
	}
	if (n) {
		for (ep = p + n; p < ep; p++) {
			s1 += p[0];
			s2 += s1;
		}
		s1 %= AdlerBase;
		s2 %= AdlerBase;
	}
	return (s2 << 16) + s1;
 }