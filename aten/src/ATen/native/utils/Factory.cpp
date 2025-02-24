#include <ATen/NamedTensorUtils.h>
#include <ATen/native/utils/Factory.h>
#include <c10/core/CPUAllocator.h>
#include <c10/util/accumulate.h>

namespace at {
namespace native {
namespace mobile {

Tensor empty_with_tail_padding(
    const IntArrayRef size,
    const caffe2::TypeMeta dtype,
    const c10::MemoryFormat memory_format,
    const DimnameList maybe_names) {
  auto* const allocator_ptr = c10::GetDefaultMobileCPUAllocator();
  const int64_t nelements = c10::multiply_integers(size);
  size_t size_bytes = nelements * dtype.itemsize();

  Tensor tensor(c10::make_intrusive<c10::TensorImpl>(
      c10::Storage{
          c10::Storage::use_byte_size_t(),
          size_bytes,
          allocator_ptr->allocate(size_bytes),
          allocator_ptr,
          /*resizable=*/true,
      },
      DispatchKeySet{DispatchKey::CPU},
      dtype));

  return namedinference::propagate_names_if_nonempty(
      tensor.resize_(size, memory_format),
      maybe_names);
}

bool is_padded_contiguous(
    const Tensor& input,
    const c10::MemoryFormat memory_format) {
  const auto* const allocator = input.storage().allocator();
  const auto* const mobile_allocator = c10::GetDefaultMobileCPUAllocator();

  return (allocator == mobile_allocator) && input.is_contiguous(memory_format);
}

Tensor allocate_padded_contiguous_if_needed(
    const Tensor& input,
    const c10::MemoryFormat memory_format) {
  // If the allocators are the same and the memory is contiguous in the requested
  // format, then there is no need to reallocate the tensor.
  if (is_padded_contiguous(input, memory_format)) {
    return input;
  }

  // If there is a need to reallocate the tensor on the other hand, either because
  // the allocators are not the same, or the allocators are the same but the input
  // is not contiguous in the requested format, then reallocate and directly copy
  // into destination.  There is no need to allocate a temporary contiguous memory
  // only to use it as the source of the copy operation onto our final destination.

  Tensor padded_input = empty_with_tail_padding(
      input.sizes(),
      input.options().dtype(),
      memory_format,
      input.names());

  return padded_input.copy_(input);
}

} // namespace mobile
} // namespace native
} // namespace at
