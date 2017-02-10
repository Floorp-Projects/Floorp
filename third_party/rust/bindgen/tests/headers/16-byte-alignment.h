
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

struct rte_ipv4_tuple {
        uint32_t        src_addr;
        uint32_t        dst_addr;
        union {
                struct {
                        uint16_t dport;
                        uint16_t sport;
                };
                uint32_t        sctp_tag;
        };
};

struct rte_ipv6_tuple {
        uint8_t         src_addr[16];
        uint8_t         dst_addr[16];
        union {
                struct {
                        uint16_t dport;
                        uint16_t sport;
                };
                uint32_t        sctp_tag;
        };
};

union rte_thash_tuple {
        struct rte_ipv4_tuple   v4;
        struct rte_ipv6_tuple   v6;
} __attribute__((aligned(16)));
