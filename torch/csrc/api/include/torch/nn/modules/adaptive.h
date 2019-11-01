#pragma once

#include <torch/nn/cloneable.h>
#include <torch/nn/module.h>
#include <torch/nn/modules/linear.h>
#include <torch/nn/modules/container/modulelist.h>
#include <torch/nn/modules/container/sequential.h>
#include <torch/nn/options/adaptive.h>
#include <torch/csrc/WindowsTorchApiMacro.h>

#include <cstddef>
#include <vector>

namespace torch {
namespace nn {

/// The output of a single invocation of an AdaptiveLogSoftmaxWithLoss
/// module's `forward()` method.
struct TORCH_API ASMoutput {
  ASMoutput(const Tensor & output_, const Tensor & loss_);
  Tensor output;
  Tensor loss;
};

/// Efficient softmax approximation as described in
/// Efficient softmax approximation for GPUs`_ by Edouard Grave, Armand Joulin,
/// Moustapha Cissé, David Grangier, and Hervé Jégou.
///
/// Adaptive softmax is an approximate strategy for training models with large
/// output spaces. It is most effective when the label distribution is highly
/// imbalanced, for example in natural language modelling, where the word
/// frequency distribution approximately follows the `Zipf's law`_.
///
/// Adaptive softmax partitions the labels into several clusters, according to
/// their frequency. These clusters may contain different number of targets
/// each.
/// Additionally, clusters containing less frequent labels assign lower
/// dimensional embeddings to those labels, which speeds up the computation.
/// For each minibatch, only clusters for which at least one target is
/// present are evaluated.
///
/// The idea is that the clusters which are accessed frequently
/// (like the first one, containing most frequent labels), should also be cheap
/// to compute -- that is, contain a small number of assigned labels.
///
/// We highly recommend taking a look at the original paper for more details.
/// _Efficient softmax approximation for GPUs:
/// https://arxiv.org/abs/1609.04309
///
/// _Zipf's law:
/// https://en.wikipedia.org/wiki/Zipf%27s_law

class TORCH_API AdaptiveLogSoftmaxWithLossImpl : public Cloneable<AdaptiveLogSoftmaxWithLossImpl> {
 public:
   AdaptiveLogSoftmaxWithLossImpl(int64_t in_features, int64_t n_classes, std::vector<int64_t> cutoff )
      : AdaptiveLogSoftmaxWithLossImpl(AdaptiveLogSoftmaxWithLossOptions(in_features,n_classes,cutoff)) {}
  explicit AdaptiveLogSoftmaxWithLossImpl(const AdaptiveLogSoftmaxWithLossOptions & options_ );

  ASMoutput forward(const Tensor& input, const Tensor& target);

  void reset() override;

  /// Pretty prints the `LocalResponseNormImpl` module into the given `stream`.
  void pretty_print(std::ostream& stream) const override;

  //Tensor _get_full_log_prob(const Tensor& input, const Tensor& head_output);

  Tensor log_prob(const Tensor& input);

  Tensor predict(const Tensor& input);

  /// The options with which this `Module` was constructed.
  AdaptiveLogSoftmaxWithLossOptions options;

  std::vector<int64_t> cutoffs;

  int64_t shortlist_size;

  int64_t n_clusters;

  int64_t head_size;

  Linear head = nullptr;

  ModuleList tail = nullptr;

  private:
  Tensor _get_full_log_prob(const Tensor &input, const Tensor& head_output);
};

TORCH_MODULE(AdaptiveLogSoftmaxWithLoss);

} // namespace nn
} // namespace torc