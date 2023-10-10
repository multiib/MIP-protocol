#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "ether.h"
#include "mip.h"
#include "pdu.h"
#include "utils.h"
#include "arp.h"

struct pdu * alloc_pdu(void)
{
	struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));
	
	pdu->ethhdr = (struct eth_hdr *)malloc(sizeof(struct eth_hdr));
	pdu->ethhdr->ethertype = htons(0xFFFF);
	
	pdu->miphdr = (struct mip_hdr *)malloc(sizeof(struct mip_hdr));
        // pdu->miphdr->dst = MIP_DST_ADDR;
        // pdu->miphdr->src = MIP_DST_ADDR ;
        // pdu->miphdr->ttl = 0x0;
        // pdu->miphdr->sdu_len = NULL;
        // pdu->miphdr->sdu_type = NULL;

	return pdu;
}

void fill_pdu(struct pdu *pdu,
	      uint8_t *src_mac_addr,
	      uint8_t *dst_mac_addr,
	      uint8_t src_mip_addr,
	      uint8_t dst_mip_addr,
	      const char *sdu,
	      uint8_t pkt_type)
{
	size_t slen = 0;
	
	memcpy(pdu->ethhdr->dst_mac, dst_mac_addr, 6);
	memcpy(pdu->ethhdr->src_mac, src_mac_addr, 6);
	pdu->ethhdr->ethertype = htons(0xFFFF);
	
        pdu->miphdr->dst = dst_mip_addr;
        pdu->miphdr->src = src_mip_addr;

	if (!pkt_type){
		pdu->miphdr->sdu_type = SDU_TYPE_LOOKUP;
    }
	if (pkt_type == 1){
		pdu->miphdr->sdu_type = SDU_TYPE_MATCH;
    }else{
        perror("Invalid packet type");
        exit(EXIT_FAILURE);
    }

	slen = strlen(sdu) + 1;

	/* SDU length must be divisible by 4 */
	if (slen % 4 != 0)
		slen = slen + (4 - (slen % 4));

	/* to get the real SDU length in bytes, the len value is multiplied by 4 */
        pdu->miphdr->sdu_len = slen / 4;

	pdu->sdu = (uint8_t *)calloc(1, slen);
	memcpy(pdu->sdu, sdu, slen);
}

size_t mip_serialize_pdu(struct pdu *pdu, uint8_t *snd_buf)
{
	size_t snd_len = 0;

	/* Copy ethernet header */
	memcpy(snd_buf + snd_len, pdu->ethhdr, sizeof(struct eth_hdr));
	snd_len += ETH_HDR_LEN;

	/* Copy MIP header */
	uint32_t miphdr = 0;
	miphdr |= (uint32_t) pdu->miphdr->dst << 24;
	miphdr |= (uint32_t) pdu->miphdr->src << 16;
	miphdr |= (uint32_t) (pdu->miphdr->ttl & 0xf) << 12;
	miphdr |= (uint32_t) (pdu->miphdr->sdu_len & 0xff) << 3;
	miphdr |= (uint32_t) (pdu->miphdr->sdu_type & 0xf);

	/* prepare it to be sent from host to network */
	miphdr = htonl(miphdr);

	memcpy(snd_buf + snd_len, &miphdr, MIP_HDR_LEN);
	snd_len += MIP_HDR_LEN;

	/* Attach SDU */
	memcpy(snd_buf + snd_len, pdu->sdu, pdu->miphdr->sdu_len * 4);
	snd_len += pdu->miphdr->sdu_len * 4;

	return snd_len;
}

size_t mip_deserialize_pdu(struct pdu *pdu, uint8_t *rcv_buf)
{
	/* pdu = (struct pdu *)malloc(sizeof(struct pdu)); */
	size_t rcv_len = 0;

	/* Unpack ethernet header */
	pdu->ethhdr = (struct eth_hdr *)malloc(ETH_HDR_LEN);
	memcpy(pdu->ethhdr, rcv_buf + rcv_len, ETH_HDR_LEN);
	rcv_len += ETH_HDR_LEN;

	pdu->miphdr = (struct mip_hdr *)malloc(MIP_HDR_LEN);
	uint32_t *tmp = (uint32_t *) (rcv_buf + rcv_len);
	uint32_t header = ntohl(*tmp);
	pdu->miphdr->dst = (uint8_t) (header >> 24);
	pdu->miphdr->src = (uint8_t) (header >> 16);
	pdu->miphdr->ttl = (size_t) (((header >> 12) & 0xf));
	pdu->miphdr->sdu_len = (uint8_t) ((header >> 3) & 0x3f);
	pdu->miphdr->sdu_type = (uint8_t) (header & 0xf);
	rcv_len += MIP_HDR_LEN;

	pdu->sdu = (uint8_t *)calloc(1, pdu->miphdr->sdu_len * 4);
	memcpy(pdu->sdu, rcv_buf + rcv_len, pdu->miphdr->sdu_len * 4);
	rcv_len += pdu->miphdr->sdu_len * 4;

	return rcv_len;
}

void print_pdu_content(struct pdu *pdu)
{
	printf("====================================================\n");
	printf("\t Source MAC address: ");
	print_mac_addr(pdu->ethhdr->src_mac, 6);
	printf("\t Destination MAC address: ");
	print_mac_addr(pdu->ethhdr->dst_mac, 6);
	printf("\t Ethertype: 0x%04x\n", pdu->ethhdr->ethertype);

	printf("\t Source MIP address: %u\n", pdu->miphdr->src);
	printf("\t Destination MIP address: %u\n", pdu->miphdr->dst);
    printf("\t TTL: %d\n", pdu->miphdr->ttl);
	printf("\t SDU length: %d\n", pdu->miphdr->sdu_len * 4);
    printf("\t SDU type: %d\n", pdu->miphdr->sdu_type);

	printf("\t SDU: %s\n", pdu->sdu);
	printf("====================================================\n");
}

void destroy_pdu(struct pdu *pdu)
{
	free(pdu->ethhdr);
	free(pdu->miphdr);
	free(pdu->sdu);
	free(pdu);
}
