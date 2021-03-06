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

#include "LpcPhy.hpp"

#include "isplib.h"
#include "uu_encode.h"


#define WAIT_RESPONSE_BUFFER_SIZE 256
#define DEBUG_LEVEL 3

#include <unistd.h>

static const char * uart_speeds[] = {
		"115200",
		"76800",
		"57600",
		"38400",
		"19200",
		"9600",
		NULL
};

enum {
	LPC_ISP_RET_CMD_SUCCESS,
	LPC_ISP_RET_INVALID_COMMAND,
	LPC_ISP_RET_SRC_ADDR_ERROR,
	LPC_ISP_RET_DST_ADDR_ERROR,
	LPC_ISP_RET_SRC_ADDR_NOT_MAPPED,
	LPC_ISP_RET_DST_ADDR_NOT_MAPPED,
	LPC_ISP_RET_COUNT_ERROR,
	LPC_ISP_RET_RET7,
	LPC_ISP_RET_RET8,
	LPC_ISP_RET_RET9,
	LPC_ISP_RET_COMPARE_ERROR,
	LPC_ISP_RET_BUSY,
	LPC_ISP_RET_PARAM_ERROR,
	LPC_ISP_RET_ADDR_ERROR,
	LPC_ISP_RET_ADDR_NOT_MAPPED,
	LPC_ISP_RET_CMD_LOCKED,
	LPC_ISP_RET_INVALID_CODE,
	LPC_ISP_RET_INVALID_BAUD_RATE,
	LPC_ISP_RET_INVALID_STOP_BIT,
	LPC_ISP_RET_TOTAL
};

enum {
	LPC_CMD_UNLOCK,
	LPC_CMD_WR_BAUD_RATE,
	LPC_CMD_ECHO,
	LPC_CMD_WR_RAM,
	LPC_CMD_RD_MEM,
	LPC_CMD_PREP_WR_OP,
	LPC_CMD_CPY_RAM_TO_FLASH,
	LPC_CMD_GO,
	LPC_CMD_ERASE_SECTOR,
	LPC_CMD_BLANK_CHECK_SECTOR,
	LPC_CMD_RD_PART_ID,
	LPC_CMD_RD_BOOT_CODE_VERSION,
	LPC_CMD_COMP,
	LPC_CMD_TOTAL
};

#define TIMEOUT 400
#define QUICK_TIMEOUT 500

//static const u32 sector_size0 = 4096;
//static const u32 sector_size1 = (32*1024);


int LpcPhy::init(const UartPinAssignment & pin_assignment){
	//Open UART
	int ret;

	u32 o_flags = Uart::FLAG_IS_PARITY_NONE | Uart::FLAG_IS_STOP1;

	if( m_uart.open(Uart::NONBLOCK | Uart::RDWR) < 0 ){
		isplib_error("Failed to open UART\n");
		m_trace.assign("Open UART");
		m_trace.trace_error();
		return -1;
	}

	if( (ret = m_uart.set_attr(o_flags, 9600, 8, pin_assignment)) < 0 ){
		m_uart.close();

		if( m_uart.open(Uart::NONBLOCK) <  0 ){
			isplib_error("Failed to open UART (second try)\n");
			m_trace.assign("Open UART (2nd Try)");
			m_trace.trace_error();
		}

		if( (ret = m_uart.set_attr(o_flags, 9600, 8, pin_assignment)) < 0 ){
			isplib_error("Failed to set_attr UART %d\n", ret);
			m_trace.sprintf( "Set UART Attr %d", ret);
			m_trace.trace_error();
			return -2;
		}
	}

	if( m_reset.open() <  0 ){
		isplib_error("Failed to init PIO reset %d\n", link_errno);
		m_trace.assign("Init PIO reset");
		m_trace.trace_error();
		return -3;
	} else {
		m_reset.set();
		m_reset.set_attr(Pin::FLAG_SET_OUTPUT);
	}

	if( m_ispreq.init(Pin::FLAG_SET_OUTPUT) < 0 ){
		isplib_error("Failed to init PIO ispreq\n");
		m_trace.assign("Init PIO ispreq");
		m_trace.trace_error();
		return -4;
	}

	return 0;
}

int LpcPhy::exit(){
	m_uart.close();
	m_reset.set_attr(Pin::FLAG_SET_INPUT);
	m_ispreq.set_attr(Pin::FLAG_SET_INPUT);
	m_reset.close();
	m_ispreq.close();
	return 0;
}

int LpcPhy::open(int crystal){
	int i;
	int id;
	int version;
	int err;
	uart_attr_t attr;
	int count;

	if ( m_reset.set() < 0 ){
		isplib_error("Failed to set reset\n");
		m_trace.assign("Set Reset");
		m_trace.trace_error();
		return -1;
	}

	if ( m_ispreq.set() < 0 ){
		isplib_error("Failed to set ispreq\n");
		m_trace.assign("Set ISPREQ");
		m_trace.trace_error();
		return -2;
	}

	m_pin_assignment.copy(attr.pin_assignment);
	attr.o_flags = Uart::FLAG_IS_PARITY_NONE | Uart::FLAG_IS_STOP1;
	attr.width = 8;

	for(i=m_max_speed; uart_speeds[i] != NULL; i++){

		attr.freq = atoi(uart_speeds[i]);
		m_trace.sprintf("Try Sync %ld", attr.freq);
		m_trace.trace_message();

		if ( m_uart.set_attr(attr) < 0 ){
			isplib_error("Failed to set baud rate\n");
			m_trace.sprintf("Set Baud rate %ld", attr.freq);
			m_trace.trace_error();
			return -4;
		}

		isplib_debug(DEBUG_LEVEL, "Testing %s bps\n", uart_speeds[i]);

		count = 0;
		while(count < 5 ){
			count++;

			//Clear the UART RX buffer
			if ( this->start_bootloader() < 0 ){
				isplib_error("failed to start bootloader\n");
				m_trace.assign("Didn't start bootloader");
				m_trace.trace_warning();
				continue;
			}

			m_trace.assign("Started bootloader");
			m_trace.trace_message();

			//Clear the UART receive buffer
			err = this->flush();
			if ( err < 0 ){
				isplib_error("failed to clear the RX buffers (%d) (%d)\n", err, link_errno);
				m_trace.sprintf( "Flush failed %d", err);
				m_trace.trace_warning();
				continue;
			}

			if ( !this->sync_bootloader(crystal) ){
				isplib_debug(DEBUG_LEVEL, "Synchronization Complete at %s bps\n", uart_speeds[i]);
				isplib_debug(DEBUG_LEVEL, "Unlocking the device\n");
				if ( this->unlock("23130") ){
					isplib_error("failed to Unlock\n");
					m_trace.assign("Unlock Failed");
					m_trace.trace_warning();
					continue;
				}

				m_trace.assign("unlocked");
				m_trace.trace_message();

				id = this->read_part_id();
				//The first command after unlock seems to fail so this is called twice
				id = this->read_part_id();
				if ( id ){
					isplib_debug(DEBUG_LEVEL, "Part ID is %d\n", id);
					m_trace.sprintf( "ID:%d", id);
					m_trace.trace_message();
				}

				version = this->read_boot_version();
				if( version ){
					isplib_debug(DEBUG_LEVEL, "Bootloader Version is %d.%d\n", (version>>8)&0xFF, version&0xFF);
					m_trace.sprintf( "Boot version:%d", version);
					m_trace.trace_message();
				}


				return 0;
			} else {
				m_trace.assign("Synchronize Failed");
				m_trace.trace_warning();
			}
		}
	}

	isplib_error("failed to sync speeds\n");
	m_trace.assign("Failed to sync");
	m_trace.trace_error();
	return -8;

}


int LpcPhy::close(){
	return 0;
}

int LpcPhy::flush(){
	char buf[64];
	while( m_uart.read(buf, 64) > 0 ){}
	return m_uart.flush();
}


/*! \brief sets the location of the RAM buffer.
 * \details The RAM memory map varies from device to device.  This function
 * allows the programmer to set the RAM buffer based on the specific device.
 *
 */
void LpcPhy::set_ram_buffer(u32 addr){
	m_ram_buffer = addr;
}


/*! \brief writes a block to the flash memory.
 * \details This function writes to the flash memory.
 * \return Number of bytes written
 */
int LpcPhy::write_memory(u32 loc, const void * buf, int nbyte, u32 sector){
	char page_buffer[LPCPHY_RAM_BUFFER_SIZE];
	u32 bytes_written;
	const char * src_data = (const char*)buf;
	u16 page_size;
	char err;
	int retry;
	bytes_written = 0;
	do {

		if ( nbyte - bytes_written < LPCPHY_RAM_BUFFER_SIZE ){
			page_size = nbyte-bytes_written;
		} else {
			page_size = LPCPHY_RAM_BUFFER_SIZE;
		}

		memset(page_buffer, 0xFF, LPCPHY_RAM_BUFFER_SIZE);
		memcpy(page_buffer, src_data + bytes_written, page_size);

		retry = 0;
		do {
			//first copy the data to RAM
			if ( this->write_ram(m_ram_buffer, page_buffer, LPCPHY_RAM_BUFFER_SIZE) ){
				retry++;
				Timer::wait_msec(100);
			} else {
				break;
			}

		} while( retry < 3 );

		if( retry == 3 ){
			printf("Failed to write RAM 0x%lX\n", m_ram_buffer);
			snprintf(m_trace.cdata(), m_trace.capacity(), "Failed to write RAM");
			m_trace.trace_error();
			return 0;
		}

		retry = 0;
		do {
			//Prepare the target sector
			if ( (err = this->prep_sector(sector, sector)) ){
				retry++;
				Timer::wait_msec(100);
			} else {
				break;
			}

		} while( retry < 3 );

		if( retry == 3 ){
			printf("Failed to prepare sector (%d)", err);
			snprintf(m_trace.cdata(), m_trace.capacity(), "Failed to prepare %d", err);
			m_trace.trace_error();
			return 0;
		}

		retry = 0;
		do {
			//copy from RAM to flash
			if ( this->copy_ram_to_flash(loc, m_ram_buffer, LPCPHY_RAM_BUFFER_SIZE) ){
				retry++;
				Timer::wait_msec(100);
			} else {
				break;
			}

		} while( retry < 3 );

		if( retry == 3 ){
			printf("Failed to copy RAM to flash\n");
			snprintf(m_trace.cdata(), m_trace.capacity(), "Failed to copy %d", LPCPHY_RAM_BUFFER_SIZE);
			m_trace.trace_error();
			return 0;
		}



		retry = 0;
		do {

			//Copy to RAM again, then compare the RAM to the flash
			if ( this->write_ram(m_ram_buffer, page_buffer, LPCPHY_RAM_BUFFER_SIZE) ){
				Timer::wait_msec(100);
				retry++;
			} else {
				break;
			}

		} while( retry < 3 );

		if( retry == 3 ){
			printf("Failed to write RAM second time\n");
			snprintf(m_trace.cdata(), m_trace.capacity(), "Failed to re-write %d", LPCPHY_RAM_BUFFER_SIZE);
			m_trace.trace_error();
			return 0;
		}

		if ( sector ){ //First sector is mapped to the bootloader and won't compare properly

			//Now compare the ram to the flash to see if the operation was successful
			retry = 0;
			do {
				if ( this->compare_memory(m_ram_buffer, loc, LPCPHY_RAM_BUFFER_SIZE) ){
					retry++;
					Timer::wait_msec(100);
				} else {
					break;
				}

			} while( retry < 3 );

			if( retry == 3 ){
				printf("Write to compare memory\n");
				snprintf(m_trace.cdata(), m_trace.capacity(), "Failed to compare\n");
				m_trace.trace_error();
				//isplib_debug(4, "Dumping RAM\n");
				//debug_dump_mem(4, page_buffer, LPCPHY_RAM_BUFFER_SIZE);
				//this->read_flash(page_buffer, addr, LPCPHY_RAM_BUFFER_SIZE);
				//isplib_debug(4, "Dumping FLASH\n");
				//debug_dump_mem(4, page_buffer, LPCPHY_RAM_BUFFER_SIZE);
				return 0;
			}

		}


		loc += page_size;
		bytes_written += page_size;
	} while ( (int)bytes_written < nbyte );

	m_trace.sprintf("Wrote %ld bytes", bytes_written);
	m_trace.trace_message();

	return bytes_written;

}



int LpcPhy::read_memory(u32 loc, void * buf, int nbyte){
	int bytes_read;
	u16 page_size;
	u16 max_page_size;
	bytes_read = 0;
	max_page_size = LPCPHY_RAM_BUFFER_SIZE;
	do {
		if ( nbyte - bytes_read < max_page_size ){
			page_size = nbyte-bytes_read;
		} else {
			page_size = max_page_size;
		}
		if ( this->read_mem(&((char*)buf)[bytes_read], loc + bytes_read, page_size) != page_size ){
			printf("Failed to read memory (%lX)\n", loc + bytes_read);
			return 0;
		}
		bytes_read += page_size;
	} while ( bytes_read < nbyte );
	return bytes_read;
}


/*! \details This function resets the device by
 * holding the reset line low then releasing it high.
 * \return Zero on success or an HWPL error code.
 */
int LpcPhy::reset(){

	isplib_debug(DEBUG_LEVEL, "RESET\n");

	//Make sure the ISP request is high (disabled)
	if ( m_reset.set() ){
		isplib_error("failed to set ISP REQ\n");
		return -1;
	}

	Timer::wait_msec(50);

	//Hold the reset line low
	if ( m_reset.clear() ){
		isplib_error("failed to clear RESET\n");
		return -1;
	}
	//Wait
	Timer::wait_msec(50);

	//Now push the reset line high
	if ( m_reset.set() ){
		isplib_error("failed to set RESET\n");
		return -1;
	}
	return 0;

}


/*! \brief resets the MCU with a request to start the ISP.
 * \details This function resets the MCU and holds the ISP hardware request line low.
 * \return Zero on success
 */
int LpcPhy::start_bootloader(){
	isplib_debug(DEBUG_LEVEL, "start bootloader\n");

	isplib_debug(DEBUG_LEVEL+1, "RST and REQ are high\n");

	if( m_reset.set() || m_ispreq.set() ){
		isplib_error("failed to set reset/ispreq()\n");
		return -1;
	}
	isplib_debug(DEBUG_LEVEL+1, "RST is high\n");

	if ( m_ispreq.clear() < 0 ){
		isplib_error("failed to clear ispreq\n");
		return -1;
	}
	isplib_debug(DEBUG_LEVEL+1, "REQ is low\n");
	Timer::wait_msec(150);


	if ( m_reset.clear() < 0 ){
		isplib_error("failed to clear reset\n");
		return -1;
	}
	isplib_debug(DEBUG_LEVEL+1, "RST is low\n");
	Timer::wait_msec(20);

	if ( m_reset.set() < 0 ){
		isplib_error("failed to set reset\n");
		return -1;
	}
	isplib_debug(DEBUG_LEVEL+1, "RST is high\n");
	Timer::wait_msec(50);

	if ( m_ispreq.set() < 0 ){
		isplib_error("failed to set ispreq\n");
		return -1;
	}
	isplib_debug(DEBUG_LEVEL+1, "REQ is high\n");
	Timer::wait_msec(150);

	return 0;

}


/*! \brief synchronizes communication with the ISP bootloader.
 * \details This function syncs communication with the ISP bootloader.  The
 * follow exchange takes place:
 *  - Host sends '?'
 *  - Host receives "Synchronized<CR><LF>"
 *  - Host sends "Synchronized<CR><LF>"
 *  - Host receives "OK<CR><LF>"
 *  - Host sends the crystal frequency in KHz (e.g. "10000<CR><LF>")
 *  - Host receives "OK<CR><LF>"
 *
 *  Once this takes place the bootloader is synchronized with the serial port.
 */

int LpcPhy::sync_bootloader(u32 crystal /*! Crystal frequency in KHz */){
	char buf[64];
	u32 bytes;
	//u8 len;
	//send a ?
	char c;

	isplib_debug(DEBUG_LEVEL+1, "Sending ?\n");
	sprintf(buf, "?");
	bytes = m_uart.write(buf, strlen(buf));
	if ( bytes != strlen(buf) ){
		isplib_error("failed to write ? (%d, %d)\n", bytes, link_errno);
		m_trace.assign("Write ?");
		m_trace.trace_error();
		return -1;
	}


	isplib_debug(DEBUG_LEVEL+1, "Waiting for 'Synchronized'\n");
	if( wait_response("Synchronized\r\n", QUICK_TIMEOUT) ){
		isplib_error("failed to wait for synchronized\n");
		m_trace.assign("Wait Synchronized");
		m_trace.trace_error();
		return -1;
	}

	m_trace.assign("Synchronize done");
	m_trace.trace_message();

	//send "Synchronized<CR><LF>"
	isplib_debug(DEBUG_LEVEL+1, "Sending 'Synchronized'\n");
	sprintf(buf, "Synchronized\r\n");
	bytes = m_uart.write(buf, strlen(buf));
	if ( bytes != strlen(buf) ){
		isplib_error("failed to write synchronized\n");
		m_trace.assign("Send Synchronized");
		m_trace.trace_error();
		return -1;
	}

	//wait echo
	isplib_debug(DEBUG_LEVEL+1, "Waiting 'Synchronized OK'\n");
	if( wait_response("Synchronized\r", QUICK_TIMEOUT) ){
		isplib_error("failed to wait for synchronizeed (2nd time)\n");
		m_trace.assign("Wait Sync OK");
		m_trace.trace_error();
		return -1;
	}

	m_trace.assign("Sync OK");
	m_trace.trace_message();

	if( m_uart.read(&c,1) ){
		if( c == '\n' ){
			Timer::wait_msec(10);
			m_trace.assign("Set return code newline");
			m_trace.trace_message();
			set_return_code_newline(true);
			if( wait_response("OK\r\n", QUICK_TIMEOUT) ){
				isplib_error("failed to get OK for lpc177x_8x\n");
				return -1;
			}


			//send crystal freq
			isplib_debug(DEBUG_LEVEL+1, "Sending Crystal Value\n");
			sprintf(buf, "%d\r\n", (int)crystal);
			bytes = m_uart.write(buf, strlen(buf));
			if ( bytes != strlen(buf) ){
				isplib_error("failed to write crystal value\n");
				return -1;
			}
			//Wait for the echo
			isplib_debug(DEBUG_LEVEL+1, "Waiting for Crystal echo\n");

			sprintf(buf, "%d\r\n", (int)crystal);
			if( wait_response(buf, QUICK_TIMEOUT) ){
				isplib_error("failed to wait for crystal Echo\n");
				return -1;
			}

			//Wait for the echo
			isplib_debug(DEBUG_LEVEL+1, "Waiting for Crystal OK\n");

			sprintf(buf, "OK\r\n");
			if( wait_response(buf, QUICK_TIMEOUT) ){
				isplib_error("failed to wait for crystal OK\n");
				return -1;
			}

			m_trace.sprintf( "Crystal OK");
			m_trace.trace_message();

		} else {
			m_trace.assign("Clear return code newline");
			m_trace.trace_message();
			set_return_code_newline(false);
			if( wait_response("K\r\n", QUICK_TIMEOUT) ){
				isplib_error("failed to get OK on lpc17xx\n");
				return -1;
			}
			//send crystal freq
			isplib_debug(DEBUG_LEVEL+1, "Sending Crystal Value\n");
			sprintf(buf, "%d\r\n", (int)crystal);
			bytes = m_uart.write(buf, strlen(buf));
			if ( bytes != strlen(buf) ){
				isplib_error("failed to write crystal value\n");
				return -1;
			}
			//Wait for the echo
			isplib_debug(DEBUG_LEVEL+1, "Waiting for Crystal OK\n");

			sprintf(buf, "%d\rOK\r\n", (int)crystal);
			if( wait_response(buf, QUICK_TIMEOUT) ){
				isplib_error("failed to wait for crystal OK\n");
				return -1;
			}

			Timer::wait_msec(10);
			m_trace.sprintf( "Crystal OK");
			m_trace.trace_message();
		}
	}







	m_echo = 1; //by default echo is on
	this->disable_echo();

	return 0;

}

/*! \brief unlocks the device.
 * \details This function sends the unlock code in order to allow erase and write
 * operations on the flash memory.
 * \return Zero no success
 */

int LpcPhy::unlock(const char * unlock_code){
	char buf[64];
	u16 ret;
	isplib_debug(DEBUG_LEVEL+1, "unlock\n");

	sprintf(buf, "U %s", unlock_code);
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to unlock\n");
		return -1;
	}
	return ret;
}


/*! \brief disables echoing from the device.
 * \details This function turns the echo off.
 * \return Zero on success
 */
int LpcPhy::disable_echo(){
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "echo off\n");

	if( (ret = send_command("A 0", QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to turn echo off\n");
		return -1;
	}

	if ( !ret ){
		m_echo = 0;
	} else{
		isplib_debug(DEBUG_LEVEL+1, "Echo is still on\n");
		m_echo = 1;
	}

	return ret;
}

/*! \brief enables the echo on the device.
 * \details This function causes the device to echo the serial data which it receives.
 * \return Zero on sucess or an LPC return code.
 */
int LpcPhy::enable_echo(){
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "echo on\n");

	if( (ret = send_command("A 1", QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to turn echo on\n");
		return -1;
	}
	return ret;
}


/*! \brief writes a block of memory to RAM.
 * \details This function writes a block of memory to RAM.
 * \return Zero on success
 */
int LpcPhy::write_ram(u32 ram_dest /*! The RAM destination address--must be a word boundary */,
		void * src /*! A pointer to the source data */,
		u32 size /*! The number of bytes to write--must be a multiple of 4 */){
	char buf[64];
	u32 bytes_written;
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "wr ram 0x%X %d\n", ram_dest, size);

	sprintf(buf, "W %d %d", (int)ram_dest, (int)size);
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to write ram\n");
		return -1;
	}


	if ( ret == 0 ){
		bytes_written = write_data(src, size);
		if ( bytes_written != size ){
			isplib_error("failed to write ram %d != %d\n", bytes_written, size);
			return -1;
		}
	}

	return ret;
}


/*! \details This function reads a block of memory from the device.
 * \return Number of bytes read
 */

u32 LpcPhy::read_mem(void * dest /*! A pointer to the destination memory */,
		u32 src_addr /*! The source address to read--must be a word boundary */,
		u32 size /*! The number of bytes to read--must be a multiple of 4 */){
	char buf[64];
	u32 bytes_read;
	int16_t ret;
	//u8 i;
	isplib_debug(DEBUG_LEVEL+1, "rd mem\n");

	if ( size % 4 ){
		printf("Size is not aligned\n");
		return -1;
	}

	printf("Read 0x%lX %ld\n", src_addr, size);
	snprintf(buf, 63, "R %d %d", (int)src_addr, (int)size);
	m_uart.flush();

	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to read ram\n");
		return -1;
	}

	if ( ret ){
		isplib_debug(DEBUG_LEVEL+1, "Error reading memory (%d)\n", ret);
		return 0;
	}


	bytes_read = read_data(dest, size);
	if ( bytes_read != size ){
		printf("Byte read mismatch (%ld != %ld)\n", bytes_read, size);
		return 0;
	}

	bytes_read = size;
	printf("Read-- %ld bytes\n", bytes_read);
	return bytes_read;
}

/*! \details This function ares the specified sectors for writing.  The sector
 * must be prepared for writing before the erase or write (copy ram to flash) operations
 * are performed on the flash memory.
 * \return Zero on success
 *
 * \sa LpcPhy::cpy_ram_to_flash(), LpcPhy::erase_sector()
 *
 */
int LpcPhy::prep_sector(u32 start /*! The first sector to prepare */,
		u32 end /*! The last sector to prepare--must be >= start */){
	char buf[64];
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "prep sectors\n");
	sprintf(buf, "P %d %d", (int)start, (int)end);
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to prep sector %d %d\n", start, end);
		return -1;
	}
	return ret;
}

/*! \brief copies a block of memory from RAM to flash.
 * \details This function copies a block of memory from RAM to flash.  The sectors
 * which are written must be prepared before writing.
 * \return Zero on success
 * \sa LpcPhy::prep_sector()
 */
int LpcPhy::copy_ram_to_flash(u32 flash_addr /*! The flash address (destination memory) */,
		u32 ram_addr /*! The RAM address (source memory) */,
		u32 size /*! The number of bytes to copy */){
	char buf[64];
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "ram to flash\n");
	sprintf(buf, "C %d %d %d", (int)flash_addr, (int)ram_addr, (int)size);
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to copy ram to flash %d %d %d\n", flash_addr, ram_addr, size);
		return -1;
	}
	return ret;
}

/*! \brief begins code execution.
 * \details This function causes the device to start executing code at the specified
 * address in the specified mode (Thumb or ARM).
 * \return Zero on success
 */
int LpcPhy::go(u32 addr /*! Where to start code execution */,
		char mode /*! 'T' for thumb mode and 'A' for arm mode--default is 'A' */){
	char buf[64];
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "go\n");
	sprintf(buf, "G %d %c", (int)addr, mode);
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to go %d %c\n", addr, mode);
		return -1;
	}
	return ret;
}

/*! \brief erases the specified sectors.
 * \details This function erases the specified flash sectors.  Each sector must
 * be prepared before it is erased.
 * \return Zero on success
 * \sa LpcPhy::prep_sector()
 */
int LpcPhy::erase_sector(u32 start /*! The first sector to erase */,
		u32 end /*! The last sector to erase--must be >= start */){
	char buf[64];
	u16 ret;
	isplib_debug(DEBUG_LEVEL+1, "erase sector\n");
	sprintf(buf, "E %d %d", (int)start, (int)end);
	if( (ret = send_command(buf, TIMEOUT, 150)) < 0 ){
		isplib_error("Failed to erase sector %d %d\n", start, end);
		return -1;
	}
	return ret;

}

/*! \details This function checks if the specified sectors are blank.
 * \return Zero if sectors are blank
 */
int LpcPhy::blank_check_sector(u32 start /*! The first sector to blank check */,
		u32 end /*! The last sector to blank check--must be >= start */){
	char buf[LPCPHY_RAM_BUFFER_SIZE];
	u16 ret;
	isplib_debug(DEBUG_LEVEL+1, "blank check\n");
	sprintf(buf, "I %d %d", (int)start, (int)end);
	if( (ret = send_command(buf, TIMEOUT)) < 0 ){
		isplib_error("Failed to blank check sector %d %d\n", start, end);
		return -1;
	}

	if ( ret != 0 ){
		while( get_line(buf, LPCPHY_RAM_BUFFER_SIZE, QUICK_TIMEOUT) ){
			Timer::wait_msec(100);
			isplib_debug(DEBUG_LEVEL+1, "Sector not blank %s\n", buf);
		}
	}

	return ret;
}

/*! \brief reads the part ID.
 * \details This function reads the part ID.
 * \return The part ID value
 */
u32 LpcPhy::read_part_id(){
	char buf[64];
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "read part id\n");
	sprintf(buf, "J");
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to read part id\n");
		return -1;
	}
	if ( ret == 0 ){
		get_line(buf, 64, TIMEOUT);
		ret = atoi(buf);
	}
	return ret;
}

/*! \brief reads the boot loader version.
 * \details This function reads the boot loader version.
 * \return The bootloader version
 */
u32 LpcPhy::read_boot_version(){
	char buf[64];
	u16 ret;
	u8 minor;
	u8 major;
	isplib_debug(DEBUG_LEVEL+1, "read boot version\n");
	sprintf(buf, "K");
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to read boot version\n");
		return -1;
	}

	if ( ret == 0 ){
		get_line(buf, 64, QUICK_TIMEOUT);
		minor = atoi(buf);
		get_line(buf, 64, QUICK_TIMEOUT);
		major = atoi(buf);
		ret = major * 256 + minor;
	}


	return ret;
}

/*! \brief compares the specified block of memory.
 * \details This function compares the specified block of memory.
 * \Zero if memory is equal.
 */
int LpcPhy::compare_memory(u32 addr0 /*! The beginning of the first block */,
		u32 addr1 /*! The beginning of the second block */,
		u32 size /*! The number of bytes to compare */){
	char buf[64];
	int ret;
	isplib_debug(DEBUG_LEVEL+1, "compare mem\n");
	sprintf(buf, "M %d %d %d", (int)addr0, (int)addr1, (int)size);
	if( (ret = send_command(buf, QUICK_TIMEOUT)) < 0 ){
		isplib_error("Failed to compare memory %d %d %d\n", addr0, addr1, size);
		return -1;
	}
	return ret;
}

/*! \brief reads the return code from the device.
 * \details This function reads a simple return code from the device.
 * \return The value of the return code or -1 on an error.
 */
int LpcPhy::read_return_code(u16 timeout){
	char buf[128];
	u32 bytes_read;
	u8 i;
	u16 ret;
	bytes_read = get_line((char*)buf, 3, timeout);
	if ( bytes_read < 3 ){
		for(i=0; i < bytes_read; i++){
			isplib_debug(5, "ret code buf[%d]=%d (%c)\n", i, buf[i], buf[i]);
		}
		return -2;
	}
	for(i=0; i < bytes_read; i++){
		isplib_debug(5, "ret code buf[%d]=%d (%c)\n", i, buf[i], buf[i]);
	}
	ret = atoi(buf);
	isplib_debug(DEBUG_LEVEL+1, "ret code is %d\n", ret);
	return ret;
}

/*! \brief waits for the specified response from the device.
 * \details This function waits for the specified response from
 * the device.
 * \return Zero on success, -1 if expected response was not received.
 */
int LpcPhy::wait_response(const char * response, u16 timeout){
	char buf[WAIT_RESPONSE_BUFFER_SIZE];
	int bytes_recv;
	int len;
	int i;
	len = strlen(response);
	memset(buf, 0, WAIT_RESPONSE_BUFFER_SIZE);
	bytes_recv = get_line((char*)buf, len, timeout);

	isplib_debug(DEBUG_LEVEL+1, "Waiting for response (%d)\n", bytes_recv);

	if ( bytes_recv <= 0 ){
		return -1;
	}

	for(i=0; i < bytes_recv; i++){
		isplib_debug(5, "buf[%d]=%d (%c)\n", i, buf[i], buf[i]);
	}

	if ( strncmp(buf, response, bytes_recv)){
		isplib_debug(DEBUG_LEVEL+1, "Mismatch -%s- and -%s-\n", buf, response);
		for(i=0; i < bytes_recv; i++){
			if ( buf[i] == '\n'){
				isplib_debug(DEBUG_LEVEL+1, "[%d] = %d (<LF>)\n", i, buf[i]);
			} else if ( buf[i] == '\r' ){
				isplib_debug(DEBUG_LEVEL+1, "[%d] = %d (<CR>)\n", i, buf[i]);
			} else {
				isplib_debug(DEBUG_LEVEL+1, "[%d] = %d (%c)\n", i, buf[i], buf[i]);
			}
		}
	} else {
		isplib_debug(DEBUG_LEVEL+1, "Match: %s\n", buf);
		return 0;
	}

	return -1;
}


/*! \brief writes UU encoded data on the UART.
 * \details This function write the data on the UART using
 * UU encoding.
 * \return Number of bytes written, <0 on error
 */
s32 LpcPhy::write_data(void * src /*! A pointer to the source data */,
		u32 size /*! The number of bytes to write */){
	u32 bytes_written;

	if( is_uuencode() ){
		char buf[1024];
		u8 line;
		int checksum_ok;
		u32 checksum;
		u8 line_size;
		u8 i;
		u32 bytes_verified;
		u16 retry;
		u32 bytes;
		char * srcp = (char*)src;

		bytes_written = 0;
		bytes_verified = 0;
		line = 0;
		checksum = 0;
		retry = 0;

		do {
			if ( size - bytes_written < 45 ){
				line_size = size - bytes_written;
			} else {
				line_size = 45;
			}
			for(i=0; i < line_size; i++){
				checksum+=((unsigned char*)src)[i+bytes_written];
			}
			isplib_debug(DEBUG_LEVEL+1, "Line size is %d (checksum=%d)\n", line_size, checksum);
			uu_encode_line(buf, &(srcp[bytes_written]), line_size);
			bytes = m_uart.write(buf, strlen(buf));
			if ( bytes != strlen(buf) ){
				return -1;
			}
			isplib_debug(DEBUG_LEVEL+1, "Sending %s (%d)\n", buf, (int)strlen(buf));
			if ( m_echo ) {
				if ( wait_response(buf, QUICK_TIMEOUT) ){
					isplib_debug(DEBUG_LEVEL+1, "Failed echo\n");
				}
			}

			bytes_written+=line_size;
			line++;
			if ( (line == 20) || (line_size != 45) ){
				if ( line == 20 ){
					isplib_debug(DEBUG_LEVEL+1, "Sending 20th line checksum\n");
				}
				line = 0;
				//send and reset the checksum
				isplib_debug(DEBUG_LEVEL+1, "Sending checksum (%d)\n", checksum);
				sprintf(buf, "%d\r\n", (int)checksum);
				bytes = m_uart.write(buf, strlen(buf));
				if ( bytes != strlen(buf) ){
					return -1;
				}
				if ( m_echo ){
					sprintf(buf, "%d\rOK\r\n", (int)checksum);
					checksum_ok = !(wait_response(buf, QUICK_TIMEOUT));
				} else {
					memset(buf, 0, 64);
					get_line(buf, 1024, TIMEOUT);
					isplib_debug(DEBUG_LEVEL+1, "Checksum response is %s\n", buf);
					checksum_ok = !(strcmp(buf, "OK\r\n"));
				}

				checksum = 0;

				//device should respond with OK or RESEND
				if ( !checksum_ok ){
					isplib_debug(DEBUG_LEVEL+1, "Error data must be resent\n");
					if ( this->flush() < 0 ){
						isplib_debug(1, "Failed to flush uart\n");
						return -1;
					}

					bytes_written = bytes_verified;
					retry++;
					if ( retry == 3 ) return -1;
				} else {
					bytes_verified = bytes_written;
					retry=0;
				}
			}
		} while(bytes_written < size);
	} else {
		bytes_written = m_uart.write(src,size);
	}

	return bytes_written;
}

/*! \brief reads UU encoded data received on the UART.
 * \details This function reads data from the UART.  The data
 * read is UU decoded and stored in the destination buffer.
 * \return Number of bytes read, <0 on error
 */
s32 LpcPhy::read_data(void * dest /*! A pointer to the destination buffer */,
		u32 size /*! The size of the destination buffer */){
	u32 bytes_read;

	if( is_uuencode() ){
		u16 j;
		u8 retry;
		u16 checksum;
		u16 line;
		char buf[128];
		char buf_dec[128];
		u32 bytes;
		u16 bytes_verified;
		u16 bytes_decoded;
		bytes_read = 0;
		bytes_verified = 0;
		retry = 0;
		line = 0;
		do {

			//Grab a line from the UART
			memset(buf, 0, 128);
			memset(buf_dec, 0, 128);
			get_line(buf, 128, TIMEOUT);
			isplib_debug(4, "rx'd:%s\n", buf);
			if ( (bytes_decoded = uu_decode_line(buf_dec, buf, 64)) ){
				//The line did uu decode--so copy the data to the destination buffer
				isplib_debug(4, "read %d bytes\n", bytes_decoded);
				memcpy(&((char*)dest)[bytes_read], buf_dec, bytes_decoded);
				bytes_read += bytes_decoded;
				line++;
			}

			if ( (line == 20) || (bytes_read == size) ){

				if ( line == 20 ){
					isplib_debug(DEBUG_LEVEL+1, "Sending 20th line checksum\n");
				}
				//The line didn't uu decode--so it must be a checksum line
				checksum = 0;
				for(j=bytes_verified; j < bytes_read; j++){
					checksum += ((unsigned char*)dest)[j];
				}

				//The last line was decoded -- grab the checksum value
				if ( bytes_read == size ){
					get_line(buf, 128, TIMEOUT);
				}
				isplib_debug(4, "This is a checksum line:  %s\n", buf);

				if ( checksum == atoi(buf) ){
					isplib_debug(DEBUG_LEVEL+1, "Checksum is Good (%d)\n", checksum);
					sprintf(buf, "OK\r\n");
					bytes = m_uart.write(buf, strlen(buf));
					if ( bytes != strlen(buf) ){
						return -1;
					}
					if ( m_echo) {
						wait_response(buf, TIMEOUT);
					}
					bytes_verified = bytes_read;
				} else {
					isplib_debug(DEBUG_LEVEL+1, "TODO--Re-read the data\n");
					sprintf(buf, "RESEND\r\n");
					bytes = m_uart.write(buf, strlen(buf));
					if ( bytes != strlen(buf) ){
						return -1;
					}
					if ( m_echo) wait_response(buf, TIMEOUT);
					bytes_read = bytes_verified;
					retry++;
					if (retry == 3 ) return bytes_read;
				}
			}

		} while( bytes_read < size );
	} else {
		u32 timeout;
		int ret;
		char * destp = (char*)dest;
		bytes_read = 0;
		timeout = 0;
		do {
			ret = m_uart.read(destp + bytes_read, size - bytes_read);
			if( ret > 0 ){
				bytes_read += ret;
				timeout = 0;
			} else {
				timeout++;
				Timer::wait_msec(1);
				if( timeout > QUICK_TIMEOUT ){
					return bytes_read;
				}
			}
		} while( bytes_read < size );
	}

	return bytes_read;
}

int LpcPhy::get_line(void * buf, int nbyte, int max_wait){
	int bytes_recv;
	int page_size;
	int bytes_read;
	int timeout;
	char * start = (char*)buf;
	int i;
	char * p;
	p = (char*)buf;

	timeout = 0;
	bytes_recv = 0;
	memset(buf, 0, nbyte);
	do {
		//this needs to read up to the newline
		page_size = (( nbyte - bytes_recv ) > 1) ? 1 : (nbyte - bytes_recv);
		if ( (bytes_read = m_uart.read(p,  page_size)) < 0 ){
#if !defined __link
			if( errno != EAGAIN ){
				m_trace.assign("uart read failed");
				m_trace.trace_error();
				return -1;
			}
#else
			return -1;
#endif
		}

		if( bytes_read > 0 ){

			isplib_debug(DEBUG_LEVEL+3, "Read %d bytes\n", bytes_read);
			bytes_recv+=bytes_read;
			p+=bytes_read;

			//scan for newline
			for(i=0; i < bytes_recv; i++){

#if defined __link
				if ( start[i] == '\n' ){
					isplib_debug(DEBUG_LEVEL+3, "<LF>\n");
				} else if ( start[i] == '\r' ){
					isplib_debug(DEBUG_LEVEL+3, "<CR>\n");
				} else {
					isplib_debug(DEBUG_LEVEL+3, "%c\n", start[i]);
				}
#endif

				if ( start[i] == '\n' ){
					start[i+1] = 0; //zero terminate
					if ( i+1 < bytes_recv ){
						isplib_debug(DEBUG_LEVEL+2, "-----------------------Extra bytes %d------------------\n", bytes_recv - i - 1);
					}
					return i+1;
				}
			}
		}

#if !defined __link
		Timer::wait_msec(1);
#endif
		timeout++;
		if ( timeout == max_wait ){
			return 0;
		}
	} while(bytes_recv < nbyte);

	return bytes_recv;
}

int LpcPhy::send_command(const char * cmd, int timeout, int wait_ms){
	u32 bytes;
	int ret;
	int len = strlen(cmd);
	char buffer[len+16];

	sprintf(buffer, "%s\r\n", cmd);
	bytes = m_uart.write(buffer, strlen(buffer));
	if ( bytes != strlen(buffer) ){
		isplib_error("send command failed %d != %d\n", bytes, (int)strlen(buffer));
		return -1;
	}

	if( wait_ms != 0 ){
		Timer::wait_msec(wait_ms);
	}

	if( m_echo ){
		if( is_return_code_newline() ){
			ret = wait_response(buffer, timeout);
			if( ret < 0 ){
				return ret;
			}
			ret = read_return_code(timeout);
		} else {
			sprintf(buffer, "%s\r0\r\n", cmd); //look for return code 0
			ret = wait_response(buffer, timeout);
		}
	} else {
		ret = read_return_code(timeout);
	}
	return ret;
}


