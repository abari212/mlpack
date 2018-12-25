/**
 * @file kde_impl.hpp
 * @author Roberto Hueso (robertohueso96@gmail.com)
 *
 * Implementation of Kernel Density Estimation.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */

#include "kde.hpp"
#include "kde_rules.hpp"

namespace mlpack {
namespace kde {

//! Construct tree that rearranges the dataset
template<typename TreeType, typename MatType>
TreeType* BuildTree(
    MatType&& dataset,
    std::vector<size_t>& oldFromNew,
    const typename std::enable_if<
        tree::TreeTraits<TreeType>::RearrangesDataset>::type* = 0)
{
  return new TreeType(std::forward<MatType>(dataset), oldFromNew);
}

//! Construct tree that doesn't rearrange the dataset
template<typename TreeType, typename MatType>
TreeType* BuildTree(
    MatType&& dataset,
    const std::vector<size_t>& /* oldFromNew */,
    const typename std::enable_if<
        !tree::TreeTraits<TreeType>::RearrangesDataset>::type* = 0)
{
  return new TreeType(std::forward<MatType>(dataset));
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::KDE() :
    kernel(new KernelType()),
    metric(new MetricType()),
    relError(0.05),
    absError(0.0),
    ownsKernel(true),
    ownsMetric(true),
    ownsReferenceTree(false),
    trained(false) { }

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
KDE(const double bandwidth,
    const double relError,
    const double absError) :
    kernel(new KernelType(bandwidth)),
    metric(new MetricType()),
    relError(relError),
    absError(absError),
    ownsKernel(true),
    ownsMetric(true),
    ownsReferenceTree(false),
    trained(false)
{
  CheckErrorValues(relError, absError);
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
KDE(MetricType& metric,
    KernelType& kernel,
    const double relError,
    const double absError) :
    kernel(&kernel),
    metric(&metric),
    relError(relError),
    absError(absError),
    ownsKernel(false),
    ownsMetric(false),
    ownsReferenceTree(false),
    trained(false)
{
  CheckErrorValues(relError, absError);
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
KDE(const KDE& other) :
    kernel(new KernelType(other.kernel)),
    metric(new MetricType(other.metric)),
    relError(other.relError),
    absError(other.absError),
    ownsKernel(other.ownsKernel),
    ownsMetric(other.ownsMetric),
    ownsReferenceTree(other.ownsReferenceTree),
    trained(other.trained)
{
  if (trained)
  {
    if (ownsReferenceTree)
    {
      oldFromNewReferences = new std::vector<size_t>;
      referenceTree = new Tree(other.referenceTree, *oldFromNewReferences);
    }
    else
    {
      oldFromNewReferences = other.oldFromNewReferences;
      referenceTree = other.referenceTree;
    }
  }
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
KDE(KDE&& other) :
    kernel(other.kernel),
    metric(other.metric),
    referenceTree(other.referenceTree),
    oldFromNewReferences(other.oldFromNewReferences),
    relError(other.relError),
    absError(other.absError),
    ownsKernel(other.ownsKernel),
    ownsMetric(other.ownsMetric),
    ownsReferenceTree(other.ownsReferenceTree),
    trained(other.trained)
{
  other.kernel = new KernelType();
  other.metric = new MetricType();
  other.referenceTree = nullptr;
  other.oldFromNewReferences = nullptr;
  other.ownsReferenceTree = false;
  other.trained = false;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>&
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
operator=(KDE other)
{
  // Clean memory
  if (ownsKernel)
    delete kernel;
  if (ownsMetric)
    delete metric;
  if (ownsReferenceTree)
  {
    delete referenceTree;
    delete oldFromNewReferences;
  }

  // Move
  this->kernel = std::move(other.kernel);
  this->metric = std::move(other.metric);
  this->referenceTree = std::move(other.referenceTree);
  this->oldFromNewReferences = std::move(other.oldFromNewReferences);
  this->relError = other.relError;
  this->absError = other.absError;
  this->ownsKernel = other.ownsKernel;
  this->ownsMetric = other.ownsMetric;
  this->ownsReferenceTree = other.ownsReferenceTree;
  this->trained = other.trained;

  return *this;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::~KDE()
{
  if (ownsKernel)
    delete kernel;
  if (ownsMetric)
    delete metric;
  if (ownsReferenceTree)
  {
    delete referenceTree;
    delete oldFromNewReferences;
  }
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
Train(MatType referenceSet)
{
  // Check if referenceSet is not an empty set.
  if (referenceSet.n_cols == 0)
    throw std::invalid_argument("cannot train KDE model with an empty "
                                "reference set");
  if (ownsReferenceTree)
  {
    delete referenceTree;
    delete oldFromNewReferences;
  }
  this->ownsReferenceTree = true;
  Timer::Start("building_reference_tree");
  this->oldFromNewReferences = new std::vector<size_t>;
  this->referenceTree = BuildTree<Tree>(std::move(referenceSet),
                                        *oldFromNewReferences);
  Timer::Stop("building_reference_tree");
  this->trained = true;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
Train(Tree* referenceTree, std::vector<size_t>* oldFromNewReferences)
{
  // Check if referenceTree dataset is not an empty set.
  if (referenceTree->Dataset().n_cols == 0)
    throw std::invalid_argument("cannot train KDE model with an empty "
                                "reference set");
  if (ownsReferenceTree == true)
  {
    delete this->referenceTree;
    delete this->oldFromNewReferences;
  }
  this->ownsReferenceTree = false;
  this->referenceTree = referenceTree;
  this->oldFromNewReferences = oldFromNewReferences;
  this->trained = true;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
Evaluate(MatType querySet, arma::vec& estimations)
{
  Timer::Start("building_query_tree");
  std::vector<size_t> oldFromNewQueries;
  Tree* queryTree = BuildTree<Tree>(std::move(querySet), oldFromNewQueries);
  Timer::Stop("building_query_tree");
  this->Evaluate(queryTree, oldFromNewQueries, estimations);
  delete queryTree;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
Evaluate(Tree* queryTree,
         const std::vector<size_t>& oldFromNewQueries,
         arma::vec& estimations)
{
  // Check querySet has at least 1 element to evaluate.
  if (queryTree->Dataset().n_cols == 0)
  {
    Log::Warn << "KDE::Evaluate(): querySet is empty, no predictions will "
              << "be returned" << std::endl;
    return;
  }
  // Check whether dimensions match.
  if (queryTree->Dataset().n_rows != referenceTree->Dataset().n_rows)
    throw std::invalid_argument("cannot evaluate KDE model: querySet and "
                                "referenceSet dimensions don't match");

  Timer::Start("computing_kde");
  // Get estimations vector ready.
  estimations.clear();
  estimations.resize(queryTree->Dataset().n_cols);
  estimations.fill(arma::fill::zeros);

  // Evaluate
  typedef KDERules<MetricType, KernelType, Tree> RuleType;
  RuleType rules = RuleType(referenceTree->Dataset(),
                            queryTree->Dataset(),
                            estimations,
                            relError,
                            absError,
                            oldFromNewQueries,
                            *metric,
                            *kernel,
                            false);

  // Create traverser.
  DualTreeTraversalType<RuleType> traverser(rules);
  traverser.Traverse(*queryTree, *referenceTree);
  estimations /= referenceTree->Dataset().n_cols;
  Timer::Stop("computing_kde");

  Log::Info << rules.Scores() << " node combinations were scored." << std::endl;
  Log::Info << rules.BaseCases() << " base cases were calculated." << std::endl;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
Evaluate(arma::vec& estimations)
{
  Timer::Start("computing_kde");
  // Get estimations vector ready.
  estimations.clear();
  estimations.resize(referenceTree->Dataset().n_cols);
  estimations.fill(arma::fill::zeros);

  // Evaluate
  typedef KDERules<MetricType, KernelType, Tree> RuleType;
  RuleType rules = RuleType(referenceTree->Dataset(),
                            referenceTree->Dataset(),
                            estimations,
                            relError,
                            absError,
                            *oldFromNewReferences,
                            *metric,
                            *kernel,
                            true);

  // Create traverser.
  DualTreeTraversalType<RuleType> traverser(rules);
  traverser.Traverse(*referenceTree, *referenceTree);
  estimations /= referenceTree->Dataset().n_cols;
  Timer::Stop("computing_kde");

  Log::Info << rules.Scores() << " node combinations were scored." << std::endl;
  Log::Info << rules.BaseCases() << " base cases were calculated." << std::endl;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
RelativeError(const double newError)
{
  CheckErrorValues(newError, absError);
  relError = newError;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
AbsoluteError(const double newError)
{
  CheckErrorValues(relError, newError);
  absError = newError;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
template<typename Archive>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
serialize(Archive& ar, const unsigned int /* version */)
{
  // Serialize preferences.
  ar & BOOST_SERIALIZATION_NVP(relError);
  ar & BOOST_SERIALIZATION_NVP(absError);
  ar & BOOST_SERIALIZATION_NVP(trained);

  // If we are loading, clean up memory if necessary.
  if (Archive::is_loading::value)
  {
    if (ownsKernel && kernel)
      delete kernel;
    if (ownsMetric && metric)
      delete metric;
    if (ownsReferenceTree && referenceTree)
    {
      delete referenceTree;
      delete oldFromNewReferences;
    }
    // After loading kernel, metric and tree, we own it.
    ownsKernel = true;
    ownsMetric = true;
    ownsReferenceTree = true;
  }

  // Serialize the rest of values.
  ar & BOOST_SERIALIZATION_NVP(kernel);
  ar & BOOST_SERIALIZATION_NVP(metric);
  ar & BOOST_SERIALIZATION_NVP(referenceTree);
  ar & BOOST_SERIALIZATION_NVP(oldFromNewReferences);
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType>
void KDE<MetricType, MatType, KernelType, TreeType, DualTreeTraversalType>::
CheckErrorValues(const double relError, const double absError) const
{
  if (relError < 0 || relError > 1)
    throw std::invalid_argument("Relative error tolerance must be a value "
                                "between 0 and 1");
  if (absError < 0)
    throw std::invalid_argument("Absolute error tolerance must be a value "
                                "greater or equal to 0");
}

} // namespace kde
} // namespace mlpack
