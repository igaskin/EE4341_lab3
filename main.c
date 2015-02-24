/* 
 * File:   main.c
 * Author: gask0032
 *
 * Created on February 24, 2015, 10:32 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <p32xxxx.h>
#include <sdmmc.h>
/*
 * 
 */
// I/O definitions
#define SDWP _RG1 // Write Protect input
#define SDCD _RF1 // Card Detect input
#define SDCS _RF0 // Card Select output
// SD card commands
#define RESET 0 // a.k.a. GO_IDLE (CMD0)
#define INIT 1 // a.k.a. SEND_OP_COND (CMD1)
#define READ_SINGLE 17
#define WRITE_SINGLE 24
#define readSPI() writeSPI( 0xFF)
#define clockSPI() writeSPI( 0xFF)
#define disableSD() SDCS = 1; clockSPI()
#define enableSD() SDCS = 0 
void initSD( void)
{
 SDCS = 1; // initially keep the SD card disabled
 _TRISF0 = 0; // make Card select an output pin
 // init the SPI2 module for a slow (safe) clock speed first
 SPI2CON = 0x8120; // ON, CKE=1; CKP=0, sample middle
 SPI2BRG = 71; // clock = Fpb/144 = 250kHz
} // initSD

// send one byte of data and receive one back at the same time
unsigned char writeSPI( unsigned char b)
{
 SPI2BUF=b; // write to buffer for TX
 while( !SPI2STATbits.SPIRBF); // wait transfer complete
 return SPI2BUF; // read the received value
}// writeSPI

int sendSDCmd( unsigned char c, unsigned a)
// c command code
// a byte address of data block
{
 int i, r;
 // enable SD card
 enableSD();
 // send a comand packet (6 bytes)
writeSPI( c | 0x40); // send command
writeSPI( a>>24); // msb of the address
writeSPI( a>>16);
writeSPI( a>>8);
writeSPI( a); // lsb
writeSPI( 0x95); // send CMD0 CRC
// now wait for a response, allow for up to 8 bytes delay
for( i=0; i<8; i++)
{
r=readSPI();
if ( r != 0xFF)
break;
 }
 return ( r);
 // NOTE CSCD is still low!
} // sendSDCmd

int initMedia( void)
// returns 0 if successful
// E_COMMAND_ACK failed to acknowledge reset command
// E_INIT_TIMEOUT failed to initialize
{
 int i, r;
 // 1. with the card NOT selected
 disableSD();
 // 2. send 80 clock cycles start up
 for ( i=0; i<10; i++)
 clockSPI();
 // 3. now select the card
 enableSD();
 // 4. send a single RESET command
 r = sendSDCmd( RESET, 0); disableSD();
 if ( r != 1) // must return Idle
 return E_COMMAND_ACK; // comand rejected
 // 5. send repeatedly INIT until Idle terminates
 for (i=0; i<I_TIMEOUT; i++)
 {
 r = sendSDCmd( INIT, 0); disableSD();
 if ( !r)
 break;
 }
 if ( i == I_TIMEOUT)
 return E_INIT_TIMEOUT; // init timed out

 // 6. increase speed
 SPI2CON = 0; // disable the SPI2 module
 SPI2BRG = 0; // Fpb/(2*(0+1))= 36/2 = 18 MHz
 SPI2CON = 0x8120; // re-enable the SPI2 module
 return 0;
} // init media

#define DATA_START 0xFE
int readSECTOR( LBA a, char *p)
// a LBA of sector requested
// p pointer to sector buffer
// returns TRUE if successful
{
 int r, i;
 // 1. send READ command
 r = sendSDCmd( READ_SINGLE, ( a << 9));
 if ( r == 0) // check if command was accepted
 {

     // 2. wait for a response
     for( i=0; i<R_TIMEOUT; i++)
     {
         r = readSPI();
         if ( r == DATA_START)
         break;
     }
     // 3. if it did not timeout, read 512 byte of data
     if ( i != R_TIMEOUT)
     {
     i = 512;
     do{
     *p++ = readSPI();
     } while (--i>0);
     // 4. ignore CRC
     readSPI();
     readSPI();
     } // data arrived
 } // command accepted
 // 5. remember to disable the card
 disableSD();
 return ( r == DATA_START); // return TRUE if successful
} // readSECTOR

#define DATA_ACCEPT 0x05
int writeSECTOR( LBA a, char *p)
// a LBA of sector requested
// p pointer to sector buffer
// returns TRUE if successful
{
 unsigned r, i;
 // 1. send WRITE command
 r = sendSDCmd( WRITE_SINGLE, ( a << 9));
 if ( r == 0) // check if command was accepted
 {
     // 2. send data
     writeSPI( DATA_START);
     
     // send 512 bytes of data
     for( i=0; i<512; i++)
        writeSPI( *p++);
     // 3. send dummy CRC
     clockSPI();
     clockSPI();
     
     // 4. check if data accepted
     r = readSPI();
     if ( (r &0xf) == DATA_ACCEPT)
     {
         // 5. wait for write completion
         for( i=0; i<W_TIMEOUT; i++)
         {
             r = readSPI();
             if ( r != 0 )
             break;
         }
     } // accepted
     else
        r = FAIL;
 } // command accepted
 // 6. remember to disable the card
 disableSD();
 return ( r); // return TRUE if successful
} // writeSECTOR

int main(int argc, char** argv) {
    initSD();
    initMedia();
    return (EXIT_SUCCESS);
}

