/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2008 Red Hat, Inc.  All rights reserved.
**  
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#ifndef __FD_DOT_H__
#define __FD_DOT_H__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>
#include <time.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>

#include <openais/saAis.h>
#include <openais/cpg.h>

#include "list.h"
#include "linux_endian.h"
#include "libfence.h"
#include "fenced.h"

/* Max name length for a group, pointless since we only ever create the
   "default" group.  Regardless, set arbitrary max to match dlm's
   DLM_LOCKSPACE_LEN 64.  The libcpg limit is larger at 128; we prefix
   the fence domain name with "fenced:" to create the cpg name. */

#define MAX_GROUPNAME_LEN	64

/* Max name length for a node.  This should match libcman's
   CMAN_MAX_NODENAME_LEN which is 255. */

#define MAX_NODENAME_LEN	255

/* Maximum members of the fence domain, or cluster.  Should match
   CPG_MEMBERS_MAX in openais/cpg.h. */

#define MAX_NODES		128

/* Max string length printed on a line, for debugging/dump output. */

#define MAXLINE			256

/* Size of the circular debug buffer. */

#define DUMP_SIZE		(1024 * 1024)

/* group_mode */

#define GROUP_LIBGROUP          2
#define GROUP_LIBCPG            3

extern int daemon_debug_opt;
extern int daemon_quit;
extern struct list_head domains;
extern int cman_quorate;
extern int our_nodeid;
extern char our_name[MAX_NODENAME_LEN+1];
extern char daemon_debug_buf[256];
extern char dump_buf[DUMP_SIZE];
extern int dump_point;
extern int dump_wrap;
extern int group_mode;

extern void daemon_dump_save(void);

#define log_debug(fmt, args...) \
do { \
	snprintf(daemon_debug_buf, 255, "%ld " fmt "\n", time(NULL), ##args); \
	if (daemon_debug_opt) fprintf(stderr, "%s", daemon_debug_buf); \
	daemon_dump_save(); \
} while (0)

#define log_error(fmt, args...) \
do { \
	log_debug(fmt, ##args); \
	syslog(LOG_ERR, fmt, ##args); \
} while (0)

/* config option defaults */

#define DEFAULT_GROUPD_COMPAT	1
#define DEFAULT_CLEAN_START	0
#define DEFAULT_POST_JOIN_DELAY	6
#define DEFAULT_POST_FAIL_DELAY	0
#define DEFAULT_OVERRIDE_TIME   3
#define DEFAULT_OVERRIDE_PATH	"/var/run/cluster/fenced_override"

struct commandline
{
	int groupd_compat;
	int clean_start;
	int post_join_delay;
	int post_fail_delay;
	int override_time;
	char *override_path;

	int8_t groupd_compat_opt;
	int8_t clean_start_opt;
	int8_t post_join_delay_opt;
	int8_t post_fail_delay_opt;
	int8_t override_time_opt;
	int8_t override_path_opt;
};

extern struct commandline comline;

#define FD_MSG_START		1
#define FD_MSG_VICTIM_DONE	2
#define FD_MSG_COMPLETE		3
#define FD_MSG_EXTERNAL		4

#define FD_MFLG_JOINING		1  /* accompanies start, we are joining */
#define FD_MFLG_COMPLETE	2  /* accompanies start, we have complete info */

struct fd_header {
	uint16_t version[3];
	uint16_t type;		/* FD_MSG_ */
	uint32_t nodeid;	/* sender */
	uint32_t to_nodeid;     /* recipient, 0 for all */
	uint32_t global_id;     /* global unique id for this domain */
	uint32_t flags;		/* FD_MFLG_ */
	uint32_t msgdata;       /* in-header payload depends on MSG type */
	uint32_t pad1;
	uint64_t pad2;
};

#define CGST_WAIT_CONDITIONS	1
#define CGST_WAIT_MESSAGES	2

struct change {
	struct list_head list;
	struct list_head members;
	struct list_head removed; /* nodes removed by this change */
	int member_count;
	int joined_count;
	int remove_count;
	int failed_count;
	int state; /* CGST_ */
	int we_joined;
	uint32_t seq; /* just used as a reference when debugging */
};

#define VIC_DONE_AGENT		1
#define VIC_DONE_MEMBER		2
#define VIC_DONE_OVERRIDE	3
#define VIC_DONE_EXTERNAL	4

struct node_history {
	struct list_head list;
	int nodeid;
	int check_quorum;
	uint64_t add_time;
	uint64_t left_time;
	uint64_t fail_time;
	uint64_t fence_time;
	uint64_t fence_external_time;
	int fence_external_node;
	int fence_master;
	int fence_how; /* VIC_DONE_ */
};

struct node {
	struct list_head 	list;
	int			nodeid;
	int			init_victim;
	char 			name[MAX_NODENAME_LEN+1];
};

struct fd {
	struct list_head	list;
	char 			name[MAX_GROUPNAME_LEN+1];

	/* libcpg domain membership */

	cpg_handle_t		cpg_handle;
	int			cpg_client;
	int			cpg_fd;
	uint32_t		change_seq;
	struct change		*started_change;
	struct list_head	changes;
	struct list_head	node_history;
	int			init_complete;

	/* general domain membership */

	int			master;
	int			joining_group;
	int			leaving_group;
	struct list_head 	victims;
	struct list_head	complete;

	/* libgroup domain membership */

	int 			last_stop;
	int 			last_start;
	int 			last_finish;
	int			first_recovery;
	int 			prev_count;
	struct list_head 	prev;
	struct list_head 	leaving;
};

/* config.c */

int read_ccs(struct fd *fd);

/* cpg.c */

void free_cg(struct change *cg);
void node_history_fence(struct fd *fd, int nodeid, int master, int how);
void send_external(struct fd *fd, int victim);
int is_fenced_external(struct fd *fd, int nodeid);
void send_victim_done(struct fd *fd, int victim, int how);
void process_fd_changes(void);
int fd_join(struct fd *fd);
int fd_leave(struct fd *fd);

/* group.c */

void process_groupd(int ci);
int setup_groupd(void);
int fd_join_group(struct fd *fd);
int fd_leave_group(struct fd *fd);

/* main.c */

void client_dead(int ci);
int client_add(int fd, void (*workfn)(int ci), void (*deadfn)(int ci));
void free_fd(struct fd *fd);
struct fd *find_fd(char *name);

/* member_cman.c */

void process_cman(int ci);
int setup_cman(void);
int is_cman_member(int nodeid);
char *nodeid_to_name(int nodeid);
struct node *get_new_node(struct fd *fd, int nodeid);

/* recover.c */

void free_node_list(struct list_head *head);
void add_complete_node(struct fd *fd, int nodeid);
int list_count(struct list_head *head);
void delay_fencing(struct fd *fd, int node_join);
void defer_fencing(struct fd *fd);
void fence_victims(struct fd *fd);

#endif				/*  __FD_DOT_H__  */
