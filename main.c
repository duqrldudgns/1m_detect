#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>		/* for NF_ACCEPT */
#include <errno.h>

#include <libnetfilter_queue/libnetfilter_queue.h>
#include <libnet.h>
#include <regex.h>

static int NF = 1;


void FileCheck(char *URL){
    char top_1m_File[100];
    char *pStr;
    FILE *pFile = NULL;

    pFile = fopen("/root/qt/1m_detect/top-100test.csv","r");
    if( pFile != NULL ){
        while(1){
            pStr = fgets( top_1m_File, sizeof(top_1m_File), pFile );
            if(feof(pFile))break;
            //printf("%s", pStr);
            if(!memcmp( URL, (char *)pStr, sizeof(pStr))){
                NF=0;
                printf("\n%s NOPE\n",URL);
            }
        }
        fclose( pFile );
    }
    else{
        printf("Fileopen fail..");
    }
}

void dump(unsigned char* data, int size) {
    /*int i;
    for (i =0; i < size; i++){
        if (i % 16 == 0 )
            printf("\n");
        printf("%02x",data[i]);
    }*/

    NF = 1;     //ACCEPT = 1 , DROP = 0

    struct libnet_ipv4_hdr* iph = (struct libnet_ipv4_hdr *)data;

    if(iph->ip_p == IPPROTO_TCP){
        struct libnet_tcp_hdr* tcph = (struct libnet_tcp_hdr *)(data+(iph->ip_hl<<2));
        int http_len = ntohs(iph->ip_len) - (iph->ip_hl<<2) - (tcph->th_off<<2);

        if( (ntohs(tcph->th_dport) ==80) && (http_len > 0) ){
            u_char* httph = (u_char *)tcph + (tcph->th_off<<2);
            regex_t preg;
            char *pattern = "Host: ([A-Za-z\\.0-9]+)";            //char *string = "Host: test.gilgil.net";
            int rc;
            size_t nmatch = 2;
            regmatch_t pmatch[2];

            printf("\n");
            if (0 != (rc = regcomp(&preg, pattern, REG_EXTENDED))) {
                printf("regcomp() failed, returning nonzero (%d)\n", rc);
                exit(EXIT_FAILURE);
            }
            if (0 != (rc = regexec(&preg, (char *)httph, nmatch, pmatch, REG_EXTENDED))) {
                printf("Failed to match '%s' with '%s',returning %d.\n",(char *)httph, pattern, rc);
            }
            else {
                printf("With the sub-expression, "
                       "a matched substring \"%.*s\" is found at position %d to %d.\n",
                       pmatch[1].rm_eo - pmatch[1].rm_so, &httph[pmatch[1].rm_so],
                        pmatch[1].rm_so, pmatch[1].rm_eo - 1);                    //test.gilgil.net
                /*
                printf("With the whole expression, "
                           "a matched substring \"%.*s\" is found at position %d to %d.\n",
                           pmatch[0].rm_eo - pmatch[0].rm_so, &httph[pmatch[0].rm_so],
                           pmatch[0].rm_so, pmatch[0].rm_eo - 1);               //Host: test.gilgil.net
*/
                //printf("\n%.*s\n",pmatch[1].rm_eo - pmatch[1].rm_so ,&httph[pmatch[1].rm_so]);   // test.gilgil.net
                //printf("\n%s\n",&httph[pmatch[1].rm_eo]);                     //html ---------
                //printf("\n%s\n",&httph[pmatch[1].rm_so]);                     //html test.gilgil------
                //printf("\n%s\n",&httph[pmatch[1].rm_eo-pmatch[1].rm_so]);     //html  Host: test.gilgil------
                char URL[100];
                sprintf(URL,"%.*s",pmatch[1].rm_eo - pmatch[1].rm_so ,&httph[pmatch[1].rm_so]);
                printf("\ncatch URL : %s\n",URL);

                FileCheck(URL);
            }
            regfree(&preg);
        }
    }
}

/* returns packet id */
static uint32_t print_pkt (struct nfq_data *tb)
{
    int id = 0;
    struct nfqnl_msg_packet_hdr *ph;
    struct nfqnl_msg_packet_hw *hwph;
    uint32_t mark, ifi, uid, gid;
    int ret;
    unsigned char *data, *secdata;

    ph = nfq_get_msg_packet_hdr(tb);
    if (ph) {
        id = ntohl(ph->packet_id);
        printf("hw_protocol=0x%04x hook=%u id=%u ",
               ntohs(ph->hw_protocol), ph->hook, id);
    }

    hwph = nfq_get_packet_hw(tb);
    if (hwph) {
        int i, hlen = ntohs(hwph->hw_addrlen);

        printf("hw_src_addr=");
        for (i = 0; i < hlen-1; i++)
            printf("%02x:", hwph->hw_addr[i]);
        printf("%02x ", hwph->hw_addr[hlen-1]);
    }

    mark = nfq_get_nfmark(tb);
    if (mark)
        printf("mark=%u ", mark);

    ifi = nfq_get_indev(tb);
    if (ifi)
        printf("indev=%u ", ifi);

    ifi = nfq_get_outdev(tb);
    if (ifi)
        printf("outdev=%u ", ifi);
    ifi = nfq_get_physindev(tb);
    if (ifi)
        printf("physindev=%u ", ifi);

    ifi = nfq_get_physoutdev(tb);
    if (ifi)
        printf("physoutdev=%u ", ifi);

    if (nfq_get_uid(tb, &uid))
        printf("uid=%u ", uid);

    if (nfq_get_gid(tb, &gid))
        printf("gid=%u ", gid);

    ret = nfq_get_secctx(tb, &secdata);
    if (ret > 0)
        printf("secctx=\"%.*s\" ", ret, secdata);

    ret = nfq_get_payload(tb, &data);
    if (ret >= 0){
        printf(" payload_len=%d ", ret);
        dump(data,ret);

    }
    fputc('\n', stdout);
    return id;
}


static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
              struct nfq_data *nfa, void *data)
{
    uint32_t id = print_pkt(nfa);
    printf("entering callback\n");
    return nfq_set_verdict(qh, id, NF, 0, NULL); //NF_DROP 0, NF_ACCEPT 1
}

int main(int argc, char **argv){
    struct nfq_handle *h;
    struct nfq_q_handle *qh;
    int fd;
    int rv;
    uint32_t queue = 0;
    char buf[4096] __attribute__ ((aligned));

    /*
    if (argc == 2) {
        queue = atoi(argv[1]);
        if (queue > 65535) {
            fprintf(stderr, "Usage: %s [<0-65535>]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
*/
    printf("opening library handle\n");
    h = nfq_open();
    if (!h) {
        fprintf(stderr, "error during nfq_open()\n");
        exit(1);
    }

    printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
    if (nfq_unbind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "error during nfq_unbind_pf()\n");
        exit(1);
    }

    printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
    if (nfq_bind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "error during nfq_bind_pf()\n");
        exit(1);
    }

    printf("binding this socket to queue '0'\n");
    qh = nfq_create_queue(h, 0, &cb, NULL);
    if (!qh) {
        fprintf(stderr, "error during nfq_create_queue()\n");
        exit(1);
    }

    printf("setting copy_packet mode\n");
    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
        fprintf(stderr, "can't set packet_copy mode\n");
        exit(1);
    }

    printf("setting flags to request UID and GID\n");
    if (nfq_set_queue_flags(qh, NFQA_CFG_F_UID_GID, NFQA_CFG_F_UID_GID)) {
        fprintf(stderr, "This kernel version does not allow to "
                        "retrieve process UID/GID.\n");
    }

    printf("setting flags to request security context\n");
    if (nfq_set_queue_flags(qh, NFQA_CFG_F_SECCTX, NFQA_CFG_F_SECCTX)) {
        fprintf(stderr, "This kernel version does not allow to "
                        "retrieve security context.\n");
    }

    printf("Waiting for packets...\n");

    fd = nfq_fd(h);

    for (;;) {
        if ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
            printf("pkt received\n");
            nfq_handle_packet(h, buf, rv);
            continue;
        }
        /* if your application is too slow to digest the packets that
         * are sent from kernel-space, the socket buffer that we use
         * to enqueue packets may fill up returning ENOBUFS. Depending
         * on your application, this error may be ignored. Please, see
         * the doxygen documentation of this library on how to improve
         * this situation.
         */
        if (rv < 0 && errno == ENOBUFS) {
            printf("losing packets!\n");
            continue;
        }
        perror("recv failed");
        break;
    }

    printf("unbinding from queue 0\n");
    nfq_destroy_queue(qh);

#ifdef INSANE
    /* normally, applications SHOULD NOT issue this command, since
     * it detaches other programs/sockets from AF_INET, too ! */
    printf("unbinding from AF_INET\n");
    nfq_unbind_pf(h, AF_INET);
#endif

    printf("closing library handle\n");
    nfq_close(h);

    exit(0);
}
