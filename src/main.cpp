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

#include <cstdio>
#include <ctype.h>
#include <unistd.h>

#include <sapi/hal.hpp>
#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include "LpcIsp.hpp"


static void show_usage(const char * name);
static bool update_progress(void * context, int progress, int max);

int main(int argc, char * argv[]){
	String image;
	String device;
	int uart_port;
	mcu_pin_t reset_pio;
	mcu_pin_t ispreq_pio;
	UartPinAssignment pin_assignment;

	Cli cli(argc, argv);

	cli.handle_version("1.3");

	if( cli.is_option("-v") ){
		cli.print_version();
		exit(1);
	}

	int ret;

	image.clear();
	device.clear();

	uart_port = 0;
	if( cli.is_option("-u") ){
		uart_port = cli.get_option_value("-u");
	}

	if( cli.is_option("-tx") ){
		pin_assignment->tx = cli.get_option_pin("-tx");
	}

	if( cli.is_option("-rx") ){
		pin_assignment->rx = cli.get_option_pin("-rx");
	}



	if( cli.is_option("-r") ){
		reset_pio = cli.get_option_pin("-r");
	} else {
		reset_pio.port = 1;
		reset_pio.pin = 0;
	}


	if( cli.is_option("-i") ){
		ispreq_pio = cli.get_option_pin("-i");
	} else {
		ispreq_pio.port = 2;
		ispreq_pio.pin = 10;
	}


	if( cli.is_option("-in") ){
		image = cli.get_option_argument("-in");

	} else {
		printf("Could not find input file (use -in option)\n");
		show_usage(argv[0]);
		exit(1);
	}

	if( cli.is_option("-d") ){
		device = cli.get_option_argument("-d");
	} else {
		printf("Could not find device (use -d option)\n");
		show_usage(argv[0]);
		exit(1);
	}


	printf("Device %s\n", device.c_str());
	printf("Image %s\n", image.c_str());
	printf("Uart:%d Reset:%d.%d Ispreq:%d.%d\n",
			uart_port,
			reset_pio.port, reset_pio.pin,
			ispreq_pio.port, ispreq_pio.pin);


	Uart uart(uart_port);
	Pin reset(reset_pio.port, reset_pio.pin);
	Pin ispreq(ispreq_pio.port, ispreq_pio.pin);

	LpcIsp isp(uart, reset, ispreq);

	printf("Init Phy\n");

	if( isp.init_phy(pin_assignment) < 0 ){
		printf("Failed to init phy\n");
		exit(1);
	}

	if( cli.is_option("-read") == false ){
		printf("Start programming %s\n", image.c_str());
		ret = isp.program(image, 12000000, device, update_progress);
		if( ret < 0 ){
			printf("Failed to program chip %d\n", ret);
		}

		printf("Programming Complete\n");
		if( isp.exit_phy() < 0 ){
			printf("Failed to exit phy\n");
		}
	} else {
		printf("Read: %s from %s\n", image.c_str(), device.c_str());
		isp.read(image.c_str(), 12000000, device.c_str(), 0, 0);
		printf("Done\n");
		exit(1);
	}

	reset.set_attr(Pin::FLAG_SET_INPUT | Pin::FLAG_IS_PULLUP);
	ispreq.set_attr(Pin::FLAG_SET_INPUT | Pin::FLAG_IS_PULLUP);

	return 0;
}

void show_usage(const char * name){
	printf("usage:\n");
	printf("\t%s [-u X] [-r X.Y] [-i X.Y] [-in path] [-rx X.Y] [-tx X.Y]\n", name);
}

bool update_progress(void * context, int progress, int max){
	printf("Programmed %d of %d\n", progress, max);
	return false; //do not abort
}
