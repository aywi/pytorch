#include <torch/data/samplers/random.h>
#include <torch/serialize/archive.h>
#include <torch/tensor.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace torch {
namespace data {
namespace samplers {
RandomSampler::RandomSampler(int64_t size, Dtype index_dtype)
    : indices_(torch::randperm(size, index_dtype)) {}

void RandomSampler::reset() {
  // This allocates a new chunk of memory every time (just FYI). It should be
  // amortized over the entire epoch hopefully.
  indices_ = torch::randperm(indices_.numel(), indices_.options());
  index_ = 0;
}

optional<std::vector<size_t>> RandomSampler::next(size_t batch_size) {
  AT_ASSERT(index_ <= indices_.numel());
  const size_t remaining_indices = indices_.numel() - index_;
  if (remaining_indices == 0) {
    return nullopt;
  }
  std::vector<size_t> index_batch(std::min(batch_size, remaining_indices));
  auto slice = indices_.slice(/*dim=*/0, index_, index_ + index_batch.size());
  // You may want to store your indices with 32-bit or less, but here we need
  // to upcast to 64-bit. A batch itself won't hold too many indices, so that
  // should be ok. Note that if this indeed results in a type promotion, there
  // will be two allocations: one for the upcast slice, and one for the
  // returned `index_batch` vector.
  slice = slice.to(torch::kInt64);
  const auto* data = slice.data<int64_t>();
  std::copy(data, data + index_batch.size(), index_batch.begin());
  index_ += index_batch.size();
  return index_batch;
}

void RandomSampler::save(serialize::OutputArchive& archive) const {
  archive.write(
      "index",
      torch::tensor(static_cast<int64_t>(index_), torch::kInt64),
      /*is_buffer=*/true);
  archive.write(
      "indices",
      indices_,
      /*is_buffer=*/true);
}

void RandomSampler::load(serialize::InputArchive& archive) {
  auto tensor = torch::empty(1, torch::kInt64);
  archive.read(
      "index",
      tensor,
      /*is_buffer=*/true);
  index_ = tensor.item<int64_t>();
  archive.read(
      "indices",
      indices_,
      /*is_buffer=*/true);
}

size_t RandomSampler::index() const noexcept {
  return index_;
}

} // namespace samplers
} // namespace data
} // namespace torch
