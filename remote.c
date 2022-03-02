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
#include "gui_event_queue.h"


void openthread_event_handler(otChangedFlags event, void *aContext);
void joiner_callback(otError aError, void *aContext);

static volatile uint8_t is_commissioned = false;
static char             mac_str[18];
static otInstance*          sInstance = NULL;

static void device_set_mac_addr_str(char *str)
{
  uint8_t eui64[8];

  // get ieee eui
  otPlatRadioGetIeeeEui64(sInstance, (uint8_t *) &eui64);

  snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X", eui64[0], eui64[1], eui64[2], eui64[5], eui64[6], eui64[7]);
  str[17] = '\0';
}


void remote_init(otInstance *instance)
{
  otError error;
  gui_event_t gui_event = {
      .flag = 0,
      .msg  = {0},
  };

  // set openthread instance
  sInstance = instance;

  // get mac str
  device_set_mac_addr_str((char *) &mac_str);

  // test logging output and application alive state
  printf("Hello from the remote app_init\r\n");

  gui_event.flag = GUI_EVENT_FLAG_LOG;
  snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "hello :)");
  ring_buffer_add(&gui_event_queue, &gui_event);

  gui_event.flag = GUI_EVENT_FLAG_NTWK_ADDR;
  snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, (char *)&mac_str);
  ring_buffer_add(&gui_event_queue, &gui_event);

  // delete previous network information
  error = otInstanceErasePersistentInfo(sInstance);
  printf("erase persistent info: %s\r\n", otThreadErrorToString(error));

  // register callback for Thread Stack Events
  error = otSetStateChangedCallback(sInstance, openthread_event_handler, (void *)sInstance);
  printf("set state changed callback: %s\r\n", otThreadErrorToString(error));

  // start network interface
  error = otIp6SetEnabled(sInstance, true);
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
  gui_event_t gui_event = {
      .flag = 0,
      .msg  = {0},
  };

  if(event & OT_CHANGED_THREAD_NETIF_STATE)
  {
      bool netif_state = otIp6IsEnabled(aContext);
      printf("network if changed: %d\r\n", netif_state);
      if(netif_state)
      {
          printf("ready for join\r\n");

          gui_event.flag = GUI_EVENT_FLAG_LOG;
          snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "press 'B' to join");
          ring_buffer_add(&gui_event_queue, &gui_event);
      }
  }

  if(event & OT_CHANGED_THREAD_NETWORK_NAME)
  {
      printf("network name changed: %s\r\n", otThreadGetNetworkName(aContext));

      gui_event.flag = GUI_EVENT_FLAG_NTWK_NAME;
      snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, otThreadGetNetworkName(aContext));
      ring_buffer_add(&gui_event_queue, &gui_event);
  }

  if(event & OT_CHANGED_THREAD_ROLE)
  {
      otDeviceRole role = otThreadGetDeviceRole(aContext);
      printf("Thread Device Role Changed: %s\r\n", otThreadDeviceRoleToString(role));

      gui_event.flag = GUI_EVENT_FLAG_NTWK_ROLE;
      snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, otThreadDeviceRoleToString(role));
      ring_buffer_add(&gui_event_queue, &gui_event);

      if(role != OT_DEVICE_ROLE_DETACHED && role != OT_DEVICE_ROLE_DISABLED)
      {
          if(!is_commissioned)
          {
              printf("coap client init: %s\r\n", otThreadErrorToString(coap_client_init(aContext)));
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
      otError error = otDatasetGetActive(aContext, &otDataset);
      if(!error)
      {
//          gui_print_network_channel(otDataset.mChannel);

          gui_event.flag = GUI_EVENT_FLAG_NTWK_CH;
          snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "%d", otDataset.mChannel);
          ring_buffer_add(&gui_event_queue, &gui_event);
      }

  }
}

/**************************************************************************//**
 * Joiner Callback Handler
 *****************************************************************************/
void joiner_callback(otError aError, void *aContext)
{
  gui_event_t gui_event = {
      .flag = 0,
      .msg  = {0},
  };

  printf("joiner_callback event: %s\r\n", otThreadErrorToString(aError));

  if(aError == OT_ERROR_NONE)
  {
      // successful join, start the thread
      // > thread start
      otError error = otThreadSetEnabled(aContext, true);
      printf("thread start: %s\r\n", otThreadErrorToString(error));

      gui_event.flag = GUI_EVENT_FLAG_LOG;
      snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "[joiner] joined :)");
      ring_buffer_add(&gui_event_queue, &gui_event);

  }
  else
  {
      gui_event.flag = GUI_EVENT_FLAG_LOG;
      snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "[joiner] %s", otThreadErrorToString(aError));
      ring_buffer_add(&gui_event_queue, &gui_event);
  }
}

/**************************************************************************//**
 * Simple Button Callback Handler
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  char temp[21];
  gui_event_t gui_event = {
      .flag = 0,
      .msg  = {0},
  };

  if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)
  {
      if(!is_commissioned)
      {
          if(handle == &sl_button_btn0)
          {
              otError error;

              // start joiner
              error = otJoinerStart(sInstance, JOINER_PSKD, NULL, NULL, NULL, NULL, NULL, joiner_callback, (void*)sInstance);
              printf("start_joiner: %s\r\n", otThreadErrorToString(error));

              gui_event.flag = GUI_EVENT_FLAG_LOG;
              snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "[joiner] searching...");
              ring_buffer_add(&gui_event_queue, &gui_event);

          }
      }
      else
      {
          if(handle == &sl_button_btn0)
          {
              gui_event.flag = GUI_EVENT_FLAG_LOG;
              snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "[coap] tx 'B'");
              ring_buffer_add(&gui_event_queue, &gui_event);

              snprintf((char *) &temp, 21, "%s: %c", mac_str, 'B');

              // send a message with some identifiable component
              coap_client_send_message(sInstance, temp);
          }

          if(handle == &sl_button_btn1)
          {
              gui_event.flag = GUI_EVENT_FLAG_LOG;
              snprintf((char *)gui_event.msg, GUI_EVENT_MSG_SIZE, "[coap] tx 'A'");
              ring_buffer_add(&gui_event_queue, &gui_event);

              snprintf((char *) &temp, 21, "%s: %c", mac_str, 'A');

              // send a message with some identifiable component
              coap_client_send_message(sInstance, temp);
          }
      }
  }

  gui_button_handler(handle);
}
