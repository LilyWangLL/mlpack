/**
 * @file tests/svd_batch_test.cpp
 * @author Sumedh Ghaisas
 *
 * Test the SVDBatchLearning class for AMF.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#include <mlpack/core.hpp>
#include <mlpack/methods/amf.hpp>

#include "catch.hpp"

using namespace std;
using namespace mlpack;
using namespace mlpack::amf;
using namespace arma;

/**
 * Make sure the SVD Batch lerning is converging.
 */
TEST_CASE("SVDBatchConvergenceElementTest", "[SVDBatchTest]")
{
  sp_mat data;
  data.sprandn(100, 100, 0.2);
  AMF<SimpleToleranceTermination<sp_mat>,
      AverageInitialization,
      SVDBatchLearning> amf;
  mat m1, m2;
  amf.Apply(data, 2, m1, m2);

  REQUIRE(amf.TerminationPolicy().Iteration() !=
          amf.TerminationPolicy().MaxIterations());
}

//! This is used to ensure we start from the same initial point.
class SpecificRandomInitialization
{
 public:
  SpecificRandomInitialization(const size_t n, const size_t r, const size_t m) :
      W(arma::randu<arma::mat>(n, r)),
      H(arma::randu<arma::mat>(r, m)) { }

  template<typename MatType>
  inline void Initialize(const MatType& /* V */,
                         const size_t /* r */,
                         arma::mat& W,
                         arma::mat& H)
  {
    W = this->W;
    H = this->H;
  }

 private:
  arma::mat W;
  arma::mat H;
};

/**
 * Make sure the momentum is working okay.
 */
TEST_CASE("SVDBatchMomentumTest", "[SVDBatchTest]")
{
  mat dataset;
  if (!data::Load("GroupLensSmall.csv", dataset))
    FAIL("Cannot load dataset GroupLensSmall.csv!");

  // Generate list of locations for batch insert constructor for sparse
  // matrices.
  arma::umat locations(2, dataset.n_cols);
  arma::vec values(dataset.n_cols);
  for (size_t i = 0; i < dataset.n_cols; ++i)
  {
    // We have to transpose it because items are rows, and users are columns.
    locations(0, i) = ((arma::uword) dataset(0, i));
    locations(1, i) = ((arma::uword) dataset(1, i));
    values(i) = dataset(2, i);
  }

  // Find maximum user and item IDs.
  const size_t maxUserID = (size_t) max(locations.row(0)) + 1;
  const size_t maxItemID = (size_t) max(locations.row(1)) + 1;

  // Fill sparse matrix.
  sp_mat cleanedData = arma::sp_mat(locations, values, maxUserID, maxItemID);

  // Create the initial matrices.
  SpecificRandomInitialization sri(cleanedData.n_rows, 2, cleanedData.n_cols);

  ValidationRMSETermination<sp_mat> vrt(cleanedData, 500);
  AMF<ValidationRMSETermination<sp_mat>,
      SpecificRandomInitialization,
      SVDBatchLearning> amf1(vrt, sri, SVDBatchLearning(0.0009, 0, 0, 0));

  mat m1, m2;
  const double regularRMSE = amf1.Apply(cleanedData, 2, m1, m2);

  AMF<ValidationRMSETermination<sp_mat>,
      SpecificRandomInitialization,
      SVDBatchLearning> amf2(vrt, sri, SVDBatchLearning(0.0009, 0, 0, 0.8));

  const double momentumRMSE = amf2.Apply(cleanedData, 2, m1, m2);

  REQUIRE(momentumRMSE <= regularRMSE + 0.1);
}

/**
 * Make sure the regularization is working okay.
 */
TEST_CASE("SVDBatchRegularizationTest", "[SVDBatchTest]")
{
  mat dataset;
  if (!data::Load("GroupLensSmall.csv", dataset))
    FAIL("Cannot load dataset GroupLensSmall.csv!");

  // Generate list of locations for batch insert constructor for sparse
  // matrices.
  arma::umat locations(2, dataset.n_cols);
  arma::vec values(dataset.n_cols);
  for (size_t i = 0; i < dataset.n_cols; ++i)
  {
    // We have to transpose it because items are rows, and users are columns.
    locations(0, i) = ((arma::uword) dataset(0, i));
    locations(1, i) = ((arma::uword) dataset(1, i));
    values(i) = dataset(2, i);
  }

  // Find maximum user and item IDs.
  const size_t maxUserID = (size_t) max(locations.row(0)) + 1;
  const size_t maxItemID = (size_t) max(locations.row(1)) + 1;

  // Fill sparse matrix.
  sp_mat cleanedData = arma::sp_mat(locations, values, maxUserID, maxItemID);

  // Create the initial matrices.
  SpecificRandomInitialization sri(cleanedData.n_rows, 2, cleanedData.n_cols);

  ValidationRMSETermination<sp_mat> vrt(cleanedData, 2000);
  AMF<ValidationRMSETermination<sp_mat>,
      SpecificRandomInitialization,
      SVDBatchLearning> amf1(vrt, sri, SVDBatchLearning(0.0009, 0, 0, 0));

  mat m1, m2;
  double regularRMSE = amf1.Apply(cleanedData, 2, m1, m2);

  AMF<ValidationRMSETermination<sp_mat>,
      SpecificRandomInitialization,
      SVDBatchLearning> amf2(vrt, sri, SVDBatchLearning(0.0009, 0.5, 0.5, 0.8));

  double momentumRMSE = amf2.Apply(cleanedData, 2, m1, m2);

  REQUIRE(momentumRMSE <= regularRMSE + 0.05);
}

/**
 * Make sure the SVD can factorize matrices with negative entries.
 */
TEST_CASE("SVDBatchNegativeElementTest", "[SVDBatchTest]")
{
  // Create two 5x3 matrices that we should be able to recover.
  mat testLeft;
  testLeft.randu(5, 3);
  testLeft -= 0.5; // Shift so elements are negative.

  mat testRight;
  testRight.randu(3, 5);
  testRight -= 0.5; // Shift so elements are negative.

  // Assemble a rank-3 matrix that is 5x5.
  mat test = testLeft * testRight;

  AMF<SimpleToleranceTermination<mat>,
      RandomInitialization,
      SVDBatchLearning> amf(SimpleToleranceTermination<mat>(),
                            RandomInitialization(),
                            SVDBatchLearning(0.1, 0.001, 0.001, 0));
  mat m1, m2;
  amf.Apply(test, 3, m1, m2);

  arma::mat result = m1 * m2;

  // 6.5% tolerance on the norm.
  REQUIRE(arma::norm(test, "fro") ==
      Approx(arma::norm(result, "fro")).epsilon(0.09));
}
