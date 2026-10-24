/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef XPON_NL_TRANSPORT_H
#define XPON_NL_TRANSPORT_H

#include <errno.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define XPON_NL_REQ_SIZE 4096
#define XPON_NL_RX_SIZE  32768
#define XPON_NL_TIMEOUT_SEC 2

#ifndef NLA_TYPE_MASK
#define NLA_TYPE_MASK ~(NLA_F_NESTED | NLA_F_NET_BYTEORDER)
#endif

struct genl_client {
	int fd;
	const char *family_name;
	uint16_t family_id;
	uint32_t family_version;
	uint32_t portid;
	uint32_t seq;
};

struct nl_request {
	unsigned char buf[XPON_NL_REQ_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
};

static struct {
	int code;
	char message[192];
} last_error;

static inline void set_error(int code, const char *message)
{
	last_error.code = code;
	if (!message)
		message = code ? strerror(code < 0 ? -code : code) : "success";
	snprintf(last_error.message, sizeof(last_error.message), "%s", message);
}

static inline void set_errno_error(int code, const char *where)
{
	char buf[sizeof(last_error.message)];
	int e = code < 0 ? -code : code;

	snprintf(buf, sizeof(buf), "%s: %s", where, strerror(e));
	set_error(-e, buf);
}

static inline size_t nla_align_len(size_t len)
{
	return (len + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1);
}

static inline void *nla_data_ptr(const struct nlattr *attr)
{
	return (char *)attr + NLA_HDRLEN;
}

static inline int nla_payload_len(const struct nlattr *attr)
{
	return (int)attr->nla_len - NLA_HDRLEN;
}

static inline bool attr_ok(const struct nlattr *attr, int remaining)
{
	return remaining >= (int)sizeof(*attr) &&
	       attr->nla_len >= sizeof(*attr) &&
	       attr->nla_len <= remaining;
}

static inline struct nlattr *attr_next(struct nlattr *attr, int *remaining)
{
	int step = (int)nla_align_len(attr->nla_len);

	*remaining -= step;
	return (struct nlattr *)((char *)attr + step);
}

static inline void parse_attrs(struct nlattr **tb, unsigned int max,
			struct nlattr *attr, int remaining)
{
	memset(tb, 0, sizeof(*tb) * (max + 1));

	while (attr_ok(attr, remaining)) {
		unsigned int type = attr->nla_type & NLA_TYPE_MASK;

		if (type <= max)
			tb[type] = attr;
		attr = attr_next(attr, &remaining);
	}
}

static inline uint8_t attr_u8(const struct nlattr *attr)
{
	uint8_t v = 0;
	if (attr && nla_payload_len(attr) >= (int)sizeof(v))
		memcpy(&v, nla_data_ptr(attr), sizeof(v));
	return v;
}

static inline int32_t attr_s32(const struct nlattr *attr)
{
	int32_t v = 0;
	if (attr && nla_payload_len(attr) >= (int)sizeof(v))
		memcpy(&v, nla_data_ptr(attr), sizeof(v));
	return v;
}

static inline uint16_t attr_u16(const struct nlattr *attr)
{
	uint16_t v = 0;
	if (attr && nla_payload_len(attr) >= (int)sizeof(v))
		memcpy(&v, nla_data_ptr(attr), sizeof(v));
	return v;
}

static inline uint32_t attr_u32(const struct nlattr *attr)
{
	uint32_t v = 0;
	if (attr && nla_payload_len(attr) >= (int)sizeof(v))
		memcpy(&v, nla_data_ptr(attr), sizeof(v));
	return v;
}

static inline uint64_t attr_u64(const struct nlattr *attr)
{
	uint64_t v = 0;
	if (attr && nla_payload_len(attr) >= (int)sizeof(v))
		memcpy(&v, nla_data_ptr(attr), sizeof(v));
	return v;
}

static inline void request_init(struct genl_client *ctx, struct nl_request *req,
			 uint16_t type, uint8_t cmd, uint8_t version,
			 uint16_t flags)
{
	memset(req, 0, sizeof(*req));
	req->nlh = (struct nlmsghdr *)req->buf;
	req->nlh->nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	req->nlh->nlmsg_type = type;
	req->nlh->nlmsg_flags = NLM_F_REQUEST | flags;
	req->nlh->nlmsg_seq = ++ctx->seq;
	req->nlh->nlmsg_pid = ctx->portid;
	req->genl = (struct genlmsghdr *)NLMSG_DATA(req->nlh);
	req->genl->cmd = cmd;
	req->genl->version = version;
}

static inline int add_attr(struct nl_request *req, uint16_t type,
		    const void *data, size_t len)
{
	size_t attr_len = NLA_HDRLEN + len;
	size_t aligned = nla_align_len(attr_len);
	struct nlattr *attr;

	if (NLMSG_ALIGN(req->nlh->nlmsg_len) + aligned > sizeof(req->buf))
		return -EMSGSIZE;

	attr = (struct nlattr *)(req->buf + NLMSG_ALIGN(req->nlh->nlmsg_len));
	attr->nla_type = type;
	attr->nla_len = attr_len;
	if (len)
		memcpy(nla_data_ptr(attr), data, len);
	if (aligned > attr_len)
		memset((char *)attr + attr_len, 0, aligned - attr_len);
	req->nlh->nlmsg_len = NLMSG_ALIGN(req->nlh->nlmsg_len) + aligned;
	return 0;
}

static inline int add_u8(struct nl_request *req, uint16_t type, uint8_t value)
{
	return add_attr(req, type, &value, sizeof(value));
}

static inline int add_u16(struct nl_request *req, uint16_t type, uint16_t value)
{
	return add_attr(req, type, &value, sizeof(value));
}

static inline int add_u32(struct nl_request *req, uint16_t type, uint32_t value)
{
	return add_attr(req, type, &value, sizeof(value));
}

static inline int nl_send_request(struct genl_client *ctx, struct nl_request *req)
{
	struct sockaddr_nl peer = { .nl_family = AF_NETLINK };
	ssize_t n;

	n = sendto(ctx->fd, req->buf, req->nlh->nlmsg_len, 0,
		   (struct sockaddr *)&peer, sizeof(peer));
	if (n < 0)
		return -errno;
	if ((size_t)n != req->nlh->nlmsg_len)
		return -EIO;
	return 0;
}

typedef int (*reply_cb_t)(struct nlmsghdr *nlh, void *arg);

static inline int recv_for_seq(struct genl_client *ctx, uint32_t seq,
                        uint16_t expected_type, bool expect_reply,
                        reply_cb_t cb, void *arg)
{
	unsigned char buf[XPON_NL_RX_SIZE];

	for (;;) {
		struct sockaddr_nl peer = {};
		struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };
		struct msghdr msg = {
			.msg_name = &peer, .msg_namelen = sizeof(peer),
			.msg_iov = &iov, .msg_iovlen = 1,
		};
		struct nlmsghdr *nlh;
		ssize_t len = recvmsg(ctx->fd, &msg, 0);
		int remaining, ret;

		if (len < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (!len)
			return -ECONNRESET;
		if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
			return -EMSGSIZE;
		if (peer.nl_family != AF_NETLINK || peer.nl_pid != 0)
			continue;
		remaining = len;
		for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, remaining);
		     nlh = NLMSG_NEXT(nlh, remaining)) {
			if (nlh->nlmsg_seq != seq)
				continue;
			if (nlh->nlmsg_flags & NLM_F_DUMP_INTR)
				return -EAGAIN;
			if (nlh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *e;

				if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*e)))
					return -EBADMSG;
				e = NLMSG_DATA(nlh);
				if (e->error || !expect_reply)
					return e->error;
				continue;
			}
			if (nlh->nlmsg_type == NLMSG_DONE) {
				int error = 0;

				if (NLMSG_PAYLOAD(nlh, 0) >= sizeof(error))
					memcpy(&error, NLMSG_DATA(nlh), sizeof(error));
				return error;
			}
			if (nlh->nlmsg_type != expected_type)
				return -EBADMSG;
			ret = cb ? cb(nlh, arg) : 0;
			if (ret || !(nlh->nlmsg_flags & NLM_F_MULTI))
				return ret;
		}
		if (remaining)
			return -EBADMSG;
	}
}

static inline int request_exec(struct genl_client *ctx, struct nl_request *req,
			bool expect_reply, reply_cb_t cb, void *arg)
{
	uint32_t seq = req->nlh->nlmsg_seq;
	int ret;

	if (!expect_reply)
		req->nlh->nlmsg_flags |= NLM_F_ACK;

	ret = nl_send_request(ctx, req);
	if (ret)
		return ret;
	return recv_for_seq(ctx, seq, req->nlh->nlmsg_type, expect_reply,
			    expect_reply ? cb : NULL, arg);
}

static inline int resolve_family_reply(struct nlmsghdr *nlh, void *arg)
{
	struct genl_client *ctx = arg;
	struct genlmsghdr *genl;
	struct nlattr *attr;
	int remaining;

	if (nlh->nlmsg_len < NLMSG_LENGTH(GENL_HDRLEN))
		return -EBADMSG;
	genl = NLMSG_DATA(nlh);
	remaining = nlh->nlmsg_len - NLMSG_HDRLEN - GENL_HDRLEN;
	attr = (struct nlattr *)((char *)genl + GENL_HDRLEN);

	while (attr_ok(attr, remaining)) {
		switch (attr->nla_type & NLA_TYPE_MASK) {
		case CTRL_ATTR_FAMILY_ID:
			ctx->family_id = attr_u16(attr);
			break;
		case CTRL_ATTR_VERSION:
			ctx->family_version = attr_u32(attr);
			break;
		default:
			break;
		}
		attr = attr_next(attr, &remaining);
	}
	return ctx->family_id ? 0 : -ENOENT;
}

static inline int resolve_family(struct genl_client *ctx)
{
	struct nl_request req;
	int ret;

	request_init(ctx, &req, GENL_ID_CTRL, CTRL_CMD_GETFAMILY, 1, 0);
	ret = add_attr(&req, CTRL_ATTR_FAMILY_NAME, ctx->family_name,
		       strlen(ctx->family_name) + 1);
	if (ret)
		return ret;
	return request_exec(ctx, &req, true, resolve_family_reply, ctx);
}

static inline int nl_open(struct genl_client *ctx, const char *name)
{
	struct sockaddr_nl local = { .nl_family = AF_NETLINK };
	struct timeval tv = { .tv_sec = XPON_NL_TIMEOUT_SEC };
	socklen_t addrlen = sizeof(local);
	int ret;

	memset(ctx, 0, sizeof(*ctx));
	ctx->family_name = name;
	ctx->fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
	if (ctx->fd < 0)
		return -errno;
	if (setsockopt(ctx->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) ||
	    setsockopt(ctx->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv))) {
		ret = -errno;
		close(ctx->fd);
		ctx->fd = -1;
		return ret;
	}
	if (bind(ctx->fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		ret = -errno;
		close(ctx->fd);
		ctx->fd = -1;
		return ret;
	}
	if (getsockname(ctx->fd, (struct sockaddr *)&local, &addrlen) < 0) {
		ret = -errno;
		close(ctx->fd);
		ctx->fd = -1;
		return ret;
	}
	ctx->portid = local.nl_pid;
	ctx->seq = (uint32_t)getpid();
	ret = resolve_family(ctx);
	if (ret) {
		close(ctx->fd);
		ctx->fd = -1;
		return ret;
	}
	return 0;
}

static inline void nl_close(struct genl_client *ctx)
{
	if (ctx->fd >= 0)
		close(ctx->fd);
	ctx->fd = -1;
}


#endif /* XPON_NL_TRANSPORT_H */
