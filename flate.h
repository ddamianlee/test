typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

/* deflate and inflate return values */
enum 
{
 	FlateOk  = 0,
 	FlateErr = -1,
	FlateIn  = -2,
 	FlateOut = -3
};
 
typedef struct FlateStream
{
 	int nin;
 	int nout;
	uchar *in;
 	uchar *out;
 	char *err;
 	void *state;
 } FlateStream;
 
 int deflate(FlateStream *s);
 int inflate(FlateStream *s);

 uint adler32(uchar *p, int n, uint adler);
 void crc32init(void);
 uint crc32(uchar *p, int n, uint crc);