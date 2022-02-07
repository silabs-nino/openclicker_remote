/***************************************************************************//**
 * @file
 * @brief Core Application Logic for the Remote Node
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

// OpenThread Includes
#include <openthread/dataset_ftd.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/misc.h>

#include <string.h>

// Platform Drivers
#include "sl_button.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"

// Utilities
#include "printf.h"

// Config
#include "remote_config.h"
#include "coap_client.h"


// declarations
otInstance *otGetInstance(void);

void openthread_event_handler(otChangedFlags event, void *aContext);
void joiner_callback(otError aError, void *aContext);

static volatile uint8_t is_commissioned = false;

void remote_init(void)
{
  otError error;

  // test logging output and application alive state
  printf("Hello from the remote app_init\r\n");

  // delete previous network information
  error = otInstanceErasePersistentInfo(otGetInstance());
  printf(otThreadErrorToString(error));
  printf("\r\n");

  // register callback for Thread Stack Events
  error = otSetStateChangedCallback(otGetInstance(), openthread_event_handler, (void *)otGetInstance());
  printf(otThreadErrorToString(error));
  printf("\r\n");

  // start network interface
  error = otIp6SetEnabled(otGetInstance(), true);
  printf(otThreadErrorToString(error));
  printf("\r\n");
}


/**************************************************************************//**
 * OpenThread Event Handler
 *
 * @param event - flags
 * @param aContext - openthread context
 *****************************************************************************/
void openthread_event_handler(otChangedFlags event, void *aContext)
{
  (void)aContext;

  if(event & OT_CHANGED_THREAD_NETIF_STATE)
  {
      bool netif_state = otIp6IsEnabled(otGetInstance());
      printf("network if changed: %d\r\n", netif_state);
      if(netif_state)
      {
          printf("ready for join\r\n");
      }
  }

  if(event & OT_CHANGED_THREAD_NETWORK_NAME)
  {
      printf("network name changed: %s\r\n", otThreadGetNetworkName(otGetInstance()));
  }

  if(event & OT_CHANGED_THREAD_ROLE)
  {
      otDeviceRole role = otThreadGetDeviceRole(otGetInstance());
      printf("Thread Device Role Changed: %s\r\n", otThreadDeviceRoleToString(role));
      if(role != OT_DEVICE_ROLE_DETACHED && role != OT_DEVICE_ROLE_DISABLED)
      {
          if(!is_commissioned)
          {
              printf("coap client init: %s\r\n", otThreadErrorToString(coap_client_init(otGetInstance())));
              is_commissioned = true;
          }
      }
      else {
          is_commissioned = false;
      }
  }
}

/**************************************************************************//**
 * Joiner Callback Handler
 *****************************************************************************/
void joiner_callback(otError aError, void *aContext)
{
  (void)aContext;

  printf("joiner_callback event: %s\r\n", otThreadErrorToString(aError));

  if(aError == OT_ERROR_NONE)
  {
      // successful join, start the thread
      // > thread start
      otError error = otThreadSetEnabled(otGetInstance(), true);
      printf("thread start: %s\r\n", otThreadErrorToString(error));
  }
}

/**************************************************************************//**
 * Simple Button Callback Handler
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if(!is_commissioned)
  {
      if(handle == &sl_button_btn0)
      {
          if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)
          {
              otError error;

              // start joiner
              error = otJoinerStart(otGetInstance(), JOINER_PSKD, NULL, NULL, NULL, NULL, NULL, joiner_callback, (void*)otGetInstance());
              printf("start_joiner: %s\r\n", otThreadErrorToString(error));

          }
      }
  }
  else {
      if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)
      {
          if(handle == &sl_button_btn0)
          {
              // send a message with some identifiable component
              coap_client_send_message(otGetInstance(), "right button");
          }

          if(handle == &sl_button_btn1)
          {
              // send a message with some identifiable component
              coap_client_send_message(otGetInstance(), "left button");
          }
      }
  }
}
