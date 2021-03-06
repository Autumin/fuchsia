// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/fidlcat/lib/syscall_decoder_dispatcher.h"

#include <zircon/system/public/zircon/errors.h>
#include <zircon/system/public/zircon/types.h>

#include <cstdint>
#include <memory>
#include <sstream>

#include "src/developer/debug/zxdb/client/process.h"
#include "src/developer/debug/zxdb/client/thread.h"
#include "tools/fidlcat/lib/inference.h"
#include "tools/fidlcat/lib/syscall_decoder.h"

namespace fidlcat {

void DisplayString(const fidl_codec::Colors& colors, const char* string, size_t size,
                   std::ostream& os) {
  if (string == nullptr) {
    os << "nullptr\n";
  } else {
    if (size == 0) {
      os << "empty\n";
    } else {
      os << colors.red << '"';
      for (size_t i = 0; i < size; ++i) {
        char value = string[i];
        switch (value) {
          case 0:
            break;
          case '\\':
            os << "\\\\";
            break;
          case '\n':
            os << "\\n";
            break;
          default:
            os << value;
            break;
        }
      }
      os << '"' << colors.reset;
    }
  }
}

std::unique_ptr<fidl_codec::Type> AccessBase::ComputeType() const {
  switch (GetSyscallType()) {
    case SyscallType::kBool:
      return std::make_unique<fidl_codec::BoolType>();
    case SyscallType::kInt32:
      return std::make_unique<fidl_codec::Int32Type>();
    case SyscallType::kInt64:
      return std::make_unique<fidl_codec::Int64Type>();
    case SyscallType::kUint8:
      return std::make_unique<fidl_codec::Uint8Type>();
    case SyscallType::kUint8Hexa:
      return std::make_unique<fidl_codec::Uint8Type>(fidl_codec::Uint8Type::Kind::kHexaDecimal);
    case SyscallType::kUint16:
      return std::make_unique<fidl_codec::Uint16Type>();
    case SyscallType::kUint16Hexa:
      return std::make_unique<fidl_codec::Uint16Type>(fidl_codec::Uint16Type::Kind::kHexaDecimal);
    case SyscallType::kUint32:
      return std::make_unique<fidl_codec::Uint32Type>();
    case SyscallType::kUint32Hexa:
      return std::make_unique<fidl_codec::Uint32Type>(fidl_codec::Uint32Type::Kind::kHexaDecimal);
    case SyscallType::kUint64:
      return std::make_unique<fidl_codec::Uint64Type>();
    case SyscallType::kUint64Hexa:
      return std::make_unique<fidl_codec::Uint64Type>(fidl_codec::Uint64Type::Kind::kHexaDecimal);
    case SyscallType::kHandle:
      return std::make_unique<fidl_codec::HandleType>();
    case SyscallType::kTime:
      return std::make_unique<fidl_codec::Int64Type>(fidl_codec::Int64Type::Kind::kTime);
    default:
      return nullptr;
  }
}

std::unique_ptr<fidl_codec::Type> SyscallInputOutputBase::ComputeType() const { return nullptr; }

std::unique_ptr<fidl_codec::Value> SyscallInputOutputBase::GenerateValue(SyscallDecoder* decoder,
                                                                         Stage stage) const {
  return std::make_unique<fidl_codec::InvalidValue>();
}

void SyscallInputOutputStringBuffer::DisplayOutline(SyscallDisplayDispatcher* dispatcher,
                                                    SyscallDecoder* decoder, Stage stage,
                                                    std::string_view line_header, int tabs,
                                                    std::ostream& os) const {
  os << line_header << std::string((tabs + 1) * fidl_codec::kTabSize, ' ') << name();
  const fidl_codec::Colors& colors = dispatcher->colors();
  os << ':' << colors.green << "string" << colors.reset << ": ";
  const char* const* buffer = buffer_->Content(decoder, stage);
  if (buffer == nullptr) {
    os << colors.red << "nullptr" << colors.reset;
  } else {
    uint32_t count = count_->Value(decoder, stage);
    if (count == 0) {
      os << "empty\n";
      return;
    }
    const char* separator = "";
    for (uint32_t i = 0; i < count; ++i) {
      if (buffer[i] != nullptr) {
        os << separator;
        const char* string = reinterpret_cast<const char*>(
            decoder->BufferContent(stage, reinterpret_cast<uint64_t>(buffer[i])));
        size_t string_size = (string == nullptr) ? 0 : strnlen(string, max_size_);
        DisplayString(colors, string, string_size, os);
        separator = ", ";
      }
    }
  }
  os << '\n';
}

const char* SyscallInputOutputFixedSizeString::DisplayInline(SyscallDisplayDispatcher* dispatcher,
                                                             SyscallDecoder* decoder, Stage stage,
                                                             const char* separator,
                                                             std::ostream& os) const {
  const fidl_codec::Colors& colors = dispatcher->colors();
  os << separator;
  os << name() << ':' << colors.green << "string" << colors.reset << ": ";
  const char* string = string_->Content(decoder, stage);
  size_t string_size = (string == nullptr) ? 0 : strnlen(string, string_size_);
  DisplayString(colors, string, string_size, os);
  return ", ";
}

std::unique_ptr<fidl_codec::Type> SyscallFidlMessageHandle::ComputeType() const {
  return std::make_unique<fidl_codec::FidlMessageType>();
}

std::unique_ptr<fidl_codec::Value> SyscallFidlMessageHandle::GenerateValue(SyscallDecoder* decoder,
                                                                           Stage stage) const {
  zx_handle_t handle_value = handle()->Value(decoder, stage);
  const uint8_t* bytes_value = bytes()->Content(decoder, stage);
  uint32_t num_bytes_value = num_bytes()->Value(decoder, stage);
  const zx_handle_t* handles_value = handles()->Content(decoder, stage);
  uint32_t num_handles_value = num_handles()->Value(decoder, stage);
  zx_handle_info_t* handle_infos_value = nullptr;
  if (num_handles_value > 0) {
    handle_infos_value = new zx_handle_info_t[num_handles_value];
    for (uint32_t i = 0; i < num_handles_value; ++i) {
      handle_infos_value[i].handle = handles_value[i];
      handle_infos_value[i].type = ZX_OBJ_TYPE_NONE;
      handle_infos_value[i].rights = 0;
    }
  }
  fidl_codec::DecodedMessage message;
  std::stringstream error_stream;
  message.DecodeMessage(decoder->dispatcher()->MessageDecoderDispatcher(), decoder->process_id(),
                        handle_value, bytes_value, num_bytes_value, handle_infos_value,
                        num_handles_value, type(), error_stream);
  auto result = std::make_unique<fidl_codec::FidlMessageValue>(
      &message, error_stream.str(), bytes_value, num_bytes_value, handle_infos_value,
      num_handles_value);
  delete[] handle_infos_value;
  if (result->is_request()) {
    if (result->matched_request()) {
      decoder->set_semantic(result->method()->semantic());
      decoder->set_decoded_request(result->decoded_request());
    }
    if (result->matched_response()) {
      decoder->set_semantic(result->method()->semantic());
      decoder->set_decoded_response(result->decoded_response());
    }
  }
  return result;
}

std::unique_ptr<fidl_codec::Type> SyscallFidlMessageHandleInfo::ComputeType() const {
  return std::make_unique<fidl_codec::FidlMessageType>();
}

std::unique_ptr<fidl_codec::Value> SyscallFidlMessageHandleInfo::GenerateValue(
    SyscallDecoder* decoder, Stage stage) const {
  zx_handle_t handle_value = handle()->Value(decoder, stage);
  const uint8_t* bytes_value = bytes()->Content(decoder, stage);
  uint32_t num_bytes_value = num_bytes()->Value(decoder, stage);
  const zx_handle_info_t* handle_infos_value = handles()->Content(decoder, stage);
  uint32_t num_handles_value = num_handles()->Value(decoder, stage);
  fidl_codec::DecodedMessage message;
  std::stringstream error_stream;
  message.DecodeMessage(decoder->dispatcher()->MessageDecoderDispatcher(), decoder->process_id(),
                        handle_value, bytes_value, num_bytes_value, handle_infos_value,
                        num_handles_value, type(), error_stream);
  auto result = std::make_unique<fidl_codec::FidlMessageValue>(
      &message, error_stream.str(), bytes_value, num_bytes_value, handle_infos_value,
      num_handles_value);
  if (result->is_request()) {
    if (result->matched_request()) {
      decoder->set_semantic(result->method()->semantic());
      decoder->set_decoded_request(result->decoded_request());
    }
    if (result->matched_response()) {
      decoder->set_semantic(result->method()->semantic());
      decoder->set_decoded_response(result->decoded_response());
    }
  }
  return result;
}

bool ComputeTypes(const std::vector<std::unique_ptr<SyscallInputOutputBase>>& fields,
                  std::vector<std::unique_ptr<fidl_codec::StructMember>>* inline_members,
                  std::vector<std::unique_ptr<fidl_codec::StructMember>>* outline_members) {
  for (const auto& field : fields) {
    std::unique_ptr<fidl_codec::Type> type = field->ComputeType();
    if (type == nullptr) {
      return false;
    }
    if (field->InlineValue()) {
      inline_members->emplace_back(
          std::make_unique<fidl_codec::StructMember>(field->name(), std::move(type)));
    } else {
      outline_members->emplace_back(
          std::make_unique<fidl_codec::StructMember>(field->name(), std::move(type)));
    }
  }
  return true;
}

void Syscall::ComputeTypes() {
  fidl_codec_values_ready_ = true;
  if (!fidlcat::ComputeTypes(inputs_, &input_inline_members_, &input_outline_members_)) {
    fidl_codec_values_ready_ = false;
    return;
  }
  if (!fidlcat::ComputeTypes(outputs_, &output_inline_members_, &output_outline_members_)) {
    fidl_codec_values_ready_ = false;
    return;
  }
}

void SyscallDecoderDispatcher::DecodeSyscall(InterceptingThreadObserver* thread_observer,
                                             zxdb::Thread* thread, Syscall* syscall) {
  uint64_t thread_id = thread->GetKoid();
  auto current = syscall_decoders_.find(thread_id);
  if (current != syscall_decoders_.end()) {
    FXL_LOG(ERROR) << thread->GetProcess()->GetName() << ' ' << thread->GetProcess()->GetKoid()
                   << ':' << thread_id << ": Internal error: already decoding the thread";
    return;
  }
  auto decoder = CreateDecoder(thread_observer, thread, syscall);
  auto tmp = decoder.get();
  syscall_decoders_[thread_id] = std::move(decoder);
  tmp->Decode();
}

void SyscallDecoderDispatcher::DecodeException(InterceptionWorkflow* workflow,
                                               zxdb::Thread* thread) {
  uint64_t thread_id = thread->GetKoid();
  auto current = exception_decoders_.find(thread_id);
  if (current != exception_decoders_.end()) {
    FXL_LOG(ERROR) << thread->GetProcess()->GetName() << ' ' << thread->GetProcess()->GetKoid()
                   << ':' << thread_id
                   << ": Internal error: already decoding an exception for the thread";
    return;
  }
  auto decoder = CreateDecoder(workflow, thread);
  auto tmp = decoder.get();
  exception_decoders_[thread_id] = std::move(decoder);
  tmp->Decode();
}

void SyscallDecoderDispatcher::DeleteDecoder(SyscallDecoder* decoder) {
  if (!decoder->aborted()) {
    zxdb::Thread* thread = decoder->get_thread();
    if (thread != nullptr) {
      thread->Continue();
    }
  }
  syscall_decoders_.erase(decoder->thread_id());
}

void SyscallDecoderDispatcher::DeleteDecoder(ExceptionDecoder* decoder) {
  zxdb::Thread* thread = decoder->get_thread();
  if (thread != nullptr) {
    thread->Continue();
  }
  exception_decoders_.erase(decoder->thread_id());
}

void SyscallDecoderDispatcher::ProcessMonitored(std::string_view name, zx_koid_t koid,
                                                std::string_view error_message) {
  if (!error_message.empty()) {
    return;
  }
  auto process = processes_.find(koid);
  if (process == processes_.end()) {
    processes_.emplace(std::make_pair(koid, std::make_unique<Process>(name, koid)));
  }
}

void SyscallDecoderDispatcher::StopMonitoring(zx_koid_t koid) {
  for (const auto& decoder : syscall_decoders_) {
    if (decoder.second->process_id() == koid) {
      decoder.second->set_aborted();
    }
  }
}

void SyscallDecoderDispatcher::ComputeTypes() {
  for (const auto& syscall : syscalls_) {
    syscall->ComputeTypes();
  }
}

std::unique_ptr<SyscallDecoder> SyscallDisplayDispatcher::CreateDecoder(
    InterceptingThreadObserver* thread_observer, zxdb::Thread* thread, const Syscall* syscall) {
  return std::make_unique<SyscallDecoder>(this, thread_observer, thread, syscall,
                                          std::make_unique<SyscallDisplay>(this, os_));
}

std::unique_ptr<ExceptionDecoder> SyscallDisplayDispatcher::CreateDecoder(
    InterceptionWorkflow* workflow, zxdb::Thread* thread) {
  return std::make_unique<ExceptionDecoder>(workflow, this, thread,
                                            std::make_unique<ExceptionDisplay>(this, os_));
}

void SyscallDisplayDispatcher::ProcessLaunched(const std::string& command,
                                               std::string_view error_message) {
  last_displayed_syscall_ = nullptr;
  if (error_message.empty()) {
    os_ << colors().green << "\nLaunched " << colors().blue << command << colors().reset << '\n';
  } else {
    os_ << colors().red << "\nCan't launch " << colors().blue << command << colors().reset << " : "
        << colors().red << error_message << colors().reset << '\n';
  }
}

void SyscallDisplayDispatcher::ProcessMonitored(std::string_view name, zx_koid_t koid,
                                                std::string_view error_message) {
  last_displayed_syscall_ = nullptr;
  if (error_message.empty()) {
    auto process = processes().find(koid);
    if (process == processes().end()) {
      os_ << colors().green << "\nMonitoring ";
    } else {
      os_ << colors().red << "\nAlready monitoring ";
    }
  } else {
    os_ << colors().red << "\nCan't monitor ";
  }

  if (name.empty()) {
    os_ << colors().reset << "process with koid ";
  } else {
    os_ << colors().blue << name << colors().reset << " koid=";
  }

  os_ << colors().red << koid << colors().reset;
  if (!error_message.empty()) {
    os_ << " : " << colors().red << error_message << colors().reset;
  }
  os_ << '\n';
  SyscallDecoderDispatcher::ProcessMonitored(name, koid, error_message);
}

void SyscallDisplayDispatcher::StopMonitoring(zx_koid_t koid) {
  last_displayed_syscall_ = nullptr;
  os_ << colors().green << "\nStop monitoring process with koid ";
  os_ << colors().red << koid << colors().reset << '\n';
  SyscallDecoderDispatcher::StopMonitoring(koid);
}

std::unique_ptr<SyscallDecoder> SyscallCompareDispatcher::CreateDecoder(
    InterceptingThreadObserver* thread_observer, zxdb::Thread* thread, const Syscall* syscall) {
  return std::make_unique<SyscallDecoder>(this, thread_observer, thread, syscall,
                                          std::make_unique<SyscallCompare>(this, comparator_, os_));
}

}  // namespace fidlcat
