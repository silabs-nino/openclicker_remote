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
#include <stdlib.h>

// Platform Drivers
#include "sl_button.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"

// Utilities
#include "printf.h"

// Config
#include "remote_config.h"
#include "coap_client.h"

#include "gui.h"


// declarations
otInstance *otGetInstance(void);

void openthread_event_handler(otChangedFlags event, void *aContext);
void joiner_callback(otError aError, void *aContext);

static volatile uint8_t is_commissioned = false;
static char             mac_str[18];

static void device_set_mac_addr_str(char *str)
{
  uint8_t eui64[8];

  // get ieee eui
  otPlatRadioGetIeeeEui64(otGetInstance(), (uint8_t *) &eui64);

  snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X", eui64[0], eui64[1], eui64[2], eui64[5], eui64[6], eui64[7]);
  str[17] = '\0';
}


void remote_init(void)
{
  otError error;

  // get mac str
  device_set_mac_addr_str((char *) &mac_str);

  // test logging output and application alive state
  printf("Hello from the remote app_init\r\n");
  gui_print_log("hello :)");
  gui_print_mac_addr((char *) &mac_str);

  // delete previous network information
  error = otInstanceErasePersistentInfo(otGetInstance());
  printf("erase persistent info: %s\r\n", otThreadErrorToString(error));

  // register callback for Thread Stack Events
  error = otSetStateChangedCallback(otGetInstance(), openthread_event_handler, (void *)otGetInstance());
  printf("set state changed callback: %s\r\n", otThreadErrorToString(error));

  // start network interface
  error = otIp6SetEnabled(otGetInstance(), true);
  printf("enable interface: %s\r\n", otThreadErrorToString(error));
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
          gui_print_log("press 'B' to join");
      }
  }

  if(event & OT_CHANGED_THREAD_NETWORK_NAME)
  {
      printf("network name changed: %s\r\n", otThreadGetNetworkName(otGetInstance()));
      gui_print_network_name(otThreadGetNetworkName(otGetInstance()));
  }

  if(event & OT_CHANGED_THREAD_ROLE)
  {
      otDeviceRole role = otThreadGetDeviceRole(otGetInstance());
      printf("Thread Device Role Changed: %s\r\n", otThreadDeviceRoleToString(role));
      gui_print_device_role(otThreadDeviceRoleToString(role));
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

  if((event & OT_CHANGED_THREAD_CHANNEL) || (event & OT_CHANGED_THREAD_NETDATA))
  {
      otOperationalDataset otDataset;
      otError error = otDatasetGetActive(otGetInstance(), &otDataset);
      if(!error)
      {
          gui_print_network_channel(otDataset.mChannel);
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
      gui_print_log("[joiner] joined :)");
  }
  else
  {
      char temp[21];
      snprintf((char *)&temp, 21, "[joiner] %s", otThreadErrorToString(aError));
      gui_print_log(temp);
  }
}

/**************************************************************************//**
 * Simple Button Callback Handler
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  char temp[21];

  if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)
  {
      if(!is_commissioned)
      {
          if(handle == &sl_button_btn0)
          {
              otError error;

              // start joiner
              error = otJoinerStart(otGetInstance(), JOINER_PSKD, NULL, NULL, NULL, NULL, NULL, joiner_callback, (void*)otGetInstance());
              printf("start_joiner: %s\r\n", otThreadErrorToString(error));
              gui_print_log("[joiner] searching...");

          }
      }
      else
      {
          if(handle == &sl_button_btn0)
          {
              gui_print_log("[coap] tx 'B'");

              snprintf((char *) &temp, 21, "%s: %c", mac_str, 'B');

              // send a message with some identifiable component
              coap_client_send_message(otGetInstance(), temp);
          }

          if(handle == &sl_button_btn1)
          {
              gui_print_log("[coap] tx 'A'");

              snprintf((char *) &temp, 21, "%s: %c", mac_str, 'A');

              // send a message with some identifiable component
              coap_client_send_message(otGetInstance(), temp);
          }
      }
  }

  gui_button_handler(handle);
}
