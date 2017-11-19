#include <stdio.h>

#define Middle ((long *)0xfff20000UL)

void main(void)
{
	if(Middle[-2] != 'WgLg')
	{
		printf("The Wedge did not run. Restart with a Mac OS ROM file in your System Folder that has the Wedge built in. This application displays the Wedge log.\n");
		return;
	}

	fwrite((char *)Middle, 1, Middle[-1], stdout);;
}
