// Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lite/kernels/nnadapter/converter/converter.h"

namespace paddle {
namespace lite {
namespace kernels {
namespace nnadapter {

int ConvertReshape(Converter* converter, OpInfo* op, Scope* scope) {
  // Extract the inputs, outputs and attributes
  auto x_name = op->Input("X").front();
  auto x_scale_name = "X0_scale";
  std::vector<float> x_scales;
  if (op->HasInputScale(x_scale_name, true)) {
    x_scales = op->GetInputScale(x_scale_name, true);
  }
  auto out_name = op->Output("Out").front();
  auto out_scale_name = "Out0_scale";
  std::vector<float> out_scales;
  if (op->HasOutputScale(out_scale_name, true)) {
    out_scales = op->GetOutputScale(out_scale_name, true);
  }
  // Check quantization mode
  bool is_quant_mode = IsValidSymmPerLayerQuantParams(out_scales);
  if (is_quant_mode) {
    CHECK(IsValidSymmPerLayerQuantParams(x_scales))
        << "Missing the quant params '" << x_scale_name << "' for the input '"
        << x_name << "'";
  }

  // Convert to NNAdapter operands and operation
  // Input operand
  auto input_operand = converter->GetMappedOperand(x_name);
  CHECK(input_operand);
  auto input_type = converter->GetOperandType(input_operand);
  // Shape operand
  // "ShapeTensor"(input) > "Shape"(input) > "shape"(attr)
  NNAdapterOperand* shape_operand = nullptr;
  if (HasInput(op, scope, "ShapeTensor")) {
    // TODO(zhupengyang): apply concat
    LOG(WARNING) << "Not support ShapeTensor!";
    return UNSUPPORTED_FEATURE;
  } else if (HasInput(op, scope, "Shape")) {
    auto shape_name = op->Input("Shape").front();
    shape_operand = converter->GetMappedOperand(shape_name);
  } else {
    std::vector<int> shape = op->GetAttr<std::vector<int>>("shape");
    shape_operand = converter->AddConstantOperand(shape);
  }
  // Output operand
  if (is_quant_mode) {
    if (IsNNInt8SymmPerLayerQuantType(*input_type)) {
      std::vector<float> quant_scales;
      CHECK(GetNNSymmQuantParams(*input_type, &quant_scales));
      CHECK(IsSameSymmQuantParams(x_scales, quant_scales));
      // TODO(hong19860320) Add a NNADAPTER_DEQUANT and NNADAPTER_QUANT
      // operation to
      // make the quant params obtained from a operand consistent with those
      // obtained from op_desc
    } else {
      // TODO(hong19860320) Add a NNADAPTER_QUANT/NNADAPTER_DEQUANT operation to
      // convert any type to int8 symm per-layer quant operand
      LOG(FATAL) << "Mixed precision will be supported in future!";
      return UNSUPPORTED_FEATURE;
    }
  } else {
    if (IsNNInt8SymmPerLayerQuantType(*input_type)) {
      // TODO(hong19860320) Add a NNADAPTER_DEQUANT to dequantize the input
      // operand to a float type operand
      LOG(FATAL) << "Mixed precision will be supported in future!";
      return UNSUPPORTED_FEATURE;
    }
  }
  auto output_operand = converter->AddOutputOperand(out_name, out_scales);
  // Reshape operation
  converter->AddOperation(
      NNADAPTER_RESHAPE, {input_operand, shape_operand}, {output_operand});
  return NO_ERROR;
}

}  // namespace nnadapter
}  // namespace kernels
}  // namespace lite
}  // namespace paddle
