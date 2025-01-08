#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include "oht/lib.h"
#include "oht/obl/mod.h"

// Decl
namespace rworam {

using namespace oht;

template <size_t Ksize, size_t Vsize>
class Rworam {
 public:
  std::vector<Oht<Ksize, Vsize>> layers_;
  std::vector<std::vector<uint8_t>> layer_prf_keys_;
  std::vector<uint32_t> kBs_;
  std::vector<uint32_t> kZs_;
  size_t batch_num_;

  Rworam(size_t layer_num, size_t batch_num, const std::vector<uint32_t> &kBs, const std::vector<uint32_t> &kZs)
      : batch_num_(batch_num), kBs_(kBs), kZs_(kZs) {
    layers_.reserve(layer_num);
    for (size_t i = 0; i < layer_num; ++i) {
      layers_.emplace_back(kBs[i], kZs[i]);
    }
    layer_prf_keys_.resize(layer_num);
  }

  void clear() {
    for (auto &layer : layers_) {
      layer.clear();
    }
  }

  void write(std::vector<Elem<Ksize, Vsize>> &&elems, const std::vector<uint8_t> &prf_key, unsigned jobs);
  void lookup(Elem<Ksize, Vsize> &elem);
};

}  // namespace rworam

// Impl
namespace rworam {

using namespace oht;

template <size_t Ksize, size_t Vsize>
void Rworam<Ksize, Vsize>::write(
  std::vector<Elem<Ksize, Vsize>> &&elems, const std::vector<uint8_t> &prf_key, unsigned jobs) {
  const auto layer_num = layers_.size();

  // Find the first empty layer, otherwise overwrite the last layer
  size_t rebuild_idx = layer_num - 1;
  for (size_t i = 0; i < layer_num - 1; ++i) {
    if (layers_[i].empty()) {
      rebuild_idx = i;
      break;
    }
  }
  // Drop the previous last layer if full
  if (rebuild_idx == layer_num - 1) {
    layers_[rebuild_idx].clear();
  }

  for (auto i = 0; i < rebuild_idx; i++) {
    std::vector<uint8_t> tags{};
    tags.reserve(layers_[i].bins1_.size());
    while (!layers_[i].bins1_.empty()) {
      tags.push_back(tag::get(layers_[i].bins1_.back().tag, tag::kTagFiller));
      layers_[rebuild_idx].prepare(std::move(layers_[i].bins1_.back()));
      layers_[i].bins1_.pop_back();
    }
    while (!layers_[i].bins2_.empty()) {
      tags.push_back(tag::get(layers_[i].bins2_.back().tag, tag::kTagFiller));
      layers_[rebuild_idx].prepare(std::move(layers_[i].bins2_.back()));
      layers_[i].bins2_.pop_back();
    }
    oht::obl::OblCompact(layers_[rebuild_idx].bins1_.begin() + (size_t(pow(2.0, i)) - 1) * batch_num_,
      layers_[rebuild_idx].bins1_.end(), tags.data());
    layers_[rebuild_idx].bins1_.resize((size_t(pow(2.0, i + 1.0)) - 1));
  }

  for (auto &&elem : elems) {
    layers_[rebuild_idx].prepare(std::move(elem));
  }
  elems.clear();

  layers_[rebuild_idx].build(prf_key, jobs);
  layer_prf_keys_[rebuild_idx] = prf_key;
}

template <size_t Ksize, size_t Vsize>
void Rworam<Ksize, Vsize>::lookup(Elem<Ksize, Vsize> &elem) {
  std::array<uint8_t, Vsize> val;
  std::memcpy(val.data(), elem.val, Vsize);
  for (auto i = 0; i < layers_.size(); i++) {
    if (layers_[i].empty()) continue;
    const auto found = layers_[i].lookup(elem, layer_prf_keys_[i]);
    std::array<uint8_t, Vsize> elem_val;
    std::memcpy(elem_val.data(), elem.val, Vsize);
    val = oht::obl::OblChoose(found, elem_val, val);
  }
  std::memcpy(elem.val, val.data(), Vsize);
}

}  // namespace rworam
