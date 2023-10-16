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
    pdu->ethhdr->ethertype = htons(ETH_P_MIP);
    
    pdu->miphdr = (struct mip_hdr *)malloc(sizeof(struct mip_hdr));
        pdu->miphdr->dst = 0;
        pdu->miphdr->src = 0 ;
        pdu->miphdr->ttl = 0x00;
        pdu->miphdr->sdu_len = 0;
        pdu->miphdr->sdu_type = 0;


    return pdu;
}

void fill_pdu(struct pdu *pdu,
          uint8_t *src_mac_addr,
          uint8_t *dst_mac_addr,
          uint8_t src_mip_addr,
          uint8_t dst_mip_addr,
          uint8_t ttl,
          uint8_t sdu_type,
          const uint32_t *sdu,
          uint8_t sdu_len)
{
    
    memcpy(pdu->ethhdr->dst_mac, dst_mac_addr, 6);
    memcpy(pdu->ethhdr->src_mac, src_mac_addr, 6);
    pdu->ethhdr->ethertype = htons(ETH_P_MIP);
    
    pdu->miphdr->dst = dst_mip_addr;
    pdu->miphdr->src = src_mip_addr;


    pdu->miphdr->ttl = ttl;

    pdu->miphdr->sdu_type = sdu_type;

    pdu->miphdr->sdu_len = sdu_len;

    pdu->sdu = (uint32_t *)calloc(1, sdu_len/4);
    memcpy(pdu->sdu, sdu, sdu_len);
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
    miphdr |= (uint32_t) (pdu->miphdr->ttl & 0xff) << 12;
    miphdr |= (uint32_t) (pdu->miphdr->sdu_len & 0xff) << 3;
    miphdr |= (uint32_t) (pdu->miphdr->sdu_type & 0xf);

    /* prepare it to be sent from host to network */
    miphdr = htonl(miphdr);

    memcpy(snd_buf + snd_len, &miphdr, MIP_HDR_LEN);
    snd_len += MIP_HDR_LEN;

    /* Attach SDU */
    memcpy(snd_buf + snd_len, pdu->sdu, pdu->miphdr->sdu_len);
    snd_len += pdu->miphdr->sdu_len;



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

    pdu->sdu = (uint32_t *)calloc(1, pdu->miphdr->sdu_len/4);
    memcpy(pdu->sdu, rcv_buf + rcv_len, pdu->miphdr->sdu_len);
    rcv_len += pdu->miphdr->sdu_len;

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
    printf("\t TTL: %u\n", pdu->miphdr->ttl);
    printf("\t SDU length: %u\n", pdu->miphdr->sdu_len);
    printf("\t SDU type: %u\n", pdu->miphdr->sdu_type);

    // Print SDU in uint32 numbers
    printf("\t SDU: ");
    for (int i = 0; i < pdu->miphdr->sdu_len/4; i++) {
        printf("%u ", pdu->sdu[i]);
    }
    printf("\n");
    printf("====================================================\n");
}

void destroy_pdu(struct pdu *pdu)
{
    free(pdu->ethhdr);
    free(pdu->miphdr);
    free(pdu->sdu);
    free(pdu);
}
