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
#define LOG_PREFIX	"UselessDB module: "

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
#define ERROR_TRANSFER_FAILED		3

static int daemon_pid=0;
struct sock *nl_sk = NULL;

static void nl_recv_msg_handler(struct sk_buff *skb);
static int send_ack_to_daemon(int new_pid);
static void transfer_msg_to_daemon(struct nlmsghdr *nlh);
static void transfer_msg_to_client(struct nlmsghdr *nlh);
static void send_error_to_src_app(int pid, int error_code);
static void put_src_pid_to_msg(void);
static void get_src_pid_from_msg(void);

static int __init uselesskm_init(void){
	printk(KERN_INFO "Starting uselessdb module\n");
    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg_handler,
    };

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USELESS_P, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT LOG_PREFIX"Error creating socket.\n");
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
    			printk(KERN_INFO LOG_PREFIX"New daemon id=%d setted\n",daemon_pid);
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
}

static int send_ack_to_daemon(int new_pid){
	int ret=0;
	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	skb_out = nlmsg_new(NLMSG_DEFAULT_SIZE, 0);
	if (!skb_out) {
		printk(KERN_ERR LOG_PREFIX"Failed to allocate new skb\n");
		ret=1;
	}
	if(ret==0){
		nlh = nlmsg_put(skb_out, 0, 0, MSGTYPE_SET_DPID_ACK, 0, NLM_F_ACK);
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		ret = nlmsg_unicast(nl_sk, skb_out, new_pid);
		//nlmsg_free(skb_out);	виснет(((
		if(ret<0){
			printk(KERN_INFO LOG_PREFIX"Error while setting new daemon id\n",daemon_pid);
		}
	}
	return ret;
}

static void transfer_msg_to_daemon(struct nlmsghdr *nlh){
	int ret=0;
	struct sk_buff *skb_out;
	struct nlmsghdr *nlh_out;
	int new_msg_size=nlmsg_len(nlh)+sizeof(nlh->nlmsg_pid);
	if(daemon_pid!=0){
		skb_out = nlmsg_new(new_msg_size, 0);
		if (!skb_out) {
			printk(KERN_ERR LOG_PREFIX"Failed to allocate new skb\n");
			ret=1;
		}
		else{
			nlh_out = nlmsg_put(skb_out, 0, 0, nlh->nlmsg_type, new_msg_size, 0);
			NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
			memcpy(nlmsg_data(nlh_out),(void *)(&nlh->nlmsg_pid),sizeof(nlh->nlmsg_pid));
			memcpy((void *)((char *)nlmsg_data(nlh_out)+sizeof(nlh->nlmsg_pid)),nlmsg_data(nlh),nlmsg_len(nlh));
			printk("%d %s\n",*((__u32 *)nlmsg_data(nlh_out)),((char *)nlmsg_data(nlh_out)+sizeof(nlh->nlmsg_pid)));	//debug str;
			ret = nlmsg_unicast(nl_sk, skb_out, daemon_pid);
			if(ret<0){
				printk(KERN_INFO LOG_PREFIX"Error while transferring msg from pid %d to daemon\n",nlh->nlmsg_pid);
				send_error_to_src_app(nlh->nlmsg_pid,ERROR_TRANSFER_FAILED);
			}
		}
	}
	else{
		printk(KERN_INFO LOG_PREFIX"Daemon unreachable\n");
		send_error_to_src_app(nlh->nlmsg_pid,ERROR_DAEMON_UNREACHABLE);
	}
}

static void transfer_msg_to_client(struct nlmsghdr *nlh){
	int ret=0;
	struct sk_buff *skb_out;
	struct nlmsghdr *nlh_out;
	int new_msg_size=nlmsg_len(nlh)-sizeof(nlh->nlmsg_pid);
	__u32 client_pid=*((__u32 *)nlmsg_data(nlh));
	skb_out = nlmsg_new(new_msg_size, 0);
	if (!skb_out) {
		printk(KERN_ERR LOG_PREFIX"Failed to allocate new skb\n");
		ret=1;
	}
	else{
		nlh_out = nlmsg_put(skb_out, 0, 0, nlh->nlmsg_type, new_msg_size, 0);
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		memcpy(nlmsg_data(nlh_out),((char *)nlmsg_data(nlh_out)+sizeof(client_pid)),new_msg_size);
		printk("%d %s\n",client_pid,(char *)nlmsg_data(nlh_out));	//todo remove
		ret = nlmsg_unicast(nl_sk, skb_out, client_pid);
		if(ret<0){
			printk(KERN_INFO LOG_PREFIX"Error while transferring msg from daemon to pid %d \n",nlh->nlmsg_pid);
		}
	}
}

static void send_error_to_src_app(int pid,int error_code){
	//todo implement
}

module_init(uselesskm_init);
module_exit(uselesskm_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR(KM_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(KM_DESC);	/* What does this module do */
