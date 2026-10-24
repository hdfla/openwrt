// SPDX-License-Identifier: GPL-2.0-only
/* Exercise the real decoder and multipart receiver without optical hardware. */
#include <assert.h>
#include <sys/socket.h>

static ssize_t test_recvmsg(int fd, struct msghdr *msg, int flags);
#define recvmsg test_recvmsg
#include "../src/xpon_ucode.c"
#undef recvmsg

static unsigned char packet[8192] __attribute__((aligned(8)));
static size_t packet_len;
static bool truncated;

static ssize_t test_recvmsg(int fd, struct msghdr *msg, int flags)
{
	struct sockaddr_nl *peer = msg->msg_name;
	(void)fd;
	(void)flags;
	assert(packet_len && packet_len <= msg->msg_iov[0].iov_len);
	memcpy(msg->msg_iov[0].iov_base, packet, packet_len);
	peer->nl_family = AF_NETLINK;
	peer->nl_pid = 0;
	msg->msg_flags = truncated ? MSG_TRUNC : 0;
	return packet_len;
}

static void append(const struct nlmsghdr *nlh)
{
	size_t len = NLMSG_ALIGN(nlh->nlmsg_len);
	assert(packet_len + len <= sizeof(packet));
	memcpy(packet + packet_len, nlh, nlh->nlmsg_len);
	packet_len += len;
}

static void init(struct nl_request *req)
{
	struct genl_client ctx = {};
	request_init(&ctx, req, 0x30, XPON_CMD_GET_STATE, XPON_GENL_VERSION, 0);
	assert(!add_u32(req, XPON_ATTR_IFINDEX, 7));
}

static void check(bool ok, const char *name)
{
	assert(ok);
	printf("PASS: %s\n", name);
}

int main(void)
{
	uc_parse_config_t config = { .raw_mode = true };
	uc_vm_t vm = {};
	struct nl_request req;
	struct reply r = { .vm = &vm, .cmd = XPON_CMD_GET_STATE, .ifindex = 7 };
	struct genl_client ctx = {};
	struct { struct nlmsghdr nlh; int error; } done = {
		.nlh = { .nlmsg_len = NLMSG_LENGTH(sizeof(int)),
			 .nlmsg_type = NLMSG_DONE, .nlmsg_seq = 1 },
	};
	uint64_t count = UINT64_MAX;
	bool found = true;
	uc_value_t *counter;

	uc_vm_init(&vm, &config);
	init(&req);
	assert(!add_u32(&req, XPON_ATTR_MODE, XPON_MODE_50GEPON));
	assert(!add_attr(&req, XPON_ATTR_RX_PACKETS, &count, sizeof(count)));
	check(!decode_reply(req.nlh, &r), "native-endian high mode bits decode");
	check(ucv_uint64_get(ucv_object_get(r.value, "mode", NULL)) == XPON_MODE_50GEPON,
	      "mode bitmap is not truncated");
	counter = ucv_object_get(r.value, "rx_oam", NULL);
	check(!strcmp(ucv_string_get(counter),
		      "18446744073709551615"), "64-bit counters survive JSON conversion");
	ucv_object_get(r.value, "los", &found);
	check(!found, "missing optical measurements remain absent");
	ucv_put(r.value);
	r.value = NULL;

	init(&req);
	assert(!add_u8(&req, XPON_ATTR_MODE, 4));
	check(decode_reply(req.nlh, &r) == -EBADMSG, "short scalar attribute is rejected");
	init(&req);
	assert(!add_u32(&req, XPON_ATTR_IFINDEX, 7));
	check(decode_reply(req.nlh, &r) == -EBADMSG, "duplicate identity attribute is rejected");
	init(&req);
	assert(!add_attr(&req, XPON_ATTR_NAME, "bad", 3));
	check(decode_reply(req.nlh, &r) == -EBADMSG, "unterminated interface name is rejected");
	init(&req);
	req.genl->version++;
	check(decode_reply(req.nlh, &r) == -EPROTO, "incompatible UAPI is rejected");
	init(&req);
	r.ifindex = 8;
	check(decode_reply(req.nlh, &r) == -EBADMSG, "reply for another device is rejected");
	r.ifindex = 7;

	init(&req);
	req.nlh->nlmsg_flags = NLM_F_MULTI;
	append(req.nlh);
	append(req.nlh);
	append(&done.nlh);
	r.array = true;
	r.value = ucv_array_new(&vm);
	check(!recv_for_seq(&ctx, 1, 0x30, true, decode_reply, &r) &&
	      ucv_array_length(r.value) == 2, "multipart receiver consumes every object and DONE");
	ucv_put(r.value);
	r.value = NULL;
	packet_len = 0;
	done.error = -EIO;
	append(&done.nlh);
	check(recv_for_seq(&ctx, 1, 0x30, true, decode_reply, &r) == -EIO,
	      "multipart completion error is propagated");
	packet_len = 0;
	req.nlh->nlmsg_flags |= NLM_F_DUMP_INTR;
	append(req.nlh);
	check(recv_for_seq(&ctx, 1, 0x30, true, decode_reply, &r) == -EAGAIN,
	      "interrupted dump is not returned as a complete inventory");
	truncated = true;
	check(recv_for_seq(&ctx, 1, 0x30, true, decode_reply, &r) == -EMSGSIZE,
	      "truncated netlink datagram is rejected");
	uc_vm_free(&vm);
	puts("Native tests passed");
	return 0;
}
