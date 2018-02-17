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

#include "AppMessenger.hpp"

static void show_usage(const char * name);


static bool update_status(void * context, const char * status);
static bool update_progress(void * context, int progress, int max);

int main(int argc, char * argv[]){
	String image;
	String device;
	mcu_pin_t reset_pio;
	mcu_pin_t ispreq_pio;
	UartAttr uart_attr;

	Cli cli(argc, argv);
	AppMessenger messenger(2048);
	AppMessenger * current_messenger;

	cli.set_publisher("Stratify Labs, Inc");
	cli.handle_version();

	int ret;
	if( cli.is_option("-message") ){
		mcu_pin_t channels = cli.get_option_pin("-message");
		current_messenger = &messenger;
		messenger.set_timeout(100);
		messenger.start("/dev/fifo", channels.port, channels.pin);
	} else {
		current_messenger = 0;
	}


	if( cli.is_option("-reset") ){
		if( cli.is_option("-r") ){
			reset_pio = cli.get_option_pin("-r");
		} else {
			reset_pio.port = 1;
			reset_pio.pin = 0;
		}


		Pin reset(reset_pio.port, reset_pio.pin);

		update_status(current_messenger, "Resetting Device\n");
		reset.set_output();
		reset = false;
		Timer::wait_msec(250);
		reset = true;

		reset.set_input(Pin::FLAG_IS_PULLUP);

		update_status(current_messenger, "Reset Complete\n");
		exit(1);

	}


	if( cli.is_option("-uart") ){

		cli.handle_uart(uart_attr);

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

		Uart uart(uart_attr.port());
		Pin reset(reset_pio.port, reset_pio.pin);
		Pin ispreq(ispreq_pio.port, ispreq_pio.pin);

		LpcIsp isp(uart, reset, ispreq);

		update_status(current_messenger, "Init Phy\n");

		UartPinAssignment pin_assignment;

		pin_assignment->rx = uart_attr.rx();
		pin_assignment->tx = uart_attr.tx();

		if( isp.init_phy(pin_assignment) < 0 ){
			update_status(current_messenger, "Failed to init phy\n");
			exit(1);
		}


		if( cli.is_option("-read") == false ){
			update_status(current_messenger, "Start programming\n");

			isp.set_context(current_messenger);
			isp.set_progress_callback(update_progress);
			isp.set_status_callback(update_status);

			ret = isp.program(image, 12000000, device);
			if( ret < 0 ){
				printf("Failed to program chip %d\n", ret);
			}

			update_status(current_messenger, "Programming Complete\n");
			if( isp.exit_phy() < 0 ){
				update_status(current_messenger, "Failed to exit phy\n");
			}
		} else {
			printf("Read: %s from %s\n", image.c_str(), device.c_str());
			isp.read(image.c_str(), 12000000, device.c_str());
			update_status(current_messenger, "Done\n");
			exit(1);
		}

		reset.set_attr(Pin::FLAG_SET_INPUT | Pin::FLAG_IS_PULLUP);
		ispreq.set_attr(Pin::FLAG_SET_INPUT | Pin::FLAG_IS_PULLUP);

	} else {
		show_usage(cli.name());
	}

	return 0;
}

void show_usage(const char * name){
	printf("usage:\n");
	printf("\t%s [-uart X] [-r X.Y] [-i X.Y] [-d device] [-in path] [-rx X.Y] [-tx X.Y]\n", name);
	printf("\t\t-r X.Y is the pin connected to reset\n");
	printf("\t\t-i X.Y is the pin connected to ISP request\n");
	printf("\t\t-in path to local image\n");
	printf("\t\t-d is the device (e.g. lpc4078)\n");
	printf("\t\t-rx X.Y is the UART rx pin (optional)\n");
	printf("\t\t-tx X.Y is the UART tx pin (optional)\n");
	printf("\t\t-message X.Y send message data on /dev/fifo channels X.Y\n");
	printf("e.g: lpcprog -uart 0 -r 1.0 -i 2.10 -in /home/boot-image.bin -d lpc4078\n");
}

bool update_progress(void * context, int progress, int max){
	AppMessenger * messenger = (AppMessenger*)context;

	if( messenger ){
		char buffer[256];
		Son message(4);
		message.create_message(buffer, 256);
		message.open_object("");
		message.write("type", "progress");
		message.write("progress", (u32)progress);
		message.write("max", (u32)max);
		message.close();
		message.open_read_message(buffer, 256);
		messenger->send_message(message);
		return messenger->is_abort();
	} else {
		printf("Programmed %d of %d\n", progress, max);
	}

	return false; //do not abort
}

bool update_status(void * context, const char * status){
	AppMessenger * messenger = (AppMessenger*)context;
	if( messenger ){
		char buffer[256];
		Son message(4);
		message.create_message(buffer, 256);
		message.open_object("");
		message.write("type", "status");
		message.write("status", status);
		message.close();
		message.open_read_message(buffer, 256);
		messenger->send_message(message);
		return messenger->is_abort();
	} else {
		printf("No Messenger:");
		printf(status);
		printf("\n");
	}
	return false;
}
