#include "drape_frontend/arrow3d.hpp"

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/glconstants.hpp"
#include "drape/glextensions_list.hpp"
#include "drape/glfunctions.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/glstate.hpp"
#include "drape/gpu_program_manager.hpp"
#include "drape/shader_def.hpp"
#include "drape/texture_manager.hpp"
#include "drape/uniform_values_storage.hpp"

#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"

#include "geometry/screenbase.hpp"

namespace df
{

double const kArrowSize = 12.0;
double const kArrow3dScaleMin = 1.0;
double const kArrow3dScaleMax = 2.2;
double const kArrow3dMinZoom = 16;

int constexpr kComponentsInVertex = 4;

Arrow3d::Arrow3d()
  : m_state(gpu::ARROW_3D_PROGRAM, dp::GLState::OverlayLayer)
{
  m_vertices = {
    0.0f, 0.0f, -1.0f, 1.0f,    -1.2f, -1.0f, 0.0f, 1.0f,   0.0f, 2.0f, 0.0f, 1.0f,
    0.0f, 0.0f, -1.0f, 1.0f,    0.0f,  2.0f, 0.0f, 1.0f,    1.2f, -1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, -1.0f, 1.0f,    0.0f, -0.5f, 0.0f, 1.0f,    -1.2f, -1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, -1.0f, 1.0f,    1.2f, -1.0f, 0.0f, 1.0f,    0.0f, -0.5f, 0.0f, 1.0f,
    
    0.0f, 2.27f, 0.0f, 0.0f,    1.4f, -1.17f, 0.0f, 0.0f,   0.0f, 2.0f, 0.0f, 1.0f,
    0.0f, 2.0f, 0.0f, 1.0f,     1.4f, -1.17f, 0.0f, 0.0f,   1.2f, -1.0f, 0.0f, 1.0f,
    0.0f, 2.27f, 0.0f, 0.0f,    0.0f, 2.0f, 0.0f, 1.0f,     -1.4f, -1.17f, 0.0f, 0.0f,
    0.0f, 2.0f, 0.0f, 1.0f,     -1.2f, -1.0f, 0.0f, 1.0f,   -1.4f, -1.17f, 0.0f, 0.0f,
    
    1.2f, -1.0f, 0.0f, 1.0f,    1.4f, -1.17f, 0.0f, 0.0f,   0.0f, -0.67f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 1.0f,    1.2f, -1.0f, 0.0f, 1.0f,    0.0f, -0.67f, 0.0f, 0.0f,
    -1.2f, -1.0f, 0.0f, 1.0f,   0.0f, -0.67f, 0.0f, 0.0f,   -1.4f, -1.17f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 1.0f,    0.0f, -0.67f, 0.0f, 0.0f,   -1.2f, -1.0f, 0.0f, 1.0f,
  };

  int constexpr kVerticesInRow = 12;
  m_normals.reserve(m_vertices.size());
  for (size_t triangle = 0; triangle < m_vertices.size() / kVerticesInRow; ++triangle)
  {
    glsl::vec4 v[3];
    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
      size_t const offset = triangle * kVerticesInRow + vertex * kComponentsInVertex;
      v[vertex] = glsl::vec4(m_vertices[offset], m_vertices[offset + 1],
                             m_vertices[offset + 2], m_vertices[offset + 3]);
    }

    glsl::vec3 normal = glsl::cross(glsl::vec3(v[1].x - v[0].x, v[1].y - v[0].y, v[1].z - v[0].z),
                                    glsl::vec3(v[2].x - v[0].x, v[2].y - v[0].y, v[2].z - v[0].z));
    normal = glsl::normalize(normal);

    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
      m_normals.push_back(normal.x);
      m_normals.push_back(normal.y);
      m_normals.push_back(normal.z);
    }
  }
}

Arrow3d::~Arrow3d()
{
  if (m_bufferId != 0)
    GLFunctions::glDeleteBuffer(m_bufferId);

  if (m_bufferNormalsId != 0)
    GLFunctions::glDeleteBuffer(m_bufferNormalsId);
}

void Arrow3d::SetPosition(const m2::PointD & position)
{
  m_position = position;
}

void Arrow3d::SetAzimuth(double azimuth)
{
  m_azimuth = azimuth;
}

void Arrow3d::SetTexture(ref_ptr<dp::TextureManager> texMng)
{
  m_state.SetColorTexture(texMng->GetSymbolsTexture());
}

void Arrow3d::Build()
{
  m_bufferId = GLFunctions::glGenBuffer();
  GLFunctions::glBindBuffer(m_bufferId, gl_const::GLArrayBuffer);
  GLFunctions::glBufferData(gl_const::GLArrayBuffer, m_vertices.size() * sizeof(m_vertices[0]),
                            m_vertices.data(), gl_const::GLStaticDraw);

  m_bufferNormalsId = GLFunctions::glGenBuffer();
  GLFunctions::glBindBuffer(m_bufferNormalsId, gl_const::GLArrayBuffer);
  GLFunctions::glBufferData(gl_const::GLArrayBuffer, m_normals.size() * sizeof(m_normals[0]),
                            m_normals.data(), gl_const::GLStaticDraw);

  GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
}

void Arrow3d::SetPositionObsolete(bool obsolete)
{
  m_obsoletePosition = obsolete;
}

void Arrow3d::Render(ScreenBase const & screen, ref_ptr<dp::GpuProgramManager> mng)
{
  // Unbind current VAO, because glVertexAttributePointer and glEnableVertexAttribute can affect it.
  if (dp::GLExtensionsList::Instance().IsSupported(dp::GLExtensionsList::VertexArrayObject))
    GLFunctions::glBindVertexArray(0);

  if (!m_isInitialized)
  {
    Build();
    m_isInitialized = true;
  }

  // Render shadow.
  if (screen.isPerspective())
  {
    ref_ptr<dp::GpuProgram> shadowProgram = mng->GetProgram(gpu::ARROW_3D_SHADOW_PROGRAM);
    RenderArrow(screen, shadowProgram, dp::Color(60, 60, 60, 60), 0.05f, false /* hasNormals */);
  }

  // Render arrow.
  ref_ptr<dp::GpuProgram> arrowProgram = mng->GetProgram(gpu::ARROW_3D_PROGRAM);
  dp::Color const color = df::GetColorConstant(GetStyleReader().GetCurrentStyle(),
                                               m_obsoletePosition ? df::Arrow3DObsolete : df::Arrow3D);
  RenderArrow(screen, arrowProgram, color, 0.0f, true /* hasNormals */);

  arrowProgram->Unbind();
  GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
}

void Arrow3d::RenderArrow(ScreenBase const & screen, ref_ptr<dp::GpuProgram> program,
                          dp::Color const & color, float dz, bool hasNormals)
{
  program->Bind();

  GLFunctions::glBindBuffer(m_bufferId, gl_const::GLArrayBuffer);
  uint32_t const attributePosition = program->GetAttributeLocation("a_pos");
  ASSERT_NOT_EQUAL(attributePosition, -1, ());
  GLFunctions::glEnableVertexAttribute(attributePosition);
  GLFunctions::glVertexAttributePointer(attributePosition, kComponentsInVertex,
                                        gl_const::GLFloatType, false, 0, 0);
  
  if (hasNormals)
  {
    GLFunctions::glBindBuffer(m_bufferNormalsId, gl_const::GLArrayBuffer);
    uint32_t const attributeNormal = program->GetAttributeLocation("a_normal");
    ASSERT_NOT_EQUAL(attributeNormal, -1, ());
    GLFunctions::glEnableVertexAttribute(attributeNormal);
    GLFunctions::glVertexAttributePointer(attributeNormal, 3, gl_const::GLFloatType, false, 0, 0);
  }
  
  dp::UniformValuesStorage uniforms;
  math::Matrix<float, 4, 4> const modelTransform = CalculateTransform(screen, dz);
  uniforms.SetMatrix4x4Value("m_transform", modelTransform.m_data);
  glsl::vec4 const c = glsl::ToVec4(color);
  uniforms.SetFloatValue("u_color", c.r, c.g, c.b, c.a);
  dp::ApplyState(m_state, program);
  dp::ApplyUniforms(uniforms, program);
  GLFunctions::glDrawArrays(gl_const::GLTriangles, 0, m_vertices.size() / kComponentsInVertex);
}

math::Matrix<float, 4, 4> Arrow3d::CalculateTransform(ScreenBase const & screen, float dz) const
{
  double arrowScale = VisualParams::Instance().GetVisualScale() * kArrowSize;
  if (screen.isPerspective())
  {
    static double const kLog2 = log(2.0);
    double const kMaxZoom = scales::UPPER_STYLE_SCALE + 1.0;
    double const zoomLevel = my::clamp(fabs(log(screen.GetScale()) / kLog2), kArrow3dMinZoom, kMaxZoom);
    double const t = (zoomLevel - kArrow3dMinZoom) / (kMaxZoom - kArrow3dMinZoom);
    arrowScale *= (kArrow3dScaleMin * (1.0 - t) + kArrow3dScaleMax * t);
  }

  double const scaleX = arrowScale * 2.0 / screen.PixelRect().SizeX();
  double const scaleY = arrowScale * 2.0 / screen.PixelRect().SizeY();
  double const scaleZ = screen.isPerspective() ? (0.002 * screen.GetDepth3d()) : 1.0;

  m2::PointD const pos = screen.GtoP(m_position);
  double const dX = 2.0 * pos.x / screen.PixelRect().SizeX() - 1.0;
  double const dY = 2.0 * pos.y / screen.PixelRect().SizeY() - 1.0;

  math::Matrix<float, 4, 4> scaleM = math::Identity<float, 4>();
  scaleM(0, 0) = scaleX;
  scaleM(1, 1) = scaleY;
  scaleM(2, 2) = scaleZ;

  math::Matrix<float, 4, 4> rotateM = math::Identity<float, 4>();
  rotateM(0, 0) = cos(m_azimuth + screen.GetAngle());
  rotateM(0, 1) = -sin(m_azimuth + screen.GetAngle());
  rotateM(1, 0) = -rotateM(0, 1);
  rotateM(1, 1) = rotateM(0, 0);

  math::Matrix<float, 4, 4> translateM = math::Identity<float, 4>();
  translateM(3, 0) = dX;
  translateM(3, 1) = -dY;
  translateM(3, 2) = dz;

  math::Matrix<float, 4, 4> modelTransform = rotateM * scaleM * translateM;
  if (screen.isPerspective())
    return modelTransform * math::Matrix<float, 4, 4>(screen.Pto3dMatrix());

  return modelTransform;
}

}  // namespace df
