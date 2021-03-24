/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
 ******************************************************************************/
#include "rail.h"
#include "rail_ble.h" // for RAIL_BLE_State_t
#include "sli_rail_util_callbacks.h" // for internal-only callback signatures
#include "sl_rail_util_init.h"
#include "sl_rail_util_protocol.h"
#include "sl_flex_assert.h"

#if 0U \
  || SL_RAIL_UTIL_INIT_RADIO_CONFIG_SUPPORT_INST0_ENABLE \
  || 0U
  #include "rail_config.h"
#endif

// Instance: inst0
static RAIL_Handle_t sl_rail_handle_inst0 = RAIL_EFR32_HANDLE;
#if SL_RAIL_UTIL_INIT_BLE_SUPPORT_INST0_ENABLE
static RAIL_BLE_State_t sl_rail_ble_state_inst0; // needed for BLE (ignored by others)
#endif // SL_RAIL_UTIL_INIT_BLE_SUPPORT_INST0_ENABLE
#if SL_RAIL_UTIL_INIT_SCHEDULER_SUPPORT_INST0_ENABLE
static RAILSched_Config_t sl_rail_sched_config_inst0;
#endif // SL_RAIL_UTIL_INIT_SCHEDULER_SUPPORT_INST0_ENABLE
static RAIL_Config_t sl_rail_config_inst0 = {
  .eventsCallback = &sli_rail_util_on_event,
#if SL_RAIL_UTIL_INIT_BLE_SUPPORT_INST0_ENABLE
  .protocol = &sl_rail_ble_state_inst0,
#endif // SL_RAIL_UTIL_INIT_BLE_SUPPORT_INST0_ENABLE
#if SL_RAIL_UTIL_INIT_SCHEDULER_SUPPORT_INST0_ENABLE
  .scheduler = &sl_rail_sched_config_inst0,
#endif // SL_RAIL_UTIL_INIT_SCHEDULER_SUPPORT_INST0_ENABLE
};

static void sl_rail_util_init_inst0(void)
{
#if SL_RAIL_UTIL_INIT_INST0_ENABLE
  RAIL_Status_t status;
  sl_rail_handle_inst0 = RAIL_Init(&sl_rail_config_inst0,
#if SL_RAIL_UTIL_INIT_INIT_COMPLETE_CALLBACK_INST0_ENABLE
                             &sli_rail_util_on_rf_ready
#else
                             NULL
#endif // SL_RAIL_UTIL_INIT_INIT_COMPLETE_CALLBACK_INST0_ENABLE
                             );
  APP_ASSERT((NULL != sl_rail_handle_inst0),
             "RAIL_Init failed, return value: NULL");

#if SL_RAIL_UTIL_INIT_DATA_FORMATS_INST0_ENABLE
  RAIL_DataConfig_t data_config = {
    .txSource = SL_RAIL_UTIL_INIT_DATA_FORMAT_INST0_TX_SOURCE,
    .rxSource = SL_RAIL_UTIL_INIT_DATA_FORMAT_INST0_RX_SOURCE,
    .txMethod = SL_RAIL_UTIL_INIT_DATA_FORMAT_INST0_TX_MODE,
    .rxMethod = SL_RAIL_UTIL_INIT_DATA_FORMAT_INST0_RX_MODE,
  };
  status = RAIL_ConfigData(sl_rail_handle_inst0, &data_config);
  APP_ASSERT((RAIL_STATUS_NO_ERROR == status),
             "RAIL_ConfigData failed, return value: %d",
             status);
#endif // SL_RAIL_UTIL_INIT_DATA_FORMATS_INST0_ENABLE

#if SL_RAIL_UTIL_INIT_PROTOCOLS_INST0_ENABLE
  const RAIL_ChannelConfig_t *channel_config = NULL;
  if (SL_RAIL_UTIL_INIT_PROTOCOL_INST0_DEFAULT
      == SL_RAIL_UTIL_PROTOCOL_PROPRIETARY) {
#if SL_RAIL_UTIL_INIT_RADIO_CONFIG_SUPPORT_INST0_ENABLE
    channel_config = channelConfigs[SL_RAIL_UTIL_INIT_PROTOCOL_PROPRIETARY_INST0_INDEX];
#else // !SL_RAIL_UTIL_INIT_RADIO_CONFIG_SUPPORT_INST0_ENABLE
    APP_ASSERT(false,
               "SL_RAIL_UTIL_INIT_RADIO_CONFIG_SUPPORT_INST0_ENABLE must be true when (SL_RAIL_UTIL_INIT_PROTOCOL_INST0_DEFAULT == SL_RAIL_UTIL_PROTOCOL_PROPRIETARY)");
#endif // SL_RAIL_UTIL_INIT_RADIO_CONFIG_SUPPORT_INST0_ENABLE
  }
  (void) RAIL_ConfigChannels(sl_rail_handle_inst0,
                             channel_config,
                             &sli_rail_util_on_channel_config_change);
  status = sl_rail_util_protocol_config(sl_rail_handle_inst0,
                                        SL_RAIL_UTIL_INIT_PROTOCOL_INST0_DEFAULT);
  APP_ASSERT((RAIL_STATUS_NO_ERROR == status),
             "sl_rail_util_protocol_config failed, return value: %d",
             status);
#endif // SL_RAIL_UTIL_INIT_PROTOCOLS_INST0_ENABLE

#if SL_RAIL_UTIL_INIT_CALIBRATIONS_INST0_ENABLE
  status = RAIL_ConfigCal(sl_rail_handle_inst0,
                          0U
                          | (SL_RAIL_UTIL_INIT_CALIBRATION_TEMPERATURE_NOTIFY_INST0_ENABLE
                             ? RAIL_CAL_TEMP : 0U)
                          | (SL_RAIL_UTIL_INIT_CALIBRATION_ONETIME_NOTIFY_INST0_ENABLE
                             ? RAIL_CAL_ONETIME : 0U));
  APP_ASSERT((RAIL_STATUS_NO_ERROR == status),
             "RAIL_ConfigCal failed, return value: %d",
             status);
#endif // SL_RAIL_UTIL_INIT_CALIBRATIONS_INST0_ENABLE

#if SL_RAIL_UTIL_INIT_EVENTS_INST0_ENABLE
  status = RAIL_ConfigEvents(sl_rail_handle_inst0,
                             RAIL_EVENTS_ALL,
                             SL_RAIL_UTIL_INIT_EVENT_INST0_MASK);
  APP_ASSERT((RAIL_STATUS_NO_ERROR == status),
             "RAIL_ConfigEvents failed, return value: %d",
             status);
#endif // SL_RAIL_UTIL_INIT_EVENTS_INST0_ENABLE

#if SL_RAIL_UTIL_INIT_TRANSITIONS_INST0_ENABLE
  RAIL_StateTransitions_t tx_transitions = {
    .success = SL_RAIL_UTIL_INIT_TRANSITION_INST0_TX_SUCCESS,
    .error = SL_RAIL_UTIL_INIT_TRANSITION_INST0_TX_ERROR
  };
  RAIL_StateTransitions_t rx_transitions = {
    .success = SL_RAIL_UTIL_INIT_TRANSITION_INST0_RX_SUCCESS,
    .error = SL_RAIL_UTIL_INIT_TRANSITION_INST0_RX_ERROR
  };
  status = RAIL_SetTxTransitions(sl_rail_handle_inst0,
                                 &tx_transitions);
  APP_ASSERT((RAIL_STATUS_NO_ERROR == status),
             "RAIL_SetTxTransitions failed, return value: %d",
             status);
  status = RAIL_SetRxTransitions(sl_rail_handle_inst0,
                                 &rx_transitions);
  APP_ASSERT((RAIL_STATUS_NO_ERROR == status),
             "RAIL_SetRxTransitions failed, return value: %d",
             status);
#endif // SL_RAIL_UTIL_INIT_TRANSITIONS_INST0_ENABLE
#else // !SL_RAIL_UTIL_INIT_INST0_ENABLE
  // Eliminate compiler warnings.
  (void) sl_rail_handle_inst0;
  (void) sl_rail_config_inst0;
#endif // SL_RAIL_UTIL_INIT_INST0_ENABLE
}

RAIL_Handle_t sl_rail_util_get_handle(sl_rail_util_handle_type_t handle)
{
  RAIL_Handle_t *sl_rail_handle_array[] = {
    &sl_rail_handle_inst0,
  };
  return *sl_rail_handle_array[handle];
}

void sl_rail_util_init(void)
{
  sl_rail_util_init_inst0();
}

#if !SL_RAIL_LIB_MULTIPROTOCOL_SUPPORT && ((0 + 1) > 1)
  #error "sl_rail_util_init.c: If you are going to use more than one rail_util_init instance, you must use the Multiprotocol RAIL library."
#endif
