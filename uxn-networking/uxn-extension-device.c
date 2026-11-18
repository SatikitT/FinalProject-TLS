/*
Uxn extension device

My idea is to have a single device for all my POSIX extensions, and probably for the Parallel device as well. It works very simple: we assume the last byte of the device is used for selection. The 15 other bytes as extension-specific
*/

/* In Uxntal

|d0 @Extension $f &sel $1 

|d0 @Network ( 0 ) &socket $1 ( 1 ) &addr $2 ( 3 ) &opt $1 ( 4 ) &optval $1 ( 5 ) &buf $2 ( 7 ) &len $2 ( 9 ) &flags $1 ( 10 ) &status $1 ( 11 ) &call $1 
|d0 @Inet ( 0 ) &addr $2 ( 2 ) &hostname $2
|d0 @Select ( 0 ) &nfds $1 ( 1 ) &fdsets $2 ( 3 ) &timeout $2

|d0 @Threads &cmd $1 &errno $1 &a0 $2 &a1 $2 &a2 $2 &r0 $2 &uselocalstorage $1

|d0 @Processes &vector $2 &command $1 &input $1 &register $1 &pointer $2

|d0 @Parallel

%use-network { #01 .Extension/sel DEO }
%use-threads { #02 .Extension/sel DEO }
%use-process { #03 .Extension/sel DEO }
%use-parallel { #04 .Extension/sel DEO }

Inside the device, we have a persistent array of 256 bytes (16 extensions) and a persistent `selected_ext` variable.

The Uxntal program writes to/reads from the 15 bytes in the Extension, which are aliased to the actual devices used. 

A call to DEO/DEI results in a copy to/from the persistent `ext_devices` space to the `uxn.dev` space 
*/

#define EXT_DEVICE 0xd0

static uint8_t selected_ext = 0;
// I simply extend uxn.dev to be 512 
// static uint8_t ext_devices[256];

void extension_deo(uint8_t addr) {
	uint8_t rel_addr = addr - EXT_DEVICE ;
	if (rel_addr == 0x0f) {
		 selected_ext = uxn.dev[addr];
	} else {
	    uxn.dev[0x100+(selected_ext-1)*16+addr] = uxn.dev[addr];
	    switch (selected_ext) {
		    case 0x01: network_deo(addr); break;
		    case 0x02: parallel_deo(addr); break;
		    case 0x03: threads_deo(addr); break;
		    case 0x04: process_deo(addr); break;
		    case 0x05: callstack_deo(addr); break;
		    case 0x06: extmem_deo(addr); break
		    default: return;
	    }
	}
}

void extension_dei(uint8_t addr) {
    uxn.dev[addr] = uxn.dev[0x100+(selected_ext-1)*16+addr];
	return uxn.dev[addr];
}

