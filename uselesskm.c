/*
 * uselesskm.c
 *
 *  Created on: 2 дек. 2016 г.
 *      Author: gorec
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#define KM_AUTHOR "Egor Dudyak"
#define KM_DESC   "Provides connection between UselesDBd and UselessDBClients"
#define LOGPREFIX	"UselessDB module: "

#define NETLINK_USELESS_P 		31
#define MAX_CLIENT_COUNT		2
#define MSGTYPE_SET_DPID		11
#define MSGTYPE_SET_DPID_ACK	12
#define MSGTYPE_REQEUEST_START	20
#define MSGTYPE_REQEUEST_END	40
#define MSGTYPE_RESPONSE_START	20
#define MSGTYPE_RESPONSE_END	40

#define ERROR_WRONG_MSG_TYPE		1
#define ERROR_DAEMON_UNREACHABLE	2

static int daemon_pid=0;
struct sock *nl_sk = NULL;
static client_struct clients[MAX_CLIENT_COUNT];

static void nl_recv_msg_handler(struct sk_buff *skb);
static int send_ack_to_daemon(int new_pid);
static void transfer_msg_to_daemon(struct nlmsghdr *nlh);
static void transfer_msg_to_client(struct nlmsghdr *nlh);
static void send_error_to_src_app(int pid, int error_code);

static int __init uselesskm_init(void){
	printk(KERN_INFO "Starting uselessdb module\n");
    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg_handler,
    };
    int i=0;
    for(i=0;i<MAX_CLIENT_COUNT;i++){
    	clients[i].pid=0;
    	clients[i].msg_cnt=0;
    }

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USELESS_P, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT LOGPREFIX"Error creating socket.\n");
        return -10;
    }

	return 0;
}

static void __exit uselesskm_exit(void){
	printk(KERN_INFO "Exiting uselessdb module\n");
    netlink_kernel_release(nl_sk);
}

static void nl_recv_msg_handler(struct sk_buff *skb){
    struct nlmsghdr *nlh;
    int pid;
    int msg_size;
    char *msg = "Hello from kernel";
    int res;
    int i=0;

    msg_size = strlen(msg);

    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid; /*pid of sending process */
    switch(nlh->nlmsg_type){
    	case MSGTYPE_SET_DPID:
    		if(!send_ack_to_daemon(pid)){
    			daemon_pid=pid;
    			printk(KERN_INFO LOGPREFIX"New daemon id=%d setted\n",daemon_pid);
    		}
    		else{
    			printk(KERN_INFO LOGPREFIX"Error while setting new daemon id\n",daemon_pid);
    		}
    		break;
    	default:
    		if(nlh->nlmsg_type>=MSGTYPE_REQEUEST_START && nlh->nlmsg_type<=MSGTYPE_REQEUEST_END){
    			transfer_msg_to_daemon(nlh);
    		}
    		else if(nlh->nlmsg_type>=MSGTYPE_RESPONSE_START && nlh->nlmsg_type<=MSGTYPE_RESPONSE_END){
    			transfer_msg_to_client(nlh);
    		}
    		else{
    			send_error_to_src_app(pid, ERROR_WRONG_MSG_TYPE);
    		}
    		break;
    }
    /*
    for(i=0;i<MAX_CLIENT_COUNT;i++){
    	if(clients[i].pid==pid){
    		clients[i].msg_cnt++;
    		break;
    	}
    	else if(clients[i].pid==0){
    		clients[i].pid=pid;
    		clients[i].msg_cnt++;
    		break;
    	}
    }
    if(i<MAX_CLIENT_COUNT){
		printk(KERN_INFO LOGPREFIX"Client_num=%d Pid=%d msg_num=%d msg_type=%d payload:%s\n",i,pid,clients[i].msg_cnt, nlh->nlmsg_type, (char *)nlmsg_data(nlh));

		skb_out = nlmsg_new(msg_size, 0);
		if (!skb_out) {
			printk(KERN_ERR LOGPREFIX"Failed to allocate new skb\n");
			return;
		}

		nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		strncpy(nlmsg_data(nlh), msg, msg_size);
		msleep(1000);
		res = nlmsg_unicast(nl_sk, skb_out, pid);
		if (res < 0)
			printk(KERN_INFO LOGPREFIX"Error while sending bak to user\n");


	}*/
}

static int send_ack_to_daemon(int new_pid){
	int ret=0;
	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	skb_out = nlmsg_new(NLMSG_DEFAULT_SIZE, 0);
	if (!skb_out) {
		printk(KERN_ERR LOGPREFIX"Failed to allocate new skb\n");
		ret=1;
	}
	if(ret==0){
		nlh = nlmsg_put(skb_out, 0, 0, MSGTYPE_SET_DPID_ACK, 0, NLM_F_ACK);
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		ret = nlmsg_unicast(nl_sk, skb_out, new_pid);
		//nlmsg_free(skb_out);	виснет(((
	}
	return ret;
}

static void transfer_msg_to_daemon(struct nlmsghdr *nlh){
	if(daemon_pid!=0){

	}
}

static void transfer_msg_to_client(struct nlmsghdr *nlh){
	//todo implement
}

static void send_error_to_src_app(int pid,int error_code){
	//todo implement
}

module_init(uselesskm_init);
module_exit(uselesskm_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR(KM_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(KM_DESC);	/* What does this module do */
