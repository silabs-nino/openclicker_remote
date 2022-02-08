/***************************************************************************//**
 * @file
 * @brief Coap Client Logic for the Remote Node
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/

#include <openthread/coap.h>
#include <openthread/thread.h>

#include <string.h>

#include "printf.h"

static uint8_t  coap_enabled  = false;
static char*    uri_path      = "question/answer";

static void coap_client_handler(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo, otError aResult);

otError coap_client_init(otInstance *aInstance)
{
  otError error = OT_ERROR_NONE;

  // start coap
  error = otCoapStart(aInstance, OT_DEFAULT_COAP_PORT);
  printf("coap client start: %s\r\n", otThreadErrorToString(error));

  if(!error)
  {
      coap_enabled = true;
  }


  return error;
}

otError coap_client_send_message(otInstance *aInstance, char* message)
{
  otError         error             = OT_ERROR_NONE;

  otCoapType      message_type      = OT_COAP_TYPE_CONFIRMABLE;

  otMessage       *request_message  = NULL;
  otMessageInfo   message_info;

  otIp6Address    dest_addr;

  // verify coap has been enabled
  if(!coap_enabled)
  {
      error = OT_ERROR_INVALID_STATE;
      printf("coap not enabled\r\n");
      goto exit;
  }

  error = otThreadGetLeaderRloc(aInstance, &dest_addr);
  if(error)
  {
      printf("get leader rloc: %s\r\n", otThreadErrorToString(error));
      goto exit;
  }

  request_message = otCoapNewMessage(aInstance, NULL);
  if(request_message == NULL)
  {
      error = OT_ERROR_NO_BUFS;
      printf("coap request init message: %s\r\n", otThreadErrorToString(error));
      goto exit;
  }

  otCoapMessageInit(request_message, message_type, OT_COAP_CODE_POST);
  otCoapMessageGenerateToken(request_message, OT_COAP_DEFAULT_TOKEN_LENGTH);

  // set URI path for requested resource
  error = otCoapMessageAppendUriPathOptions(request_message, uri_path);
  if(error)
  {
      printf("coap request append uri-path: %s\r\n", otThreadErrorToString(error));
      goto exit;
  }

  // set payload marker to indicate beginning of payload in coap message
  error = otCoapMessageSetPayloadMarker(request_message);
  if(error)
  {
      printf("set payload marker: %s\r\n", otThreadErrorToString(error));
      goto exit;
  }

  printf("message to append: %s, len: %d\r\n", message, strlen(message));

  // add message
  error = otMessageAppend(request_message, message, strlen(message));
  if(error)
  {
      printf("append payload to message: %s\r\n", otThreadErrorToString(error));
      goto exit;
  }

  // set destination address and udp port
  memset(&message_info, 0, sizeof(message_info));
  message_info.mPeerAddr = dest_addr;
  message_info.mPeerPort = OT_DEFAULT_COAP_PORT;

  char dest_addr_str[OT_IP6_ADDRESS_STRING_SIZE];
  otIp6AddressToString((otIp6Address *) &dest_addr, (char *) &dest_addr_str, OT_IP6_ADDRESS_STRING_SIZE);
  printf("sending '%s' to %s\r\n", message, dest_addr_str);

  // send coap request
  error = otCoapSendRequestWithParameters(aInstance, request_message, &message_info, &coap_client_handler, aInstance, NULL);
  if(error)
  {
      printf("send coap request: %s\r\n", otThreadErrorToString(error));
      goto exit;
  }

exit:
  if(error != OT_ERROR_NONE && request_message != NULL)
  {
      otMessageFree(request_message);
  }

  return error;
}


static void coap_client_handler(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo, otError aResult)
{
  (void)aContext;
  (void)aMessage;
  (void)aMessageInfo;
  (void)aResult;
  printf("coap client handler\r\n");
}
