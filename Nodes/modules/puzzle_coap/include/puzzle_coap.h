/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef EXTERNAL_PUZZLE_COAP_H
#define EXTERNAL_PUZZLE_COAP_H

#include <stdio.h>

#include "uri_parser.h"
#include "net/gcoap.h"
#include "net/cord/common.h"
#include "net/cord/ep.h"
#include "net/sock/util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PUZZLE_RESOURCE_TYPE ";rt=\"puzzle\";obs"

/* 119 is the max size without touching the gcoap.h */
/* the default CONFIG_GCOAP_PDU_BUF_SIZE is 128 */
#define CBOR_BUF_SIZE 100
static const uint8_t psk_id[] = PSK_ID;
static const uint8_t psk_key[] = PSK;

/**
 * Puzzle info structure 
 */
typedef struct {
    bool (*get_solved_handler)(void);   /**< bool handler thats get called to see if a puzzle is solved */
    bool (*get_ready_handler)(void);    /**< bool handler thats get called to see if a puzzle is ready or in maintainance */
    void (*set_ready_handler)(bool maintainance);/**< void handler thats get called to set a puzzle in ready or maintainance mode*/
    const char *resource_dir_uri;   /**< char* URI to a CoRE RD */
    const char *name;               /**< char* name of the puzzle */
    const char *serial;             /**< char* serial number of the puzzle */
} puzzle_t;

void puzzle_init(const puzzle_t *puzzle);
void puzzle_update(void);

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* EXTERNAL_PUZZLE_COAP_H */
