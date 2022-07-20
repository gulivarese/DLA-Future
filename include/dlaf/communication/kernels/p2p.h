//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2022, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//

#pragma once

/// @file

#include <mpi.h>

#include "dlaf/common/callable_object.h"
#include "dlaf/common/pipeline.h"
#include "dlaf/communication/communicator.h"
#include "dlaf/communication/message.h"
#include "dlaf/communication/rdma.h"
#include "dlaf/matrix/tile.h"
#include "dlaf/sender/traits.h"
#include "dlaf/sender/transform_mpi.h"
#include "dlaf/sender/when_all_lift.h"
#include "dlaf/sender/with_temporary_tile.h"

namespace dlaf::comm {

namespace internal {

// Non-blocking point to point send
template <class T, Device D>
void send(const Communicator& comm, IndexT_MPI dest, IndexT_MPI tag,
          const matrix::Tile<const T, D>& tile, MPI_Request* req) {
#if !defined(DLAF_WITH_CUDA_RDMA)
  static_assert(D == Device::CPU, "DLAF_WITH_CUDA_RDMA=off, MPI accepts just CPU memory.");
#endif

  auto msg = comm::make_message(common::make_data(tile));
  DLAF_MPI_CHECK_ERROR(
      MPI_Isend(const_cast<T*>(msg.data()), msg.count(), msg.mpi_type(), dest, tag, comm, req));
}

DLAF_MAKE_CALLABLE_OBJECT(send);

// Non-blocking point to point receive
template <class T, Device D>
auto recv(const Communicator& comm, IndexT_MPI source, IndexT_MPI tag, const matrix::Tile<T, D>& tile,
          MPI_Request* req) {
#if !defined(DLAF_WITH_CUDA_RDMA)
  static_assert(D == Device::CPU, "DLAF_WITH_CUDA_RDMA=off, MPI accepts just CPU memory.");
#endif

  auto msg = comm::make_message(common::make_data(tile));
  DLAF_MPI_CHECK_ERROR(MPI_Irecv(msg.data(), msg.count(), msg.mpi_type(), source, tag, comm, req));
}

DLAF_MAKE_CALLABLE_OBJECT(recv);
}

template <class CommSender, class Sender>
[[nodiscard]] auto scheduleSend(CommSender&& pcomm, IndexT_MPI dest, IndexT_MPI tag, Sender&& tile) {
  using dlaf::comm::internal::send_o;
  using dlaf::comm::internal::transformMPI;
  using dlaf::internal::CopyFromDestination;
  using dlaf::internal::CopyToDestination;
  using dlaf::internal::RequireContiguous;
  using dlaf::internal::SenderSingleValueType;
  using dlaf::internal::whenAllLift;
  using dlaf::internal::withTemporaryTile;

  auto recv = [dest, tag, pcomm = std::forward<CommSender>(pcomm)](auto const& tile_comm) mutable {
    return whenAllLift(std::move(pcomm), dest, tag, std::cref(tile_comm)) | transformMPI(send_o);
  };

  constexpr Device in_device_type = SenderSingleValueType<std::decay_t<Sender>>::D;
  constexpr Device comm_device_type = CommunicationDevice_v<in_device_type>;

  return withTemporaryTile<comm_device_type, CopyToDestination::No, CopyFromDestination::Yes,
                           RequireContiguous::No>(std::forward<Sender>(tile), std::move(recv));
}

template <class CommSender, class Sender>
[[nodiscard]] auto scheduleRecv(CommSender&& pcomm, IndexT_MPI source, IndexT_MPI tag, Sender&& tile) {
  using dlaf::comm::internal::recv_o;
  using dlaf::comm::internal::transformMPI;
  using dlaf::internal::CopyFromDestination;
  using dlaf::internal::CopyToDestination;
  using dlaf::internal::RequireContiguous;
  using dlaf::internal::SenderSingleValueType;
  using dlaf::internal::whenAllLift;
  using dlaf::internal::withTemporaryTile;

  auto recv = [source, tag, pcomm = std::forward<CommSender>(pcomm)](auto const& tile_comm) mutable {
    return whenAllLift(std::move(pcomm), source, tag, std::cref(tile_comm)) | transformMPI(recv_o);
  };

  constexpr Device in_device_type = SenderSingleValueType<std::decay_t<Sender>>::D;
  constexpr Device comm_device_type = CommunicationDevice_v<in_device_type>;

  return withTemporaryTile<comm_device_type, CopyToDestination::No, CopyFromDestination::Yes,
                           RequireContiguous::No>(std::forward<Sender>(tile), std::move(recv));
}
}