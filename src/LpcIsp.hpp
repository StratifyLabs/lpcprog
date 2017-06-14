/*! \file */ //Copyright 2011-2016 Tyler Gilbert; All Rights Reserved

#ifndef LPCISP_HPP_
#define LPCISP_HPP_

#include <mcu/types.h>
#include <stfy/hal.hpp>
#include <stfy/sys.hpp>

#include "LpcPhy.hpp"


class LpcIsp {
public:

	/*! \details Construct an LPC ISP object using the specified UART, reset pin and isp request pin */
	LpcIsp(hal::Uart & uart, hal::Pin & rst, hal::Pin & ispreq) : m_phy(uart, rst, ispreq){}

	int program(const char * filename, int crystal, const char * dev, bool (*progress)(void*,int, int), void * context = 0);
	int read(const char * filename, int crystal, const char * dev, bool (*progress)(void*,int, int), void * context = 0);
	char ** getlist();

	int copy_names(char * device, char * pio0, char * pio1);
	int init_phy(int uart_pinassign){ return m_phy.init(uart_pinassign); }
	int exit_phy(){ return m_phy.exit(); }
	int reset(){ return m_phy.reset(); }


private:
	LpcPhy m_phy;
	const char * m_device;
	int init_prog_interface(int crystal);
	int erase_dev();
	u32 write_progmem(void * data, u32 addr, u32 size, bool (*progress)(void*,int, int), void * context);
	u32 read_progmem(void * data, u32 addr, u32 size, bool (*progress)(void*,int, int), void * context);
	u16 verify_progmem(
			void * data,
			void * original_data,
			u32 addr,
			u32 size,
			int (*progress)(int, int), void * context);

	int write_vector_checksum(unsigned char * hex_buffer, const char * dev);
	int prog_shutdown();

	Trace m_trace;

};


#endif /* LPCISP_HPP_ */
