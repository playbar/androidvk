#include <cstring>
#include <mylog.h>
#include "vulkan_global.h"

void StrToHex(unsigned char *pbDest, unsigned char *pbSrc, int nLen)
{
	char h1,h2;
	unsigned char s1,s2;
	int i;

	for (i=0; i<nLen; i++)
	{
		h1 = pbSrc[2*i];
		h2 = pbSrc[2*i+1];

		s1 = toupper(h1) - 0x30;
		if (s1 > 9)
			s1 -= 7;

		s2 = toupper(h2) - 0x30;
		if (s2 > 9)
			s2 -= 7;

		pbDest[i] = s1*16 + s2;
	}
}
