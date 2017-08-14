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
#ifndef LPCISP_HPP_
#define LPCISP_HPP_

#include <sapi/hal.hpp>
#include <sapi/sys.hpp>

#include "LpcPhy.hpp"


class LpcIsp {
public:

	/*! \details Construct an LPC ISP object using the specified UART, reset pin and isp request pin */
	LpcIsp(hal::Uart & uart, hal::Pin & rst, hal::Pin & ispreq) : m_phy(uart, rst, ispreq){}

	int program(const char * filename, int crystal, const char * dev, bool (*progress)(void*,int, int), void * context = 0);
	int read(const char * filename, int crystal, const char * dev, bool (*progress)(void*,int, int), void * context = 0);
	char ** getlist();

	int copy_names(char * device, char * pio0, char * pio1);
	int init_phy(const UartPinAssignment & pin_assignment){ return m_phy.init(pin_assignment); }
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
