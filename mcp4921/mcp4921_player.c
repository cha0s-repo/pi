/*
 * wav (PCM) format music player with raspberry pi 2 model B and MCP4921 DAC.
 * program depends on bcm2835 lib which descripted as follow:

// C and C++ support for Broadcom BCM 2835 as used in Raspberry Pi
// http://elinux.org/RPi_Low-level_peripherals
// http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
//
// Author: Mike McCauley
// Copyright (C) 2011-2013 Mike McCauley
// $Id: bcm2835.c,v 1.23 2015/03/31 04:55:41 mikem Exp mikem $

 * Apart that, this program is totally FREE to use.
 Pins connection:
	pi   DAC
  +----+----+ 
  |  4 | VCC| 
  |  19| SDI| 
  |  23| SCK| 
  |  24| CS | 
  |  30| GND| 
  |  34|LDAC| 
  +----+----+ 
*/


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <bcm2835.h>

#define RCHN    0x1000
#define LCHN    0xb000
#define MSIZE   4*1024*1024

struct wav_header{
  	unsigned int CID;
  	unsigned int CSize;
  	unsigned int Fmt;
  	unsigned int Sub1ID;
  	unsigned int Sub1Size;
  	unsigned short AudioFmt;
  	unsigned short ChnNum;
  	unsigned int SampleRate;
  	unsigned int ByteRate;
  	unsigned short BlkAlign;
  	unsigned short BitsPerSp;
  	unsigned int Sub2ID;
  	unsigned int Sub2Size;
}__attribute__((packed));

union trans {
  	unsigned short s;
  	unsigned char c[2];
};

int udelay(int us) {
  	int i,j;
  	int tmp;

  	for (i = 0; i < us; i++){
  		for (j = 0; j < 70; j++) {
  			tmp ++;
  		}
  	}
  	return tmp;
}

int play_wav(int fd)
{
  	unsigned char header[44];
  	unsigned char leav[2];
  	int gap,chn, deep;
  	short cur, old;
  	char shf;
  	int num;
  	char *data;
  	int i,j;
  	struct wav_header *wav = header;
  	union trans tmp;

  	int ret = read(fd, header, 44);

  	if (ret < 0)
  	{
  		printf("Read wav header failed\n");
  		return -1;
  	}

  	gap = 1000000 / wav->SampleRate;

  	deep = wav->BitsPerSp / 8;
  	if (deep < 1 || deep >2) {
  		printf ("%d SampleBits not supported\n", deep);
  		return -1;
  	}

  	chn = wav->ByteRate / wav->SampleRate / deep;
  	if (chn < 1 || chn >2) {
  		printf ("%d channel not supported\n", chn);
  		return -1;
  	}

  	printf ("SampleRate: %d\tByteRate: %d\tBitsPerSample: %d\tLength: %d\tgap: %d\n",
  		wav->SampleRate, wav->ByteRate, wav->BitsPerSp, wav->CSize, gap);

  	data = malloc(MSIZE);
  	if (data == NULL) {
  		printf ( "Malloc failed\n");
  		return -1;
  	}

  	num = read(fd, data, MSIZE);
  	old = 0;
  	i = 0;
  	while (num  > 0)
  	{
  		i += num;

  		for (j = 0; j < num; j+=(chn*deep))
  		{
  			tmp.c[0] = data[j];
  			tmp.c[1] = 0;
  			if (deep > 1) {
  				tmp.c[1] = data[j + 1];
  				tmp.s = (tmp.s + 0x8000) >> 4 & 0x0fff;
  			}else {
  				tmp.s = tmp.s << 4;
  			}
            //tmp.s = old + ( tmp.s-old)/ 32;
            //old = tmp.s;
  			tmp.s += RCHN;
  			shf = tmp.c[0];
  			tmp.c[0] = tmp.c[1];
  			tmp.c[1] = shf;
  			bcm2835_spi_transfernb(tmp.c, leav, 2);

  			if (chn == 2) {
                // chip only has one channel;
  			}

  			udelay(gap);

  		}

  		num = read(fd, data, MSIZE);
  	}
  	return 0;
}

int spi_init(void) {
  	if (!bcm2835_init())
  	{
  		printf("bcm2835_init failed. Are you running as root??\n");
  		return 1;
  	}
  	if (!bcm2835_spi_begin())
  	{
  		printf("bcm2835_spi_begin failedg. Are you running as root??\n");
  		return 1;
  	}

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); // The default
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default

    return 0;
}

int spi_close(void) {
	bcm2835_spi_end();
	bcm2835_close();
}

int main()
{
	int fd;

	if (spi_init())
		return 0;

	fd = open("/home/pi/8.wav", O_RDONLY);
	play_wav(fd);
	close(fd);

	spi_close();
	return 0;
}