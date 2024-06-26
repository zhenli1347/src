/*	$OpenBSD: ospf6d.h,v 1.52 2024/05/18 11:17:30 jsg Exp $ */

/*
 * Copyright (c) 2004, 2007 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _OSPF6D_H_
#define _OSPF6D_H_

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <net/if.h>
#include <netinet/in.h>
#include <event.h>

#include <imsg.h>
#include "ospf6.h"
#include "log.h"

#define CONF_FILE		"/etc/ospf6d.conf"
#define	OSPF6D_SOCKET		"/var/run/ospf6d.sock"
#define OSPF6D_USER		"_ospf6d"

#define NBR_HASHSIZE		128
#define LSA_HASHSIZE		512

#define NBR_IDSELF		1
#define NBR_CNTSTART		(NBR_IDSELF + 1)

#define	READ_BUF_SIZE		65535
#define	PKG_DEF_SIZE		512	/* compromise */
#define	RT_BUF_SIZE		16384
#define	MAX_RTSOCK_BUF		(2 * 1024 * 1024)

#define	OSPFD_FLAG_NO_FIB_UPDATE	0x0001
#define	OSPFD_FLAG_STUB_ROUTER		0x0002

#define	F_OSPFD_INSERTED	0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0008
#define	F_DOWN			0x0010
#define	F_STATIC		0x0020
#define	F_DYNAMIC		0x0040
#define	F_REJECT		0x0080
#define	F_BLACKHOLE		0x0100
#define	F_REDISTRIBUTED		0x0200

static const char * const log_procnames[] = {
	"parent",
	"ospfe",
	"rde"
};

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	void			*data;
	short			 events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_RELOAD,
	IMSG_CTL_SHOW_INTERFACE,
	IMSG_CTL_SHOW_DATABASE,
	IMSG_CTL_SHOW_DB_EXT,
	IMSG_CTL_SHOW_DB_LINK,
	IMSG_CTL_SHOW_DB_NET,
	IMSG_CTL_SHOW_DB_RTR,
	IMSG_CTL_SHOW_DB_INTRA,
	IMSG_CTL_SHOW_DB_SELF,
	IMSG_CTL_SHOW_DB_SUM,
	IMSG_CTL_SHOW_DB_ASBR,
	IMSG_CTL_SHOW_NBR,
	IMSG_CTL_SHOW_RIB,
	IMSG_CTL_SHOW_SUM,
	IMSG_CTL_SHOW_SUM_AREA,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_FIB_RELOAD,
	IMSG_CTL_AREA,
	IMSG_CTL_IFACE,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_END,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_CONTROLFD,
	IMSG_KROUTE_CHANGE,
	IMSG_KROUTE_DELETE,
	IMSG_IFINFO,
	IMSG_IFADDRNEW,
	IMSG_IFADDRDEL,
	IMSG_NEIGHBOR_UP,
	IMSG_NEIGHBOR_DOWN,
	IMSG_NEIGHBOR_CHANGE,
	IMSG_NETWORK_ADD,
	IMSG_NETWORK_DEL,
	IMSG_AREA_CHANGE,
	IMSG_DD,
	IMSG_DD_END,
	IMSG_DB_SNAPSHOT,
	IMSG_DB_END,
	IMSG_LS_REQ,
	IMSG_LS_UPD,
	IMSG_LS_SNAP,
	IMSG_LS_ACK,
	IMSG_LS_FLOOD,
	IMSG_LS_BADREQ,
	IMSG_LS_MAXAGE,
	IMSG_ABR_UP,
	IMSG_ABR_DOWN,
	IMSG_RECONF_CONF,
	IMSG_RECONF_AREA,
	IMSG_RECONF_END,
	IMSG_DEMOTE
};

/* area */
struct vertex;
struct rde_nbr;
RB_HEAD(lsa_tree, vertex);

struct area {
	LIST_ENTRY(area)	 entry;
	struct in_addr		 id;
	struct lsa_tree		 lsa_tree;

	LIST_HEAD(, iface)	 iface_list;
	LIST_HEAD(, rde_nbr)	 nbr_list;
/*	list			 addr_range_list; */
	char			 demote_group[IFNAMSIZ];
	u_int32_t		 stub_default_cost;
	u_int32_t		 num_spf_calc;
	int			 active;
	u_int8_t		 transit;
	u_int8_t		 stub;
	u_int8_t		 dirty;
	u_int8_t		 demote_level;
};

/* interface states */
#define	IF_STA_NEW		0x00	/* dummy state for reload */
#define	IF_STA_DOWN		0x01
#define	IF_STA_LOOPBACK		0x02
#define	IF_STA_WAITING		0x04
#define	IF_STA_POINTTOPOINT	0x08
#define	IF_STA_DROTHER		0x10
#define	IF_STA_BACKUP		0x20
#define	IF_STA_DR		0x40
#define IF_STA_DRORBDR		(IF_STA_DR | IF_STA_BACKUP)
#define	IF_STA_MULTI		(IF_STA_DROTHER | IF_STA_BACKUP | IF_STA_DR)
#define	IF_STA_ANY		0x7f

/* interface events */
enum iface_event {
	IF_EVT_NOTHING,
	IF_EVT_UP,
	IF_EVT_WTIMER,
	IF_EVT_BACKUP_SEEN,
	IF_EVT_NBR_CHNG,
	IF_EVT_LOOP,
	IF_EVT_UNLOOP,
	IF_EVT_DOWN
};

/* interface actions */
enum iface_action {
	IF_ACT_NOTHING,
	IF_ACT_STRT,
	IF_ACT_ELECT,
	IF_ACT_RST
};

/* interface types */
enum iface_type {
	IF_TYPE_POINTOPOINT,
	IF_TYPE_BROADCAST,
	IF_TYPE_NBMA,
	IF_TYPE_POINTOMULTIPOINT,
	IF_TYPE_VIRTUALLINK
};

/* neighbor states */
#define	NBR_STA_DOWN		0x0001
#define	NBR_STA_ATTEMPT		0x0002
#define	NBR_STA_INIT		0x0004
#define	NBR_STA_2_WAY		0x0008
#define	NBR_STA_XSTRT		0x0010
#define NBR_STA_SNAP		0x0020
#define	NBR_STA_XCHNG		0x0040
#define	NBR_STA_LOAD		0x0080
#define	NBR_STA_FULL		0x0100
#define	NBR_STA_ACTIVE		(~NBR_STA_DOWN)
#define	NBR_STA_FLOOD		(NBR_STA_XCHNG | NBR_STA_LOAD | NBR_STA_FULL)
#define	NBR_STA_ADJFORM		(NBR_STA_XSTRT | NBR_STA_SNAP | NBR_STA_FLOOD)
#define	NBR_STA_BIDIR		(NBR_STA_2_WAY | NBR_STA_ADJFORM)
#define	NBR_STA_PRELIM		(NBR_STA_DOWN | NBR_STA_ATTEMPT | NBR_STA_INIT)
#define	NBR_STA_ANY		0xffff

/* neighbor events */
enum nbr_event {
	NBR_EVT_NOTHING,
	NBR_EVT_HELLO_RCVD,
	NBR_EVT_2_WAY_RCVD,
	NBR_EVT_NEG_DONE,
	NBR_EVT_SNAP_DONE,
	NBR_EVT_XCHNG_DONE,
	NBR_EVT_BAD_LS_REQ,
	NBR_EVT_LOAD_DONE,
	NBR_EVT_ADJ_OK,
	NBR_EVT_SEQ_NUM_MIS,
	NBR_EVT_1_WAY_RCVD,
	NBR_EVT_KILL_NBR,
	NBR_EVT_ITIMER,
	NBR_EVT_LL_DOWN,
	NBR_EVT_ADJTMOUT
};

/* neighbor actions */
enum nbr_action {
	NBR_ACT_NOTHING,
	NBR_ACT_RST_ITIMER,
	NBR_ACT_STRT_ITIMER,
	NBR_ACT_EVAL,
	NBR_ACT_SNAP,
	NBR_ACT_SNAP_DONE,
	NBR_ACT_XCHNG_DONE,
	NBR_ACT_ADJ_OK,
	NBR_ACT_RESTRT_DD,
	NBR_ACT_DEL,
	NBR_ACT_CLR_LST,
	NBR_ACT_HELLO_CHK
};

/* spf states */
enum spf_state {
	SPF_IDLE,
	SPF_DELAY,
	SPF_HOLD,
	SPF_HOLDQUEUE
};

enum dst_type {
	DT_NET,
	DT_RTR
};

enum path_type {
	PT_INTRA_AREA,
	PT_INTER_AREA,
	PT_TYPE1_EXT,
	PT_TYPE2_EXT
};

enum rib_type {
	RIB_NET = 1,
	RIB_RTR,
	RIB_EXT
};

struct iface_addr {
	TAILQ_ENTRY(iface_addr)	 entry;
	struct in6_addr		 addr;
	struct in6_addr		 dstbrd;
	u_int8_t		 prefixlen;
	u_int8_t		 redistribute;
};

/* lsa list used in RDE and OE */
TAILQ_HEAD(lsa_head, lsa_entry);

struct iface {
	LIST_ENTRY(iface)	 entry;
	TAILQ_ENTRY(iface)	 list;
	struct event		 hello_timer;
	struct event		 wait_timer;
	struct event		 lsack_tx_timer;

	LIST_HEAD(, nbr)	 nbr_list;
	TAILQ_HEAD(, iface_addr) ifa_list;
	struct lsa_head		 ls_ack_list;

	struct lsa_tree		 lsa_tree;	/* LSA with link local scope */

	char			 name[IF_NAMESIZE];
	char			 demote_group[IFNAMSIZ];
	char			 dependon[IFNAMSIZ];
	struct in6_addr		 addr;
	struct in6_addr		 dst;
	struct in_addr		 abr_id;
	struct nbr		*dr;	/* designated router */
	struct nbr		*bdr;	/* backup designated router */
	struct nbr		*self;
	struct area		*area;

	u_int64_t		 baudrate;
	u_int32_t		 ls_ack_cnt;
	time_t			 uptime;
	unsigned int		 ifindex;
	u_int			 rdomain;
	int			 fd;
	int			 state;
	int			 mtu;
	int			 depend_ok;
	u_int16_t		 flags;
	u_int16_t		 transmit_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 rxmt_interval;
	u_int16_t		 dead_interval;
	u_int16_t		 metric;
	enum iface_type		 type;
	u_int8_t		 if_type;
	u_int8_t		 linkstate;
	u_int8_t		 priority;
	u_int8_t		 cflags;
#define F_IFACE_PASSIVE		0x01
#define F_IFACE_CONFIGURED	0x02
};

struct ifaddrchange {
	struct in6_addr		 addr;
	struct in6_addr		 dstbrd;
	unsigned int		 ifindex;
	u_int8_t		 prefixlen;
};

/* ospf_conf */
enum ospfd_process {
	PROC_MAIN,
	PROC_OSPF_ENGINE,
	PROC_RDE_ENGINE
};
extern enum ospfd_process ospfd_process;

#define	REDIST_CONNECTED	0x01
#define	REDIST_STATIC		0x02
#define	REDIST_LABEL		0x04
#define	REDIST_ADDR		0x08
#define	REDIST_NO		0x10
#define	REDIST_DEFAULT		0x20

struct redistribute {
	SIMPLEQ_ENTRY(redistribute)	entry;
	struct in6_addr			addr;
	u_int32_t			metric;
	u_int16_t			label;
	u_int16_t			type;
	u_int8_t			prefixlen;
	char				dependon[IFNAMSIZ];
};
SIMPLEQ_HEAD(redist_list, redistribute);

struct ospfd_conf {
	struct event		ev;
	struct in_addr		rtr_id;
	LIST_HEAD(, area)	area_list;
	LIST_HEAD(, vertex)	cand_list;
	struct redist_list	redist_list;

	u_int32_t		opts;
#define OSPFD_OPT_VERBOSE	0x00000001
#define OSPFD_OPT_VERBOSE2	0x00000002
#define OSPFD_OPT_NOACTION	0x00000004
#define OSPFD_OPT_STUB_ROUTER	0x00000008
#define OSPFD_OPT_FORCE_DEMOTE	0x00000010
	u_int32_t		spf_delay;
	u_int32_t		spf_hold_time;
	time_t			uptime;
	int			spf_state;
	int			ospf_socket;
	int			flags;
	int			redist_label_or_prefix;
	u_int8_t		border;
	u_int8_t		redistribute;
	u_int8_t		fib_priority;
	u_int			rdomain;
	char			*csock;
};

/* kroute */
struct kroute {
	struct in6_addr	prefix;
	struct in6_addr	nexthop;
	u_int32_t	ext_tag;
	u_int32_t	metric;
	unsigned int	scope;		/* scope of nexthop */
	u_int16_t	flags;
	u_int16_t	rtlabel;
	u_short		ifindex;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

/* name2id */
struct n2id_label {
	TAILQ_ENTRY(n2id_label)	 entry;
	char			*name;
	u_int16_t		 id;
	u_int32_t		 ext_tag;
	int			 ref;
};

TAILQ_HEAD(n2id_labels, n2id_label);
extern struct n2id_labels rt_labels;

/* control data structures */
struct ctl_iface {
	char			 name[IF_NAMESIZE];
	struct in6_addr		 addr;
	struct in_addr		 area;
	struct in_addr		 rtr_id;
	struct in_addr		 dr_id;
	struct in6_addr		 dr_addr;
	struct in_addr		 bdr_id;
	struct in6_addr		 bdr_addr;
	time_t			 hello_timer;
	time_t			 uptime;
	u_int64_t		 baudrate;
	u_int32_t		 dead_interval;
	unsigned int		 ifindex;
	int			 state;
	int			 mtu;
	int			 nbr_cnt;
	int			 adj_cnt;
	u_int16_t		 transmit_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 flags;
	u_int16_t		 metric;
	u_int16_t		 rxmt_interval;
	enum iface_type		 type;
	u_int8_t		 linkstate;
	u_int8_t		 if_type;
	u_int8_t		 priority;
	u_int8_t		 passive;
};

struct ctl_nbr {
	char			 name[IF_NAMESIZE];
	struct in_addr		 id;
	struct in6_addr		 addr;
	struct in_addr		 dr;
	struct in_addr		 bdr;
	struct in_addr		 area;
	time_t			 dead_timer;
	time_t			 uptime;
	u_int32_t		 db_sum_lst_cnt;
	u_int32_t		 ls_req_lst_cnt;
	u_int32_t		 ls_retrans_lst_cnt;
	u_int32_t		 state_chng_cnt;
	u_int32_t		 options;
	int			 nbr_state;
	int			 iface_state;
	u_int8_t		 priority;
};

struct ctl_rt {
	struct in6_addr		 prefix;
	struct in6_addr		 nexthop;
	struct in_addr		 area;
	struct in_addr		 adv_rtr;
	time_t			 uptime;
	u_int32_t		 cost;
	u_int32_t		 cost2;
	unsigned int		 ifindex;	/* scope of nexthop */
	enum path_type		 p_type;
	enum dst_type		 d_type;
	u_int8_t		 flags;
	u_int8_t		 prefixlen;
	u_int8_t		 connected;
};

struct ctl_sum {
	struct in_addr		 rtr_id;
	u_int32_t		 spf_delay;
	u_int32_t		 spf_hold_time;
	u_int32_t		 num_ext_lsa;
	u_int32_t		 num_area;
	time_t			 uptime;
};

struct ctl_sum_area {
	struct in_addr		 area;
	u_int32_t		 num_iface;
	u_int32_t		 num_adj_nbr;
	u_int32_t		 num_spf_calc;
	u_int32_t		 num_lsa;
};

struct demote_msg {
	char			 demote_group[IF_NAMESIZE];
	int			 level;
};

/* area.c */
struct area	*area_new(void);
int		 area_del(struct area *);
struct area	*area_find(struct ospfd_conf *, struct in_addr);
void		 area_track(struct area *);
int		 area_border_router(struct ospfd_conf *);
u_int32_t	 area_ospf_options(struct area *);

/* carp.c */
int		 carp_demote_init(char *, int);
void		 carp_demote_shutdown(void);
int		 carp_demote_get(char *);
int		 carp_demote_set(char *, int);

/* parse.y */
struct ospfd_conf	*parse_config(char *, int);
int			 cmdline_symset(char *);
void			 conf_clear_redist_list(struct redist_list *);

/* interface.c */
int		 if_init(void);
struct iface	*if_find(unsigned int);
struct iface	*if_findname(char *);
struct iface	*if_new(u_short, char *);
void		 if_update(struct iface *, int, int, u_int8_t, u_int8_t,
		    u_int64_t, u_int32_t);

/* iso_cksum.c */
u_int16_t	 iso_cksum(void *, u_int16_t, u_int16_t);

/* kroute.c */
int		 kr_init(int, u_int, int, u_int8_t);
int		 kr_change(struct kroute *, int);
int		 kr_delete(struct kroute *);
void		 kr_shutdown(void);
void		 kr_fib_couple(void);
void		 kr_fib_decouple(void);
void		 kr_fib_reload(void);
void		 kr_fib_update_prio(u_int8_t);
void		 kr_dispatch_msg(int, short, void *);
void		 kr_show_route(struct imsg *);
void		 kr_reload(int);

void		 embedscope(struct sockaddr_in6 *);
void		 recoverscope(struct sockaddr_in6 *);
void		 addscope(struct sockaddr_in6 *, u_int32_t);
void		 clearscope(struct in6_addr *);
u_int8_t	 mask2prefixlen(struct sockaddr_in6 *);
struct in6_addr	*prefixlen2mask(u_int8_t);
void		inet6applymask(struct in6_addr *, const struct in6_addr *, int);

int		fetchifs(u_short);

/* logmsg.h */
const char	*log_in6addr(const struct in6_addr *);
const char	*log_in6addr_scope(const struct in6_addr *, unsigned int);
const char	*log_rtr_id(u_int32_t);
const char	*log_sockaddr(void *);
const char	*nbr_state_name(int);
const char	*if_state_name(int);
const char	*if_type_name(enum iface_type);
const char	*dst_type_name(enum dst_type);
const char	*path_type_name(enum path_type);

/* name2id.c */
u_int16_t	 rtlabel_name2id(const char *);
const char	*rtlabel_id2name(u_int16_t);
void		 rtlabel_unref(u_int16_t);
u_int32_t	 rtlabel_id2tag(u_int16_t);
u_int16_t	 rtlabel_tag2id(u_int32_t);
void		 rtlabel_tag(u_int16_t, u_int32_t);

/* ospf6d.c */
void	main_imsg_compose_ospfe(int, pid_t, void *, u_int16_t);
void	main_imsg_compose_ospfe_fd(int, pid_t, int);
void	main_imsg_compose_rde(int, pid_t, void *, u_int16_t);
int	ospf_redistribute(struct kroute *, u_int32_t *);
void	merge_config(struct ospfd_conf *, struct ospfd_conf *);
void	imsg_event_add(struct imsgev *);
int	imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t,
	    pid_t, int, void *, u_int16_t);
int	ifstate_is_up(struct iface *iface);

/* printconf.c */
void	print_config(struct ospfd_conf *);

#endif	/* _OSPF6D_H_ */
