// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MEDIA_AUDIO_DRIVERS_GAUSS_TDM_TAS57XX_H_
#define SRC_MEDIA_AUDIO_DRIVERS_GAUSS_TDM_TAS57XX_H_

#include <memory>

#include <ddk/debug.h>
#include <ddk/protocol/i2c.h>

namespace audio {
namespace gauss {

class Tas57xx : public std::unique_ptr<Tas57xx> {
 public:
  static std::unique_ptr<Tas57xx> Create(i2c_protocol_t *i2c, uint32_t index);
  bool ValidGain(float gain);
  zx_status_t SetGain(float gain);
  zx_status_t GetGain(float *gain);
  zx_status_t Init(uint8_t slot);
  zx_status_t Reset();
  zx_status_t Standby();
  zx_status_t ExitStandby();

 private:
  friend class std::default_delete<Tas57xx>;
  static constexpr float kMaxGain = 24.0;
  static constexpr float kMinGain = -103.0;
  Tas57xx();
  ~Tas57xx();

  zx_status_t WriteReg(uint8_t reg, uint8_t value);

  zx_status_t SetStandby(bool stdby);

  i2c_protocol_t i2c_;

  float current_gain_ = 0;
};
}  // namespace gauss
}  // namespace audio

#endif  // SRC_MEDIA_AUDIO_DRIVERS_GAUSS_TDM_TAS57XX_H_
