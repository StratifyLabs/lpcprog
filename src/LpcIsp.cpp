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

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>

#include <sapi/sys.hpp>

#include "LpcIsp.hpp"

#include "isplib.h"
#include "lpc_devices.h"

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 2
#endif

static const char * device_list[] = {
		"lpc1342",
		"lpc1343",
		"lpc1751",
		"lpc1752",
		"lpc1754",
		"lpc1756",
		"lpc1757",
		"lpc1758",
		"lpc1764",
		"lpc1765",
		"lpc1767",
		"lpc1768",
		"lpc1769",
		"lpc4078",
		NULL
};


int LpcIsp::copy_names(char * device, char * pio0, char * pio1){
	strcpy(device, "lpc");
	strcpy(pio0, "Reset");
	strcpy(pio1, "ISP Req");
	return 0;
}


int LpcIsp::program(const char * filename, int crystal, const char * dev){
	int ret;
	File f;
	u8 image_buffer[LPCPHY_RAM_BUFFER_SIZE];
	int image_page_size = LPCPHY_RAM_BUFFER_SIZE;
	u32 size;
	u32 bytes_read;
	u32 start_address;
	u8 failed;
	u32 bytes_written = 0;

	m_device = dev;

	if( strncmp(dev, "lpc8", 4) == 0 ){
		m_phy.set_max_speed(LpcPhy::MAX_SPEED_9600);
		m_phy.set_uuencode(false);
		m_trace.assign("Program LPC8 mode");
		m_trace.trace_message();
	} else {
		m_phy.set_uuencode(true);
	}

	sys::Timer::wait_msec(10);

	status_printf("Device %s\n", dev);
	status_printf("Image %s\n", filename);

	status_printf("Init programming interface");
	if ( (ret = init_prog_interface(crystal)) < 0 ){
		m_trace.assign("Start interface");
		m_trace.trace_error();
		isplib_error("Failed to start interface");
		return ret;
	}

	status_printf("Erase device");
	sys::Timer::wait_msec(10);
	if ( erase_dev() ){
		m_trace.assign("Erase device");
		m_trace.trace_error();
		isplib_error("Failed to erase device");
		return -1;
	}

	status_printf("Open binary file");
	sys::Timer::wait_msec(10);
	if( f.open(filename, File::READONLY) < 0 ){
		m_trace.assign("Didn't open file");
		m_trace.trace_error();
		isplib_error("Could not open file %s", filename);
		return -1;
	}


	size = f.size();
	sys::Timer::wait_msec(10);
	m_trace.sprintf("Image size:%d", (int)size);
	m_trace.trace_message();


	if (!size){
		isplib_error("Error:  Binary File Error");
		m_trace.assign("Size error");
		m_trace.trace_error();
		return -2;
	}

	isplib_debug(DEBUG_LEVEL, "File size is %d", (int)size);
	memset(image_buffer, 0xFF, image_page_size);

	if( image_page_size > (int)size ){
		image_page_size = size;
	}

	f.seek(0, File::SET);


	if ( (int)(bytes_read = f.read(image_buffer, image_page_size)) != image_page_size ){
		m_trace.assign("Failed to read file");
		m_trace.trace_error();
		isplib_error("Could not read file %s (%d of %d bytes read)", filename, bytes_read, size);
		f.close();
		return -1;
	}


	start_address = 0;

	isplib_debug(DEBUG_LEVEL, "Starting address is %d", start_address);

	status_printf("Write vector checksum");
	//Write the patch to the vector checksum
	if ( write_vector_checksum(image_buffer, dev) ) {
		sys::Timer::wait_msec(10);
		m_trace.assign("failed to set checksum");
		m_trace.trace_error();
		isplib_error("Device %s is not supported", dev);
		printf("Device is not supported");
		f.close();
		return -1;
	}

	m_phy.set_ram_buffer( lpc_device_get_ram_start(dev) );
	snprintf(m_trace.cdata(), m_trace.capacity(), "RAM Start 0x%lX", m_phy.ram_buffer());
	m_trace.trace_message();

	//Write the program memory
	failed = 0;
	bytes_written = 0;
	status_printf("Programming %d bytes at 0x%lX", size, start_address);
	do {

		if( bytes_written > 0 ){
			bytes_read = f.read(image_buffer, image_page_size);
		}

		if( bytes_read > 0 ){
			if ( !write_progmem(image_buffer, start_address + bytes_written, bytes_read, 0, 0) ){
				m_trace.assign("failed to write image");
				m_trace.trace_error();
				failed = 1;
				status_printf("Failed to write program memory");
				f.close();
				return -1;
			}

			bytes_written += bytes_read;
		} else {
			break;
		}

		if ( update_progress(bytes_written, size) ){
			m_trace.assign("Aborted");
			m_trace.trace_warning();
			return 0; //abort requested
		}


	} while( bytes_written < size );
	prog_shutdown();

	f.close();

	if ( !failed && (bytes_written == size) ){
		status_printf("Device Successfully Programmed");
	} else {
		status_printf("Device Failed to program correctly");
		return -1;
	}


	return 0;

}

int LpcIsp::read(const char * filename, int crystal, const char * dev){

	File f;
	int bytes_read;
	int bytes_total;
	char data[LPCPHY_RAM_BUFFER_SIZE];

	m_device = dev;

	if( strncmp(dev, "lpc8", 4) == 0 ){
		m_phy.set_max_speed(LpcPhy::MAX_SPEED_38400);
		m_phy.set_uuencode(false);

		printf("LPC8xx mode");
		m_trace.assign("Read LPC8 mode");
		m_trace.trace_message();
	} else {
		m_phy.set_max_speed(LpcPhy::MAX_SPEED_38400);
		m_phy.set_uuencode(true);
	}

	printf("device is %s", m_device);

	if ( init_prog_interface(crystal) ){
		status_printf("Failed to init prog interface");
		return -1;
	}

	if( f.create(filename) < 0 ){
		status_printf("Could not create file %s", filename);
		return -2;
	}

	bytes_total = 0;
	memset(data, 0x00, LPCPHY_RAM_BUFFER_SIZE);
	while( (bytes_read = m_phy.read_memory(bytes_total, data, LPCPHY_RAM_BUFFER_SIZE)) > 0 ){
		printf("Read %d bytes", bytes_read);
		if ( f.write(data, bytes_read) != bytes_read ){
			f.close();
			printf("Failed to write data to file");
			return -1;
		}
		bytes_total += bytes_read;
		if( bytes_total >= 1024 ){
			break;
		}
		memset(data, 0x00, LPCPHY_RAM_BUFFER_SIZE);

	}

	f.close();

	return 0;
}

char ** LpcIsp::getlist(){
	return (char**)device_list;
}


int LpcIsp::init_prog_interface(int crystal){
	int ret;
	//Open the ISP interface using phy.open()
	if ( ( ret = m_phy.open(crystal)) == 0 ){
		return 0;
	}

	m_phy.close();
	return ret;
}


u32 LpcIsp::read_progmem(void * data, u32 addr, u32 size, bool (*progress)(void*,int, int), void * context){
	u32 buffer_size;
	u32 bytes_read;
	u16 page_size;

	//First read the buffer size
	buffer_size = LPCPHY_RAM_BUFFER_SIZE;
	bytes_read = 0;

	do {
		if ( (size-bytes_read) > buffer_size ){
			page_size = buffer_size;
		} else {
			page_size = size-bytes_read;
		}

		if ( m_phy.read_memory(addr + bytes_read, &((char*)data)[bytes_read], page_size) != page_size ){
			status_printf("Error reading data at address 0x%04lX", (u32)(addr + bytes_read));
			return -1;
		}

		bytes_read += page_size;

		if ( progress ){
			if ( progress(context, bytes_read, size) != 0 ){
				return 0; //abort requested
			}
		}

	} while ( bytes_read < size );

	return bytes_read;
}

u32 LpcIsp::write_progmem(void * data, u32 addr, u32 size, bool (*progress)(void*,int, int), void * context){
	int bytes_written;
	int page_size;
	int j;
	int sector;
	//char err;

	//First read the buffer size
	bytes_written = 0;
	do {

		if ( (int)(size-bytes_written) > LPCPHY_RAM_BUFFER_SIZE ){
			page_size = LPCPHY_RAM_BUFFER_SIZE;
		} else {
			page_size = size-bytes_written;
		}

		//Check to see if data is already all 0xFF
		for(j=0; j < page_size; j++){
			if ( ((unsigned char*)data)[bytes_written+j] != 0xFF ){
				break;
			}
		}

		if ( j < page_size ){ //only write if data has non 0xFF values
			isplib_debug(DEBUG_LEVEL+1, "lpc_wr_pgmmem():Writing page starting at %d", addr + bytes_written);
			sector = lpc_device_get_sector_number(m_device, addr+bytes_written);
			if ( m_phy.write_memory(addr + bytes_written,
					&((char*)data)[bytes_written], page_size,
					sector ) != page_size ){
				printf("failed writing data at address 0x%04lX", (u32)(addr + bytes_written));
				m_trace.sprintf("failed to write 0x%04lX", (u32)(addr + bytes_written));
				m_trace.trace_error();
				return 0;
			}
		}
		//delay_ms(2);
		bytes_written+=page_size;
	} while( bytes_written < (int)size );

	return bytes_written;
}


int LpcIsp::erase_dev(){
	int sectors;
	int ret;

	//first see how many sectors there are
	sectors = 0;
	do {
		ret = m_phy.prep_sector(0, sectors);
		if ( !ret ){
			sectors++;
		}
	} while ( !ret );

	status_printf("Erase %d sectors", sectors);
	//Now erase all the sectors
	ret = m_phy.erase_sector(0, --sectors);
	if ( ret != 0 ){
		isplib_error("Failed to erase device");
		return -1;
	}

	ret = m_phy.blank_check_sector(1, sectors);
	if ( ret < 0 ){
		isplib_error("Device not blank");
		return ret;
	}

	return 0;
}

int LpcIsp::write_vector_checksum(unsigned char * hex_buffer, const char * dev){
	u16 i;
	int32_t addr;
	u32 check;
	u32 * hex32 = (u32*)hex_buffer;

	//Get the device specific checksum address
	addr = lpc_device_get_checksum_addr(dev);

	if ( addr < 0 ){
		m_trace.trace_error();
		status_printf("Device %s not supported", dev);
		return -1;
	}

	check = 0;
	for(i=0; i < addr/4; i++){
		check += hex32[i];
	}

	check = (u32)(check*-1);

	hex32[addr/4] = check;

	status_printf("Position %lX patched: checksum = 0x%08lX", addr, check);
	return 0;
}

int LpcIsp::prog_shutdown(){
	int err;
	isplib_debug(DEBUG_LEVEL, "Restarting the device");

	err = m_phy.reset();
	if ( err ){
		isplib_error("Could not reset device (%d)", err);
		return -1;
	}

	err = m_phy.close();
	if ( err ){
		isplib_error("Failed to close ISP interface (%d)", err);
		return -1;
	}

	return 0;
}

bool LpcIsp::update_progress(int progress, int max){

	if( m_progress_callback ){
		return m_progress_callback(m_context, progress, max);
	}
	return false;
}

bool LpcIsp::status_printf(const char * format, ...){
	if( m_status_callback ){
		char buffer[256];
		buffer[255] = 0;
		va_list args;
		va_start (args, format);
		vsnprintf(buffer, 255, format, args);
		return m_status_callback(m_context, buffer);
	}
	return false;
}

