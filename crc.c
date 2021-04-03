#include "flate.h"

uint crctab[256];

void crc32init(void) {
	static const uint poly = 0xedb88320;
	int i,j;

	for (i = 0; i < 256; ++i) {
		uint crc = i;

		for (j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		crctab[i] = crc;
	}
}

uint crc32(uchar *p, int n, uint crc) {
 	uchar *ep = p + n;

	crc ^= 0xffffffff;
	while (p < ep)
		crc = crctab[(crc & 0xff) ^ *p++] ^ (crc >> 8);
	return crc ^ 0xffffffff;
}