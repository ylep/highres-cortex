/*
Copyright Télécom ParisTech (2015).

Contributor: Yann Leprince <yann.leprince@ylep.fr>.

This file is part of highres-cortex, a collection of software designed
to process high-resolution magnetic resonance images of the cerebral
cortex.

This software is governed by the CeCILL licence under French law and
abiding by the rules of distribution of free software. You can use,
modify and/or redistribute the software under the terms of the CeCILL
licence as circulated by CEA, CNRS and INRIA at the following URL:
<http://www.cecill.info/>.

As a counterpart to the access to the source code and rights to copy,
modify and redistribute granted by the licence, users are provided only
with a limited warranty and the software's author, the holder of the
economic rights, and the successive licensors have only limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading, using, modifying and/or developing or reproducing the
software by the user in light of its specific status of scientific
software, that may mean that it is complicated to manipulate, and that
also therefore means that it is reserved for developers and experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and, more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL licence and that you accept its terms.
*/

#ifndef LAPLACE_SOLVER_HH_INCLUDED
#define LAPLACE_SOLVER_HH_INCLUDED

#include <limits>

#include <boost/static_assert.hpp>

#include <cartodata/volume/volume.h>

namespace yl
{

template <typename Real>
class LaplaceSolver
{
  BOOST_STATIC_ASSERT_MSG(std::numeric_limits<Real>::is_specialized,
                          "LaplaceSolver can only be specialized for types"
                          " with std::numeric_limits support");
  BOOST_STATIC_ASSERT_MSG(!std::numeric_limits<Real>::is_integer,
                          "LaplaceSolver can only be specialized for "
                          " floating-point (non-integer) types");

public:
  explicit LaplaceSolver(const carto::VolumeRef<int16_t>& classif);

  /** Initialize the output volume to a reasonable value
   *
   *  0 in CSF, 1 in white matter, 0.5 in the cortex
   */
  void initialize_solution();

  void SOR(Real absolute_precision,
           float typical_cortical_thickness);
  void clamp_to_range(Real min, Real max);
  void eliminate_extrema();

  carto::VolumeRef<Real> solution() const;

  static Real best_precision() {
    return 11 * std::numeric_limits<Real>::epsilon();
  }

  void set_verbosity(const int verbosity) {
    m_verbosity = verbosity;
  }

private:
  static const int s_border_width = 1;

  const carto::VolumeRef<int16_t> m_classif;
  carto::VolumeRef<Real> m_solution;
  int m_verbosity;
};

} // namespace yl

#endif // !defined(LAPLACE_SOLVER_HH_INCLUDED)
