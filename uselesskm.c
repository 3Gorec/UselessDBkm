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
#include "useless_nl_core/useless_nl_config.h"
#define KM_AUTHOR "Egor Dudyak"
#define KM_DESC   "Provides connection between UselesDBd and UselessDBClients"
#define LOG_PREFIX	"UselessDB module: "

static int daemon_pid=0;
struct sock *nl_sk = NULL;

static void nl_recv_msg_handler(struct sk_buff *skb);
static void transfer_msg_to_daemon(struct nlmsghdr *nlh);
static void transfer_msg_to_client(struct nlmsghdr *nlh);
static int send_error_ack(struct nlmsghdr *nlh, int dest_pid,int error_code);
static int send_nl_msg(__u32 dest_pid,__u16 msg_type, void* payload, int payload_size);
static void inject_client_pid_to_msg(__u32 client_pid, struct nlmsghdr *orig_nlh, void *new_payload); //payload should be previously allocated
static __u32 withdraw_client_pid_from_msg(struct nlmsghdr *orig_nlh, void *new_payload, int new_payload_size); //payload should be previously allocated

static int __init uselesskm_init(void){
	struct netlink_kernel_cfg cfg={
		.input = nl_recv_msg_handler,
	};
	printk(KERN_INFO "Starting uselessdb module\n");

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
    __u32 pid;

    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid; /*pid of sending process */
    switch(nlh->nlmsg_type){
    	case MSGTYPE_SET_DPID:
    		if(send_error_ack(nlh,pid,ERROR_NO)==0){
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
    			printk(KERN_INFO LOG_PREFIX"Wrong message type %d\n",nlh->nlmsg_type);
    			send_error_ack(nlh,pid, ERROR_WRONG_MSG_TYPE);
    		}
    		break;
    }
}

static void transfer_msg_to_daemon(struct nlmsghdr *nlh){
	int ret=0;
	int new_msg_size=nlmsg_len(nlh)+sizeof(nlh->nlmsg_pid);
	__u32 client_pid=nlh->nlmsg_pid;
	if(daemon_pid!=0){
		char *payload_buf=kmalloc(new_msg_size,GFP_KERNEL);
		inject_client_pid_to_msg(client_pid,nlh,payload_buf);
		ret=send_nl_msg(daemon_pid,nlh->nlmsg_type,payload_buf,new_msg_size);
		kfree(payload_buf);
		if(ret!=0){
			printk(KERN_INFO LOG_PREFIX"Error while transferring msg from pid %d to daemon\n",nlh->nlmsg_pid);
			send_error_ack(nlh,nlh->nlmsg_pid,ERROR_TRANSFER_FAILED);
		}
	}
	else{
		printk(KERN_INFO LOG_PREFIX"Daemon unreachable\n");
		send_error_ack(nlh,nlh->nlmsg_pid,ERROR_DAEMON_UNREACHABLE);
	}
}

static void transfer_msg_to_client(struct nlmsghdr *nlh){
	int ret=0;
	int new_msg_size=nlmsg_len(nlh)-sizeof(nlh->nlmsg_pid);
	char *payload_buf=kmalloc(new_msg_size,GFP_KERNEL);
	__u32 client_pid=withdraw_client_pid_from_msg(nlh,payload_buf,new_msg_size);
	ret=send_nl_msg(client_pid,nlh->nlmsg_type,payload_buf,new_msg_size);
	kfree(payload_buf);
	if(ret!=0){
		printk(KERN_ERR LOG_PREFIX"Error transfer msg from daemon to client\n");
	}
}

static void inject_client_pid_to_msg(__u32 client_pid, struct nlmsghdr *orig_nlh, void *new_payload){
	memcpy(new_payload,&client_pid,sizeof(client_pid));
	memcpy(new_payload+sizeof(client_pid),nlmsg_data(orig_nlh),nlmsg_len(orig_nlh));
}

static __u32 withdraw_client_pid_from_msg(struct nlmsghdr *orig_nlh, void *new_payload, int new_payload_size){
	__u32 client_pid=*((__u32 *)nlmsg_data(orig_nlh));
	memcpy(new_payload,nlmsg_data(orig_nlh)+sizeof(client_pid),new_payload_size);
	return client_pid;
}

static int send_error_ack(struct nlmsghdr *nlh, int dest_pid,int error_code){
	int ret=0;
	struct nlmsgerr error;
	error.error=error_code;
	error.msg=*nlh;
	ret=send_nl_msg(dest_pid,NLMSG_ERROR,(void*)&error,sizeof(error));
	return ret;
}

static int send_nl_msg(__u32 dest_pid,__u16 msg_type, void* payload, int payload_size){
	int ret=0;
	struct sk_buff *skb_out;
	struct nlmsghdr *nlh_out;
	skb_out = nlmsg_new(payload_size, 0);
	if (!skb_out) {
		printk(KERN_ERR LOG_PREFIX"Failed to allocate new skb\n");
		ret=1;
	}
	if(ret==0){
		nlh_out = nlmsg_put(skb_out, 0, 0, msg_type, payload_size, 0);
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		memcpy(nlmsg_data(nlh_out),payload,payload_size);
		ret = nlmsg_unicast(nl_sk, skb_out, dest_pid);
		if(ret<0){
			printk(KERN_INFO LOG_PREFIX"Error while transferring msg type %d to pid %d \n",msg_type,dest_pid);
		}
	}
	return ret;
}

module_init(uselesskm_init);
module_exit(uselesskm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(KM_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(KM_DESC);	/* What does this module do */
