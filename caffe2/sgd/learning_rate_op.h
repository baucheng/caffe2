#ifndef CAFFE2_SGD_LEARNING_RATE_OP_H_
#define CAFFE2_SGD_LEARNING_RATE_OP_H_

#include <cfloat>
#include <cmath>
#include "caffe2/core/context.h"
#include "caffe2/core/operator.h"
#include "caffe2/sgd/learning_rate_functors.h"

namespace caffe2 {

template <typename T, class Context>
class LearningRateOp final : public Operator<Context> {
 public:
  LearningRateOp(const OperatorDef& operator_def, Workspace* ws)
      : Operator<Context>(operator_def, ws), functor_(nullptr),
        base_lr_(
            OperatorBase::template GetSingleArgument<float>(
                "base_lr", FLT_MAX)) {
    CAFFE_CHECK_NE(base_lr_, FLT_MAX) << "Base learning rate must be set.";
    const string policy = OperatorBase::GetSingleArgument<string>("policy", "");
    CAFFE_CHECK(policy.size()) << "Must specify a learning rate policy.";
    if (policy == "fixed") {
      functor_.reset(new FixedLearningRate<T>());
    } else if (policy == "step") {
      int stepsize =
          OperatorBase::template GetSingleArgument<int>("stepsize", 0);
      T gamma = OperatorBase::template GetSingleArgument<float>("gamma", 0);
      CAFFE_DCHECK_GT(stepsize, 0);
      CAFFE_DCHECK_GT(gamma, 0);
      functor_.reset(new StepLearningRate<T>(stepsize, gamma));
    } else if (policy == "exp") {
      T gamma = OperatorBase::template GetSingleArgument<float>("gamma", 0);
      CAFFE_DCHECK_GT(gamma, 0);
      functor_.reset(new ExpLearningRate<T>(gamma));
    } else if (policy == "inv") {
      T gamma = OperatorBase::template GetSingleArgument<float>("gamma", 0);
      T power = OperatorBase::template GetSingleArgument<float>("power", 0);
      CAFFE_DCHECK_GT(gamma, 0);
      CAFFE_DCHECK_GT(power, 0);
      functor_.reset(new InvLearningRate<T>(gamma, power));
    } else {
      CAFFE_LOG_FATAL << "Unknown learning rate policy: " << policy;
    }
  }
  USE_OPERATOR_CONTEXT_FUNCTIONS;

  bool RunOnDevice() override {
    int iter = OperatorBase::Input<TensorCPU>(0).template data<int>()[0];
    T learning_rate = base_lr_ * (*functor_)(iter);
    // Write to output.
    auto* output = Output(0);
    output->Reshape(std::vector<int>());
    device_context_.template Copy<T, CPUContext, Context>(
        1, &learning_rate, Output(0)->template mutable_data<T>());
    return true;
  }

 private:
  unique_ptr<LearningRateFunctor<T> > functor_;
  T base_lr_;

  INPUT_OUTPUT_STATS(1, 1, 1, 1);
  DISABLE_COPY_AND_ASSIGN(LearningRateOp);
};

}  // namespace caffe2

#endif  // CAFFE2_SGD_LEARNING_RATE_OP_H_
