/*
File: printf.c

Copyright (C) 2004  Kustaa Nyholm

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "WedgeLibC.h"

typedef void (*putcf) (void*,char);
void asm_putc (char);


#ifdef PRINTF_LONG_SUPPORT

static void uli2a(unsigned long int num, unsigned int base, int uc,char * bf)
	{
	int n=0;
	unsigned int d=1;
	while (num/d >= base)
		d*=base;         
	while (d!=0) {
		int dgt = num / d;
		num%=d;
		d/=base;
		if (n || dgt>0|| d==0) {
			*bf++ = dgt+(dgt<10 ? '0' : (uc ? 'A' : 'a')-10);
			++n;
			}
		}
	*bf=0;
	}

static void li2a (long num, char * bf)
	{
	if (num<0) {
		num=-num;
		*bf++ = '-';
		}
	uli2a(num,10,0,bf);
	}

#endif

static void ui2a(unsigned int num, unsigned int base, int uc,char * bf)
	{
	int n=0;
	unsigned int d=1;
	while (num/d >= base)
		d*=base;        
	while (d!=0) {
		int dgt = num / d;
		num%= d;
		d/=base;
		if (n || dgt>0 || d==0) {
			*bf++ = dgt+(dgt<10 ? '0' : (uc ? 'A' : 'a')-10);
			++n;
			}
		}
	*bf=0;
	}

static void i2a (int num, char * bf)
	{
	if (num<0) {
		num=-num;
		*bf++ = '-';
		}
	ui2a(num,10,0,bf);
	}

static int a2d(char ch)
	{
	if (ch>='0' && ch<='9') 
		return ch-'0';
	else if (ch>='a' && ch<='f')
		return ch-'a'+10;
	else if (ch>='A' && ch<='F')
		return ch-'A'+10;
	else return -1;
	}

static char a2i(char ch, char** src,int base,int* nump)
	{
	char* p= *src;
	int num=0;
	int digit;
	while ((digit=a2d(ch))>=0) {
		if (digit>base) break;
		num=num*base+digit;
		ch=*p++;
		}
	*src=p;
	*nump=num;
	return ch;
	}

static void putchw(int n, char z, char* bf)
	{
	char fc=z? '0' : ' ';
	char ch;
	char* p=bf;
	while (*p++ && n > 0)
		n--;
	while (n-- > 0) 
		asm_putc(fc);
	while ((ch= *bf++))
		asm_putc(ch);
	}

void tfp_format(char *fmt, va_list va)
	{
	char bf[12];
	
	char ch;


	while ((ch=*(fmt++))) {
		if (ch!='%') 
			asm_putc(ch);
		else {
			char lz=0;
#ifdef  PRINTF_LONG_SUPPORT
			char lng=0;
#endif
			int w=0;
			ch=*(fmt++);
			if (ch=='0') {
				ch=*(fmt++);
				lz=1;
				}
			if (ch>='0' && ch<='9') {
				ch=a2i(ch,&fmt,10,&w);
				}
#ifdef  PRINTF_LONG_SUPPORT
			if (ch=='l') {
				ch=*(fmt++);
				lng=1;
			}
#endif
			switch (ch) {
				case 0: 
					goto abort;
				case 'u' : {
#ifdef  PRINTF_LONG_SUPPORT
					if (lng)
						uli2a(va_arg(va, unsigned long int),10,0,bf);
					else
#endif
					ui2a(va_arg(va, unsigned int),10,0,bf);
					putchw(w,lz,bf);
					break;
					}
				case 'd' :  {
#ifdef  PRINTF_LONG_SUPPORT
					if (lng)
						li2a(va_arg(va, unsigned long int),bf);
					else
#endif
					i2a(va_arg(va, int),bf);
					putchw(w,lz,bf);
					break;
					}
				case 'x': case 'X' : 
#ifdef  PRINTF_LONG_SUPPORT
					if (lng)
						uli2a(va_arg(va, unsigned long int),16,(ch=='X'),bf);
					else
#endif
					ui2a(va_arg(va, unsigned int),16,(ch=='X'),bf);
					putchw(w,lz,bf);
					break;
				case 'c' : 
					asm_putc((char)(va_arg(va, int)));
					break;
				case 's' : 
					putchw(w,0,va_arg(va, char*));
					break;
				case '%' :
					asm_putc(ch);
				default:
					break;
				}
			}
		}
	abort:;
	}


extern int printf(char *fmt, ...)
	{
	va_list va;
	va_start(va,fmt);
	tfp_format(fmt,va);
	va_end(va);
	return 0; // should return character count
	}


extern void *memmove(void *dest, void *src, unsigned int n)
{
	unsigned int i;

	char *d = (char *)dest;
	char *s = (char *)src;

	if(dest < src)			/* copy left to right */
	{
		for(i=0; i<n; i++) d[i] = s[i];
	}
	else					/* copy right to left */
	{
		for(i=n; i!=0; i--) d[i-1] = s[i-1];
	}

	return dest;
}


extern void *memcpy(void *dest, void *src, unsigned int n)
{
	return memmove(dest, src, n);
}


extern void *memset(void *dest, int v, unsigned int n)
{
	char *d = (char *)dest;

	while(n) d[--n] = (char)v;

	return dest;
}
