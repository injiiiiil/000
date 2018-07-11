#include "caffe2/operators/generate_proposals_op_util_boxes.h"

#include <gtest/gtest.h>

namespace caffe2 {

TEST(UtilsBoxesTest, TestBboxTransformRandom) {
  using EMatXf = Eigen::MatrixXf;

  EMatXf bbox(5, 4);
  bbox << 175.62031555, 20.91103172, 253.352005, 155.0145874, 169.24636841,
      4.85241556, 228.8605957, 105.02092743, 181.77426147, 199.82876587,
      192.88427734, 214.0255127, 174.36262512, 186.75761414, 296.19091797,
      231.27906799, 22.73153877, 92.02596283, 135.5695343, 208.80291748;

  EMatXf deltas(5, 4);
  deltas << 0.47861834, 0.13992102, 0.14961673, 0.71495209, 0.29915856,
      -0.35664671, 0.89018666, 0.70815367, -0.03852064, 0.44466892, 0.49492538,
      0.71409376, 0.28052918, 0.02184832, 0.65289006, 1.05060139, -0.38172557,
      -0.08533806, -0.60335309, 0.79052375;

  EMatXf result_gt(5, 4);
  result_gt << 206.94953073, -30.71519157, 298.3876512, 245.44846569,
      143.8712194, -83.34289038, 291.50227513, 122.05339902, 177.43029521,
      198.66623633, 197.29527254, 229.70308414, 152.25190373, 145.43156421,
      388.21547899, 275.59425266, 5.06242193, 11.04094661, 67.32890274,
      270.68622005;

  const float BBOX_XFORM_CLIP = log(1000.0 / 16.0);
  auto result = utils::bbox_transform(
      bbox.array(),
      deltas.array(),
      std::vector<float>{1.0, 1.0, 1.0, 1.0},
      BBOX_XFORM_CLIP);
  EXPECT_NEAR((result.matrix() - result_gt).norm(), 0.0, 1e-4);
}

TEST(UtilsBoxesTest, TestBboxTransformRotated) {
  // Test rotated bbox transform w/o angle normalization
  using EMatXf = Eigen::MatrixXf;

  EMatXf bbox(5, 5);
  bbox << 214.986, 88.4628, 78.7317, 135.104, 0.0, 199.553, 55.4367, 60.6142,
      101.169, 45.0, 187.829, 207.427, 0012.11, 15.1967, 90.0, 235.777, 209.518,
      122.828, 45.5215, -60.0, 79.6505, 150.914, 113.838, 117.777, 170.5;

  EMatXf deltas(5, 5);
  // 0.174533 radians -> 10 degrees
  deltas << 0.47861834, 0.13992102, 0.14961673, 0.71495209, 0.0, 0.29915856,
      -0.35664671, 0.89018666, 0.70815367, 0.174533, -0.03852064, 0.44466892,
      0.49492538, 0.71409376, 0.174533, 0.28052918, 0.02184832, 0.65289006,
      1.05060139, 0.174533, -0.38172557, -0.08533806, -0.60335309, 0.79052375,
      0.174533;

  EMatXf result_gt(5, 5);
  result_gt << 252.668, 107.367, 91.4381, 276.165, 0.0, 217.686, 19.3551,
      147.631, 205.397, 55.0, 187.363, 214.185, 19.865, 31.0368, 100.0, 270.234,
      210.513, 235.963, 130.163, -50.0, 36.1956, 140.863, 62.2665, 259.645,
      180.5;

  const float BBOX_XFORM_CLIP = log(1000.0 / 16.0);
  auto result = utils::bbox_transform(
      bbox.array(),
      deltas.array(),
      std::vector<float>{1.0, 1.0, 1.0, 1.0},
      BBOX_XFORM_CLIP,
      true, /* correct_transform_coords */
      false /* angle_bound_on */);
  EXPECT_NEAR((result.matrix() - result_gt).norm(), 0.0, 1e-2);
}

TEST(UtilsBoxesTest, TestBboxTransformRotatedNormalized) {
  // Test rotated bbox transform with angle normalization
  using EMatXf = Eigen::MatrixXf;

  EMatXf bbox(5, 5);
  bbox << 214.986, 88.4628, 78.7317, 135.104, 0.0, 199.553, 55.4367, 60.6142,
      101.169, 45.0, 187.829, 207.427, 0012.11, 15.1967, 90.0, 235.777, 209.518,
      122.828, 45.5215, -60.0, 79.6505, 150.914, 113.838, 117.777, 170.5;

  EMatXf deltas(5, 5);
  // 0.174533 radians -> 10 degrees
  deltas << 0.47861834, 0.13992102, 0.14961673, 0.71495209, 0.0, 0.29915856,
      -0.35664671, 0.89018666, 0.70815367, 0.174533, -0.03852064, 0.44466892,
      0.49492538, 0.71409376, 0.174533, 0.28052918, 0.02184832, 0.65289006,
      1.05060139, 0.174533, -0.38172557, -0.08533806, -0.60335309, 0.79052375,
      0.174533;

  EMatXf result_gt(5, 5);
  result_gt << 252.668, 107.367, 91.4381, 276.165, 0.0, 217.686, 19.3551,
      147.631, 205.397, 55.0, 187.363, 214.185, 19.865, 31.0368, -80.0, 270.234,
      210.513, 235.963, 130.163, -50.0, 36.1956, 140.863, 62.2665, 259.645, 0.5;

  const float BBOX_XFORM_CLIP = log(1000.0 / 16.0);
  auto result = utils::bbox_transform(
      bbox.array(),
      deltas.array(),
      std::vector<float>{1.0, 1.0, 1.0, 1.0},
      BBOX_XFORM_CLIP,
      true, /* correct_transform_coords */
      true, /* angle_bound_on */
      -90, /* angle_bound_lo */
      90 /* angle_bound_hi */);
  EXPECT_NEAR((result.matrix() - result_gt).norm(), 0.0, 1e-2);
}

} // namespace caffe2
