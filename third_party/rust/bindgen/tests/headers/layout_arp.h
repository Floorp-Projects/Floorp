typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define ETHER_ADDR_LEN  6 /**< Length of Ethernet address. */

/**
 * Ethernet address:
 * A universally administered address is uniquely assigned to a device by its
 * manufacturer. The first three octets (in transmission order) contain the
 * Organizationally Unique Identifier (OUI). The following three (MAC-48 and
 * EUI-48) octets are assigned by that organization with the only constraint
 * of uniqueness.
 * A locally administered address is assigned to a device by a network
 * administrator and does not contain OUIs.
 * See http://standards.ieee.org/regauth/groupmac/tutorial.html
 */
struct ether_addr {
	uint8_t addr_bytes[ETHER_ADDR_LEN]; /**< Addr bytes in tx order */
} __attribute__((__packed__));

/**
 * ARP header IPv4 payload.
 */
struct arp_ipv4 {
	struct ether_addr arp_sha;  /**< sender hardware address */
	uint32_t          arp_sip;  /**< sender IP address */
	struct ether_addr arp_tha;  /**< target hardware address */
	uint32_t          arp_tip;  /**< target IP address */
} __attribute__((__packed__));

/**
 * ARP header.
 */
struct arp_hdr {
	uint16_t arp_hrd;    /* format of hardware address */
#define ARP_HRD_ETHER     1  /* ARP Ethernet address format */

	uint16_t arp_pro;    /* format of protocol address */
	uint8_t  arp_hln;    /* length of hardware address */
	uint8_t  arp_pln;    /* length of protocol address */
	uint16_t arp_op;     /* ARP opcode (command) */
#define	ARP_OP_REQUEST    1 /* request to resolve address */
#define	ARP_OP_REPLY      2 /* response to previous request */
#define	ARP_OP_REVREQUEST 3 /* request proto addr given hardware */
#define	ARP_OP_REVREPLY   4 /* response giving protocol address */
#define	ARP_OP_INVREQUEST 8 /* request to identify peer */
#define	ARP_OP_INVREPLY   9 /* response identifying peer */

	struct arp_ipv4 arp_data;
} __attribute__((__packed__));