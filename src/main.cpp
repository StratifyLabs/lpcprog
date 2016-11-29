
#include <cstdio>
#include <ctype.h>
#include <unistd.h>

#include <stfy/hal.hpp>
#include <stfy/var.hpp>
#include <stfy/sys.hpp>
#include <stfy/isp.hpp>

static void show_usage(const char * name);
static bool update_progress(void * context, int progress, int max);

int main(int argc, char * argv[]){
	String image;
	String arg;
	String device;

	int i;
	int ret;

	//set default values
	int uart_port = 0;
	int uart_pinassign = 0;
	int reset_port = 1;
	int reset_pin = 0;
	int ispreq_port = 2;
	int ispreq_pin = 10;
	int led_port = -1;
	int led_pin = 0;

	image.clear();

	//parse the arguments
	for(i=1; i < argc; i++){
		Token token;
		arg = argv[i];

		if( (arg == "-u") ){
			if( argc > i+1 ){
				token.assign(argv[i+1]);
				token.parse(".");
				if( token.size() > 0 ){ uart_port = atoi( token.at(0) ); }
				if( token.size() > 1 ){ uart_pinassign = atoi( token.at(1) ); }
			}
		}

		if( (arg == "-r") ){
			if( argc > i+1 ){
				token.assign(argv[i+1]);
				token.parse(".");
				if( token.size() > 0 ){ reset_port = atoi( token.at(0) ); }
				if( token.size() > 1 ){ reset_pin = atoi( token.at(1) ); }
			}
		}

		if( (arg == "-i") ){
			if( argc > i+1 ){
				token.assign(argv[i+1]);
				token.parse(".");
				if( token.size() > 0 ){ ispreq_port = atoi( token.at(0) ); }
				if( token.size() > 1 ){ ispreq_pin = atoi( token.at(1) ); }
			}
		}

		if( (arg == "-led") ){
			if( argc > i+1 ){
				token.assign(argv[i+1]);
				token.parse(".");
				if( token.size() > 0 ){ led_port = atoi( token.at(0) ); }
				if( token.size() > 1 ){ led_pin = atoi( token.at(1) ); }
			}
		}

		if( (arg == "-in") ){
			if( argc > i+1 ){
				image = argv[i+1];
			}
		}

		if( (arg == "-d") ){
			if( argc > i+1 ){
				device = argv[i+1];
			}
		}
	}

	if( image.empty() ){
		show_usage(argv[0]);
	}


	printf("Program %s\n", image.c_str());
	printf("Uart:%d,%d Reset:%d.%d Ispreq:%d.%d\n",
			uart_port, uart_pinassign, reset_port, reset_pin, ispreq_port, ispreq_pin);

	Uart uart(uart_port);
	Pin reset(reset_port, reset_pin);
	Pin ispreq(ispreq_port, ispreq_pin);
	Pin led(led_port, led_pin);
	LpcIsp isp(uart, reset, ispreq);

	printf("Init Phy\n");
	if( isp.init_phy(uart_pinassign) < 0 ){
		printf("Failed to init phy\n");
		exit(1);
	}

	printf("Start programming\n");
	ret = isp.program(image, 12000000, device, update_progress);
	if( ret < 0 ){
		printf("Failed to program chip %d\n", ret);
	}

	printf("Programming Complete\n");
	if( isp.exit_phy() < 0 ){
		printf("Failed to exit phy\n");
	}

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
