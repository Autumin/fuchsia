// WARNING: This file is machine generated by fidlgen.

#include <fuchsia/hardware/skipblock/llcpp/fidl.h>
#include <memory>

namespace llcpp {

namespace fuchsia {
namespace hardware {
namespace skipblock {

namespace {

[[maybe_unused]]
constexpr uint64_t kSkipBlock_GetPartitionInfo_Ordinal = 0x61812f6e00000000lu;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_GetPartitionInfo_GenOrdinal = 0xf95442de92af21dlu;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockGetPartitionInfoRequestTable;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockGetPartitionInfoResponseTable;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_Read_Ordinal = 0x68e3be6200000000lu;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_Read_GenOrdinal = 0x28fdd003439e980lu;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockReadRequestTable;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockReadResponseTable;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_Write_Ordinal = 0x697d770a00000000lu;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_Write_GenOrdinal = 0x36b3b73dc0b576c8lu;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockWriteRequestTable;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockWriteResponseTable;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_WriteBytes_Ordinal = 0x4ce2ddf400000000lu;
[[maybe_unused]]
constexpr uint64_t kSkipBlock_WriteBytes_GenOrdinal = 0x6e1f96cd22fd1c2dlu;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockWriteBytesRequestTable;
extern "C" const fidl_type_t fuchsia_hardware_skipblock_SkipBlockWriteBytesResponseTable;

}  // namespace
template <>
SkipBlock::ResultOf::GetPartitionInfo_Impl<SkipBlock::GetPartitionInfoResponse>::GetPartitionInfo_Impl(zx::unowned_channel _client_end) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<GetPartitionInfoRequest, ::fidl::MessageDirection::kSending>();
  ::fidl::internal::AlignedBuffer<_kWriteAllocSize> _write_bytes_inlined;
  auto& _write_bytes_array = _write_bytes_inlined;
  uint8_t* _write_bytes = _write_bytes_array.view().data();
  memset(_write_bytes, 0, GetPartitionInfoRequest::PrimarySize);
  ::fidl::BytePart _request_bytes(_write_bytes, _kWriteAllocSize, sizeof(GetPartitionInfoRequest));
  ::fidl::DecodedMessage<GetPartitionInfoRequest> _decoded_request(std::move(_request_bytes));
  Super::SetResult(
      SkipBlock::InPlace::GetPartitionInfo(std::move(_client_end), Super::response_buffer()));
}

SkipBlock::ResultOf::GetPartitionInfo SkipBlock::SyncClient::GetPartitionInfo() {
  return ResultOf::GetPartitionInfo(zx::unowned_channel(this->channel_));
}

SkipBlock::ResultOf::GetPartitionInfo SkipBlock::Call::GetPartitionInfo(zx::unowned_channel _client_end) {
  return ResultOf::GetPartitionInfo(std::move(_client_end));
}

template <>
SkipBlock::UnownedResultOf::GetPartitionInfo_Impl<SkipBlock::GetPartitionInfoResponse>::GetPartitionInfo_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _response_buffer) {
  FIDL_ALIGNDECL uint8_t _write_bytes[sizeof(GetPartitionInfoRequest)] = {};
  ::fidl::BytePart _request_buffer(_write_bytes, sizeof(_write_bytes));
  memset(_request_buffer.data(), 0, GetPartitionInfoRequest::PrimarySize);
  _request_buffer.set_actual(sizeof(GetPartitionInfoRequest));
  ::fidl::DecodedMessage<GetPartitionInfoRequest> _decoded_request(std::move(_request_buffer));
  Super::SetResult(
      SkipBlock::InPlace::GetPartitionInfo(std::move(_client_end), std::move(_response_buffer)));
}

SkipBlock::UnownedResultOf::GetPartitionInfo SkipBlock::SyncClient::GetPartitionInfo(::fidl::BytePart _response_buffer) {
  return UnownedResultOf::GetPartitionInfo(zx::unowned_channel(this->channel_), std::move(_response_buffer));
}

SkipBlock::UnownedResultOf::GetPartitionInfo SkipBlock::Call::GetPartitionInfo(zx::unowned_channel _client_end, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::GetPartitionInfo(std::move(_client_end), std::move(_response_buffer));
}

::fidl::DecodeResult<SkipBlock::GetPartitionInfoResponse> SkipBlock::InPlace::GetPartitionInfo(zx::unowned_channel _client_end, ::fidl::BytePart response_buffer) {
  constexpr uint32_t _write_num_bytes = sizeof(GetPartitionInfoRequest);
  ::fidl::internal::AlignedBuffer<_write_num_bytes> _write_bytes;
  ::fidl::BytePart _request_buffer = _write_bytes.view();
  _request_buffer.set_actual(_write_num_bytes);
  ::fidl::DecodedMessage<GetPartitionInfoRequest> params(std::move(_request_buffer));
  SkipBlock::SetTransactionHeaderFor::GetPartitionInfoRequest(params);
  auto _encode_request_result = ::fidl::Encode(std::move(params));
  if (_encode_request_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::GetPartitionInfoResponse>::FromFailure(
        std::move(_encode_request_result));
  }
  auto _call_result = ::fidl::Call<GetPartitionInfoRequest, GetPartitionInfoResponse>(
    std::move(_client_end), std::move(_encode_request_result.message), std::move(response_buffer));
  if (_call_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::GetPartitionInfoResponse>::FromFailure(
        std::move(_call_result));
  }
  return ::fidl::Decode(std::move(_call_result.message));
}

template <>
SkipBlock::ResultOf::Read_Impl<SkipBlock::ReadResponse>::Read_Impl(zx::unowned_channel _client_end, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<ReadRequest, ::fidl::MessageDirection::kSending>();
  ::fidl::internal::AlignedBuffer<_kWriteAllocSize> _write_bytes_inlined;
  auto& _write_bytes_array = _write_bytes_inlined;
  uint8_t* _write_bytes = _write_bytes_array.view().data();
  memset(_write_bytes, 0, ReadRequest::PrimarySize);
  auto& _request = *reinterpret_cast<ReadRequest*>(_write_bytes);
  _request.op = std::move(op);
  ::fidl::BytePart _request_bytes(_write_bytes, _kWriteAllocSize, sizeof(ReadRequest));
  ::fidl::DecodedMessage<ReadRequest> _decoded_request(std::move(_request_bytes));
  Super::SetResult(
      SkipBlock::InPlace::Read(std::move(_client_end), std::move(_decoded_request), Super::response_buffer()));
}

SkipBlock::ResultOf::Read SkipBlock::SyncClient::Read(::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op) {
  return ResultOf::Read(zx::unowned_channel(this->channel_), std::move(op));
}

SkipBlock::ResultOf::Read SkipBlock::Call::Read(zx::unowned_channel _client_end, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op) {
  return ResultOf::Read(std::move(_client_end), std::move(op));
}

template <>
SkipBlock::UnownedResultOf::Read_Impl<SkipBlock::ReadResponse>::Read_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op, ::fidl::BytePart _response_buffer) {
  if (_request_buffer.capacity() < ReadRequest::PrimarySize) {
    Super::SetFailure(::fidl::DecodeResult<ReadResponse>(ZX_ERR_BUFFER_TOO_SMALL, ::fidl::internal::kErrorRequestBufferTooSmall));
    return;
  }
  memset(_request_buffer.data(), 0, ReadRequest::PrimarySize);
  auto& _request = *reinterpret_cast<ReadRequest*>(_request_buffer.data());
  _request.op = std::move(op);
  _request_buffer.set_actual(sizeof(ReadRequest));
  ::fidl::DecodedMessage<ReadRequest> _decoded_request(std::move(_request_buffer));
  Super::SetResult(
      SkipBlock::InPlace::Read(std::move(_client_end), std::move(_decoded_request), std::move(_response_buffer)));
}

SkipBlock::UnownedResultOf::Read SkipBlock::SyncClient::Read(::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::Read(zx::unowned_channel(this->channel_), std::move(_request_buffer), std::move(op), std::move(_response_buffer));
}

SkipBlock::UnownedResultOf::Read SkipBlock::Call::Read(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::Read(std::move(_client_end), std::move(_request_buffer), std::move(op), std::move(_response_buffer));
}

::fidl::DecodeResult<SkipBlock::ReadResponse> SkipBlock::InPlace::Read(zx::unowned_channel _client_end, ::fidl::DecodedMessage<ReadRequest> params, ::fidl::BytePart response_buffer) {
  SkipBlock::SetTransactionHeaderFor::ReadRequest(params);
  auto _encode_request_result = ::fidl::Encode(std::move(params));
  if (_encode_request_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::ReadResponse>::FromFailure(
        std::move(_encode_request_result));
  }
  auto _call_result = ::fidl::Call<ReadRequest, ReadResponse>(
    std::move(_client_end), std::move(_encode_request_result.message), std::move(response_buffer));
  if (_call_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::ReadResponse>::FromFailure(
        std::move(_call_result));
  }
  return ::fidl::Decode(std::move(_call_result.message));
}

template <>
SkipBlock::ResultOf::Write_Impl<SkipBlock::WriteResponse>::Write_Impl(zx::unowned_channel _client_end, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<WriteRequest, ::fidl::MessageDirection::kSending>();
  ::fidl::internal::AlignedBuffer<_kWriteAllocSize> _write_bytes_inlined;
  auto& _write_bytes_array = _write_bytes_inlined;
  uint8_t* _write_bytes = _write_bytes_array.view().data();
  memset(_write_bytes, 0, WriteRequest::PrimarySize);
  auto& _request = *reinterpret_cast<WriteRequest*>(_write_bytes);
  _request.op = std::move(op);
  ::fidl::BytePart _request_bytes(_write_bytes, _kWriteAllocSize, sizeof(WriteRequest));
  ::fidl::DecodedMessage<WriteRequest> _decoded_request(std::move(_request_bytes));
  Super::SetResult(
      SkipBlock::InPlace::Write(std::move(_client_end), std::move(_decoded_request), Super::response_buffer()));
}

SkipBlock::ResultOf::Write SkipBlock::SyncClient::Write(::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op) {
  return ResultOf::Write(zx::unowned_channel(this->channel_), std::move(op));
}

SkipBlock::ResultOf::Write SkipBlock::Call::Write(zx::unowned_channel _client_end, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op) {
  return ResultOf::Write(std::move(_client_end), std::move(op));
}

template <>
SkipBlock::UnownedResultOf::Write_Impl<SkipBlock::WriteResponse>::Write_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op, ::fidl::BytePart _response_buffer) {
  if (_request_buffer.capacity() < WriteRequest::PrimarySize) {
    Super::SetFailure(::fidl::DecodeResult<WriteResponse>(ZX_ERR_BUFFER_TOO_SMALL, ::fidl::internal::kErrorRequestBufferTooSmall));
    return;
  }
  memset(_request_buffer.data(), 0, WriteRequest::PrimarySize);
  auto& _request = *reinterpret_cast<WriteRequest*>(_request_buffer.data());
  _request.op = std::move(op);
  _request_buffer.set_actual(sizeof(WriteRequest));
  ::fidl::DecodedMessage<WriteRequest> _decoded_request(std::move(_request_buffer));
  Super::SetResult(
      SkipBlock::InPlace::Write(std::move(_client_end), std::move(_decoded_request), std::move(_response_buffer)));
}

SkipBlock::UnownedResultOf::Write SkipBlock::SyncClient::Write(::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::Write(zx::unowned_channel(this->channel_), std::move(_request_buffer), std::move(op), std::move(_response_buffer));
}

SkipBlock::UnownedResultOf::Write SkipBlock::Call::Write(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::ReadWriteOperation op, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::Write(std::move(_client_end), std::move(_request_buffer), std::move(op), std::move(_response_buffer));
}

::fidl::DecodeResult<SkipBlock::WriteResponse> SkipBlock::InPlace::Write(zx::unowned_channel _client_end, ::fidl::DecodedMessage<WriteRequest> params, ::fidl::BytePart response_buffer) {
  SkipBlock::SetTransactionHeaderFor::WriteRequest(params);
  auto _encode_request_result = ::fidl::Encode(std::move(params));
  if (_encode_request_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::WriteResponse>::FromFailure(
        std::move(_encode_request_result));
  }
  auto _call_result = ::fidl::Call<WriteRequest, WriteResponse>(
    std::move(_client_end), std::move(_encode_request_result.message), std::move(response_buffer));
  if (_call_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::WriteResponse>::FromFailure(
        std::move(_call_result));
  }
  return ::fidl::Decode(std::move(_call_result.message));
}

template <>
SkipBlock::ResultOf::WriteBytes_Impl<SkipBlock::WriteBytesResponse>::WriteBytes_Impl(zx::unowned_channel _client_end, ::llcpp::fuchsia::hardware::skipblock::WriteBytesOperation op) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<WriteBytesRequest, ::fidl::MessageDirection::kSending>();
  ::fidl::internal::AlignedBuffer<_kWriteAllocSize> _write_bytes_inlined;
  auto& _write_bytes_array = _write_bytes_inlined;
  uint8_t* _write_bytes = _write_bytes_array.view().data();
  memset(_write_bytes, 0, WriteBytesRequest::PrimarySize);
  auto& _request = *reinterpret_cast<WriteBytesRequest*>(_write_bytes);
  _request.op = std::move(op);
  ::fidl::BytePart _request_bytes(_write_bytes, _kWriteAllocSize, sizeof(WriteBytesRequest));
  ::fidl::DecodedMessage<WriteBytesRequest> _decoded_request(std::move(_request_bytes));
  Super::SetResult(
      SkipBlock::InPlace::WriteBytes(std::move(_client_end), std::move(_decoded_request), Super::response_buffer()));
}

SkipBlock::ResultOf::WriteBytes SkipBlock::SyncClient::WriteBytes(::llcpp::fuchsia::hardware::skipblock::WriteBytesOperation op) {
  return ResultOf::WriteBytes(zx::unowned_channel(this->channel_), std::move(op));
}

SkipBlock::ResultOf::WriteBytes SkipBlock::Call::WriteBytes(zx::unowned_channel _client_end, ::llcpp::fuchsia::hardware::skipblock::WriteBytesOperation op) {
  return ResultOf::WriteBytes(std::move(_client_end), std::move(op));
}

template <>
SkipBlock::UnownedResultOf::WriteBytes_Impl<SkipBlock::WriteBytesResponse>::WriteBytes_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::WriteBytesOperation op, ::fidl::BytePart _response_buffer) {
  if (_request_buffer.capacity() < WriteBytesRequest::PrimarySize) {
    Super::SetFailure(::fidl::DecodeResult<WriteBytesResponse>(ZX_ERR_BUFFER_TOO_SMALL, ::fidl::internal::kErrorRequestBufferTooSmall));
    return;
  }
  memset(_request_buffer.data(), 0, WriteBytesRequest::PrimarySize);
  auto& _request = *reinterpret_cast<WriteBytesRequest*>(_request_buffer.data());
  _request.op = std::move(op);
  _request_buffer.set_actual(sizeof(WriteBytesRequest));
  ::fidl::DecodedMessage<WriteBytesRequest> _decoded_request(std::move(_request_buffer));
  Super::SetResult(
      SkipBlock::InPlace::WriteBytes(std::move(_client_end), std::move(_decoded_request), std::move(_response_buffer)));
}

SkipBlock::UnownedResultOf::WriteBytes SkipBlock::SyncClient::WriteBytes(::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::WriteBytesOperation op, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::WriteBytes(zx::unowned_channel(this->channel_), std::move(_request_buffer), std::move(op), std::move(_response_buffer));
}

SkipBlock::UnownedResultOf::WriteBytes SkipBlock::Call::WriteBytes(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::llcpp::fuchsia::hardware::skipblock::WriteBytesOperation op, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::WriteBytes(std::move(_client_end), std::move(_request_buffer), std::move(op), std::move(_response_buffer));
}

::fidl::DecodeResult<SkipBlock::WriteBytesResponse> SkipBlock::InPlace::WriteBytes(zx::unowned_channel _client_end, ::fidl::DecodedMessage<WriteBytesRequest> params, ::fidl::BytePart response_buffer) {
  SkipBlock::SetTransactionHeaderFor::WriteBytesRequest(params);
  auto _encode_request_result = ::fidl::Encode(std::move(params));
  if (_encode_request_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::WriteBytesResponse>::FromFailure(
        std::move(_encode_request_result));
  }
  auto _call_result = ::fidl::Call<WriteBytesRequest, WriteBytesResponse>(
    std::move(_client_end), std::move(_encode_request_result.message), std::move(response_buffer));
  if (_call_result.status != ZX_OK) {
    return ::fidl::DecodeResult<SkipBlock::WriteBytesResponse>::FromFailure(
        std::move(_call_result));
  }
  return ::fidl::Decode(std::move(_call_result.message));
}


bool SkipBlock::TryDispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn) {
  if (msg->num_bytes < sizeof(fidl_message_header_t)) {
    zx_handle_close_many(msg->handles, msg->num_handles);
    txn->Close(ZX_ERR_INVALID_ARGS);
    return true;
  }
  fidl_message_header_t* hdr = reinterpret_cast<fidl_message_header_t*>(msg->bytes);
  switch (hdr->ordinal) {
    case kSkipBlock_GetPartitionInfo_Ordinal:
    case kSkipBlock_GetPartitionInfo_GenOrdinal:
    {
      auto result = ::fidl::DecodeAs<GetPartitionInfoRequest>(msg);
      if (result.status != ZX_OK) {
        txn->Close(ZX_ERR_INVALID_ARGS);
        return true;
      }
      impl->GetPartitionInfo(
        Interface::GetPartitionInfoCompleter::Sync(txn));
      return true;
    }
    case kSkipBlock_Read_Ordinal:
    case kSkipBlock_Read_GenOrdinal:
    {
      auto result = ::fidl::DecodeAs<ReadRequest>(msg);
      if (result.status != ZX_OK) {
        txn->Close(ZX_ERR_INVALID_ARGS);
        return true;
      }
      auto message = result.message.message();
      impl->Read(std::move(message->op),
        Interface::ReadCompleter::Sync(txn));
      return true;
    }
    case kSkipBlock_Write_Ordinal:
    case kSkipBlock_Write_GenOrdinal:
    {
      auto result = ::fidl::DecodeAs<WriteRequest>(msg);
      if (result.status != ZX_OK) {
        txn->Close(ZX_ERR_INVALID_ARGS);
        return true;
      }
      auto message = result.message.message();
      impl->Write(std::move(message->op),
        Interface::WriteCompleter::Sync(txn));
      return true;
    }
    case kSkipBlock_WriteBytes_Ordinal:
    case kSkipBlock_WriteBytes_GenOrdinal:
    {
      auto result = ::fidl::DecodeAs<WriteBytesRequest>(msg);
      if (result.status != ZX_OK) {
        txn->Close(ZX_ERR_INVALID_ARGS);
        return true;
      }
      auto message = result.message.message();
      impl->WriteBytes(std::move(message->op),
        Interface::WriteBytesCompleter::Sync(txn));
      return true;
    }
    default: {
      return false;
    }
  }
}

bool SkipBlock::Dispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn) {
  bool found = TryDispatch(impl, msg, txn);
  if (!found) {
    zx_handle_close_many(msg->handles, msg->num_handles);
    txn->Close(ZX_ERR_NOT_SUPPORTED);
  }
  return found;
}


void SkipBlock::Interface::GetPartitionInfoCompleterBase::Reply(int32_t status, ::llcpp::fuchsia::hardware::skipblock::PartitionInfo partition_info) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<GetPartitionInfoResponse, ::fidl::MessageDirection::kSending>();
  FIDL_ALIGNDECL uint8_t _write_bytes[_kWriteAllocSize] = {};
  auto& _response = *reinterpret_cast<GetPartitionInfoResponse*>(_write_bytes);
  SkipBlock::SetTransactionHeaderFor::GetPartitionInfoResponse(
      ::fidl::DecodedMessage<GetPartitionInfoResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              GetPartitionInfoResponse::PrimarySize,
              GetPartitionInfoResponse::PrimarySize)));
  _response.status = std::move(status);
  _response.partition_info = std::move(partition_info);
  ::fidl::BytePart _response_bytes(_write_bytes, _kWriteAllocSize, sizeof(GetPartitionInfoResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<GetPartitionInfoResponse>(std::move(_response_bytes)));
}

void SkipBlock::Interface::GetPartitionInfoCompleterBase::Reply(::fidl::BytePart _buffer, int32_t status, ::llcpp::fuchsia::hardware::skipblock::PartitionInfo partition_info) {
  if (_buffer.capacity() < GetPartitionInfoResponse::PrimarySize) {
    CompleterBase::Close(ZX_ERR_INTERNAL);
    return;
  }
  auto& _response = *reinterpret_cast<GetPartitionInfoResponse*>(_buffer.data());
  SkipBlock::SetTransactionHeaderFor::GetPartitionInfoResponse(
      ::fidl::DecodedMessage<GetPartitionInfoResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              GetPartitionInfoResponse::PrimarySize,
              GetPartitionInfoResponse::PrimarySize)));
  _response.status = std::move(status);
  _response.partition_info = std::move(partition_info);
  _buffer.set_actual(sizeof(GetPartitionInfoResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<GetPartitionInfoResponse>(std::move(_buffer)));
}

void SkipBlock::Interface::GetPartitionInfoCompleterBase::Reply(::fidl::DecodedMessage<GetPartitionInfoResponse> params) {
  SkipBlock::SetTransactionHeaderFor::GetPartitionInfoResponse(params);
  CompleterBase::SendReply(std::move(params));
}


void SkipBlock::Interface::ReadCompleterBase::Reply(int32_t status) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<ReadResponse, ::fidl::MessageDirection::kSending>();
  FIDL_ALIGNDECL uint8_t _write_bytes[_kWriteAllocSize] = {};
  auto& _response = *reinterpret_cast<ReadResponse*>(_write_bytes);
  SkipBlock::SetTransactionHeaderFor::ReadResponse(
      ::fidl::DecodedMessage<ReadResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              ReadResponse::PrimarySize,
              ReadResponse::PrimarySize)));
  _response.status = std::move(status);
  ::fidl::BytePart _response_bytes(_write_bytes, _kWriteAllocSize, sizeof(ReadResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<ReadResponse>(std::move(_response_bytes)));
}

void SkipBlock::Interface::ReadCompleterBase::Reply(::fidl::BytePart _buffer, int32_t status) {
  if (_buffer.capacity() < ReadResponse::PrimarySize) {
    CompleterBase::Close(ZX_ERR_INTERNAL);
    return;
  }
  auto& _response = *reinterpret_cast<ReadResponse*>(_buffer.data());
  SkipBlock::SetTransactionHeaderFor::ReadResponse(
      ::fidl::DecodedMessage<ReadResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              ReadResponse::PrimarySize,
              ReadResponse::PrimarySize)));
  _response.status = std::move(status);
  _buffer.set_actual(sizeof(ReadResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<ReadResponse>(std::move(_buffer)));
}

void SkipBlock::Interface::ReadCompleterBase::Reply(::fidl::DecodedMessage<ReadResponse> params) {
  SkipBlock::SetTransactionHeaderFor::ReadResponse(params);
  CompleterBase::SendReply(std::move(params));
}


void SkipBlock::Interface::WriteCompleterBase::Reply(int32_t status, bool bad_block_grown) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<WriteResponse, ::fidl::MessageDirection::kSending>();
  FIDL_ALIGNDECL uint8_t _write_bytes[_kWriteAllocSize] = {};
  auto& _response = *reinterpret_cast<WriteResponse*>(_write_bytes);
  SkipBlock::SetTransactionHeaderFor::WriteResponse(
      ::fidl::DecodedMessage<WriteResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              WriteResponse::PrimarySize,
              WriteResponse::PrimarySize)));
  _response.status = std::move(status);
  _response.bad_block_grown = std::move(bad_block_grown);
  ::fidl::BytePart _response_bytes(_write_bytes, _kWriteAllocSize, sizeof(WriteResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<WriteResponse>(std::move(_response_bytes)));
}

void SkipBlock::Interface::WriteCompleterBase::Reply(::fidl::BytePart _buffer, int32_t status, bool bad_block_grown) {
  if (_buffer.capacity() < WriteResponse::PrimarySize) {
    CompleterBase::Close(ZX_ERR_INTERNAL);
    return;
  }
  auto& _response = *reinterpret_cast<WriteResponse*>(_buffer.data());
  SkipBlock::SetTransactionHeaderFor::WriteResponse(
      ::fidl::DecodedMessage<WriteResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              WriteResponse::PrimarySize,
              WriteResponse::PrimarySize)));
  _response.status = std::move(status);
  _response.bad_block_grown = std::move(bad_block_grown);
  _buffer.set_actual(sizeof(WriteResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<WriteResponse>(std::move(_buffer)));
}

void SkipBlock::Interface::WriteCompleterBase::Reply(::fidl::DecodedMessage<WriteResponse> params) {
  SkipBlock::SetTransactionHeaderFor::WriteResponse(params);
  CompleterBase::SendReply(std::move(params));
}


void SkipBlock::Interface::WriteBytesCompleterBase::Reply(int32_t status, bool bad_block_grown) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<WriteBytesResponse, ::fidl::MessageDirection::kSending>();
  FIDL_ALIGNDECL uint8_t _write_bytes[_kWriteAllocSize] = {};
  auto& _response = *reinterpret_cast<WriteBytesResponse*>(_write_bytes);
  SkipBlock::SetTransactionHeaderFor::WriteBytesResponse(
      ::fidl::DecodedMessage<WriteBytesResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              WriteBytesResponse::PrimarySize,
              WriteBytesResponse::PrimarySize)));
  _response.status = std::move(status);
  _response.bad_block_grown = std::move(bad_block_grown);
  ::fidl::BytePart _response_bytes(_write_bytes, _kWriteAllocSize, sizeof(WriteBytesResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<WriteBytesResponse>(std::move(_response_bytes)));
}

void SkipBlock::Interface::WriteBytesCompleterBase::Reply(::fidl::BytePart _buffer, int32_t status, bool bad_block_grown) {
  if (_buffer.capacity() < WriteBytesResponse::PrimarySize) {
    CompleterBase::Close(ZX_ERR_INTERNAL);
    return;
  }
  auto& _response = *reinterpret_cast<WriteBytesResponse*>(_buffer.data());
  SkipBlock::SetTransactionHeaderFor::WriteBytesResponse(
      ::fidl::DecodedMessage<WriteBytesResponse>(
          ::fidl::BytePart(reinterpret_cast<uint8_t*>(&_response),
              WriteBytesResponse::PrimarySize,
              WriteBytesResponse::PrimarySize)));
  _response.status = std::move(status);
  _response.bad_block_grown = std::move(bad_block_grown);
  _buffer.set_actual(sizeof(WriteBytesResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<WriteBytesResponse>(std::move(_buffer)));
}

void SkipBlock::Interface::WriteBytesCompleterBase::Reply(::fidl::DecodedMessage<WriteBytesResponse> params) {
  SkipBlock::SetTransactionHeaderFor::WriteBytesResponse(params);
  CompleterBase::SendReply(std::move(params));
}



void SkipBlock::SetTransactionHeaderFor::GetPartitionInfoRequest(const ::fidl::DecodedMessage<SkipBlock::GetPartitionInfoRequest>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_GetPartitionInfo_Ordinal);
}
void SkipBlock::SetTransactionHeaderFor::GetPartitionInfoResponse(const ::fidl::DecodedMessage<SkipBlock::GetPartitionInfoResponse>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_GetPartitionInfo_Ordinal);
}

void SkipBlock::SetTransactionHeaderFor::ReadRequest(const ::fidl::DecodedMessage<SkipBlock::ReadRequest>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_Read_Ordinal);
}
void SkipBlock::SetTransactionHeaderFor::ReadResponse(const ::fidl::DecodedMessage<SkipBlock::ReadResponse>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_Read_Ordinal);
}

void SkipBlock::SetTransactionHeaderFor::WriteRequest(const ::fidl::DecodedMessage<SkipBlock::WriteRequest>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_Write_Ordinal);
}
void SkipBlock::SetTransactionHeaderFor::WriteResponse(const ::fidl::DecodedMessage<SkipBlock::WriteResponse>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_Write_Ordinal);
}

void SkipBlock::SetTransactionHeaderFor::WriteBytesRequest(const ::fidl::DecodedMessage<SkipBlock::WriteBytesRequest>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_WriteBytes_Ordinal);
}
void SkipBlock::SetTransactionHeaderFor::WriteBytesResponse(const ::fidl::DecodedMessage<SkipBlock::WriteBytesResponse>& _msg) {
  fidl_init_txn_header(&_msg.message()->_hdr, 0, kSkipBlock_WriteBytes_Ordinal);
}

}  // namespace skipblock
}  // namespace hardware
}  // namespace fuchsia
}  // namespace llcpp
