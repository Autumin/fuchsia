/*
 * Copyright (c) 2019 The Fuchsia Authors
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/sim/sim_fw.h"

#include <zircon/assert.h>

#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/bcdc.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/brcm_hw_ids.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/brcmu_d11.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/cfg80211.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/common.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/debug.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/fweh.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/fwil.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/fwil_types.h"
#include "src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/sim/sim.h"

namespace wlan::brcmfmac {

#define SIM_FW_CHK_CMD_LEN(dcmd_len, exp_len) \
  (((dcmd_len) < (exp_len)) ? ZX_ERR_INVALID_ARGS : ZX_OK)

SimFirmware::SimFirmware(brcmf_simdev* simdev, simulation::Environment* env)
    : simdev_(simdev), hw_(env) {
  // Configure the chanspec encode/decoder
  d11_inf_.io_type = kIoType;
  brcmu_d11_attach(&d11_inf_);

  // Configure the (simulated) hardware => (simulated) firmware callbacks
  SimHardware::EventHandlers handlers = {
      .rx_handler = std::bind(&SimFirmware::Rx, this, std::placeholders::_1, std::placeholders::_2),
  };
  hw_.SetCallbacks(handlers);
  country_code_ = {};

  // The real FW always creates the first interface
  struct brcmf_mbss_ssid_le default_mbss = {};
  if (HandleIfaceTblReq(true, &default_mbss, nullptr) != ZX_OK) {
    ZX_PANIC("Unable to create default interface\n");
  }
}

SimFirmware::~SimFirmware() = default;

simulation::StationIfc* SimFirmware::GetHardwareIfc() { return &hw_; }

void SimFirmware::GetChipInfo(uint32_t* chip, uint32_t* chiprev) {
  *chip = BRCM_CC_4356_CHIP_ID;
  *chiprev = 2;
}

int32_t SimFirmware::GetPM() { return power_mode_; }

zx_status_t SimFirmware::BusPreinit() {
  // Currently nothing to do
  return ZX_OK;
}

void SimFirmware::BusStop() { ZX_PANIC("%s unimplemented", __FUNCTION__); }

zx_status_t SimFirmware::BusTxData(struct brcmf_netbuf* netbuf) {
  // For now we just save data and len and ignore bcdc header, further operations will be added
  // soon.
  uint32_t data_size = netbuf->len - BCDC_HEADER_LEN;
  last_pkt_buf_.data.reset(new uint8_t[data_size]);

  memcpy(last_pkt_buf_.data.get(), netbuf->data + BCDC_HEADER_LEN, data_size);
  last_pkt_buf_.len = data_size;
  last_pkt_buf_.allocated_size_of_buf_in = netbuf->allocated_size;
  return ZX_OK;
}

// Returns a bufer that can be used for BCDC-formatted communications, with the requested
// payload size and an initialized BCDC header. "offset_out" represents the offset of the
// payload within the returned buffer.
std::unique_ptr<std::vector<uint8_t>> SimFirmware::CreateBcdcBuffer(size_t requested_size,
                                                                    size_t* offset_out) {
  size_t header_size = sizeof(brcmf_proto_bcdc_header);
  size_t total_size = header_size + requested_size;

  auto buf = std::make_unique<std::vector<uint8_t>>(total_size);
  auto header = reinterpret_cast<brcmf_proto_bcdc_header*>(buf->data());

  header->flags = (BCDC_PROTO_VER << BCDC_FLAG_VER_SHIFT) & BCDC_FLAG_VER_MASK;
  header->priority = 0xff & BCDC_PRIORITY_MASK;
  header->flags2 = 0;

  // Data immediately follows the header
  header->data_offset = 0;

  *offset_out = header_size;
  return buf;
}

// Set or get the value of an iovar. The format of the message is a null-terminated string
// containing the iovar name, followed by the value to assign to that iovar.
zx_status_t SimFirmware::BcdcVarOp(uint16_t ifidx, brcmf_proto_bcdc_dcmd* dcmd, uint8_t* data,
                                   size_t len, bool is_set) {
  zx_status_t status = ZX_OK;

  char* str_begin = reinterpret_cast<char*>(data);
  uint8_t* str_end = static_cast<uint8_t*>(memchr(str_begin, '\0', dcmd->len));
  if (str_end == nullptr) {
    BRCMF_DBG(SIM, "SET_VAR: iovar name not null-terminated\n");
    return ZX_ERR_INVALID_ARGS;
  }

  size_t str_len = str_end - data;

  // IovarsSet returns the input unchanged
  // IovarsGet modifies the buffer in-place
  if (is_set) {
    void* value_start = str_end + 1;
    size_t value_len = dcmd->len - (str_len + 1);
    status = IovarsSet(ifidx, str_begin, value_start, value_len);
  } else {
    status = IovarsGet(ifidx, str_begin, data, dcmd->len);
  }

  if (status == ZX_OK) {
    bcdc_response_.Set(reinterpret_cast<uint8_t*>(dcmd), len);
  } else {
    // Return empty message on failure
    bcdc_response_.Clear();
  }
  return status;
}

// Process a TX CTL message. These have a BCDC header, followed by a payload that is determined
// by the type of command.
zx_status_t SimFirmware::BusTxCtl(unsigned char* msg, unsigned int len) {
  brcmf_proto_bcdc_dcmd* dcmd;
  constexpr size_t hdr_size = sizeof(struct brcmf_proto_bcdc_dcmd);
  uint32_t value;
  uint32_t ifidx;

  if (len < hdr_size) {
    BRCMF_DBG(SIM, "Message length (%u) smaller than BCDC header size (%zd)\n", len, hdr_size);
    return ZX_ERR_INVALID_ARGS;
  }
  dcmd = reinterpret_cast<brcmf_proto_bcdc_dcmd*>(msg);
  // The variable-length payload immediately follows the header
  uint8_t* data = reinterpret_cast<uint8_t*>(dcmd) + hdr_size;

  if (dcmd->len > (len - hdr_size)) {
    BRCMF_DBG(SIM, "BCDC total message length (%zd) exceeds buffer size (%u)\n",
              dcmd->len + hdr_size, len);
    return ZX_ERR_INVALID_ARGS;
  }

  // Retrieve ifidx from the command and validate if the corresponding
  // IF entry was previously allocated.
  ifidx = BCDC_DCMD_IFIDX(dcmd->flags);
  if (ifidx >= kMaxIfSupported || !iface_tbl_[ifidx].allocated) {
    BRCMF_DBG(SIM, "IF idx: %d invalid or not allocated\n", ifidx);
    return ZX_ERR_INVALID_ARGS;
  }

  zx_status_t status = ZX_OK;
  switch (dcmd->cmd) {
    // Get/Set a firmware IOVAR. This message is comprised of a NULL-terminated string
    // for the variable name, followed by the value to assign to it.
    case BRCMF_C_SET_VAR:
    case BRCMF_C_GET_VAR:
      return BcdcVarOp(ifidx, dcmd, data, len, dcmd->cmd == BRCMF_C_SET_VAR);
      break;
    case BRCMF_C_GET_REVINFO: {
      struct brcmf_rev_info_le rev_info;
      hw_.GetRevInfo(&rev_info);
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(rev_info))) == ZX_OK) {
        memcpy(data, &rev_info, sizeof(rev_info));
      }
      break;
    }
    case BRCMF_C_GET_VERSION: {
      // GET_VERSION is a bit of a misnomer. It's really the 802.11 supported spec
      // (e.g., n or ac).
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(kIoType))) == ZX_OK) {
        std::memcpy(data, &kIoType, sizeof(kIoType));
      }
      break;
    }
    case BRCMF_C_SET_PASSIVE_SCAN: {
      // Specify whether to use a passive scan by default (instead of an active scan)
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(uint32_t))) == ZX_OK) {
        value = *(reinterpret_cast<uint32_t*>(data));
        default_passive_scan_ = value > 0;
      }
      break;
    }
    case BRCMF_C_SET_SCAN_PASSIVE_TIME:
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(default_passive_time_))) == ZX_OK) {
        default_passive_time_ = *(reinterpret_cast<uint32_t*>(data));
      }
      break;
    case BRCMF_C_SET_PM:
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(power_mode_))) == ZX_OK) {
        power_mode_ = *(reinterpret_cast<int32_t*>(data));
      }
      break;
    case BRCMF_C_SET_SCAN_CHANNEL_TIME:
    case BRCMF_C_SET_SCAN_UNASSOC_TIME:
      BRCMF_DBG(SIM, "Ignoring firmware message %d\n", dcmd->cmd);
      break;
    case BRCMF_C_DISASSOC: {
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(brcmf_scb_val_le))) == ZX_OK) {
        // Initiate Disassoc from AP
        auto scb_val = reinterpret_cast<brcmf_scb_val_le*>(data);
        DisassocLocalClient(scb_val);
      } else {
        // Driver indicating disassoc (no data)
        if (assoc_state_.state == AssocState::ASSOCIATED) {
          assoc_state_.state = AssocState::NOT_ASSOCIATED;
        }
      }
      break;
    }
    case BRCMF_C_SET_ROAM_TRIGGER:
    case BRCMF_C_SET_ROAM_DELTA:
      break;
    case BRCMF_C_UP:
      // The value in the IOVAR does not matter (according to Broadcom)
      // TODO(karthikrish) Use dev_is_up_ to disable Tx, Rx, etc.
      dev_is_up_ = true;
      break;
    case BRCMF_C_DOWN:
      // The value in the IOVAR does not matter (according to Broadcom)
      dev_is_up_ = false;
      break;
    case BRCMF_C_SET_INFRA:
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(uint32_t))) == ZX_OK) {
        iface_tbl_[ifidx].ap_config.infra_mode = *(reinterpret_cast<uint32_t*>(data));
      }
      break;
    case BRCMF_C_SET_AP:
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(uint32_t))) == ZX_OK) {
        value = *(reinterpret_cast<uint32_t*>(data));
        if (value) {
          ZX_ASSERT_MSG(iface_tbl_[ifidx].ap_config.infra_mode, "Only Infra mode AP is supported");
          iface_tbl_[ifidx].ap_mode = true;
        } else
          iface_tbl_[ifidx].ap_mode = false;
      }
      break;
    case BRCMF_C_SET_BCNPRD:
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(uint32_t))) == ZX_OK) {
        // Beacon period
        iface_tbl_[ifidx].ap_config.beacon_period = *(reinterpret_cast<uint32_t*>(data));
      }
      break;
    case BRCMF_C_SET_DTIMPRD:
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(uint32_t))) == ZX_OK) {
        // DTIM
        iface_tbl_[ifidx].ap_config.dtim_period = *(reinterpret_cast<uint32_t*>(data));
      }
      break;
    case BRCMF_C_SET_SSID: {
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(brcmf_join_params))) == ZX_OK) {
        auto join_params = (reinterpret_cast<brcmf_join_params*>(data));
        if (iface_tbl_[ifidx].ap_mode == true) {
          iface_tbl_[ifidx].ap_config.ssid = join_params->ssid_le;
          if (join_params->ssid_le.SSID_len) {
            // non-zero SSID - assume AP start
            ZX_ASSERT(iface_tbl_[ifidx].ap_config.ap_started == false);
            // Indicate success to driver
            SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_SUCCESS, ifidx, nullptr,
                              BRCMF_EVENT_MSG_LINK);
            iface_tbl_[ifidx].ap_config.ap_started = true;

            // Set the channel to the value specified in "chanspec" iovar
            wlan_channel_t channel;
            chanspec_to_channel(&d11_inf_, iface_tbl_[ifidx].chanspec, &channel);
            hw_.SetChannel(channel);
            // And Enable Rx
            hw_.EnableRx();
          } else {
            // AP stop
            ZX_ASSERT(iface_tbl_[ifidx].ap_config.ap_started == true);
            // Disassoc and remove all the associated clients
            for (auto client : iface_tbl_[ifidx].ap_config.clients) {
              simulation::SimDisassocReqFrame disassoc_req_frame(iface_tbl_[ifidx].mac_addr, client,
                                                                 0);
              hw_.Tx(&disassoc_req_frame);
            }
            iface_tbl_[ifidx].ap_config.clients.clear();
            SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_SUCCESS, ifidx);
            iface_tbl_[ifidx].ap_config.ap_started = false;
            iface_tbl_[ifidx].chanspec = 0;
            BRCMF_DBG(SIM, "AP Stop processed\n");
          }
        } else {
          // When iface_tbl_[ifidx].ap_mode == false, start an association
          ZX_ASSERT(join_params->params_le.chanspec_num == 1);
          auto assoc_opts = std::make_unique<AssocOpts>();
          wlan_channel_t channel;

          chanspec_to_channel(&d11_inf_, join_params->params_le.chanspec_list[0], &channel);
          memcpy(assoc_opts->bssid.byte, join_params->params_le.bssid, ETH_ALEN);
          assoc_opts->ssid.len = join_params->ssid_le.SSID_len;
          memcpy(assoc_opts->ssid.ssid, join_params->ssid_le.SSID, IEEE80211_MAX_SSID_LEN);

          AssocInit(std::move(assoc_opts), ifidx, channel);
          AuthStart();
        }
      }
      break;
    }
    case BRCMF_C_GET_RSSI: {
      if ((status = SIM_FW_CHK_CMD_LEN(dcmd->len, sizeof(int32_t))) == ZX_OK) {
        int32_t rssi = -20;
        std::memcpy(data, &rssi, sizeof(rssi));
      }
      break;
    }
    default:
      BRCMF_DBG(SIM, "Unimplemented firmware message %d\n", dcmd->cmd);
      return ZX_ERR_NOT_SUPPORTED;
  }
  if (status == ZX_OK) {
    bcdc_response_.Set(msg, len);
  }
  return status;
}

uint16_t SimFirmware::GetNumClients(uint16_t ifidx) {
  if (ifidx >= kMaxIfSupported || !iface_tbl_[ifidx].allocated || !iface_tbl_[ifidx].ap_mode) {
    BRCMF_DBG(SIM, "GetNumClients: invalid if: %d\n", ifidx);
    return 0;
  }
  return iface_tbl_[ifidx].ap_config.clients.size();
}

// Process an RX CTL message. We simply pass back the results of the previous TX CTL
// operation, which has been stored in bcdc_response_. In real hardware, we may have to
// indicate that the TX CTL operation has not completed. In simulated hardware, we perform
// all operations synchronously.
zx_status_t SimFirmware::BusRxCtl(unsigned char* msg, uint len, int* rxlen_out) {
  if (bcdc_response_.IsClear()) {
    return ZX_ERR_UNAVAILABLE;
  }

  size_t actual_len;
  zx_status_t result = bcdc_response_.Get(msg, len, &actual_len);
  if (result == ZX_OK) {
    // Responses are not re-sent on subsequent requests
    bcdc_response_.Clear();
    *rxlen_out = actual_len;
  }
  return result;
}

struct pktq* SimFirmware::BusGetTxQueue() {
  ZX_PANIC("%s unimplemented", __FUNCTION__);
  return nullptr;
}

void SimFirmware::BusWowlConfig(bool enabled) { ZX_PANIC("%s unimplemented", __FUNCTION__); }

size_t SimFirmware::BusGetRamsize() {
  ZX_PANIC("%s unimplemented", __FUNCTION__);
  return 0;
}

zx_status_t SimFirmware::BusGetMemdump(void* data, size_t len) {
  ZX_PANIC("%s unimplemented", __FUNCTION__);
  return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t SimFirmware::BusGetFwName(uint chip, uint chiprev, unsigned char* fw_name,
                                      size_t* fw_name_size) {
  constexpr char kFirmwareName[] = "sim-fake-fw.bin";
  strlcpy((char*)fw_name, kFirmwareName, *fw_name_size);
  *fw_name_size = sizeof(kFirmwareName);
  return ZX_OK;
}

zx_status_t SimFirmware::BusGetBootloaderMacAddr(uint8_t* mac_addr) {
  // Rather than simulate a fixed MAC address, return NOT_SUPPORTED, which will force
  // us to use a randomly-generated value
  return ZX_ERR_NOT_SUPPORTED;
}

void SimFirmware::BcdcResponse::Clear() { len_ = 0; }

zx_status_t SimFirmware::BcdcResponse::Get(uint8_t* data, size_t len, size_t* len_out) {
  if (len < len_) {
    return ZX_ERR_BUFFER_TOO_SMALL;
  }

  memcpy(data, msg_, len_);
  *len_out = len_;
  return ZX_OK;
}

bool SimFirmware::BcdcResponse::IsClear() { return len_ == 0; }

void SimFirmware::BcdcResponse::Set(uint8_t* data, size_t new_len) {
  ZX_DEBUG_ASSERT(new_len <= sizeof(msg_));
  len_ = new_len;
  memcpy(msg_, data, new_len);
}

zx_status_t SimFirmware::HandleIfaceTblReq(const bool add_entry, const void* data,
                                           uint8_t* iface_id) {
  if (add_entry) {
    auto ssid_info = static_cast<const brcmf_mbss_ssid_le*>(data);

    // Allocate the first available entry
    for (int i = 0; i < kMaxIfSupported; i++) {
      if (!iface_tbl_[i].allocated) {
        iface_tbl_[i].allocated = true;
        iface_tbl_[i].iface_id = i;
        iface_tbl_[i].bsscfgidx = ssid_info->bsscfgidx;
        if (iface_id)
          *iface_id = i;
        return ZX_OK;
      }
    }
  } else {
    auto bsscfgidx = static_cast<const int32_t*>(data);
    for (int i = 0; i < kMaxIfSupported; i++) {
      if (iface_tbl_[i].allocated && iface_tbl_[i].bsscfgidx == *bsscfgidx) {
        if (iface_id)
          *iface_id = iface_tbl_[i].iface_id;
        // If AP is in started state, send disassoc req to all clients
        if (iface_tbl_[i].ap_mode) {
          if (iface_tbl_[i].ap_config.ap_started) {
            BRCMF_DBG(SIM, "AP is still started...disassoc all clients\n");
            for (auto client : iface_tbl_[i].ap_config.clients) {
              simulation::SimDisassocReqFrame disassoc_req_frame(iface_tbl_[i].mac_addr, client, 0);
              hw_.Tx(&disassoc_req_frame);
            }
          }
          // Clear out the clients
          iface_tbl_[i].ap_config.clients.clear();
        }
        iface_tbl_[i] = {};
        BRCMF_DBG(SIM, "Interface Delete ifidx: %d done\n", i);
        return ZX_OK;
      }
    }
  }
  return ZX_ERR_IO;
}

zx_status_t SimFirmware::HandleIfaceRequest(const bool add_iface, const void* data,
                                            const size_t len) {
  uint8_t iface_id;
  size_t payload_size = sizeof(brcmf_if_event);
  auto buf = std::make_unique<std::vector<uint8_t>>(payload_size);

  uint8_t* buffer_data = buf->data();
  struct brcmf_if_event* ifevent = reinterpret_cast<brcmf_if_event*>(buffer_data);

  ifevent->role = 1;

  if (HandleIfaceTblReq(add_iface, data, &iface_id) == ZX_OK) {
    if (add_iface) {
      auto ssid_info = static_cast<const brcmf_mbss_ssid_le*>(data);
      ifevent->action = BRCMF_E_IF_ADD;
      ifevent->bsscfgidx = ssid_info->bsscfgidx;
    } else {
      auto bsscfgidx = static_cast<const int32_t*>(data);
      ifevent->action = BRCMF_E_IF_DEL;
      ifevent->bsscfgidx = static_cast<const uint8_t>(*bsscfgidx);
    }
    ifevent->ifidx = iface_id;
    char ifname[IFNAMSIZ];
    sprintf(ifname, "wl0.%d", iface_id);
    SendEventToDriver(payload_size, std::move(buf), BRCMF_E_IF, BRCMF_E_STATUS_SUCCESS, iface_id,
                      ifname);
  } else {
    SendEventToDriver(payload_size, std::move(buf), BRCMF_E_IF, BRCMF_E_STATUS_ERROR, 0);
  }
  return ZX_OK;
}

// Handle association request from a client to the SoftAP interface
// ifidx is expected to be a valid index (allocated and configured as AP)
#define TWO_ZERO_LEN_TLVS_LEN (4)
zx_status_t SimFirmware::HandleAssocReq(uint16_t ifidx, const common::MacAddr& src_mac) {
  auto buf = std::make_unique<std::vector<uint8_t>>(TWO_ZERO_LEN_TLVS_LEN);

  uint8_t* tlv_buf = buf->data();
  // The driver expects ssid and rsne in TLV format, just fake it for now
  *tlv_buf++ = WLAN_IE_TYPE_SSID;
  *tlv_buf++ = 0;
  *tlv_buf++ = WLAN_IE_TYPE_RSNE;
  *tlv_buf++ = 0;
  SendEventToDriver(TWO_ZERO_LEN_TLVS_LEN, std::move(buf), BRCMF_E_ASSOC_IND,
                    BRCMF_E_STATUS_SUCCESS, ifidx, nullptr, 0, 0, src_mac);
  return ZX_OK;
}

void SimFirmware::AssocInit(std::unique_ptr<AssocOpts> assoc_opts, const uint16_t ifidx,
                            wlan_channel_t& channel) {
  assoc_state_.ifidx = ifidx;
  assoc_state_.state = AssocState::ASSOCIATING;
  assoc_state_.opts = std::move(assoc_opts);
  assoc_state_.num_attempts = 0;

  // Values stored in assoc_state_ will be use in authentication step.
  common::MacAddr srcAddr(mac_addr_);
  common::MacAddr bssid(assoc_state_.opts->bssid);

  uint16_t chanspec = channel_to_chanspec(&d11_inf_, &channel);
  SetIFChanspec(assoc_state_.ifidx, chanspec);
  hw_.SetChannel(channel);
  hw_.EnableRx();
}

void SimFirmware::AssocScanResultSeen(const ScanResult& scan_result) {
  // Check ssid filter
  if (scan_state_.opts->ssid) {
    ZX_ASSERT(scan_state_.opts->ssid->len <= sizeof(scan_state_.opts->ssid->ssid));
    if (scan_result.ssid.len != scan_state_.opts->ssid->len) {
      return;
    }
    if (std::memcmp(scan_result.ssid.ssid, scan_state_.opts->ssid->ssid, scan_result.ssid.len)) {
      return;
    }
  }

  // Check bssid filter
  if ((scan_state_.opts->bssid) && (scan_result.bssid != *scan_state_.opts->bssid)) {
    return;
  }

  assoc_state_.scan_results.push_back(scan_result);
}

void SimFirmware::AssocScanDone() {
  ZX_ASSERT(assoc_state_.state == AssocState::SCANNING);

  // Operation fails if we don't have at least one scan result
  if (assoc_state_.scan_results.size() == 0) {
    SendEventToDriver(0, nullptr, BRCMF_E_SET_SSID, BRCMF_E_STATUS_NO_NETWORKS, scan_state_.ifidx);
    assoc_state_.state = AssocState::NOT_ASSOCIATED;
    return;
  }

  // For now, just pick the first AP. The real firmware can select based on signal strength and
  // band, but since wlanstack makes its own decision, we won't bother to model that here for now.
  ScanResult& ap = assoc_state_.scan_results.front();

  auto assoc_opts = std::make_unique<AssocOpts>();
  assoc_opts->bssid = ap.bssid;
  if (scan_state_.opts->ssid)
    assoc_opts->ssid = scan_state_.opts->ssid.value();

  AssocInit(std::move(assoc_opts), scan_state_.ifidx, ap.channel);
  // Send an event of the first scan result to driver when assoc scan is done.
  EscanResultSeen(ap);

  AuthStart();
}

void SimFirmware::AssocClearContext() {
  assoc_state_.state = AssocState::NOT_ASSOCIATED;
  assoc_state_.opts = nullptr;
  assoc_state_.scan_results.clear();
  // Clear out the channel setting
  iface_tbl_[assoc_state_.ifidx].chanspec = 0;
}

void SimFirmware::AuthClearContext() {
  auth_state_.state = AuthState::NOT_AUTHENTICATED;
  auth_state_.sec_type = simulation::SEC_PROTO_TYPE_OPEN;
}

void SimFirmware::AssocHandleFailure() {
  if (assoc_state_.num_attempts >= assoc_max_retries_) {
    SendEventToDriver(0, nullptr, BRCMF_E_SET_SSID, BRCMF_E_STATUS_FAIL, assoc_state_.ifidx);
    AssocClearContext();
  } else {
    assoc_state_.num_attempts++;
    auth_state_.state = AuthState::NOT_AUTHENTICATED;
    AuthStart();
  }
}

void SimFirmware::AuthStart() {
  common::MacAddr srcAddr(mac_addr_);
  common::MacAddr bssid(assoc_state_.opts->bssid);

  auto callback = new std::function<void()>;
  *callback = std::bind(&SimFirmware::AssocHandleFailure, this);
  hw_.RequestCallback(callback, kAuthTimeout, &auth_state_.auth_timer_id);

  ZX_ASSERT(auth_state_.state == AuthState::NOT_AUTHENTICATED);
  simulation::SimAuthType auth_type;
  if (auth_state_.auth_type == BRCMF_AUTH_MODE_OPEN) {
    // Sequence number start from 0, end with at most 3.
    auth_type = simulation::AUTH_TYPE_OPEN;
  } else {
    // When auth_state_.auth_type == BRCMF_AUTH_MODE_AUTO
    auth_type = simulation::AUTH_TYPE_SHARED_KEY;
  }

  simulation::SimAuthFrame auth_req_frame(srcAddr, bssid, 1, auth_type, WLAN_AUTH_RESULT_SUCCESS);

  if (wsec_ == WEP_ENABLED) {
    ZX_ASSERT(wpa_auth_ == WPA_AUTH_DISABLED);
    auth_state_.sec_type = simulation::SEC_PROTO_TYPE_WEP;
  }

  if (wpa_auth_ == WPA_AUTH_PSK) {
    ZX_ASSERT((wsec_ & (uint32_t)WSEC_NONE) == 0U);
    auth_state_.sec_type = simulation::SEC_PROTO_TYPE_WPA1;
  }

  if (wpa_auth_ == WPA2_AUTH_PSK) {
    ZX_ASSERT((wsec_ & (uint32_t)WSEC_NONE) == 0U);
    auth_state_.sec_type = simulation::SEC_PROTO_TYPE_WPA2;
  }

  auth_req_frame.sec_proto_type_ = auth_state_.sec_type;
  auth_state_.state = AuthState::EXPECTING_SECOND;
  hw_.Tx(&auth_req_frame);
}

void SimFirmware::RxAuthResp(const simulation::SimAuthFrame* frame) {
  // If we are not expecting auth resp packets, ignore it,
  if (auth_state_.state == AuthState::NOT_AUTHENTICATED ||
      auth_state_.state == AuthState::AUTHENTICATED) {
    return;
  }
  // Ignore if this is not intended for us
  common::MacAddr mac_addr(mac_addr_);
  if (frame->dst_addr_ != mac_addr) {
    return;
  }
  // Ignore if this is not from the bssid with which we were trying to authenticate
  if (frame->src_addr_ != assoc_state_.opts->bssid) {
    return;
  }

  // It should not be an auth req frame if its dst addr is a client
  if (frame->seq_num_ != 2 && frame->seq_num_ != 4) {
    return;
  }

  // Response received, cancel timer
  hw_.CancelCallback(auth_state_.auth_timer_id);

  if (auth_state_.auth_type == BRCMF_AUTH_MODE_OPEN) {
    ZX_ASSERT(auth_state_.state == AuthState::EXPECTING_SECOND);
    ZX_ASSERT(frame->seq_num_ == 2);
    if (frame->status_ == WLAN_AUTH_RESULT_REFUSED) {
      AssocHandleFailure();
      return;
    }
    auth_state_.state = AuthState::AUTHENTICATED;
    // Remember the last auth'd bssid
    auth_state_.bssid = assoc_state_.opts->bssid;
    AssocStart();
  } else {
    // When auth_state_.auth_type == BRCMF_AUTH_MODE_AUTO
    if (auth_state_.state == AuthState::EXPECTING_SECOND && frame->seq_num_ == 2) {
      // Retry with AUTH_TYPE_OPEN_SYSTEM when refused in AUTH_TYPE_SHARED_KEY mode
      if (frame->status_ == WLAN_AUTH_RESULT_REFUSED) {
        auth_state_.state = AuthState::NOT_AUTHENTICATED;
        auth_state_.auth_type = BRCMF_AUTH_MODE_OPEN;
        AuthStart();
        return;
      }
      // If we receive the second auth frame when we are expecting it, we send out the third one and
      // set another timer for it.
      auto callback = new std::function<void()>;
      *callback = std::bind(&SimFirmware::AssocHandleFailure, this);
      hw_.RequestCallback(callback, kAuthTimeout, &auth_state_.auth_timer_id);

      auth_state_.state = AuthState::EXPECTING_FOURTH;

      common::MacAddr srcAddr(mac_addr_);
      common::MacAddr bssid(assoc_state_.opts->bssid);
      simulation::SimAuthFrame auth_req_frame(srcAddr, bssid, frame->seq_num_ + 1,
                                              simulation::AUTH_TYPE_SHARED_KEY,
                                              WLAN_AUTH_RESULT_SUCCESS);
      auth_req_frame.sec_proto_type_ = auth_state_.sec_type;
      hw_.Tx(&auth_req_frame);
    } else if (auth_state_.state == AuthState::EXPECTING_FOURTH && frame->seq_num_ == 4) {
      // If we receive the fourth auth frame when we are expecting it, start association
      auth_state_.state = AuthState::AUTHENTICATED;
      // Remember the last auth'd bssid
      auth_state_.bssid = assoc_state_.opts->bssid;
      AssocStart();
    }
  }
}

// Remove the client from the list. If found return true else false.
bool SimFirmware::FindAndRemoveClient(const uint16_t ifidx, const common::MacAddr client_mac,
                                      uint16_t reason) {
  for (auto client : iface_tbl_[ifidx].ap_config.clients) {
    if (client == client_mac) {
      iface_tbl_[ifidx].ap_config.clients.remove(client_mac);
      // Send DISASSOC_IND and DEAUTH events to driver
      SendEventToDriver(0, nullptr, BRCMF_E_DISASSOC_IND, BRCMF_E_STATUS_SUCCESS, ifidx, nullptr,
                        BRCMF_EVENT_MSG_LINK, WLAN_DEAUTH_REASON_LEAVING_NETWORK_DISASSOC,
                        client_mac);
      SendEventToDriver(0, nullptr, BRCMF_E_DEAUTH_IND, BRCMF_E_STATUS_SUCCESS, ifidx, nullptr, 0,
                        reason, client_mac);
      return true;
    }
  }
  return false;
}
void SimFirmware::RxDeauthReq(const simulation::SimDeauthFrame* frame) {
  BRCMF_DBG(SIM, "Deauth from %s for %s reason: %d\n", MACSTR(frame->src_addr_),
            MACSTR(frame->dst_addr_), frame->reason_);
  // First check if this is a deauth meant for a client associated to our SoftAP
  auto ifidx = GetIfidxByMac(frame->dst_addr_);
  if (ifidx == -1) {
    // Not meant for any of the valid IFs, ignore
    return;
  }
  if (!iface_tbl_[ifidx].ap_mode) {
    // Not meant for the SoftAP. Check if it is meant for the client interface
    HandleDisconnectForClientIF(frame, ifidx, auth_state_.bssid, frame->reason_);
    return;
  }
  // Remove the client from the list (if found)
  if (FindAndRemoveClient(ifidx, frame->src_addr_, frame->reason_)) {
    BRCMF_DBG(SIM, "Deauth done Num Clients: %lu\n", iface_tbl_[ifidx].ap_config.clients.size());
    return;
  }
  BRCMF_DBG(SIM, "Deauth Client not found in List\n");
}

void SimFirmware::AssocStart() {
  common::MacAddr srcAddr(mac_addr_);

  auto callback = new std::function<void()>;
  *callback = std::bind(&SimFirmware::AssocHandleFailure, this);
  hw_.RequestCallback(callback, kAssocTimeout, &assoc_state_.assoc_timer_id);

  // We can't use assoc_state_.opts->bssid directly because it may get free'd during TxAssocReq
  // handling if a response is sent.
  common::MacAddr bssid(assoc_state_.opts->bssid);
  simulation::SimAssocReqFrame assoc_req_frame(srcAddr, bssid, assoc_state_.opts->ssid);
  hw_.Tx(&assoc_req_frame);
}

// Get the index of the SoftAP IF based on Mac.
int16_t SimFirmware::GetIfidxByMac(const common::MacAddr& addr) {
  for (uint8_t i = 0; i < kMaxIfSupported; i++) {
    if (iface_tbl_[i].allocated && iface_tbl_[i].mac_addr == addr) {
      return i;
    }
  }
  return -1;
}

// Get the index of IF
int16_t SimFirmware::GetIfidx(bool is_ap) {
  for (uint8_t i = 0; i < kMaxIfSupported; i++) {
    if (iface_tbl_[i].allocated && (is_ap == iface_tbl_[i].ap_mode)) {
      return i;
    }
  }
  return -1;
}

// Get channel of IF
wlan_channel_t SimFirmware::GetIfChannel(bool is_ap) {
  wlan_channel_t channel;

  // Get chanspec
  int16_t ifidx = GetIfidx(false);
  ZX_ASSERT_MSG(ifidx != -1, "No client found!\n");
  uint16_t chanspec = iface_tbl_[ifidx].chanspec;
  ZX_ASSERT_MSG(chanspec != 0, "No chanspec assigned to client.\n");

  // convert to channel
  chanspec_to_channel(&d11_inf_, chanspec, &channel);
  return channel;
}

// This routine for now only handles Disassoc Request meant for the SoftAP IF.
void SimFirmware::RxDisassocReq(const simulation::SimDisassocReqFrame* frame) {
  BRCMF_DBG(SIM, "Disassoc from %s for %s reason: %d\n", MACSTR(frame->src_addr_),
            MACSTR(frame->dst_addr_), frame->reason_);
  // First check if this is a disassoc meant for a client associated to our SoftAP
  auto ifidx = GetIfidxByMac(frame->dst_addr_);
  if (ifidx == -1) {
    // Not meant for any of the valid IFs, ignore
    return;
  }
  if (!iface_tbl_[ifidx].ap_mode) {
    // Not meant for the SoftAP. Check if it is meant for the client interface
    HandleDisconnectForClientIF(frame, ifidx, assoc_state_.opts->bssid, frame->reason_);
    return;
  }
  // Remove the client from the list (if found)
  if (FindAndRemoveClient(ifidx, frame->src_addr_, WLAN_DEAUTH_REASON_LEAVING_NETWORK_DISASSOC)) {
    BRCMF_DBG(SIM, "Disassoc done Num Clients: %lu\n", iface_tbl_[ifidx].ap_config.clients.size());
    return;
  }
  BRCMF_DBG(SIM, "Client not found in List\n");
}

void SimFirmware::RxAssocResp(const simulation::SimAssocRespFrame* frame) {
  // Ignore if we are not trying to associate
  if (assoc_state_.state != AssocState::ASSOCIATING) {
    return;
  }

  // Ignore if this is not intended for us
  common::MacAddr mac_addr(mac_addr_);
  if (frame->dst_addr_ != mac_addr) {
    return;
  }

  // Ignore if this is not from the bssid with which we were trying to associate
  if (frame->src_addr_ != assoc_state_.opts->bssid) {
    return;
  }
  // Response received, cancel timer
  hw_.CancelCallback(assoc_state_.assoc_timer_id);
  if (frame->status_ == WLAN_ASSOC_RESULT_SUCCESS) {
    // Notify the driver that association succeeded
    assoc_state_.state = AssocState::ASSOCIATED;
    SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_SUCCESS, assoc_state_.ifidx, nullptr,
                      BRCMF_EVENT_MSG_LINK);
    SendEventToDriver(0, nullptr, BRCMF_E_SET_SSID, BRCMF_E_STATUS_SUCCESS, assoc_state_.ifidx,
                      nullptr, 0, 0, assoc_state_.opts->bssid);
  } else {
    AssocHandleFailure();
  }
}

// Disassociate the Local Client (request coming in from the driver)
void SimFirmware::DisassocLocalClient(brcmf_scb_val_le* scb_val) {
  uint32_t reason = scb_val->val;
  common::MacAddr* bssid;

  bssid = reinterpret_cast<common::MacAddr*>(scb_val->ea);
  if (assoc_state_.state == AssocState::ASSOCIATED) {
    common::MacAddr srcAddr(mac_addr_);

    // Transmit the disassoc req and since there is no response for it, indicate disassoc done to
    // driver now
    simulation::SimDisassocReqFrame disassoc_req_frame(srcAddr, *bssid, reason);
    hw_.Tx(&disassoc_req_frame);
    SetStateToDisassociated(assoc_state_.ifidx);
  } else {
    SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_FAIL, assoc_state_.ifidx);
  }
  AssocClearContext();
}

// Disassoc/deauth Request from FakeAP for the Client IF.
void SimFirmware::HandleDisconnectForClientIF(const simulation::SimManagementFrame* frame,
                                              const uint16_t ifidx, const common::MacAddr& bssid,
                                              const uint16_t reason) {
  // Ignore if this is not intended for us
  common::MacAddr mac_addr(iface_tbl_[ifidx].mac_addr);
  if (frame->dst_addr_ != mac_addr) {
    return;
  }

  // Ignore if this is not from the bssid with which we are associated/authenticated
  if (frame->src_addr_ != bssid) {
    return;
  }

  if (frame->MgmtFrameType() == simulation::SimManagementFrame::FRAME_TYPE_DEAUTH) {
    // The client could receive a deauth even after disassociation. Notify the driver always
    SendEventToDriver(0, nullptr, BRCMF_E_DEAUTH, BRCMF_E_STATUS_SUCCESS, ifidx, 0, 0, reason);
    if (assoc_state_.state == AuthState::AUTHENTICATED) {
      AuthClearContext();
    }
    // DEAUTH implies disassoc, so continue
  }
  // disassoc
  if (assoc_state_.state != AssocState::ASSOCIATED) {
    // Already disassoc'd, nothing more to do.
    return;
  }

  SetStateToDisassociated(ifidx);
  AssocClearContext();
}

// precondition: was associated
void SimFirmware::SetStateToDisassociated(const uint16_t ifidx) {
  // Disable beacon watchdog that triggers disconnect
  DisableBeaconWatchdog();

  // Proprogate disassociation to driver code
  SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_SUCCESS, ifidx);
}

// Assoc Request from Client for the SoftAP IF
void SimFirmware::RxAssocReq(const simulation::SimAssocReqFrame* frame) {
  BRCMF_DBG(SIM, "Assoc from %s for %s\n", MACSTR(frame->src_addr_), MACSTR(frame->bssid_));
  for (uint8_t i = 0; i < kMaxIfSupported; i++) {
    if (iface_tbl_[i].allocated && iface_tbl_[i].ap_mode) {
      if (std::memcmp(iface_tbl_[i].mac_addr.byte, frame->bssid_.byte, ETH_ALEN) == 0) {
        // Indicate Assoc success to driver - by sending AUTH_IND and ASSOC_IND
        // AUTH_IND is a simple event with the source mac address included
        SendEventToDriver(0, nullptr, BRCMF_E_AUTH_IND, BRCMF_E_STATUS_SUCCESS, i, nullptr, 0, 0,
                          frame->src_addr_);
        // ASSOC_IND contains some TLVs
        HandleAssocReq(i, frame->src_addr_);
        // Add the client to the list
        iface_tbl_[i].ap_config.clients.push_back(frame->src_addr_);
        simulation::SimAssocRespFrame assoc_resp_frame(frame->bssid_, frame->src_addr_,
                                                       WLAN_ASSOC_RESULT_SUCCESS);
        hw_.Tx(&assoc_resp_frame);
        BRCMF_DBG(SIM, "Assoc done Num Clients : %lu\n", iface_tbl_[i].ap_config.clients.size());
        break;
      }
    }
  }
}

zx_status_t SimFirmware::HandleJoinRequest(const void* value, size_t value_len, uint16_t ifidx) {
  auto join_params = reinterpret_cast<const brcmf_ext_join_params_le*>(value);

  // Verify that the channel count is consistent with the size of the structure
  size_t max_channels =
      (value_len - offsetof(brcmf_ext_join_params_le, assoc_le.chanspec_list)) / sizeof(uint16_t);
  size_t num_channels = join_params->assoc_le.chanspec_num;
  if (max_channels < num_channels) {
    BRCMF_DBG(SIM, "Bad join request: message size (%zd) too short for %zd channels\n", value_len,
              num_channels);
    return ZX_ERR_INVALID_ARGS;
  }

  if (assoc_state_.state != AssocState::NOT_ASSOCIATED) {
    ZX_ASSERT_MSG(assoc_state_.state != AssocState::ASSOCIATED,
                  "Need to add support for automatically disassociating\n");

    BRCMF_DBG(SIM, "Attempt to associate while association already in progress\n");
    return ZX_ERR_BAD_STATE;
  }

  if (scan_state_.state != ScanState::STOPPED) {
    BRCMF_DBG(SIM, "Attempt to associate while scan already in progress\n");
  }

  auto scan_opts = std::make_unique<ScanOpts>();

  // scan_opts->sync_id is unused, since we're not reporting our results back to the driver

  switch (join_params->scan_le.scan_type) {
    case BRCMF_SCANTYPE_DEFAULT:
      // Use the default
      scan_opts->is_active = !default_passive_scan_;
      break;
    case BRCMF_SCANTYPE_PASSIVE:
      scan_opts->is_active = false;
      break;
    case BRCMF_SCANTYPE_ACTIVE:
      // FIXME: this should be true, but this is the mode used by the firmware and active scans are
      // not supported yet.
      scan_opts->is_active = false;
      break;
    default:
      return ZX_ERR_INVALID_ARGS;
  }

  // Specify the SSID filter, if applicable
  const struct brcmf_ssid_le* req_ssid = &join_params->ssid_le;
  ZX_ASSERT(IEEE80211_MAX_SSID_LEN == sizeof(scan_opts->ssid->ssid));
  if (req_ssid->SSID_len != 0) {
    wlan_ssid_t ssid;
    ssid.len = req_ssid->SSID_len;
    std::copy(&req_ssid->SSID[0], &req_ssid->SSID[IEEE80211_MAX_SSID_LEN], ssid.ssid);
    scan_opts->ssid = ssid;
  }

  // Specify BSSID filter, if applicable
  common::MacAddr bssid(join_params->assoc_le.bssid);
  if (!bssid.IsZero()) {
    scan_opts->bssid = bssid;
  }

  // Determine dwell time
  if (scan_opts->is_active) {
    if (join_params->scan_le.active_time == static_cast<uint32_t>(-1)) {
      // If we hit this, we need to determine how to set the default active time
      ZX_ASSERT("Attempt to use default active scan time, but we don't know how to set this\n");
    }
    if (join_params->scan_le.active_time == 0) {
      return ZX_ERR_INVALID_ARGS;
    }
    scan_opts->dwell_time = zx::msec(join_params->scan_le.active_time);
  } else if (join_params->scan_le.passive_time == static_cast<uint32_t>(-1)) {
    // Use default passive time
    if (default_passive_time_ == static_cast<uint32_t>(-1)) {
      // If we hit this, we need to determine the default default passive time
      ZX_ASSERT("Attempt to use default passive scan time, but it hasn't been set yet\n");
    }
    scan_opts->dwell_time = zx::msec(default_passive_time_);
  } else {
    scan_opts->dwell_time = zx::msec(join_params->scan_le.passive_time);
  }

  // Copy channels from request
  scan_opts->channels.resize(num_channels);
  const uint16_t* chanspecs = &join_params->assoc_le.chanspec_list[0];
  std::copy(&chanspecs[0], &chanspecs[num_channels], scan_opts->channels.data());

  scan_opts->on_result_fn =
      std::bind(&SimFirmware::AssocScanResultSeen, this, std::placeholders::_1);
  scan_opts->on_done_fn = std::bind(&SimFirmware::AssocScanDone, this);

  // Reset assoc state
  assoc_state_.state = AssocState::SCANNING;
  assoc_state_.scan_results.clear();

  zx_status_t status = ScanStart(std::move(scan_opts), ifidx);
  if (status != ZX_OK) {
    BRCMF_DBG(SIM, "Failed to start scan: %s\n", zx_status_get_string(status));
    assoc_state_.state = AssocState::NOT_ASSOCIATED;
  }
  return status;
}

zx_status_t SimFirmware::SetIFChanspec(uint16_t ifidx, uint16_t chanspec) {
  if (ifidx < 0 || ifidx >= kMaxIfSupported || !iface_tbl_[ifidx].allocated) {
    return ZX_ERR_INVALID_ARGS;
  }

  if (iface_tbl_[ifidx].ap_mode) {
    int16_t client = GetIfidx(false);
    // When it's set for softAP, and if there is a client with a chanspec
    if (client == -1 || iface_tbl_[client].chanspec == 0) {
      // If no client is activated, just set the chanspec
      iface_tbl_[ifidx].chanspec = chanspec;
      return ZX_OK;
    }

    if (iface_tbl_[ifidx].chanspec == 0) {
      // When a new softAP iface is created, set the chanspec to client iface chanspec, ignore
      // the input.
      iface_tbl_[ifidx].chanspec = iface_tbl_[client].chanspec;
      return ZX_OK;
    } else {
      // If the softAP iface already have a chanspec, refuse the operation.
      return ZX_ERR_BAD_STATE;
    }
  } else {
    // If it's set for clients, change all chanspecs of existing ifaces into the same one(the one we
    // want to set).
    for (uint16_t i = 0; i < kMaxIfSupported; i++) {
      if (iface_tbl_[i].allocated) {
        // TODO(zhiyichen): If this operation change the chanspec for softAP iface, send out CSA
        // announcement when there is any client connecting to it.
        iface_tbl_[i].chanspec = chanspec;
      }
    }
  }
  return ZX_OK;
}

zx_status_t SimFirmware::HandleBssCfgSet(const char* name, const void* value, size_t value_len) {
  if (!std::strcmp(name, "interface_remove")) {
    if (value_len < sizeof(int32_t)) {
      return ZX_ERR_IO;
    }
    return HandleIfaceRequest(false, value, value_len);
  }

  if (!std::strcmp(name, "ssid")) {
    if (value_len < sizeof(brcmf_mbss_ssid_le)) {
      return ZX_ERR_IO;
    }
    return HandleIfaceRequest(true, value, value_len);
  }

  BRCMF_DBG(SIM, "Ignoring request to set bsscfg iovar '%s'\n", name);
  return ZX_OK;
}

zx_status_t SimFirmware::IovarsSet(uint16_t ifidx, const char* name, const void* value,
                                   size_t value_len) {
  const size_t bsscfg_prefix_len = strlen(BRCMF_FWIL_BSSCFG_PREFIX);
  if (!std::strncmp(name, BRCMF_FWIL_BSSCFG_PREFIX, bsscfg_prefix_len)) {
    return HandleBssCfgSet(name + bsscfg_prefix_len, value, value_len);
  }

  if (!std::strcmp(name, "country")) {
    auto cc_req = static_cast<const brcmf_fil_country_le*>(value);
    country_code_ = *cc_req;
    return ZX_OK;
  }

  if (!std::strcmp(name, "cur_etheraddr")) {
    if (value_len == ETH_ALEN) {
      return SetMacAddr(ifidx, static_cast<const uint8_t*>(value));
    } else {
      return ZX_ERR_INVALID_ARGS;
    }
  }

  if (!std::strcmp(name, "escan")) {
    return HandleEscanRequest(static_cast<const brcmf_escan_params_le*>(value), value_len, ifidx);
  }

  if (!std::strcmp(name, "join")) {
    if (value_len < sizeof(brcmf_ext_join_params_le)) {
      return ZX_ERR_IO;
    }
    // Don't cast yet because last element is variable length
    return HandleJoinRequest(value, value_len, ifidx);
  }

  if (!std::strcmp(name, "pfn_macaddr")) {
    auto pfn_mac = static_cast<const brcmf_pno_macaddr_le*>(value);
    memcpy(pfn_mac_addr_.byte, pfn_mac->mac, ETH_ALEN);
  }

  if (!std::strcmp(name, "wsec")) {
    auto wsec = static_cast<const uint32_t*>(value);
    wsec_ = *wsec;
  }

  if (!std::strcmp(name, "wsec_key")) {
    auto wk_req = static_cast<const brcmf_wsec_key_le*>(value);
    wsec_key_ = *wk_req;
  }

  if (!std::strcmp(name, "assoc_retry_max")) {
    auto assoc_max_retries = static_cast<const uint32_t*>(value);
    assoc_max_retries_ = *assoc_max_retries;
  }

  if (!std::strcmp(name, "chanspec")) {
    if (value_len < sizeof(uint16_t)) {
      return ZX_ERR_IO;
    }
    auto chanspec = static_cast<const uint16_t*>(value);
    // TODO(karthikrish) Add multi channel support in SIM Env. For now ensure all IFs use the same
    // channel
    return (SetIFChanspec(ifidx, *chanspec));
  }

  if (!std::strcmp(name, "mpc")) {
    if (value_len < sizeof(uint32_t)) {
      return ZX_ERR_IO;
    }
    auto mpc = static_cast<const uint32_t*>(value);
    // Ensure that mpc is never enabled when AP has been started
    int16_t ap_ifidx = GetIfidx(true);
    if (ap_ifidx != -1) {
      // A SoftAP IF has been created
      if (iface_tbl_[ifidx].ap_config.ap_started) {
        // Ensure that mpc is 0 if the SoftAP has been started
        ZX_ASSERT_MSG(*mpc == 0, "mpc should be 0 when SoftAP is active");
      }
    }
    mpc_ = *mpc;
  }

  if (!std::strcmp(name, "wpa_auth")) {
    auto wpa_auth = static_cast<const uint32_t*>(value);
    wpa_auth_ = *wpa_auth;
  }

  if (!std::strcmp(name, "auth")) {
    auto auth = static_cast<const uint32_t*>(value);
    auth_state_.auth_type = *auth;
  }
  // FIXME: For now, just pretend that we successfully set the value even when we did nothing
  BRCMF_DBG(SIM, "Ignoring request to set iovar '%s'\n", name);
  return ZX_OK;
}

const char* kFirmwareVer = "wl0: Sep 10 2018 16:37:38 version 7.35.79 (r487924) FWID 01-c76ab99a";

zx_status_t SimFirmware::IovarsGet(uint16_t ifidx, const char* name, void* value_out,
                                   size_t value_len) {
  if (!std::strcmp(name, "ver")) {
    if (value_len >= (strlen(kFirmwareVer) + 1)) {
      strlcpy(static_cast<char*>(value_out), kFirmwareVer, value_len);
    } else {
      return ZX_ERR_INVALID_ARGS;
    }
  } else if (!std::strcmp(name, "country")) {
    if (value_len >= (sizeof(brcmf_fil_country_le))) {
      memcpy(value_out, &country_code_, sizeof(brcmf_fil_country_le));
    } else {
      return ZX_ERR_INVALID_ARGS;
    }
  } else if (!std::strcmp(name, "cur_etheraddr")) {
    if (value_len < ETH_ALEN) {
      return ZX_ERR_INVALID_ARGS;
    } else {
      // Return mac address of iface if set else return the system mac address
      if (iface_tbl_[ifidx].mac_addr_set)
        memcpy(value_out, iface_tbl_[ifidx].mac_addr.byte, ETH_ALEN);
      else
        memcpy(value_out, mac_addr_.data(), ETH_ALEN);
    }
  } else if (!std::strcmp(name, "pfn_macaddr")) {
    if (value_len < ETH_ALEN) {
      return ZX_ERR_INVALID_ARGS;
    } else {
      memcpy(value_out, pfn_mac_addr_.byte, ETH_ALEN);
    }
  } else if (!std::strcmp(name, "assoc_retry_max")) {
    if (value_len < sizeof(assoc_max_retries_)) {
      return ZX_ERR_INVALID_ARGS;
    } else {
      memcpy(value_out, &assoc_max_retries_, sizeof(assoc_max_retries_));
    }
  } else if (!std::strcmp(name, "mpc")) {
    if (value_len < sizeof(uint32_t)) {
      return ZX_ERR_INVALID_ARGS;
    } else {
      memcpy(value_out, &mpc_, sizeof(uint32_t));
    }
  } else if (!std::strcmp(name, "wsec")) {
    if (value_len < sizeof(wsec_)) {
      return ZX_ERR_INVALID_ARGS;
    }
    memcpy(value_out, &wsec_, sizeof(wsec_));
  } else if (!std::strcmp(name, "wpa_auth")) {
    if (value_len < sizeof(wpa_auth_)) {
      return ZX_ERR_INVALID_ARGS;
    }
    memcpy(value_out, &wpa_auth_, sizeof(wpa_auth_));
  } else if (!std::strcmp(name, "auth")) {
    if (value_len < sizeof(auth_state_.auth_type)) {
      return ZX_ERR_INVALID_ARGS;
    }
    memcpy(value_out, &auth_state_.auth_type, sizeof(auth_state_.auth_type));
  } else if (!std::strcmp(name, "wsec_key")) {
    if (value_len < sizeof(wsec_key_)) {
      return ZX_ERR_INVALID_ARGS;
    }
    memcpy(value_out, &wsec_key_, sizeof(wsec_key_));
  } else if (!std::strcmp(name, "chanspec")) {
    if (value_len < sizeof(uint16_t)) {
      return ZX_ERR_INVALID_ARGS;
    }
    if (!iface_tbl_[ifidx].allocated) {
      return ZX_ERR_BAD_STATE;
    }
    memcpy(value_out, &iface_tbl_[ifidx].chanspec, sizeof(uint16_t));
  } else if (!std::strcmp(name, "snr")) {
    if (value_len < sizeof(int32_t)) {
      return ZX_ERR_INVALID_ARGS;
    }
    if (!iface_tbl_[ifidx].allocated) {
      return ZX_ERR_BAD_STATE;
    }
    int32_t sim_snr = 40;
    memcpy(value_out, &sim_snr, sizeof(sim_snr));
  } else {
    // FIXME: We should return an error for an unrecognized firmware variable
    BRCMF_DBG(SIM, "Ignoring request to read iovar '%s'\n", name);
    memset(value_out, 0, value_len);
  }
  return ZX_OK;
}

// If setting for the first time, save it as system mac address as well
zx_status_t SimFirmware::SetMacAddr(uint16_t ifidx, const uint8_t* mac_addr) {
  if (mac_addr_set_ == false) {
    memcpy(mac_addr_.data(), mac_addr, ETH_ALEN);
    memcpy(pfn_mac_addr_.byte, mac_addr, ETH_ALEN);
    mac_addr_set_ = true;
  }
  memcpy(iface_tbl_[ifidx].mac_addr.byte, mac_addr, ETH_ALEN);
  iface_tbl_[ifidx].mac_addr_set = true;

  BRCMF_DBG(SIM, "Setting mac addr ifidx: %d: %02x:%02x:%02x:%02x:%02x:%02x\n", ifidx, mac_addr[0],
            mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  return ZX_OK;
}

zx_status_t SimFirmware::ScanStart(std::unique_ptr<ScanOpts> opts, uint16_t ifidx) {
  if (scan_state_.state != ScanState::STOPPED) {
    // Can't start a scan while another is in progress
    return ZX_ERR_NOT_SUPPORTED;
  }

  // I believe in real firmware this will just search all channels. We need to define that set in
  // order for this functionality to work.
  ZX_ASSERT_MSG(opts->channels.size() >= 1,
                "No channels provided to escan start request - unsupported\n");

  // Configure state
  scan_state_.state = ScanState::SCANNING;
  scan_state_.opts = std::move(opts);
  scan_state_.channel_index = 0;
  scan_state_.ifidx = ifidx;

  // Start scan
  uint16_t chanspec = scan_state_.opts->channels[scan_state_.channel_index++];
  wlan_channel_t channel;
  chanspec_to_channel(&d11_inf_, chanspec, &channel);
  hw_.SetChannel(channel);

  // Do an active scan using random mac
  if (scan_state_.opts->is_active) {
    simulation::SimProbeReqFrame probe_req_frame(pfn_mac_addr_);
    hw_.Tx(&probe_req_frame);
  }
  hw_.EnableRx();

  std::function<void()>* callback = new std::function<void()>;
  *callback = std::bind(&SimFirmware::ScanNextChannel, this);
  hw_.RequestCallback(callback, scan_state_.opts->dwell_time);
  return ZX_OK;
}

// If a scan is in progress, switch to the next channel.
void SimFirmware::ScanNextChannel() {
  switch (scan_state_.state) {
    case ScanState::STOPPED:
      // We may see this event if a scan was cancelled -- just ignore it
      return;
    case ScanState::HOME:
      // We don't yet support intermittent scanning
      return;
    case ScanState::SCANNING:
      if (scan_state_.channel_index >= scan_state_.opts->channels.size()) {
        // Scanning complete
        if (scan_state_.opts->is_active) {
          memcpy(pfn_mac_addr_.byte, mac_addr_.data(), ETH_ALEN);
        }
        hw_.DisableRx();

        scan_state_.state = ScanState::STOPPED;
        // Restore the operating channel since Scan is done. This applies
        // only if the scan was started when the IF is already associated
        if (iface_tbl_[scan_state_.ifidx].chanspec) {
          wlan_channel_t channel;
          chanspec_to_channel(&d11_inf_, iface_tbl_[scan_state_.ifidx].chanspec, &channel);
          hw_.SetChannel(channel);
        }
        scan_state_.opts->on_done_fn();
        scan_state_.opts = nullptr;
      } else {
        // Scan next channel
        uint16_t chanspec = scan_state_.opts->channels[scan_state_.channel_index++];
        wlan_channel_t channel;
        chanspec_to_channel(&d11_inf_, chanspec, &channel);
        hw_.SetChannel(channel);
        if (scan_state_.opts->is_active) {
          simulation::SimProbeReqFrame probe_req_frame(pfn_mac_addr_);
          hw_.Tx(&probe_req_frame);
        }
        std::function<void()>* callback = new std::function<void()>;
        *callback = std::bind(&SimFirmware::ScanNextChannel, this);
        hw_.RequestCallback(callback, scan_state_.opts->dwell_time);
      }
  }
}

// Send an event to the firmware notifying them that the scan has completed.
zx_status_t SimFirmware::HandleEscanRequest(const brcmf_escan_params_le* escan_params,
                                            size_t params_len, uint16_t ifidx) {
  if (escan_params->version != BRCMF_ESCAN_REQ_VERSION) {
    BRCMF_DBG(SIM, "Mismatched escan version (expected %d, saw %d) - ignoring request\n",
              BRCMF_ESCAN_REQ_VERSION, escan_params->version);
    return ZX_ERR_NOT_SUPPORTED;
  }

  switch (escan_params->action) {
    case WL_ESCAN_ACTION_START:
      return EscanStart(escan_params->sync_id, &escan_params->params_le,
                        params_len - offsetof(brcmf_escan_params_le, params_le), ifidx);
    case WL_ESCAN_ACTION_CONTINUE:
      ZX_ASSERT_MSG(0, "Unimplemented escan option WL_ESCAN_ACTION_CONTINUE\n");
      return ZX_ERR_NOT_SUPPORTED;
    case WL_ESCAN_ACTION_ABORT:
      ZX_ASSERT_MSG(0, "Unimplemented escan option WL_ESCAN_ACTION_ABORT\n");
      return ZX_ERR_NOT_SUPPORTED;
    default:
      ZX_ASSERT_MSG(0, "Unrecognized escan option %d\n", escan_params->action);
      return ZX_ERR_NOT_SUPPORTED;
  }

  return ZX_OK;
}

// When asked to start an escan, we will listen on each of the specified channels for the requested
// duration (dwell time). We accomplish this by setting up a future event for the next channel,
// iterating until we have scanned all channels.
zx_status_t SimFirmware::EscanStart(uint16_t sync_id, const brcmf_scan_params_le* params,
                                    size_t params_len, uint16_t ifidx) {
  auto scan_opts = std::make_unique<ScanOpts>();

  scan_opts->sync_id = sync_id;

  switch (params->scan_type) {
    case BRCMF_SCANTYPE_ACTIVE:
      scan_opts->is_active = true;
      if (params->active_time == static_cast<uint32_t>(-1)) {
        BRCMF_ERR("No active scan time in parameter\n");
        return ZX_ERR_INVALID_ARGS;
      } else {
        scan_opts->dwell_time = zx::msec(params->active_time);
      }
      break;
    case BRCMF_SCANTYPE_PASSIVE:
      scan_opts->is_active = false;
      // Determine dwell time. If specified in the request, use that value. Otherwise, if a default
      // dwell time has been specified, use that value. Otherwise, fail.
      if (params->passive_time == static_cast<uint32_t>(-1)) {
        if (default_passive_time_ == static_cast<uint32_t>(-1)) {
          BRCMF_ERR("Attempt to use default passive time, iovar hasn't been set yet\n");
          return ZX_ERR_INVALID_ARGS;
        }
        scan_opts->dwell_time = zx::msec(default_passive_time_);
      } else {
        scan_opts->dwell_time = zx::msec(params->passive_time);
      }
      break;
    default:
      BRCMF_DBG(SIM, "Invalid scan type requested: %d\n", params->scan_type);
      return ZX_ERR_INVALID_ARGS;
  }

  size_t num_channels = params->channel_num & BRCMF_SCAN_PARAMS_COUNT_MASK;

  // Configure state
  scan_opts->channels.resize(num_channels);
  std::copy(&params->channel_list[0], &params->channel_list[num_channels],
            scan_opts->channels.data());

  scan_opts->on_result_fn = std::bind(&SimFirmware::EscanResultSeen, this, std::placeholders::_1);
  scan_opts->on_done_fn = std::bind(&SimFirmware::EscanComplete, this);
  return ScanStart(std::move(scan_opts), ifidx);
}

void SimFirmware::EscanComplete() {
  SendEventToDriver(0, nullptr, BRCMF_E_ESCAN_RESULT, BRCMF_E_STATUS_SUCCESS, scan_state_.ifidx);
}

void SimFirmware::Rx(const simulation::SimFrame* frame, simulation::WlanRxInfo& info) {
  if (frame->FrameType() == simulation::SimFrame::FRAME_TYPE_MGMT) {
    auto mgmt_frame = static_cast<const simulation::SimManagementFrame*>(frame);
    RxMgmtFrame(mgmt_frame, info);
  }
}

void SimFirmware::RxMgmtFrame(const simulation::SimManagementFrame* mgmt_frame,
                              simulation::WlanRxInfo& info) {
  switch (mgmt_frame->MgmtFrameType()) {
    case simulation::SimManagementFrame::FRAME_TYPE_BEACON: {
      auto beacon = static_cast<const simulation::SimBeaconFrame*>(mgmt_frame);
      RxBeacon(info.channel, beacon);
      break;
    }

    case simulation::SimManagementFrame::FRAME_TYPE_PROBE_RESP: {
      auto probe_resp = static_cast<const simulation::SimProbeRespFrame*>(mgmt_frame);
      RxProbeResp(info.channel, probe_resp, info.signal_strength);
      break;
    }

    case simulation::SimManagementFrame::FRAME_TYPE_ASSOC_REQ: {
      auto assoc_req = static_cast<const simulation::SimAssocReqFrame*>(mgmt_frame);
      RxAssocReq(assoc_req);
      break;
    }

    case simulation::SimManagementFrame::FRAME_TYPE_ASSOC_RESP: {
      auto assoc_resp = static_cast<const simulation::SimAssocRespFrame*>(mgmt_frame);
      RxAssocResp(assoc_resp);
      break;
    }

    case simulation::SimManagementFrame::FRAME_TYPE_DISASSOC_REQ: {
      auto disassoc_req = static_cast<const simulation::SimDisassocReqFrame*>(mgmt_frame);
      RxDisassocReq(disassoc_req);
      break;
    }

    case simulation::SimManagementFrame::FRAME_TYPE_AUTH: {
      auto auth_resp = static_cast<const simulation::SimAuthFrame*>(mgmt_frame);
      RxAuthResp(auth_resp);
      break;
    }

    case simulation::SimManagementFrame::FRAME_TYPE_DEAUTH: {
      auto deauth_req = static_cast<const simulation::SimDeauthFrame*>(mgmt_frame);
      RxDeauthReq(deauth_req);
      break;
    }

    default:
      break;
  }
}

// Start or restart the beacon watchdog. This is a timeout event mirroring how the firmware can
// detect when a connection is lost from the lack of beacons received.
void SimFirmware::RestartBeaconWatchdog() {
  DisableBeaconWatchdog();
  assoc_state_.is_beacon_watchdog_active = true;
  auto handler = new std::function<void()>;
  *handler = std::bind(&SimFirmware::HandleBeaconTimeout, this);
  hw_.RequestCallback(handler, beacon_timeout_, &assoc_state_.beacon_watchdog_id_);
}

void SimFirmware::DisableBeaconWatchdog() {
  if (assoc_state_.is_beacon_watchdog_active) {
    hw_.CancelCallback(assoc_state_.beacon_watchdog_id_);
  }
}

void SimFirmware::HandleBeaconTimeout() {
  // Ignore if we are not associated
  if (assoc_state_.state != AssocState::ASSOCIATED) {
    return;
  }

  assoc_state_.is_beacon_watchdog_active = false;
  // Indicate to the driver that we're disassociating due to lost beacons
  SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_SUCCESS, assoc_state_.ifidx, 0,
                    BRCMF_E_REASON_LOW_RSSI);
  SendEventToDriver(0, nullptr, BRCMF_E_LINK, BRCMF_E_STATUS_SUCCESS, assoc_state_.ifidx, 0,
                    BRCMF_E_REASON_DEAUTH);
  AssocClearContext();
}

void SimFirmware::ConductChannelSwitch(wlan_channel_t& dst_channel, uint8_t mode) {
  // Change fw and hw channel
  uint16_t chanspec;
  int16_t ifidx = GetIfidx(false);
  ZX_ASSERT_MSG(ifidx != -1, "No client found!\n");

  hw_.SetChannel(dst_channel);
  chanspec = channel_to_chanspec(&d11_inf_, &dst_channel);
  SetIFChanspec((uint16_t)ifidx, chanspec);

  // Send up CSA event to driver
  auto buf = std::make_unique<std::vector<uint8_t>>(sizeof(uint8_t));
  *(buf->data()) = mode;
  SendEventToDriver(sizeof(uint8_t), std::move(buf), BRCMF_E_CSA_COMPLETE_IND,
                    BRCMF_E_STATUS_SUCCESS, (uint16_t)ifidx);

  // Clear state
  channel_switch_state_.state = ChannelSwitchState::HOME;
}

void SimFirmware::RxBeacon(const wlan_channel_t& channel, const simulation::SimBeaconFrame* frame) {
  if (scan_state_.state == ScanState::SCANNING && !scan_state_.opts->is_active) {
    ScanResult scan_result = {.channel = channel, .ssid = frame->ssid_, .bssid = frame->bssid_};

    scan_result.bss_capability.set_val(frame->capability_info_.val());
    scan_state_.opts->on_result_fn(scan_result);
    // TODO(fxb/49350): Channel switch during scanning need to be supported.
  } else if (assoc_state_.state == AssocState::ASSOCIATED &&
             frame->bssid_ == assoc_state_.opts->bssid) {
    // if we're associated with this AP, start/restart the beacon watchdog
    RestartBeaconWatchdog();

    auto ie = frame->FindIE(simulation::InformationElement::IE_TYPE_CSA);
    if (ie) {
      // If CSA IE exist.
      auto csa_ie = static_cast<simulation::CSAInformationElement*>(ie.get());

      // Get current chanspec of client ifidx and convert to channel.
      wlan_channel_t channel = GetIfChannel(false);

      zx::duration SwitchDelay = frame->interval_ * (int64_t)csa_ie->channel_switch_count_;

      if (channel_switch_state_.state == ChannelSwitchState::HOME) {
        // If the destination channel is the same as current channel, just ignore it.
        if (csa_ie->new_channel_number_ == channel.primary) {
          return;
        }

        channel.primary = csa_ie->new_channel_number_;
        channel_switch_state_.new_channel = csa_ie->new_channel_number_;

        channel_switch_state_.state = ChannelSwitchState::SWITCHING;
      } else {
        ZX_ASSERT(channel_switch_state_.state == ChannelSwitchState::SWITCHING);
        if (csa_ie->new_channel_number_ == channel_switch_state_.new_channel) {
          return;
        }

        // If the new channel is different from the previous dst channel, cancel callback.
        hw_.CancelCallback(channel_switch_state_.switch_timer_id);

        // If it's the same as current channel for this client before switching, just simply cancel
        // the switch event and clear state.
        if (csa_ie->new_channel_number_ == channel.primary) {
          channel_switch_state_.state = ChannelSwitchState::HOME;
          return;
        }

        // Schedule a new event when dst channel change.
        channel.primary = csa_ie->new_channel_number_;
      }

      auto callback = new std::function<void()>;
      *callback = std::bind(&SimFirmware::ConductChannelSwitch, this, channel,
                            csa_ie->channel_switch_mode_);
      hw_.RequestCallback(callback, SwitchDelay, &channel_switch_state_.switch_timer_id);
    }
  }
}

void SimFirmware::RxProbeResp(const wlan_channel_t& channel,
                              const simulation::SimProbeRespFrame* frame, double signal_strength) {
  if (scan_state_.state != ScanState::SCANNING || !scan_state_.opts->is_active) {
    return;
  }

  // truncate signal strength to rssi unit
  int8_t rssi_dbm;
  if (signal_strength > INT8_MAX) {
    rssi_dbm = INT8_MAX;
  } else if (signal_strength < INT8_MIN) {
    rssi_dbm = INT8_MIN;
  } else {
    rssi_dbm = signal_strength;
  }

  ScanResult scan_result = {
      .channel = channel, .ssid = frame->ssid_, .bssid = frame->src_addr_, .rssi_dbm = rssi_dbm};

  scan_result.bss_capability.set_val(frame->capability_info_.val());
  scan_state_.opts->on_result_fn(scan_result);
}

// Handle an Rx Beacon sent to us from the hardware, using it to fill in all of the fields in a
// brcmf_escan_result.
void SimFirmware::EscanResultSeen(const ScanResult& result_in) {
  // For now, the only IE we will include will be for the SSID
  size_t ssid_ie_size = 2 + result_in.ssid.len;

  // scan_result_size includes all BSS info structures (each including IEs). We (like the firmware)
  // only send one result back at a time.
  size_t scan_result_size = roundup(sizeof(brcmf_escan_result_le) + ssid_ie_size, 4);

  auto buf = std::make_unique<std::vector<uint8_t>>(scan_result_size);

  uint8_t* buffer_data = buf->data();
  auto result_out = reinterpret_cast<brcmf_escan_result_le*>(buffer_data);
  result_out->buflen = scan_result_size;
  result_out->version = BRCMF_BSS_INFO_VERSION;
  result_out->sync_id = scan_state_.opts->sync_id;
  result_out->bss_count = 1;

  struct brcmf_bss_info_le* bss_info = &result_out->bss_info_le;
  bss_info->version = BRCMF_BSS_INFO_VERSION;

  // length of this record (includes IEs)
  bss_info->length = roundup(sizeof(brcmf_bss_info_le) + ssid_ie_size, 4);
  // channel
  bss_info->chanspec = channel_to_chanspec(&d11_inf_, &result_in.channel);
  // capability
  bss_info->capability = result_in.bss_capability.val();

  // ssid
  ZX_ASSERT(sizeof(bss_info->SSID) == sizeof(result_in.ssid.ssid));
  ZX_ASSERT(result_in.ssid.len <= sizeof(result_in.ssid.ssid));
  bss_info->SSID_len = 0;  // SSID will go into an IE

  // bssid
  ZX_ASSERT(sizeof(bss_info->BSSID) == common::kMacAddrLen);
  memcpy(bss_info->BSSID, result_in.bssid.byte, common::kMacAddrLen);

  // RSSI
  bss_info->RSSI = result_in.rssi_dbm;

  // IEs
  bss_info->ie_offset = sizeof(brcmf_bss_info_le);

  // IE: SSID
  size_t ie_offset = sizeof(brcmf_escan_result_le);
  size_t ie_len = 0;
  uint8_t* ie_data = &buffer_data[ie_offset];
  ie_data[ie_len++] = IEEE80211_ASSOC_TAG_SSID;
  ie_data[ie_len++] = result_in.ssid.len;
  memcpy(&ie_data[ie_len], result_in.ssid.ssid, result_in.ssid.len);
  ie_len += result_in.ssid.len;

  bss_info->ie_length = ie_len;

  // Wrap this in an event and send it back to the driver
  SendEventToDriver(scan_result_size, std::move(buf), BRCMF_E_ESCAN_RESULT, BRCMF_E_STATUS_PARTIAL,
                    scan_state_.ifidx);
}

std::unique_ptr<std::vector<uint8_t>> SimFirmware::CreateEventBuffer(
    size_t requested_size, brcmf_event_msg_be** msg_out_be, size_t* payload_offset_out) {
  size_t total_size = sizeof(brcmf_event) + requested_size;
  size_t event_data_offset;
  auto buf = CreateBcdcBuffer(total_size, &event_data_offset);

  uint8_t* buffer_data = buf->data();
  auto event = reinterpret_cast<brcmf_event*>(&buffer_data[event_data_offset]);

  memcpy(event->eth.h_dest, mac_addr_.data(), ETH_ALEN);
  memcpy(event->eth.h_source, mac_addr_.data(), ETH_ALEN);

  // Disable local bit - we do this because, well, the real firmware does this.
  event->eth.h_source[0] &= ~0x2;
  event->eth.h_proto = htobe16(ETH_P_LINK_CTL);

  auto hdr_be = &event->hdr;
  // hdr_be->subtype unused
  hdr_be->length = htobe16(total_size);
  hdr_be->version = 0;
  memcpy(&hdr_be->oui, BRCM_OUI, sizeof(hdr_be->oui));
  hdr_be->usr_subtype = htobe16(BCMILCP_BCM_SUBTYPE_EVENT);

  // Set the generic fields of the event msg
  *msg_out_be = &event->msg;
  (*msg_out_be)->version = htobe16(2);
  (*msg_out_be)->datalen = htobe32(requested_size);
  memcpy((*msg_out_be)->addr, mac_addr_.data(), ETH_ALEN);
  memcpy((*msg_out_be)->ifname, kDefaultIfcName, strlen(kDefaultIfcName));

  // Payload immediately follows the brcmf_event structure
  if (payload_offset_out != nullptr) {
    *payload_offset_out = event_data_offset + sizeof(brcmf_event);
  }

  return buf;
}

void SimFirmware::SendEventToDriver(size_t payload_size,
                                    std::unique_ptr<std::vector<uint8_t>> buffer_in,
                                    uint32_t event_type, uint32_t status, uint16_t ifidx,
                                    char* ifname, uint16_t flags, uint32_t reason,
                                    std::optional<common::MacAddr> addr) {
  brcmf_event_msg_be* msg_be;
  size_t payload_offset;
  // Assert if ifidx is not valid
  if (event_type != BRCMF_E_IF)
    ZX_ASSERT(ifidx < kMaxIfSupported && iface_tbl_[ifidx].allocated);
  auto buf = CreateEventBuffer(payload_size, &msg_be, &payload_offset);
  msg_be->flags = htobe16(flags);
  msg_be->event_type = htobe32(event_type);
  msg_be->status = htobe32(status);
  msg_be->reason = htobe32(reason);
  msg_be->ifidx = ifidx;
  msg_be->bsscfgidx = iface_tbl_[ifidx].bsscfgidx;

  if (ifname)
    memcpy(msg_be->ifname, ifname, IFNAMSIZ);

  if (addr)
    memcpy(msg_be->addr, addr->byte, ETH_ALEN);

  if (payload_size != 0) {
    ZX_ASSERT(buffer_in != nullptr);
    uint8_t* buf_data = buf->data();
    memcpy(&buf_data[payload_offset], buffer_in->data(), payload_size);
  }

  brcmf_sim_rx_event(simdev_, std::move(buf));
}
}  // namespace wlan::brcmfmac
