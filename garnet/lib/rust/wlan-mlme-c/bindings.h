// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_RUST_WLAN_MLME_C_BINDINGS_H_
#define GARNET_LIB_RUST_WLAN_MLME_C_BINDINGS_H_

// Warning:
// This file was autogenerated by cbindgen.
// Do not modify this file manually.

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * IEEE Std 802.11-2016, 9.4.1.7
 */
typedef enum {
  WLAN_REASON_CODE_UNSPECIFIED_REASON = 1,
  WLAN_REASON_CODE_INVALID_AUTHENTICATION = 2,
  WLAN_REASON_CODE_LEAVING_NETWORK_DEAUTH = 3,
  WLAN_REASON_CODE_REASON_INACTIVITY = 4,
  WLAN_REASON_CODE_NO_MORE_STAS = 5,
  WLAN_REASON_CODE_INVALID_CLASS2_FRAME = 6,
  WLAN_REASON_CODE_INVALID_CLASS3_FRAME = 7,
  WLAN_REASON_CODE_LEAVING_NETWORK_DISASSOC = 8,
  WLAN_REASON_CODE_NOT_AUTHENTICATED = 9,
  WLAN_REASON_CODE_UNACCEPTABLE_POWER_CAPABILITY = 10,
  WLAN_REASON_CODE_UNACCEPTABLE_SUPPORTED_CHANNELS = 11,
  WLAN_REASON_CODE_BSS_TRANSITION_DISASSOC = 12,
  WLAN_REASON_CODE_REASON_INVALID_ELEMENT = 13,
  WLAN_REASON_CODE_MIC_FAILURE = 14,
  WLAN_REASON_CODE_FOURWAY_HANDSHAKE_TIMEOUT = 15,
  WLAN_REASON_CODE_GK_HANDSHAKE_TIMEOUT = 16,
  WLAN_REASON_CODE_HANDSHAKE_ELEMENT_MISMATCH = 17,
  WLAN_REASON_CODE_REASON_INVALID_GROUP_CIPHER = 18,
  WLAN_REASON_CODE_REASON_INVALID_PAIRWISE_CIPHER = 19,
  WLAN_REASON_CODE_REASON_INVALID_AKMP = 20,
  WLAN_REASON_CODE_UNSUPPORTED_RSNE_VERSION = 21,
  WLAN_REASON_CODE_INVALID_RSNE_CAPABILITIES = 22,
  WLAN_REASON_CODE_IEEE8021_XAUTH_FAILED = 23,
  WLAN_REASON_CODE_REASON_CIPHER_OUT_OF_POLICY = 24,
  WLAN_REASON_CODE_TDLS_PEER_UNREACHABLE = 25,
  WLAN_REASON_CODE_TDLS_UNSPECIFIED_REASON = 26,
  WLAN_REASON_CODE_SSP_REQUESTED_DISASSOC = 27,
  WLAN_REASON_CODE_NO_SSP_ROAMING_AGREEMENT = 28,
  WLAN_REASON_CODE_BAD_CIPHER_OR_AKM = 29,
  WLAN_REASON_CODE_NOT_AUTHORIZED_THIS_LOCATION = 30,
  WLAN_REASON_CODE_SERVICE_CHANGE_PRECLUDES_TS = 31,
  WLAN_REASON_CODE_UNSPECIFIED_QOS_REASON = 32,
  WLAN_REASON_CODE_NOT_ENOUGH_BANDWIDTH = 33,
  WLAN_REASON_CODE_MISSING_ACKS = 34,
  WLAN_REASON_CODE_EXCEEDED_TXOP = 35,
  WLAN_REASON_CODE_STA_LEAVING = 36,
  WLAN_REASON_CODE_END_TS_BA_DLS = 37,
  WLAN_REASON_CODE_UNKNOWN_TS_BA = 38,
  WLAN_REASON_CODE_TIMEOUT = 39,
  WLAN_REASON_CODE_PEERKEY_MISMATCH = 45,
  WLAN_REASON_CODE_PEER_INITIATED = 46,
  WLAN_REASON_CODE_AP_INITIATED = 47,
  WLAN_REASON_CODE_REASON_INVALID_FT_ACTION_FRAME_COUNT = 48,
  WLAN_REASON_CODE_REASON_INVALID_PMKID = 49,
  WLAN_REASON_CODE_REASON_INVALID_MDE = 50,
  WLAN_REASON_CODE_REASON_INVALID_FTE = 51,
  WLAN_REASON_CODE_MESH_PEERING_CANCELED = 52,
  WLAN_REASON_CODE_MESH_MAX_PEERS = 53,
  WLAN_REASON_CODE_MESH_CONFIGURATION_POLICY_VIOLATION = 54,
  WLAN_REASON_CODE_MESH_CLOSE_RCVD = 55,
  WLAN_REASON_CODE_MESH_MAX_RETRIES = 56,
  WLAN_REASON_CODE_MESH_CONFIRM_TIMEOUT = 57,
  WLAN_REASON_CODE_MESH_INVALID_GTK = 58,
  WLAN_REASON_CODE_MESH_INCONSISTENT_PARAMETERS = 59,
  WLAN_REASON_CODE_MESH_INVALID_SECURITY_CAPABILITY = 60,
  WLAN_REASON_CODE_MESH_PATH_ERROR_NO_PROXY_INFORMATION = 61,
  WLAN_REASON_CODE_MESH_PATH_ERROR_NO_FORWARDING_INFORMATION = 62,
  WLAN_REASON_CODE_MESH_PATH_ERROR_DESTINATION_UNREACHABLE = 63,
  WLAN_REASON_CODE_MAC_ADDRESS_ALREADY_EXISTS_IN_MBSS = 64,
  WLAN_REASON_CODE_MESH_CHANNEL_SWITCH_REGULATORY_REQUIREMENTS = 65,
  WLAN_REASON_CODE_MESH_CHANNEL_SWITCH_UNSPECIFIED = 66,
} wlan_reason_code;

/**
 * IEEE Std 802.11-2016, 9.4.1.9, Table 9-46
 */
typedef enum {
  WLAN_STATUS_CODE_SUCCESS = 0,
  WLAN_STATUS_CODE_REFUSED = 1,
  WLAN_STATUS_CODE_TDLS_REJECTED_ALTERNATIVE_PROVIDED = 2,
  WLAN_STATUS_CODE_TDLS_REJECTED = 3,
  WLAN_STATUS_CODE_SECURITY_DISABLED = 5,
  WLAN_STATUS_CODE_UNACCEPTABLE_LIFETIME = 6,
  WLAN_STATUS_CODE_NOT_IN_SAME_BSS = 7,
  WLAN_STATUS_CODE_REFUSED_CAPABILITIES_MISMATCH = 10,
  WLAN_STATUS_CODE_DENIED_NO_ASSOCIATION_EXISTS = 11,
  WLAN_STATUS_CODE_DENIED_OTHER_REASON = 12,
  WLAN_STATUS_CODE_UNSUPPORTED_AUTH_ALGORITHM = 13,
  WLAN_STATUS_CODE_TRANSACTION_SEQUENCE_ERROR = 14,
  WLAN_STATUS_CODE_CHALLENGE_FAILURE = 15,
  WLAN_STATUS_CODE_REJECTED_SEQUENCE_TIMEOUT = 16,
  WLAN_STATUS_CODE_DENIED_NO_MORE_STAS = 17,
  WLAN_STATUS_CODE_REFUSED_BASIC_RATES_MISMATCH = 18,
  WLAN_STATUS_CODE_DENIED_NO_SHORT_PREAMBLE_SUPPORT = 19,
  WLAN_STATUS_CODE_REJECTED_SPECTRUM_MANAGEMENT_REQUIRED = 22,
  WLAN_STATUS_CODE_REJECTED_BAD_POWER_CAPABILITY = 23,
  WLAN_STATUS_CODE_REJECTED_BAD_SUPPORTED_CHANNELS = 24,
  WLAN_STATUS_CODE_DENIED_NO_SHORT_SLOT_TIME_SUPPORT = 25,
  WLAN_STATUS_CODE_DENIED_NO_HT_SUPPORT = 27,
  WLAN_STATUS_CODE_R0KH_UNREACHABLE = 28,
  WLAN_STATUS_CODE_DENIED_PCO_TIME_NOT_SUPPORTED = 29,
  WLAN_STATUS_CODE_REFUSED_TEMPORARILY = 30,
  WLAN_STATUS_CODE_ROBUST_MANAGEMENT_POLICY_VIOLATION = 31,
  WLAN_STATUS_CODE_UNSPECIFIED_QOS_FAILURE = 32,
  WLAN_STATUS_CODE_DENIED_INSUFFICIENT_BANDWIDTH = 33,
  WLAN_STATUS_CODE_DENIED_POOR_CHANNEL_CONDITIONS = 34,
  WLAN_STATUS_CODE_DENIED_QOS_NOT_SUPPORTED = 35,
  WLAN_STATUS_CODE_REQUEST_DECLINED = 37,
  WLAN_STATUS_CODE_INVALID_PARAMETERS = 38,
  WLAN_STATUS_CODE_REJECTED_WITH_SUGGESTED_CHANGES = 39,
  WLAN_STATUS_CODE_STATUS_INVALID_ELEMENT = 40,
  WLAN_STATUS_CODE_STATUS_INVALID_GROUP_CIPHER = 41,
  WLAN_STATUS_CODE_STATUS_INVALID_PAIRWISE_CIPHER = 42,
  WLAN_STATUS_CODE_STATUS_INVALID_AKMP = 43,
  WLAN_STATUS_CODE_UNSUPPORTED_RSNE_VERSION = 44,
  WLAN_STATUS_CODE_INVALID_RSNE_CAPABILITIES = 45,
  WLAN_STATUS_CODE_STATUS_CIPHER_OUT_OF_POLICY = 46,
  WLAN_STATUS_CODE_REJECTED_FOR_DELAY_PERIOD = 47,
  WLAN_STATUS_CODE_DLS_NOT_ALLOWED = 48,
  WLAN_STATUS_CODE_NOT_PRESENT = 49,
  WLAN_STATUS_CODE_NOT_QOS_STA = 50,
  WLAN_STATUS_CODE_DENIED_LISTEN_INTERVAL_TOO_LARGE = 51,
  WLAN_STATUS_CODE_STATUS_INVALID_FT_ACTION_FRAME_COUNT = 52,
  WLAN_STATUS_CODE_STATUS_INVALID_PMKID = 53,
  WLAN_STATUS_CODE_STATUS_INVALID_MDE = 54,
  WLAN_STATUS_CODE_STATUS_INVALID_FTE = 55,
  WLAN_STATUS_CODE_REQUESTED_TCLAS_NOT_SUPPORTED56 = 56,
  WLAN_STATUS_CODE_INSUFFICIENT_TCLAS_PROCESSING_RESOURCES = 57,
  WLAN_STATUS_CODE_TRY_ANOTHER_BSS = 58,
  WLAN_STATUS_CODE_GAS_ADVERTISEMENT_PROTOCOL_NOT_SUPPORTED = 59,
  WLAN_STATUS_CODE_NO_OUTSTANDING_GAS_REQUEST = 60,
  WLAN_STATUS_CODE_GAS_RESPONSE_NOT_RECEIVED_FROM_SERVER = 61,
  WLAN_STATUS_CODE_GAS_QUERY_TIMEOUT = 62,
  WLAN_STATUS_CODE_GAS_QUERY_RESPONSE_TOO_LARGE = 63,
  WLAN_STATUS_CODE_REJECTED_HOME_WITH_SUGGESTED_CHANGES = 64,
  WLAN_STATUS_CODE_SERVER_UNREACHABLE = 65,
  WLAN_STATUS_CODE_REJECTED_FOR_SSP_PERMISSIONS = 67,
  WLAN_STATUS_CODE_REFUSED_UNAUTHENTICATED_ACCESS_NOT_SUPPORTED = 68,
  WLAN_STATUS_CODE_INVALID_RSNE = 72,
  WLAN_STATUS_CODE_UAPSD_COEXISTANCE_NOT_SUPPORTED = 73,
  WLAN_STATUS_CODE_UAPSD_COEX_MODE_NOT_SUPPORTED = 74,
  WLAN_STATUS_CODE_BAD_INTERVAL_WITH_UAPSD_COEX = 75,
  WLAN_STATUS_CODE_ANTI_CLOGGING_TOKEN_REQUIRED = 76,
  WLAN_STATUS_CODE_UNSUPPORTED_FINITE_CYCLIC_GROUP = 77,
  WLAN_STATUS_CODE_CANNOT_FIND_ALTERNATIVE_TBTT = 78,
  WLAN_STATUS_CODE_TRANSMISSION_FAILURE = 79,
  WLAN_STATUS_CODE_REQUESTED_TCLAS_NOT_SUPPORTED80 = 80,
  WLAN_STATUS_CODE_TCLAS_RESOURCES_EXHAUSTED = 81,
  WLAN_STATUS_CODE_REJECTED_WITH_SUGGESTED_BSS_TRANSITION = 82,
  WLAN_STATUS_CODE_REJECT_WITH_SCHEDULE = 83,
  WLAN_STATUS_CODE_REJECT_NO_WAKEUP_SPECIFIED = 84,
  WLAN_STATUS_CODE_SUCCESS_POWER_SAVE_MODE = 85,
  WLAN_STATUS_CODE_PENDING_ADMITTING_FST_SESSION = 86,
  WLAN_STATUS_CODE_PERFORMING_FST_NOW = 87,
  WLAN_STATUS_CODE_PENDING_GAP_IN_BA_WINDOW = 88,
  WLAN_STATUS_CODE_REJECT_UPID_SETTING = 89,
  WLAN_STATUS_CODE_REFUSED_EXTERNAL_REASON = 92,
  WLAN_STATUS_CODE_REFUSED_AP_OUT_OF_MEMORY = 93,
  WLAN_STATUS_CODE_REJECTED_EMERGENCY_SERVICES_NOT_SUPPORTED = 94,
  WLAN_STATUS_CODE_QUERY_RESPONSE_OUTSTANDING = 95,
  WLAN_STATUS_CODE_REJECT_DSE_BAND = 96,
  WLAN_STATUS_CODE_TCLAS_PROCESSING_TERMINATED = 97,
  WLAN_STATUS_CODE_TS_SCHEDULE_CONFLICT = 98,
  WLAN_STATUS_CODE_DENIED_WITH_SUGGESTED_BAND_AND_CHANNEL = 99,
  WLAN_STATUS_CODE_MCCAOP_RESERVATION_CONFLICT = 100,
  WLAN_STATUS_CODE_MAF_LIMIT_EXCEEDED = 101,
  WLAN_STATUS_CODE_MCCA_TRACK_LIMIT_EXCEEDED = 102,
  WLAN_STATUS_CODE_DENIED_DUE_TO_SPECTRUM_MANAGEMENT = 103,
  WLAN_STATUS_CODE_DENIED_VHT_NOT_SUPPORTED = 104,
  WLAN_STATUS_CODE_ENABLEMENT_DENIED = 105,
  WLAN_STATUS_CODE_RESTRICTION_FROM_AUTHORIZED_GDB = 106,
  WLAN_STATUS_CODE_AUTHORIZATION_DEENABLED = 107,
} wlan_status_code;

/**
 * Manages all SNS for a STA.
 */
typedef struct mlme_sequence_manager_t mlme_sequence_manager_t;

typedef struct {
  void *device;
  int32_t (*deliver_ethernet)(void *device, const uint8_t *data, uintptr_t len);
} mlme_device_ops_t;

/**
 * An input buffer will always be returned to its original owner when no longer
 * being used. An input buffer is used for every buffer handed from C++ to Rust.
 */
typedef struct {
  /**
   * Returns the buffer's ownership and free it.
   */
  void (*free_buffer)(void *raw);
  /**
   * Pointer to the buffer's underlying data structure.
   */
  void *raw;
  /**
   * Pointer to the start of the buffer's data portion and its length.
   */
  uint8_t *data;
  uintptr_t len;
} mlme_in_buf_t;

typedef struct {
  /**
   * Acquire a `InBuf` with a given minimum length from the provider.
   * The provider must release the underlying buffer's ownership and transfer it
   * to this crate. The buffer will be returned via the `free_buffer` callback
   * when it's no longer used.
   */
  mlme_in_buf_t (*get_buffer)(uintptr_t min_len);
} mlme_buffer_provider_ops_t;

/**
 * An output buffer requires its owner to manage the underlying buffer's memory
 * themselves. An output buffer is used for every buffer handed from Rust to
 * C++.
 */
typedef struct {
  /**
   * Pointer to the buffer's underlying data structure.
   */
  void *raw;
  /**
   * Pointer to the start of the buffer's data portion and the amount of bytes
   * written.
   */
  uint8_t *data;
  uintptr_t written_bytes;
} mlme_out_buf_t;

extern "C" int32_t mlme_handle_data_frame(const mlme_device_ops_t *device,
                                          const uint8_t *data_frame,
                                          uintptr_t data_frame_len,
                                          bool has_padding);

extern "C" int32_t mlme_is_valid_open_auth_resp(const uint8_t *data,
                                                uintptr_t len);

extern "C" void mlme_sequence_manager_delete(mlme_sequence_manager_t *mgr);

extern "C" mlme_sequence_manager_t *mlme_sequence_manager_new(void);

extern "C" uint32_t mlme_sequence_manager_next_sns1(
    mlme_sequence_manager_t *mgr, const uint8_t (*sta_addr)[6]);

extern "C" uint32_t mlme_sequence_manager_next_sns2(
    mlme_sequence_manager_t *mgr, const uint8_t (*sta_addr)[6], uint16_t tid);

extern "C" int32_t mlme_write_deauth_frame(mlme_buffer_provider_ops_t provider,
                                           mlme_sequence_manager_t *seq_mgr,
                                           const uint8_t (*bssid)[6],
                                           const uint8_t (*client_addr)[6],
                                           uint16_t reason_code,
                                           mlme_out_buf_t *out_buf);

extern "C" int32_t mlme_write_eapol_data_frame(
    mlme_buffer_provider_ops_t provider, mlme_sequence_manager_t *seq_mgr,
    const uint8_t (*dest)[6], const uint8_t (*src)[6], bool is_protected,
    const uint8_t *eapol_frame_ptr, uintptr_t eapol_frame_len,
    mlme_out_buf_t *out_buf);

extern "C" int32_t mlme_write_keep_alive_resp_frame(
    mlme_buffer_provider_ops_t provider, mlme_sequence_manager_t *seq_mgr,
    const uint8_t (*bssid)[6], const uint8_t (*client_addr)[6],
    mlme_out_buf_t *out_buf);

extern "C" int32_t mlme_write_open_auth_frame(
    mlme_buffer_provider_ops_t provider, mlme_sequence_manager_t *seq_mgr,
    const uint8_t (*bssid)[6], const uint8_t (*client_addr)[6],
    mlme_out_buf_t *out_buf);

#endif /* GARNET_LIB_RUST_WLAN_MLME_C_BINDINGS_H_ */
