/*
 * useless_nl_config.h
 *
 *  Created on: 7 дек. 2016 г.
 *      Author: gorec
 */

#ifndef USELESS_NL_CONFIG_H_
#define USELESS_NL_CONFIG_H_

#define MSGTYPE_ERROR			2	//redefine NLMSG_ERROR!
#define NL_MAX_PAYLOAD			1024
#define NETLINK_USELESS_P 		31
#define MSGTYPE_SET_DPID		11
#define MSGTYPE_REQEUEST_START	20
#define MSGTYPE_REQEUEST_END	40
#define MSGTYPE_RESPONSE_START	50
#define MSGTYPE_RESPONSE_END	70

#define ERROR_NO					0
#define ERROR_WRONG_MSG_TYPE		1
#define ERROR_DAEMON_UNREACHABLE	2
#define ERROR_TRANSFER_FAILED		3

#endif /* USELESS_NL_CONFIG_H_ */
