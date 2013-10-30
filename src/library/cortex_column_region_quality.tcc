#include <yleprince/cortex_column_region_quality.hh>

#include <cmath>
#include <limits>
#include <algorithm>

#include <gsl/gsl_eigen.h>

#include <aims/data/fastAllocationData.h>
#include <aims/math/eigen.h>

using aims::AimsFastAllocationData;

namespace
{

const float pi = 3.14159265358979323846f;

inline float square(float x)
{
  return x * x;
}

inline bool proj_is_valid(float xproj, float, float)
{
  // Invalid values are (-1, -1, -1), and no valid coordinate is supposed to be
  // negative
  return xproj >= 0;
}

struct MomentAccumulator
{
public:
  MomentAccumulator()
    : m_000(0.f),
      m_100(0.f), m_010(0.f), m_001(0.f),
      m_200(0.f),
      m_110(0.f), m_020(0.f),
      m_101(0.f), m_011(0.f), m_002(0.f) {}

  void update(float x, float y, float z)
  {
    m_000 += 1;
    m_100 += x; m_010 += y; m_001 += z;
    m_200 += x * x;
    m_110 += y * x; m_020 += y * y;
    m_101 += z * x; m_011 += z * y; m_002 += z * z;
  };

  const AimsFastAllocationData<float>& degenerate_eigenvalues()
  {
    static AimsFastAllocationData<float> ret(3);
    ret = 0.f;
    return ret;
  };

  float large_eigenvalue(gsl_eigen_symm_workspace* w)
  {
    if(m_000 == 0)
      return 0.f;

    double centered_moment_matrix[3 * 3];

    // Only the lower triangular part is referenced
    centered_moment_matrix[0 * 3 + 0]
      = (m_200 - m_100 * m_100 / m_000) / m_000;
    centered_moment_matrix[1 * 3 + 0]
      = (m_110 - m_100 * m_010 / m_000) / m_000;
    centered_moment_matrix[1 * 3 + 1]
      = (m_020 - m_010 * m_010 / m_000) / m_000;
    centered_moment_matrix[2 * 3 + 0]
      = (m_101 - m_100 * m_001 / m_000) / m_000;
    centered_moment_matrix[2 * 3 + 1]
      = (m_011 - m_010 * m_001 / m_000) / m_000;
    centered_moment_matrix[2 * 3 + 2]
      = (m_002 - m_001 * m_001 / m_000) / m_000;

    double eigenvalues[3];

    gsl_matrix_view mat_view
      = gsl_matrix_view_array(centered_moment_matrix, 3, 3);
    gsl_vector_view eval_view
      = gsl_vector_view_array(eigenvalues, 3);

    gsl_eigen_symm(&mat_view.matrix, &eval_view.vector, w);

    return std::max(0.f, static_cast<float>(gsl_vector_max(&eval_view.vector)));
  };

  float large_eigenvalue()
  {
    struct WorkspaceHolder
    {
      WorkspaceHolder(gsl_eigen_symm_workspace* w_) : w(w_) {};
      ~WorkspaceHolder() {gsl_eigen_symm_free(w);};
      gsl_eigen_symm_workspace* const w;
    };
    WorkspaceHolder wh(gsl_eigen_symm_alloc(3));
    return large_eigenvalue(wh.w);
  };

  std::size_t m_000;
  float m_100, m_010, m_001;
  float m_200;
  float m_110, m_020;
  float m_101, m_011, m_002;
};


template <class BaseIterator>
class ChainedIterator
  : public boost::iterator_adaptor<
  ChainedIterator<BaseIterator>,
  BaseIterator,
  boost::use_default,
  boost::forward_traversal_tag>
{
public:
  ChainedIterator()
    : ChainedIterator::iterator_adaptor_(0) {}

  ChainedIterator(const BaseIterator& begin_first,
                  const BaseIterator& end_first,
                  const BaseIterator& begin_second)
    : ChainedIterator::iterator_adaptor_(begin_first),
      m_end_first(end_first),
      m_begin_second(begin_second) {}

  ChainedIterator(const BaseIterator& begin)
    : ChainedIterator::iterator_adaptor_(begin),
      m_end_first(),
      m_begin_second() {}


private:
  const BaseIterator m_end_first, m_begin_second;

  friend class boost::iterator_core_access;
  void increment()
  {
    BaseIterator& base = this->base_reference();
    ++base;
    if(base == m_end_first && m_end_first != BaseIterator())
      base = m_begin_second;
  }
};

} // end of anonymous namespace


namespace yl
{

template <class PointIterator>
inline float CortexColumnRegionQuality::
evaluate(const PointIterator& point_it_begin,
         const PointIterator& point_it_end) const
{
  MomentAccumulator CSF_moment, white_moment;

  std::size_t region_size = 0;
  for(PointIterator point_it = point_it_begin;
      point_it != point_it_end;
      ++point_it)
  {
    const Point3d& point = *point_it;
    const int x = point[0];
    const int y = point[1];
    const int z = point[2];

    ++region_size;

    {
      const float xproj = m_CSF_projections.at(x, y, z, 0);
      const float yproj = m_CSF_projections.at(x, y, z, 1);
      const float zproj = m_CSF_projections.at(x, y, z, 2);
      if(proj_is_valid(xproj, yproj, zproj)) {
        CSF_moment.update(xproj, yproj, zproj);
      }
    }

    {
      const float xproj = m_white_projections.at(x, y, z, 0);
      const float yproj = m_white_projections.at(x, y, z, 1);
      const float zproj = m_white_projections.at(x, y, z, 2);
      if(proj_is_valid(xproj, yproj, zproj)) {
        white_moment.update(xproj, yproj, zproj);
      }
    }
  }

  // The moments are calculated based on the projected point cloud, thus the
  // thicker regions lead to a denser point cloud and weigh more. Is this good?

  const float CSF_large_eigenval = CSF_moment.large_eigenvalue();
  const float white_large_eigenval = white_moment.large_eigenvalue();

  float pseudo_circular_area = CSF_large_eigenval + white_large_eigenval;

  if(pseudo_circular_area < m_pseudo_area_reliability_threshold)
    pseudo_circular_area = 0.5f * (m_pseudo_area_reliability_threshold
                                   + pseudo_circular_area);

  if(pseudo_circular_area > m_pseudo_area_cutoff)
    pseudo_circular_area = square(pseudo_circular_area) / m_pseudo_area_cutoff;

  const float ret = std::min(region_size / pseudo_circular_area,
                             m_max_criterion);
  assert(std::isfinite(ret));
  return ret;
}

template <typename Tlabel>
float CortexColumnRegionQuality::
evaluate(const LabelVolume<Tlabel>& label_vol, const Tlabel label) const
{
  return evaluate(label_vol.region_begin(label),
                  label_vol.region_end(label));
}

template <typename Tlabel>
float CortexColumnRegionQuality::
evaluate(const LabelVolume<Tlabel>& label_vol,
         const Tlabel label1,
         const Tlabel label2) const
{
  typedef ChainedIterator<typename LabelVolume<Tlabel>::const_point_iterator>
    ChainedPointIterator;
  return evaluate(ChainedPointIterator(label_vol.region_begin(label1),
                                       label_vol.region_end(label1),
                                       label_vol.region_begin(label2)),
                  ChainedPointIterator(label_vol.region_end(label2)));
}

}; // namespace yl
