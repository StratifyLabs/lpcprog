/*
 * AppMessenger.hpp
 *
 *  Created on: Dec 28, 2017
 *      Author: tgil
 */

#ifndef APPMESSENGER_HPP_
#define APPMESSENGER_HPP_

#include <sapi/sys.hpp>
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>

class AppMessenger : public Messenger {
public:
	AppMessenger(int stack_size);
	void handle_message(Son & message);

	bool is_abort() const { return m_is_abort; }

private:
	bool m_is_abort;

};

#endif /* APPMESSENGER_HPP_ */
