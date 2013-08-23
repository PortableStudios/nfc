/*-
 * Free/Libre Near Field Communication (NFC) library
 *
 * Libnfc historical contributors:
 * Copyright (C) 2009      Roel Verdult
 * Copyright (C) 2009-2013 Romuald Conty
 * Copyright (C) 2010-2012 Romain Tartière
 * Copyright (C) 2010-2013 Philippe Teuwen
 * Copyright (C) 2012-2013 Ludovic Rousseau
 * Additional contributors of this file:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1) Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  2 )Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Note that this license only applies on the examples, NFC library itself is under LGPL
 *
 */

/**
 * @file nfc-read-forum-tag3.c
 * @brief Extract NDEF Message from a NFC Forum Tag Type 3
 * This utility extract (if available) the NDEF Message contained in an NFC Forum Tag Type 3.
 */

/*
 * This implementation was written based on information provided by the
 * following documents:
 *
 * NFC Forum Type 3 Tag Operation Specification
 *  Technical Specification
 *  NFCForum-TS-Type-3-Tag_1.1 - 2011-06-28
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <nfc/nfc.h>

#include "nfc-utils.h"

#if defined(WIN32) && defined(__GNUC__) /* mingw compiler */
#include <getopt.h>
#endif

static nfc_device *pnd;
static nfc_context *context;

static void
print_usage(char *progname)
{
  fprintf(stderr, "usage: %s -o FILE\n", progname);
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, "  -o     Extract NDEF message if available in FILE\n");
}

static void stop_select(int sig)
{
  (void) sig;
  if (pnd != NULL) {
    nfc_abort_command(pnd);
  } else {
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }
}

static void
build_felica_frame(const nfc_felica_info nfi, const uint8_t command, const uint8_t *payload, const size_t payload_len, uint8_t *frame, size_t *frame_len)
{
  frame[0] = 1 + 1 + 8 + payload_len;
  *frame_len = frame[0];
  frame[1] = command;
  memcpy(frame + 2, nfi.abtId, 8);
  memcpy(frame + 10, payload, payload_len);
}

#define CHECK 		0x06
static int
nfc_forum_tag_type3_check(nfc_device *dev, const nfc_target nt, const uint16_t block, const uint8_t block_count, uint8_t *data, size_t *data_len)
{
  uint8_t payload[1024] = {
    1, // Services
    0x0B, 0x00, // NFC Forum Tag Type 3's Service code
    block_count,
    0x80, block, // block 0
  };

  size_t payload_len = 1 + 2 + 1;
  for (uint8_t b = 0; b < block_count; b++) {
    if (block < 0x100) {
      payload[payload_len++] = 0x80;
      payload[payload_len++] = block + b;
    } else {
      payload[payload_len++] = 0x00;
      payload[payload_len++] = (block + b) >> 8;
      payload[payload_len++] = (block + b) & 0xff;
    }
  }

  uint8_t frame[1024];
  size_t frame_len = sizeof(frame);
  build_felica_frame(nt.nti.nfi, CHECK, payload, payload_len, frame, &frame_len);

  uint8_t rx[1024];
  int res;
  if ((res = nfc_initiator_transceive_bytes(dev, frame, frame_len, rx, sizeof(rx), 0)) < 0) {
    return res;
  }
  const int res_overhead = 1 + 1 + 8 + 2;  // 1+1+8+2: LEN + CMD + NFCID2 + STATUS
  if (res < res_overhead) {
    // Not enough data
    return -1;
  }
  uint8_t felica_res_len = rx[0];
  if (res != felica_res_len) {
    // Error while receiving felica frame
    return -1;
  }
  if ((CHECK + 1) != rx[1]) {
    // Command return does not match
    return -1;
  }
  if (0 != memcmp(&rx[2], nt.nti.nfi.abtId, 8)) {
    // NFCID2 does not match
    return -1;
  }
  const uint8_t status_flag1 = rx[10];
  const uint8_t status_flag2 = rx[11];
  if ((status_flag1) || (status_flag2)) {
    // Felica card's error
    fprintf(stderr, "Status bytes: %02x, %02x\n", status_flag1, status_flag2);
    return -1;
  }
  // const uint8_t res_block_count = res[12];
  *data_len = res - res_overhead + 1; // +1 => block count is stored on 1 byte
  memcpy(data, &rx[res_overhead + 1], *data_len);
  return *data_len;
}

int
main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  int ch;
  char *ndef_output = NULL;
  while ((ch = getopt(argc, argv, "ho:")) != -1) {
    switch (ch) {
      case 'h':
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case 'o':
        ndef_output = optarg;
        break;
      case '?':
        if (optopt == 'o')
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      default:
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (ndef_output == NULL) {
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  FILE *message_stream = NULL;
  FILE *ndef_stream = NULL;

  if ((strlen(ndef_output) == 1) && (ndef_output[0] == '-')) {
    message_stream = stderr;
    ndef_stream = stdout;
  } else {
    message_stream = stdout;
    ndef_stream = fopen(ndef_output, "wb");
    if (!ndef_stream) {
      fprintf(stderr, "Could not open file %s.\n", ndef_output);
      exit(EXIT_FAILURE);
    }
  }

  nfc_init(&context);
  if (context == NULL) {
    ERR("Unable to init libnfc (malloc)\n");
    exit(EXIT_FAILURE);
  }

  pnd = nfc_open(context, NULL);

  if (pnd == NULL) {
    ERR("Unable to open NFC device");
    fclose(ndef_stream);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  fprintf(message_stream, "NFC device: %s opened\n", nfc_device_get_name(pnd));

  nfc_modulation nm = {
    .nmt = NMT_FELICA,
    .nbr = NBR_212,
  };

  signal(SIGINT, stop_select);

  nfc_target nt;

  if (nfc_initiator_init(pnd) < 0) {
    nfc_perror(pnd, "nfc_initiator_init");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }
  fprintf(message_stream, "Place your NFC Forum Tag Type 3 in the field...\n");

  // Polling payload (SENSF_REQ) must be present (see NFC Digital Protol)
  const uint8_t *pbtSensfReq = (uint8_t *)"\x00\xff\xff\x01\x00";
  if (nfc_initiator_select_passive_target(pnd, nm, pbtSensfReq, 5, &nt) <= 0) {
    nfc_perror(pnd, "nfc_initiator_select_passive_target");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  // Check if System Code equals 0x12fc
  const uint8_t abtNfcForumSysCode[] = { 0x12, 0xfc };
  if (0 != memcmp(nt.nti.nfi.abtSysCode, abtNfcForumSysCode, 2)) {
    // Retry with special polling
    const uint8_t *pbtSensfReqNfcForum = (uint8_t *)"\x00\x12\xfc\x01\x00";
    if (nfc_initiator_select_passive_target(pnd, nm, pbtSensfReqNfcForum, 5, &nt) <= 0) {
      nfc_perror(pnd, "nfc_initiator_select_passive_target");
      fclose(ndef_stream);
      nfc_close(pnd);
      nfc_exit(context);
      exit(EXIT_FAILURE);
    }
    // Check again if System Code equals 0x12fc
    if (0 != memcmp(nt.nti.nfi.abtSysCode, abtNfcForumSysCode, 2)) {
      fprintf(stderr, "Tag is not NFC Forum Tag Type 3 compliant.\n");
      fclose(ndef_stream);
      nfc_close(pnd);
      nfc_exit(context);
      exit(EXIT_FAILURE);
    }
  }

  //print_nfc_felica_info(nt.nti.nfi, true);

  if ((nfc_device_set_property_bool(pnd, NP_EASY_FRAMING, false) < 0) || (nfc_device_set_property_bool(pnd, NP_INFINITE_SELECT, false) < 0)) {
    nfc_perror(pnd, "nfc_device_set_property_bool");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  uint8_t data[1024];
  size_t data_len = sizeof(data);

  if (nfc_forum_tag_type3_check(pnd, nt, 0, 1, data, &data_len) <= 0) {
    nfc_perror(pnd, "nfc_forum_tag_type3_check");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  const int ndef_major_version = (data[0] & 0xf0) >> 4;
  const int ndef_minor_version = (data[0] & 0x0f);
  fprintf(message_stream, "NDEF Mapping version: %d.%d\n", ndef_major_version, ndef_minor_version);

  const int available_block_count = (data[3] << 8) + data[4];
  fprintf(message_stream, "NFC Forum Tag Type 3 capacity: %d bytes\n", available_block_count * 16);

  uint32_t ndef_data_len = (data[11] << 16) + (data[12] << 8) + data[13];
  fprintf(message_stream, "NDEF data length: %d bytes\n", ndef_data_len);

  uint16_t ndef_calculated_checksum = 0;
  for (size_t n = 0; n < 14; n++)
    ndef_calculated_checksum += data[n];

  const uint16_t ndef_checksum = (data[14] << 8) + data[15];
  if (ndef_calculated_checksum != ndef_checksum) {
    fprintf(stderr, "NDEF CRC does not match with calculated one\n");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  if (!ndef_data_len) {
    fprintf(stderr, "Empty NFC Forum Tag Type 3\n");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  const uint8_t block_max_per_check = data[1];
  const uint16_t block_count_to_check = (ndef_data_len / 16) + 1;

  data_len = 0;
  for (uint16_t b = 0; b < (block_count_to_check / block_max_per_check); b += block_max_per_check) {
    size_t size = sizeof(data) - data_len;
    if (!nfc_forum_tag_type3_check(pnd, nt, 1 + b, MIN(block_max_per_check, (block_count_to_check - (b * block_max_per_check))), data + data_len, &size)) {
      nfc_perror(pnd, "nfc_forum_tag_type3_check");
      fclose(ndef_stream);
      nfc_close(pnd);
      nfc_exit(context);
      exit(EXIT_FAILURE);
    }
    data_len += size;
  }
  if (fwrite(data, 1, data_len, ndef_stream) != data_len) {
    fprintf(stderr, "Could not write to file.\n");
    fclose(ndef_stream);
    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_FAILURE);
  }

  fclose(ndef_stream);
  nfc_close(pnd);
  nfc_exit(context);
  exit(EXIT_SUCCESS);
}
