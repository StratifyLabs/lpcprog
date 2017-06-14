
#include <cstdio>
#include <ctype.h>
#include <unistd.h>

#include <stfy/hal.hpp>
#include <stfy/var.hpp>
#include <stfy/sys.hpp>
#include "LpcIsp.hpp"


static void show_usage(const char * name);
static bool update_progress(void * context, int progress, int max);

int main(int argc, char * argv[]){
	String image;
	String device;
	pio_t uart_pio;
	pio_t reset_pio;
	pio_t ispreq_pio;

	Cli cli(argc, argv);

	cli.set_version("1.3");

	if( cli.is_option("-v") ){
		cli.print_version();
		exit(1);
	}

	int ret;

	image.clear();
	device.clear();

	uart_pio.port = 0;
	uart_pio.pin = 0;
	if( cli.is_option("-u") ){
		uart_pio = cli.get_option_pio("-u");
	}


	if( cli.is_option("-r") ){
		reset_pio = cli.get_option_pio("-r");
	} else {
		reset_pio.port = 1;
		reset_pio.pin = 0;
	}


	if( cli.is_option("-i") ){
		ispreq_pio = cli.get_option_pio("-i");
	} else {
		ispreq_pio.port = 2;
		ispreq_pio.pin = 10;
	}


	if( cli.is_option("-in") ){
		image = cli.get_option_argument("-in");

	} else {
		show_usage(argv[0]);
		exit(1);
	}

	if( cli.is_option("-d") ){
		device = cli.get_option_argument("-d");
	} else {
		show_usage(argv[0]);
		exit(1);
	}


	printf("Device %s\n", device.c_str());
	printf("Image %s\n", image.c_str());
	printf("Uart:%d.%d Reset:%d.%d Ispreq:%d.%d\n",
			uart_pio.port, uart_pio.pin,
			reset_pio.port, reset_pio.pin,
			ispreq_pio.port, ispreq_pio.pin);


	Uart uart(uart_pio.port);
	Pin reset(reset_pio.port, reset_pio.pin);
	Pin ispreq(ispreq_pio.port, ispreq_pio.pin);

	LpcIsp isp(uart, reset, ispreq);

	printf("Init Phy\n");
	if( isp.init_phy(uart_pio.pin) < 0 ){
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

	reset.set_attr(Pin::INPUT | Pin::PULLUP);
	ispreq.set_attr(Pin::INPUT | Pin::PULLUP);

	return 0;
}

void show_usage(const char * name){
	printf("usage:\n");
	printf("\t%s [-uart X.Y] [-reset X.Y]\n", name);
}

bool update_progress(void * context, int progress, int max){
	printf("Programmed %d of %d\n", progress, max);
	return false; //do not abort
}
