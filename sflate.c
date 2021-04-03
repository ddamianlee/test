#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flate.h"
 
static void set32(uchar *p, uint n) 
{
 	p[0] = n >> 24;
  	p[1] = n >> 16;
 	p[2] = n >> 8;
 	p[3] = n;
}
 
static void set32le(uchar *p, uint n) {
	p[0] = n;
	p[1] = n >> 8;
	p[2] = n >> 16;
	p[3] = n >> 24;
}

static int check32(uchar *p, uint n) 
{
 	return n == ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}
static int check32le(uchar *p, uint n) 
{
	return n == (p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}
 
enum 
{
 	ZlibCM    = 7 << 4,
 	ZlibCINFO = 8,
	ZlibFLEV  = 3 << 6,
	ZlibFDICT = 1 << 5,
 	ZlibFCHK  = 31 - (((ZlibCM | ZlibCINFO) << 8) | ZlibFLEV) % 31
};

int deflate_zlib_header(uchar *p, int n) 
{
	if (n < 2)
		return FlateErr;
	p[0] = ZlibCM | ZlibCINFO;  /* deflate method, 32K window size */
	p[1] = ZlibFLEV | ZlibFCHK; /* highest compression */
 	return 2;
}

int deflate_zlib_footer(uchar *p, int n, uint sum, uint len, uint zlen) {
	if (n < 4)
		return FlateErr;
	set32(p, sum);
	return 4;
 }

int inflate_zlib_header(uchar *p, int n) 
{
	if (n < 2)
		return FlateErr;
	if (((p[0] << 8) | p[1]) % 31)
		return FlateErr;
	if ((p[0] & 0xf0) != ZlibCM || (p[0] & 0x0f) > ZlibCINFO)
 		return FlateErr;
	if (p[1] & ZlibFDICT)
		return FlateErr;
	return 2;
}

int inflate_zlib_footer(uchar *p, int n, uint sum, uint len, uint zlen) 
{
	if (n < 4 || !check32(p, sum))
		return FlateErr;
	return 4;
}


enum 
{
	GZipID1    = 0x1f,
	GZipID2    = 0x8b,
	GZipCM     = 8,
	GZipFHCRC  = 1 << 1,
	GZipFEXTRA = 1 << 2,
	GZipFNAME  = 1 << 3,
	GZipFCOMM  = 1 << 4,
	GZipXFL    = 2,
	GZipOS     = 255
};

int deflate_gzip_header(uchar *p, int n) 
{
	if (n < 10)
 		return FlateErr;
 	memset(p, 0, 10);
	p[0] = GZipID1;
	p[1] = GZipID2;
	p[2] = GZipCM;
	p[8] = GZipXFL;
 	p[9] = GZipOS;
 	return 10;
}

int deflate_gzip_footer(uchar *p, int n, uint sum, uint len, uint zlen) 
{
	if (n < 8)
		return FlateErr;
 	set32le(p, sum);
	set32le(p+4, len);
	return 8;
}

int inflate_gzip_header(uchar *p, int n) 
{
	int k = 10;

	if (k > n)
 		return FlateErr;
    if (p[0] != GZipID1 || p[1] != GZipID2 || p[2] != GZipCM)
		return FlateErr;
 	if (p[3] & GZipFEXTRA) 
    {
		k += 2 + ((p[k] << 8) | p[k+1]);
		if (k > n)
			return FlateErr;
	}
	if (p[3] & GZipFNAME) 
    {
		for (; k < n; k++)
			if (p[k] == 0)
 				break;
		k++;
		if (k > n)
			return FlateErr;
 	}
	if (p[3] & GZipFCOMM) 
    {
		for (; k < n; k++)
			if (p[k] == 0)
 				break;
		k++;
		if (k > n)
			return FlateErr;
	}
	if (p[3] & GZipFHCRC) 
    {
		k += 2;
		if (k > n)
			return FlateErr;
	}
 	return k;
}
 
int inflate_gzip_footer(uchar *p, int n, uint sum, uint len, uint zlen) 
{
	if (n < 8 || !check32le(p, sum) || !check32le(p+4, len))
	return FlateErr;
	return 8;
}

 static char pkname[] = "sflate_stream";

enum 
{
	PKHeadID   = 0x04034b50,
	PKDataID   = 0x08074b50,
	PKDirID    = 0x02014b50,
	PKFootID   = 0x06054b50,
	PKVersion  = 20,
 	PKFlag     = 1 << 3,
	PKMethod   = 8,
 	PKDate     = ((2009 - 1980) << 25) | (1 << 21) | (1 << 16),
	PKHeadSize = 30,
	PKDirSize  = 46,
	PKNameLen  = sizeof(pkname) - 1
};

int deflate_pkzip_header(uchar *p, int n) 
{
	if (n < PKHeadSize + PKNameLen)
		return FlateErr;
	memset(p, 0, PKHeadSize);
	set32le(p, PKHeadID);
	set32le(p+4, PKVersion);
	set32le(p+6, PKFlag);
	set32le(p+8, PKMethod);
	set32le(p+10, PKDate);
	set32le(p+26, PKNameLen);
	memcpy(p + PKHeadSize, pkname, PKNameLen);
	return PKHeadSize + PKNameLen;
}

int deflate_pkzip_footer(uchar *p, int n, uint sum, uint len, uint zlen) {
 	if (n < PKDirSize + PKNameLen + 22)
		return FlateErr;
	/* unzip bug */
/*
	if (n < 16 + PKDirSize + PKNameLen + 22)
		return FlateErr;
	set32le(p, PKDataID);
	set32le(p+4, sum);
	set32le(p+8, zlen);
	set32le(p+12, len);
	p += 16;
*/
	memset(p, 0, PKDirSize);
	set32le(p, PKDirID);
	set32le(p+4, PKVersion | (PKVersion << 16));
 	set32le(p+8, PKFlag);
	set32le(p+10, PKMethod);
	set32le(p+12, PKDate);
	set32le(p+16, sum);
	set32le(p+20, zlen);
	set32le(p+24, len);
	set32le(p+28, PKNameLen);
 	memcpy(p + PKDirSize, pkname, PKNameLen);
	p += PKDirSize + PKNameLen;
 	memset(p, 0, 22);
	set32le(p, PKFootID);
 	p[8] = p[10] = 1;
 	set32le(p+12, PKDirSize + PKNameLen);
	set32le(p+16, zlen + PKHeadSize + PKNameLen);
	return PKDirSize + PKNameLen + 22;
/*
	set32le(p+12, 16 + PKDirSize + PKNameLen);
	set32le(p+16, zlen + PKHeadSize + PKNameLen);
	return 16 + PKDirSize + PKNameLen + 22;
*/
}

int inflate_pkzip_header(uchar *p, int n) 
{
	int k = 30;

  	if (k > n)
		return FlateErr;
	if (!check32le(p, PKHeadID))
		return FlateErr;
	if ((p[4] | (p[5] << 8)) > PKVersion)
		return FlateErr;
 	if ((p[8] | (p[9] << 8)) != PKMethod)
		return FlateErr;
	k += p[26] | (p[27] << 8);
 	k += p[28] | (p[29] << 8);
	if (k > n)
		return FlateErr;
	return k;
}

int inflate_pkzip_footer(uchar *p, int n, uint sum, uint len, uint zlen) 
{
	int k = PKDirSize + 22;

	if (k > n)
		return FlateErr;
 	if (check32le(p, PKDataID)) 
    {
		p += 16;
 		k += 16;
 		if (k > n)
 			return FlateErr;
 	}
	if (!check32le(p, PKDirID))
		return FlateErr;
	if (!check32le(p+16, sum))
		return FlateErr;
	if (!check32le(p+20, zlen))
 		return FlateErr;
	if (!check32le(p+24, len))
		return FlateErr;
	return k;
}


/* example usage */

static int (*header)(uchar *, int);
static int (*footer)(uchar *, int, uint, uint, uint);
static uint (*checksum)(uchar *, int, uint);
static char *err;
static uint sum;
static uint nin;
static uint nout;
static uint headerlen;
static uint footerlen;
static uint extralen;

static int dummyheader(uchar *p, int n) 
{
	return 0;
}
static int dummyfooter(uchar *p, int n, uint sum, uint len, uint zlen) 
{
	return 0;
}
static uint dummysum(uchar *p, int n, uint sum) 
{
	return 0;
}

/* compress, using FlateStream interface */
int compress_stream(FILE *in, FILE *out) 
{
	FlateStream stream;
	int k, n;
	enum {Nin = 1<<15, Nout = 1<<15};

 	stream.in = malloc(Nin);
 	stream.out = malloc(Nout);
	stream.nin = 0;
	stream.nout = Nout;
	stream.err = 0;
	stream.state = 0;

	k = header(stream.out, stream.nout);
	if (k == FlateErr) 
    {
		stream.err = "header error.";
		n = FlateErr;
	} else 
    {
		headerlen = stream.nout = k;
		n = FlateOut;
 	}
 	for (;; n = deflate(&stream))
		switch (n) 
        {
		case FlateOk:
 			k = footer(stream.out, stream.nout, sum, nin, nout - headerlen);
			if (k == FlateErr) 
            {
				stream.err = "footer error.";
 				n = FlateErr;
			} else if (k != fwrite(stream.out, 1, k, out)) 
            {
				stream.err = "write error.";
			n = FlateErr;
			} else {
				footerlen = k;
				nout += k;
 			}
		case FlateErr:
			free(stream.in);
			free(stream.out);
			err = stream.err;
			return n;
		case FlateIn:
			stream.nin = fread(stream.in, 1, Nin, in);
			nin += stream.nin;
			sum = checksum(stream.in, stream.nin, sum);
			break;
		case FlateOut:
			k = fwrite(stream.out, 1, stream.nout, out);
			if (k != stream.nout)
				stream.err = "write error.";
			nout += k;
			stream.nout = Nout;
			break;
 		}
}

/* decompress, using FlateStream interface */
int decompress_stream(FILE *in, FILE *out) 
{
	FlateStream stream;
	uchar *begin;
	int k, n;
	enum {Nin = 1<<15, Nout = 1<<15};

	stream.in = begin = malloc(Nin);
	stream.out = malloc(Nout);
	stream.nout = Nout;
 	stream.err = 0;
	stream.state = 0;
	
	// read file

	// fread return number of bytes
	stream.nin = fread(stream.in, 1, Nin, in);
	nin += stream.nin;
	k = header(stream.in, stream.nin);
	if (k == FlateErr) 
    {
		stream.err = "header error.";
		n = FlateErr;
	} else {
		headerlen = k;
		stream.nin -= k;
		stream.in += k;
		n = inflate(&stream);
	}
	for (;; n = inflate(&stream))
		switch (n) 
        {
		case FlateOk:
			memmove(begin, stream.in, stream.nin);
			k = fread(begin, 1, Nin-stream.nin, in);
			nin += k;
			stream.nin += k;
		k = footer(begin, stream.nin, sum, nout, nin - stream.nin - headerlen);
		if (k == FlateErr) {
				stream.err = "footer error.";
				n = FlateErr;
			} else {
				footerlen = k;
				extralen = stream.nin - k;
			}
		case FlateErr:
			free(begin);
			free(stream.out);
			return n;
    		case FlateIn:
   			stream.in = begin;
			stream.nin = fread(stream.in, 1, Nin, in);
 			nin += stream.nin;
 			break;
		case FlateOut:
 			k = fwrite(stream.out, 1, stream.nout, out);
			if (k != stream.nout)
				stream.err = "write error.";
			sum = checksum(stream.out, k, sum);
			nout += k;
 			stream.nout = Nout;
 			break;
		}
}

int main(int argc, char *argv[]) 
{
	char comp = 'c';
 	char fmt = 'r';
  	char verbose = 'q';
	int (*call)(FILE *, FILE*);
	int n, i;

	for (i = 1; i < argc; i++) 
    {
		if (argv[i][0] == '-' && argv[i][1] && argv[i][2] == 0)
			switch (argv[i][1]) 
            {
			case 'q':
			case 'v':
				verbose = argv[i][1];
				continue;
 			case 'c':
			case 'd':
 				comp = argv[i][1];
				continue;
			case 'r':
			case 'g':
			case 'z':
 			case 'p':
				fmt = argv[i][1];
				continue;
			}
 		fprintf(stderr, "usage: %s [-q|-v] [-c|-d] [-r|-g|-z|-p]\n\n"
  			"deflate stream compression\n"
			" -q quiet (default)\n"
			" -v verbose\n"
 			" -c compress (default)\n"
 			" -d decompress\n"
			" -r raw (default)\n"
			" -g gzip\n"
			" -z zlib\n"
 			" -p pkzip\n", argv[0]);
		return -1;
	}
 	call = comp == 'c' ? compress_stream : decompress_stream;
	switch (fmt) 
    {
	case 'r':
		header = dummyheader;
		footer = dummyfooter;
		checksum = dummysum;
		n = call(stdin, stdout);
 		break;
 	case 'g':
		if (comp == 'c') 
        {
			header = deflate_gzip_header;
			footer = deflate_gzip_footer;
 		} else {
			header = inflate_gzip_header;
			footer = inflate_gzip_footer;
		}
		checksum = crc32;
		crc32init();
		n = call(stdin, stdout);
		break;
	case 'z':
		if (comp == 'c') 
        {
			header = deflate_zlib_header;
			footer = deflate_zlib_footer;
 		} else {
 			header = inflate_zlib_header;
			footer = inflate_zlib_footer;
		}
		checksum = adler32;
 		n = call(stdin, stdout);
		break;
	case 'p':
		if (comp == 'c') 
        {
			header = deflate_pkzip_header;
 			footer = deflate_pkzip_footer;
		} else {
			header = inflate_pkzip_header;
			footer = inflate_pkzip_footer;
 		}
 		checksum = crc32;
		crc32init();
 		n = call(stdin, stdout);
		break;
	default:
 		err = "uninplemented.";
		n = FlateErr;
		break;
 	}
	if (verbose == 'v')
		fprintf(stderr, "in:%d out:%d checksum: 0x%08x (header:%d data:%d footer:%d extra input:%s)\n",
			nin, nout, sum, headerlen, (comp == 'c' ? nout : nin) - headerlen - footerlen - extralen,
			footerlen, extralen ? "yes" : "no");
	if (n != FlateOk)
		fprintf(stderr, "error: %s\n", err);
	return n;
}