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
#ifndef ISPOBJECT_HPP_
#define ISPOBJECT_HPP_


namespace isp {

/*! \brief Base class for in-circuit serial programming
 * \details This is the base class for objects that implement pure
 * virtual serial programming.
 */
class Isp {
public:
	Isp(){}

	/*! \details Program the device with the specified filename */
	virtual int program(const char * filename, int crystal, const char * dev, bool (*progress)(void*,int,int), void * context = 0) = 0;
	/*! \details Read the image from the device */
	virtual int read(const char * filename, int crystal, const char * dev, bool (*progress)(void*,int,int), void * context = 0) = 0;
	virtual char ** getlist() = 0;
	virtual int copy_names(char * device, char * pio0, char * pio1) = 0;

	/*! \details Initialize the physical layer interface */
	virtual int init_phy(int uart_pinassign) = 0;
	/*! \details De-initialize the physical layer interface */
	virtual int exit_phy() = 0;
	/*! \details Reset the target device */
	virtual int reset() = 0;

	static int name_maxsize(){ return 32; }

};
};

#endif /* ISPOBJECT_HPP_ */
