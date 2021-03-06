/*

Copyright 2011-2017 Tyler Gilbert

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

 */

#include "lpc_devices.h"

#include <string.h>
#include <stdint.h>

#include "isplib.h"

typedef struct {
	char prefix[12];
	uint32_t checksum_addr;
	uint32_t ram_start;
	uint16_t sectors;
	uint16_t sector_table[128];
} lpc_device_t;


#define TOTAL_DEVICES 5

static lpc_device_t devices[] = {
		{
				.prefix = "lpc21",
				.checksum_addr = 0x14,
				.ram_start = 0x40000300
		},
		{
				.prefix = "lpc8",
				.checksum_addr = 0x1C,
				.ram_start = 0x10000400,
				.sectors = 32,
				.sector_table[0] = 1024,
				.sector_table[1] = 1024,
				.sector_table[2] = 1024,
				.sector_table[3] = 1024,
				.sector_table[4] = 1024,
				.sector_table[5] = 1024,
				.sector_table[6] = 1024,
				.sector_table[7] = 1024,
				.sector_table[8] = 1024,
				.sector_table[9] = 1024,
				.sector_table[10] = 1024,
				.sector_table[11] = 1024,
				.sector_table[12] = 1024,
				.sector_table[13] = 1024,
				.sector_table[14] = 1024,
				.sector_table[15] = 1024,
				.sector_table[16] = 1024,
				.sector_table[17] = 1024,
				.sector_table[18] = 1024,
				.sector_table[19] = 1024,
				.sector_table[20] = 1024,
				.sector_table[21] = 1024,
				.sector_table[22] = 1024,
				.sector_table[23] = 1024,
				.sector_table[24] = 1024,
				.sector_table[25] = 1024,
				.sector_table[26] = 1024,
				.sector_table[27] = 1024,
				.sector_table[28] = 1024,
				.sector_table[29] = 1024,
				.sector_table[30] = 1024,
				.sector_table[31] = 1024
		},
		{
				.prefix = "lpc13",
				.checksum_addr = 0x1C,
				.ram_start = 0x10000300,
				.sectors = 8,
				.sector_table[0] = 4096,
				.sector_table[1] = 4096,
				.sector_table[2] = 4096,
				.sector_table[3] = 4096,
				.sector_table[4] = 4096,
				.sector_table[5] = 4096,
				.sector_table[6] = 4096,
				.sector_table[7] = 4096
		},

		{
				.prefix = "lpc17",
				.checksum_addr = 0x1C,
				.ram_start = 0x10000300,
				.sectors = 30,
				.sector_table[0] = 4096,
				.sector_table[1] = 4096,
				.sector_table[2] = 4096,
				.sector_table[3] = 4096,
				.sector_table[4] = 4096,
				.sector_table[5] = 4096,
				.sector_table[6] = 4096,
				.sector_table[7] = 4096,
				.sector_table[8] = 4096,
				.sector_table[9] = 4096,
				.sector_table[10] = 4096,
				.sector_table[11] = 4096,
				.sector_table[12] = 4096,
				.sector_table[13] = 4096,
				.sector_table[14] = 4096,
				.sector_table[15] = 4096,
				.sector_table[16] = 32768,
				.sector_table[17] = 32768,
				.sector_table[18] = 32768,
				.sector_table[19] = 32768,
				.sector_table[20] = 32768,
				.sector_table[21] = 32768,
				.sector_table[22] = 32768,
				.sector_table[23] = 32768,
				.sector_table[24] = 32768,
				.sector_table[25] = 32768,
				.sector_table[26] = 32768,
				.sector_table[27] = 32768,
				.sector_table[28] = 32768,
				.sector_table[29] = 32768,
				.sector_table[30] = 32768,
				.sector_table[31] = 32768
		},

		{
				.prefix = "lpc40",
				.checksum_addr = 0x1C,
				.ram_start = 0x10000300,
				.sectors = 30,
				.sector_table[0] = 4096,
				.sector_table[1] = 4096,
				.sector_table[2] = 4096,
				.sector_table[3] = 4096,
				.sector_table[4] = 4096,
				.sector_table[5] = 4096,
				.sector_table[6] = 4096,
				.sector_table[7] = 4096,
				.sector_table[8] = 4096,
				.sector_table[9] = 4096,
				.sector_table[10] = 4096,
				.sector_table[11] = 4096,
				.sector_table[12] = 4096,
				.sector_table[13] = 4096,
				.sector_table[14] = 4096,
				.sector_table[15] = 4096,
				.sector_table[16] = 32768,
				.sector_table[17] = 32768,
				.sector_table[18] = 32768,
				.sector_table[19] = 32768,
				.sector_table[20] = 32768,
				.sector_table[21] = 32768,
				.sector_table[22] = 32768,
				.sector_table[23] = 32768,
				.sector_table[24] = 32768,
				.sector_table[25] = 32768,
				.sector_table[26] = 32768,
				.sector_table[27] = 32768,
				.sector_table[28] = 32768,
				.sector_table[29] = 32768,
				.sector_table[30] = 32768,
				.sector_table[31] = 32768
		}
};


static uint32_t lpc_device_lookup_sector_by_dev(int dev, uint32_t addr);

int32_t lpc_device_get_checksum_addr(const char * dev){
	int i;
	for(i=0; i < TOTAL_DEVICES; i++){
		isplib_debug(3, "Checking for %s at 0x%X\n", devices[i].prefix, devices[i].checksum_addr);
		if ( !strncmp(dev, devices[i].prefix, strlen(devices[i].prefix)) ){
			return devices[i].checksum_addr;
		}
	}
	return -1;
}


uint32_t lpc_device_get_ram_start(const char * dev){
	int i;
	for(i=0; i < TOTAL_DEVICES; i++){
		if ( !strncmp(dev, devices[i].prefix, strlen(devices[i].prefix)) ){
			return devices[i].ram_start;
		}
	}
	return -1;
}

uint32_t lpc_device_get_sector_number(const char * dev, uint32_t addr){
	int i;
	for(i=0; i < TOTAL_DEVICES; i++){
		if ( !strncmp(dev, devices[i].prefix, strlen(devices[i].prefix)) ){
			return lpc_device_lookup_sector_by_dev(i, addr);
		}
	}
	return -1;
}

uint32_t lpc_device_lookup_sector_by_dev(int dev, uint32_t addr){
	uint32_t tmp = 0;
	int i;
	i = 0;
	do {
		tmp += devices[dev].sector_table[i++];
		if ( i == devices[dev].sectors ){
			break;
		}
	} while( tmp <= addr);
	return --i;
}


