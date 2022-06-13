/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       The puzzle_coap module, provides automated resource & RD handling
 *
 * @author      Bennet Blischke <bennet.blischke@haw-hamburg.de>
 *
 * @}
 */

#include "puzzle_coap.h"
#include "cbor.h"
#include "net/credman.h"

#define DTLS_TAG 1

static const puzzle_t *_puzzle_info;

static ssize_t _puzzle_info_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx);
static ssize_t _puzzle_ready_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx);
static ssize_t _puzzle_maintainance_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx);
static ssize_t _puzzle_encoder(const coap_resource_t *resource, char *buf, size_t maxlen, coap_link_encoder_ctx_t *context);

static ssize_t _puzzle_serial_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx);


/* CoAP resources. Must be sorted by path (ASCII order). */
static coap_resource_t _resources[] = {
    { "/node/info", COAP_GET, _puzzle_info_handler, NULL },
    { "/node/maintainance", COAP_PUT, _puzzle_maintainance_handler, NULL },
    { "/node/ready", COAP_PUT, _puzzle_ready_handler, NULL },
    /*{ "/node/serial", COAP_GET, _puzzle_serial_handler, NULL },*/
};

static coap_resource_t _serials[] = {
    { "/node/serial", COAP_GET, _puzzle_serial_handler, NULL },
};

static gcoap_listener_t _default_listener = {
    _resources,
    ARRAY_SIZE(_resources),
    GCOAP_SOCKET_TYPE_DTLS,
    _puzzle_encoder,
    NULL,
    NULL
};

static gcoap_listener_t _unsecure_listener = {
    _serials,
    ARRAY_SIZE(_serials),
    GCOAP_SOCKET_TYPE_UDP,
    NULL,
    NULL,
    NULL
};

static const credman_credential_t credential = {
    .type = CREDMAN_TYPE_PSK,
    .tag = DTLS_TAG,
    .params = {
        .psk = {
            .key = { .s = psk_key, .len = sizeof(psk_key) - 1, },
            .id = { .s = psk_id, .len = sizeof(psk_id) - 1, },
        }
    },
};

static int _make_sock_ep(sock_udp_ep_t *ep, uri_parser_result_t *uri)
{
    ep->port = 0;
    if (sock_udp_name2ep(ep, uri->host) < 0) {
        return -1;
    }

    /* if netif not specified in addr */
    if ((ep->netif == SOCK_ADDR_ANY_NETIF) && (gnrc_netif_numof() == 1)) {
        /* assign the single interface found in gnrc_netif_numof() */
        ep->netif = (uint16_t)gnrc_netif_iter(NULL)->pid;
    }
    ep->family  = AF_INET6;
    if (uri->port_len == 0) {
        ep->port = COAP_PORT;
    } else {
        ep->port = atoi(uri->port); // Fix me!
    }
    return 0;
}

static size_t _puzzle_build_cbor_buffer(uint8_t *cborbuf, size_t len)
{
    /* 
     * Encode the puzzle info in cbor:
     * {
     *  "name": <string>,
     *  "nodeState": <'provisioned'|'standby'>,
     *  "puzzleState": <'solved'|'ready'|'maintainance'>, 
     * }
     */
    assert(_puzzle_info);

    CborEncoder encoder, mapEncoder;
    cbor_encoder_init(&encoder, cborbuf, len, 0);
    cbor_encoder_create_map(&encoder, &mapEncoder, 3);
    cbor_encode_text_stringz(&mapEncoder, "name");
    /* Potentially dangerous! The string might not be null terminated */
    cbor_encode_text_stringz(&mapEncoder, _puzzle_info->name);

    cbor_encode_text_stringz(&mapEncoder, "puzzleState");
    /* are we ready or are we in maintainance mode? */
    if (_puzzle_info->get_ready_handler()){
        if (_puzzle_info->get_solved_handler()){
            cbor_encode_text_stringz(&mapEncoder, "solved");
        } else {
            cbor_encode_text_stringz(&mapEncoder, "ready");
        }
    } else {
        cbor_encode_text_stringz(&mapEncoder, "maintainance");
    }

    cbor_encode_text_stringz(&mapEncoder, "nodeState");
    cbor_encode_text_stringz(&mapEncoder, "standby"); 

    cbor_encoder_close_container(&encoder, &mapEncoder);
    return cbor_encoder_get_buffer_size(&encoder, cborbuf);
}


static ssize_t _build_cbor_coap_packet(coap_pkt_t *pdu, uint8_t *buf, size_t len)
{
    uint8_t cborbuf[CBOR_BUF_SIZE];

    /* set the format option to CBOR */
    coap_opt_add_format(pdu, COAP_FORMAT_CBOR);

    /* finish the options sections */
    /* it is important to keep track of the amount of used bytes (resp_len) */
    size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);
    size_t cbor_len = _puzzle_build_cbor_buffer(cborbuf, sizeof(cborbuf));


    if (pdu->payload_len >= cbor_len && cbor_len < sizeof(cborbuf)){
        memcpy(pdu->payload, cborbuf, cbor_len);
        return resp_len + cbor_len;
    } else {
        /* in this case we use a simple convenience function to create the
         * response, it only allows to set a payload and a response code. */
        puts("puzzle_coap: msg buffer too small for given puzzle cbor object");
        return gcoap_response(pdu, buf, len, COAP_CODE_INTERNAL_SERVER_ERROR);
    }
}


void puzzle_init(const puzzle_t *puzzle)
{
    int res = 0;
    assert(puzzle);
    assert(puzzle->name);
    assert(puzzle->resource_dir_uri);
    assert(puzzle->serial);
    assert(puzzle->get_solved_handler);
    assert(puzzle->get_ready_handler);
    assert(puzzle->set_ready_handler);

    _puzzle_info = puzzle;
    gcoap_register_listener(&_unsecure_listener);

    sock_udp_ep_t remote;
    uri_parser_result_t uri_result;
    assert(uri_parser_is_absolute_string(puzzle->resource_dir_uri));
    assert(uri_parser_process_string(&uri_result, puzzle->resource_dir_uri) == 0);
    assert(_make_sock_ep(&remote, &uri_result) == 0);

    res = credman_add(&credential);
    if (res < 0 && res != CREDMAN_EXIST) {
        /* ignore duplicate credentials */
        printf("gcoap: cannot add credential to system: %d\n", res);
        return;
    }
    sock_dtls_t *gcoap_sock_dtls = gcoap_get_sock_dtls();
    res = sock_dtls_add_credential(gcoap_sock_dtls, DTLS_TAG);
    if (res < 0) {
        printf("gcoap: cannot add credential to DTLS sock: %d\n", res);
    }

    gcoap_register_listener(&_default_listener);

    puts("Registering with RD now, this may take a short while...");
    #ifdef CONFIG_CORD_EP
    printf("Using serial: %s\n", CONFIG_CORD_EP);
    #endif
    if (cord_ep_register(&remote, NULL) != CORD_EP_OK) {
        puts("error: registration failed");
        return;
    } else {
        puts("registration successfull");
    }
}


void puzzle_update(void)
{
    size_t len = 0;
    uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;

    switch (gcoap_obs_init(&pdu, buf, sizeof(buf), &_resources[0])) {
        case GCOAP_OBS_INIT_OK:
            len = _build_cbor_coap_packet(&pdu, buf, sizeof(buf));
            if(gcoap_obs_send(buf, len, &_resources[0]) == 0) {
                puts("Notification failed to send");
            }
            break;
        case GCOAP_OBS_INIT_ERR:
            puts("Notification of the observers failed");
            break;
        case GCOAP_OBS_INIT_UNUSED:
            break;
    }
}

/*
 * Server callback for /node/info. Accepts only GET.
 *
 * GET: Returns the puzzle status in CBOR
 */
static ssize_t _puzzle_info_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    /* since the init functions asserts all other members, 
     * its safe to just assert the base pointer */
    assert(_puzzle_info);

    /* initialize a new coap response */
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);

    return _build_cbor_coap_packet(pdu, buf, len);

}

/*
 * Server callback for /node/ready. Accepts only PUT.
 * Sets the puzzle into the ready state
 * PUT: Returns the puzzle status in CBOR
 */
static ssize_t _puzzle_ready_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    /* since the init functions asserts all other members, 
     * its safe to just assert the base pointer */
    assert(_puzzle_info);

    unsigned method = coap_method2flag(coap_get_code_detail(pdu));

    switch (method) {
        case COAP_PUT:
            _puzzle_info->set_ready_handler(false);
            break;
        default:
            /* we don't care about anything else */
            return gcoap_response(pdu, buf, len, COAP_CODE_BAD_REQUEST);
    }


    /* initialize a new coap response */
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CHANGED);

    return _build_cbor_coap_packet(pdu, buf, len);

}


/*
 * Server callback for /node/maintainance. Accepts only PUT.
 * Sets the puzzle into the ready state
 * PUT: Returns the puzzle status in CBOR
 */
static ssize_t _puzzle_maintainance_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    /* since the init functions asserts all other members, 
     * its safe to just assert the base pointer */
    assert(_puzzle_info);

    unsigned method = coap_method2flag(coap_get_code_detail(pdu));

    switch (method) {
        case COAP_PUT:
            _puzzle_info->set_ready_handler(true);
            break;
        default:
            /* we don't care about anything else */
            return gcoap_response(pdu, buf, len, COAP_CODE_BAD_REQUEST);
    }


    /* initialize a new coap response */
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CHANGED);

    return _build_cbor_coap_packet(pdu, buf, len);

}


/* Adds link format params to resource list */
static ssize_t _puzzle_encoder(const coap_resource_t *resource, char *buf,
                            size_t maxlen, coap_link_encoder_ctx_t *context) {
    ssize_t res = gcoap_encode_link(resource, buf, maxlen, context);

    if (res > 0) {
        if (strlen(PUZZLE_RESOURCE_TYPE) < (maxlen - res)){
            if (buf) {
                memcpy(buf+res, PUZZLE_RESOURCE_TYPE, strlen(PUZZLE_RESOURCE_TYPE));
            }
            res += strlen(PUZZLE_RESOURCE_TYPE);
        }
    }

    return res;
}


/*
 * Server callback for /node/serial. Accepts only GET.
 *
 * GET: Returns the puzzle serial
 */
static ssize_t _puzzle_serial_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    /* since the init functions asserts all other members, 
     * its safe to just assert the base pointer */
    assert(_puzzle_info);

    /* initialize a new coap response */
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);

    coap_opt_add_format(pdu, COAP_FORMAT_TEXT);

    /* finish the options sections */
    /* it is important to keep track of the amount of used bytes (resp_len) */
    size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);


    if (pdu->payload_len >= strlen(_puzzle_info->serial)){
        memcpy(pdu->payload, _puzzle_info->serial, strlen(_puzzle_info->serial));
        return resp_len + strlen(_puzzle_info->serial);
    } else {
        /* in this case we use a simple convenience function to create the
         * response, it only allows to set a payload and a response code. */
        puts("puzzle_coap: msg buffer too small for given puzzle cbor object");
        return gcoap_response(pdu, buf, len, COAP_CODE_INTERNAL_SERVER_ERROR);
    }
    return resp_len;
}
