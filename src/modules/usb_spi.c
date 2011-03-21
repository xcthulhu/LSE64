/*
    Copyright 2011 John P. Doty and Matthew P. Wampler-Doty

    This file is part of LSE64.

    LSE64 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    LSE64 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LSE64; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdlib.h>
#include <string.h>
#include "lse64.h"
#include "config.h"
#include "libsub.h"

/* 
    SPIerror yields nothing
    
    Prints whatever error message this module last set 
*/
static char spierr[64];
void SPIerror(void) {
	printf("%s\n",spierr);
}

/*
# In the following:
# chan is the SPI channel number on the local SPI interface
# id is the peripheral id
*/

/* 
# Channels are interpreted as xdimax SUB20 devices.  Associated with
# each device is a number representing its configuration, and the number
# of bits per transaction.
*/  

typedef struct {
	unsigned int conf, bits, baud;
	sub_handle * sh;
} channel;


/*
# We represent all of the channels as an array.
# We initialize this array by detecting all of the devices,
# iteratively lengthening our array to accomodate all of them.  
# Note that we first clear this array if necessary, which is governed
# by a seperate cleanup function. 
*/

static channel * spi = NULL;
static int chanN = 0;

void SPIchan_clear(void) {
  if (spi != NULL) {
    int i;
    for(i = 0; i < chanN; i++) sub_close(spi[i].sh);
    free(spi);
  }
  chanN = 0;
  spi = NULL;
}

void SPIchan_init(void) {
  struct usb_device* dev = NULL;
  flag = 0;
  // If necessary, clear the channels, free the memory
  SPIchan_clear();

  // Try to detect devices for channels and open
  while(dev = sub_find_devices(dev)) {
    spi = (channel *) realloc(spi,(chanN+1)*sizeof(channel));
    spi[chanN].sh = sub_open(dev);
    if (!spi[chanN].sh) {
      SPIchan_clear();
      strcpy(spierr, sub_strerror(sub_errno));
      return;
    }
    spi[chanN].conf = 0;
    spi[chanN].bits = 0;
    spi[chanN].baud = 0;
    chanN += 1;
  }
  flag = 1;
}


// We will commonly want to check whether we can access a channel before trying
int check_chan(int cn) {
	if (spi == NULL) { strcpy(spierr, "Channels uninitialized"); return 0; }
	if (cn >= chanN) { 
		sprintf(spierr, "Channel %d out of range (MAX VALUE: %d)", cn, chanN - 1); 
		return 0; }
	if (spi[cn].sh == NULL) { 
		sprintf(spierr, "Device for channel %d not open", cn); 
		return 0; }
	return 1;
}

/*
# chan id mode bits master SPIinit yields nothing

# mode is conventional SPI mode (0-4)
# bits is the number of bits to transfer per transaction
# master is 1 for master mode, 0 for slave mode

# Initializes the parameters for a particular peripheral. 
# Sets baud rate to 125 kHZ
# In slave mode, only one id per channel is allowed, but in master
# mode you may set up multiple peripherals on a channel.

# ***** BUGS: (1) The Xdimax SUB20 can't have different configurations      *****
# *****           for different IDs, so we ignore id in this configuration  *****
# *****           routine.                                                  *****
# *****                                                                     *****
# *****       (2) SLAVE mode is broken. Does not support data transfer      *****
# *****                                                                     *****
# *****       (3) The SUB20 API can only handle transactions of some        *****
# *****           multiple of a byte at a time, so bits are always          *****
# *****           rounded to the next highest multiple of 8 in practice.    *****
*/

void SPIinit(void) {
	int oldconfig, err; 
        unsigned int cn = *sp++, id = *sp++, mode = *sp++, bits = *sp++, master = *sp++;
	int cpol = mode & 1,        // First bit
            cpha = (mode >> 1) & 1; // Second bit

	if (!check_chan(cn)) {flag = 0; return;}
	if (bits > 64) {
		flag = 0;
		sprintf(spierr, "Bit rate %d invalid; must be < 64 bits per transaction", bits);
		return;
	} else spi[cn].bits = bits;
        // Minimally our configuration must have SPI_ENABLE
	spi[cn].conf = SPI_ENABLE;
	spi[cn].conf |= SPI_MSB_FIRST; // Most significant bit first

	// if the baud rate isn't set, set it to 125kHZ
	if (!spi[cn].baud) spi[cn].baud |= SPI_CLK_125KHZ;

	// See Wikipedia for the meaning of CPOL and CPHA for SPI buses:
	// http://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus
	if      (cpol == 0)   spi[cn].conf |= SPI_CPOL_RISE;
	else if (cpol == 1)   spi[cn].conf |= SPI_CPOL_FALL;
	if      (cpha == 0)   spi[cn].conf |= SPI_SMPL_SETUP;
	else if (cpha == 1)   spi[cn].conf |= SPI_SETUP_SMPL;
	// Master or Slave mode
	if      (master == 0) spi[cn].conf |= SPI_SLAVE; // BROKEN !!!!

	if(!(err = sub_spi_config(spi[cn].sh, spi[cn].conf | spi[cn].baud, &oldconfig))) {
		flag = 1;
		return;
	} else {
		flag = 0;
		strcpy(spierr, sub_strerror(err));
		return;
	}
}

/*
# chan id baud SPIbaud yields nothing
# baud is an integer, units are kilohertz

# sets the baud rate to the nearest supported rate to the specified
# parameter 
# ***** BUG:  ignores id  *****
*/

void SPIbaud(void) {
	int oldconfig, err;
	unsigned int cn = *sp++, id = *sp++, baud = *sp++;
	if (!check_chan(cn)) {flag = 0; return;}

	if ( baud <= 187) spi[cn].baud = SPI_CLK_125KHZ;
	else if (185 < baud && baud <= 375) spi[cn].baud = SPI_CLK_250KHZ;
	else if (375 < baud && baud <= 750) spi[cn].baud = SPI_CLK_500KHZ;
	else if (750 < baud && baud <= 1500) spi[cn].baud = SPI_CLK_1MHZ;
	else if (1500 < baud && baud <= 3000) spi[cn].baud = SPI_CLK_2MHZ;
	else if (3000 < baud && baud <= 6000) spi[cn].baud = SPI_CLK_4MHZ;
	else if (6000 < baud) spi[cn].baud = SPI_CLK_8MHZ;
	
	if(!(err = sub_spi_config(spi[cn].sh, spi[cn].conf | spi[cn].baud, &oldconfig))) flag = 1;
	else {
		flag = 0;
		strcpy(spierr, sub_strerror(err));
		return;
	}
}
	
/*
# chan id odata SPIx yields idata
#
# Performs an SPI data exchange.
# In master mode, spin waits if channel is busy.
# In slave mode, spin waits until master finishes.
*/

// The canonical ordering for Boolean algebras
// See your favorite Lattice Theory text for details
#define entails(X, Y)  (((X) & (Y)) == (X)) 

// We need to chunk/unchunk data from 8 byte pieces
char * chunk(cell odata, int sz) {
	char * out = malloc(sizeof(char)*sz);
	int i = 0;
	while (sz > 0) out[--sz] = (odata >> (8*i++)) & 0xFF;
	return out;
}

cell unchunk( char * in, int sz) {
	cell idata = 0;
	int i;
	for(i = 0; i < sz; i++) idata = (idata << 8*i) | in[i];
	return idata;
}

void SPIx(void) {
	unsigned int cn = *sp++, id = *sp++;
	cell odata = *sp++, idata;
	char * out, * in;
	int sz, err;
	if (!check_chan(cn)) {flag = 0; return;}

	// We cannot write if we are in slave mode
	if (entails(SPI_SLAVE, spi[cn].conf)) {
		flag = 0;
		sprintf(spierr, "Channel %d is in slave mode, cannot write", cn);
		return;
	}

	sz = spi[cn].bits/8 + 1;
	out = chunk(odata, sz); // Chunk data into 8-bit chunks
	in = malloc(sizeof(char)*sz);
	if(!(err = sub_spi_transfer(spi[cn].sh, out, in, sz, SS_CONF(id,SS_LO)))) { 
		flag = 1;
		*sp-- = unchunk(out, sz); // Inverse of chunk
	} else {
		flag = 0;
		strcpy(spierr, sub_strerror(err));
	}
	free(in); free(out);
}

/*
# chan SPIsn yields int
#
# Grabs the serial number (in HEX) from the indicated channel (a xdimax SUB20 device)
*/

void SPIsn(void) {
  unsigned int cn = *sp++;
  unsigned int sn;
  char buf[64];
  if (!check_chan(cn)) {flag = 0; return;}
  if (sub_get_serial_number(spi[cn].sh, buf, sizeof(buf))){
    flag = 1;
    sscanf(buf, "%x", &sn);
    *--sp = sn;
  } else flag = 0;
}
	
/* My Initializer */
void __attribute__((constructor)) mod_init(void) {
  SPIchan_init(); // Initialize the channels
  // Broken; sub_find_device is not implemented correctly!!!
  //build_primitive( SPIchan_clear , "SPIchan_clear" );
  //build_primitive( SPIchan_init , "SPIchan_init" );
  build_primitive( SPIinit , "SPIinit" );
  build_primitive( SPIbaud , "SPIbaud" );
  build_primitive( SPIx , "SPIx" );
  build_primitive( SPIerror , "SPIerror" );
  build_primitive( SPIsn , "SPIsn" );
}
	
/* Test function for LSE Module */
int lse_mod_test(void) { return 1; }
