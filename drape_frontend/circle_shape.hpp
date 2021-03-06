#pragma once

#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/shape_view_params.hpp"

namespace df
{

class CircleShape : public MapShape
{
public:
  CircleShape(m2::PointF const & mercatorPt, CircleViewParams const & params);

  void Draw(ref_ptr<dp::Batcher> batcher, ref_ptr<dp::TextureManager> textures) const override;
  MapShapeType GetType() const override { return MapShapeType::OverlayType; }

private:
  uint64_t GetOverlayPriority() const;

  m2::PointF m_pt;
  CircleViewParams m_params;
};

} //namespace df
