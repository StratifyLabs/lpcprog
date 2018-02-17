/*
 * AppMessenger.cpp
 *
 *  Created on: Dec 28, 2017
 *      Author: tgil
 */

#include <sapi/var.hpp>
#include "AppMessenger.hpp"

AppMessenger::AppMessenger(int stack_size) : Messenger(stack_size) {
	// TODO Auto-generated constructor stub
	m_is_abort = false;
}


void AppMessenger::handle_message(Son & message){
	String command;

	message.get_error();

	printf("Got a message %d\n", message.get_message_size());
	if( message.read_str("command", command) >= 0 ){
		printf("command %s\n", command.c_str());
		if( command == "abort" ){
			m_is_abort = true;
		}
		printf("error is %d\n", message.get_error());
	} else {
		printf("Didn't read command %d\n", message.get_error());
	}

}

