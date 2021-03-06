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
 * @file nfc-list.c
 * @brief Lists the first target present of each founded device
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#include <err.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <nfc/nfc.h>

#include "nfc-utils.h"

#define MAX_DEVICE_COUNT 16
#define MAX_TARGET_COUNT 16

static nfc_device *pnd;

static void
print_usage(const char *progname)
{
  printf("usage: %s [-v]\n", progname);
  printf("  -v\t verbose display\n");
}

int
main(int argc, const char *argv[])
{
  (void) argc;
  const char *acLibnfcVersion;
  size_t  i;
  bool verbose = false;
  int res = 0;

  nfc_context *context;
  nfc_init(&context);
  if (context == NULL) {
    ERR("Unable to init libnfc (malloc)");
    exit(EXIT_FAILURE);
  }

  // Display libnfc version
  acLibnfcVersion = nfc_version();
  printf("%s uses libnfc %s\n", argv[0], acLibnfcVersion);
  if (argc != 1) {
    if ((argc == 2) && (0 == strcmp("-v", argv[1]))) {
      verbose = true;
    } else {
      print_usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  /* Lazy way to open an NFC device */
#if 0
  pnd = nfc_open(context, NULL);
#endif

  /* If specific device is wanted, i.e. an ARYGON device on /dev/ttyUSB0 */
#if 0
  nfc_device_desc_t ndd;
  ndd.pcDriver = "ARYGON";
  ndd.pcPort = "/dev/ttyUSB0";
  ndd.uiSpeed = 115200;
  pnd = nfc_open(context, &ndd);
#endif

  /* If specific device is wanted, i.e. a SCL3711 on USB */
#if 0
  nfc_device_desc_t ndd;
  ndd.pcDriver = "PN533_USB";
  strcpy(ndd.acDevice, "SCM Micro / SCL3711-NFC&RW");
  pnd = nfc_open(context, &ndd);
#endif
  nfc_connstring connstrings[MAX_DEVICE_COUNT];
  size_t szDeviceFound = nfc_list_devices(context, connstrings, MAX_DEVICE_COUNT);

  if (szDeviceFound == 0) {
    printf("No NFC device found.\n");
  }

  for (i = 0; i < szDeviceFound; i++) {
    nfc_target ant[MAX_TARGET_COUNT];
    pnd = nfc_open(context, connstrings[i]);

    if (pnd == NULL) {
      ERR("Unable to open NFC device: %s", connstrings[i]);
      continue;
    }
    if (nfc_initiator_init(pnd) < 0) {
      nfc_perror(pnd, "nfc_initiator_init");
      nfc_exit(context);
      exit(EXIT_FAILURE);
    }

    printf("NFC device: %s opened\n", nfc_device_get_name(pnd));

    nfc_modulation nm;

    nm.nmt = NMT_ISO14443A;
    nm.nbr = NBR_106;
    // List ISO14443A targets
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d ISO14443A passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nmt = NMT_FELICA;
    nm.nbr = NBR_212;
    // List Felica tags
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d Felica (212 kbps) passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nbr = NBR_424;
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d Felica (424 kbps) passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nmt = NMT_ISO14443B;
    nm.nbr = NBR_106;
    // List ISO14443B targets
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d ISO14443B passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nmt = NMT_ISO14443BI;
    nm.nbr = NBR_106;
    // List ISO14443B' targets
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d ISO14443B' passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nmt = NMT_ISO14443B2SR;
    nm.nbr = NBR_106;
    // List ISO14443B-2 ST SRx family targets
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d ISO14443B-2 ST SRx passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nmt = NMT_ISO14443B2CT;
    nm.nbr = NBR_106;
    // List ISO14443B-2 ASK CTx family targets
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d ISO14443B-2 ASK CTx passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }

    nm.nmt = NMT_JEWEL;
    nm.nbr = NBR_106;
    // List Jewel targets
    if ((res = nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT)) >= 0) {
      int n;
      if (verbose || (res > 0)) {
        printf("%d Jewel passive target(s) found%s\n", res, (res == 0) ? ".\n" : ":");
      }
      for (n = 0; n < res; n++) {
        print_nfc_target(&ant[n], verbose);
        printf("\n");
      }
    }
    nfc_close(pnd);
  }

  nfc_exit(context);
  exit(EXIT_SUCCESS);
}
