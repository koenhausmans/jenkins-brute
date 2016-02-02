#include <stdio.h>
#include <stdlib.h>

#include "jenkins.hpp"

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define mix(a,b,c) \
{ \
	a -= c;  a ^= rot(c, 4);  c += b; \
	b -= a;  b ^= rot(a, 6);  a += c; \
	c -= b;  c ^= rot(b, 8);  b += a; \
	a -= c;  a ^= rot(c,16);  c += b; \
	b -= a;  b ^= rot(a,19);  a += c; \
	c -= b;  c ^= rot(b, 4);  b += a; \
}

#define final(a,b,c) \
{ \
	c ^= b; c -= rot(b,14); \
	a ^= c; a -= rot(c,11); \
	b ^= a; b -= rot(a,25); \
	c ^= b; c -= rot(b,16); \
	a ^= c; a -= rot(c,4);  \
	b ^= a; b -= rot(a,14); \
	c ^= b; c -= rot(b,24); \
}

unsigned int jenkins(const void *key, int length, unsigned int initval)
{
	unsigned int a,b,c;
	union { const void *ptr; unsigned int i; } u;

	/* Set up the internal state */
	a = b = c = 0xDEADBEEF + ((unsigned int)length) + initval;

	u.ptr = key;

	const unsigned int *k = (const unsigned int *)key; /* read 32-bit chunks */

	/*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
	while (length > 12)
	{
		a += k[0];
		b += k[1];
		c += k[2];
		mix(a,b,c);
		length -= 12;
		k += 3;
	}

	/*----------------------------- handle the last (probably partial) block */
	switch(length)
	{
	case 12: c+=k[2];            b+=k[1];            a+=k[0];            break;
	case 11: c+=k[2]&0x00FFFFFF; b+=k[1];            a+=k[0];            break;
	case 10: c+=k[2]&0x0000FFFF; b+=k[1];            a+=k[0];            break;
	case 9 : c+=k[2]&0x000000FF; b+=k[1];            a+=k[0];            break;
	case 8 :                     b+=k[1];            a+=k[0];            break;
	case 7 :                     b+=k[1]&0x00FFFFFF; a+=k[0];            break;
	case 6 :                     b+=k[1]&0x0000FFFF; a+=k[0];            break;
	case 5 :                     b+=k[1]&0x000000FF; a+=k[0];            break;
	case 4 :                                         a+=k[0];            break;
	case 3 :                                         a+=k[0]&0x00FFFFFF; break;
	case 2 :                                         a+=k[0]&0x0000FFFF; break;
	case 1 :                                         a+=k[0]&0x000000FF; break;
	case 0 : return c;
	}

	final(a,b,c);
	return c;
}
