// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingIndicator.h"

#include <array>
#include <cmath>
#include <numeric>

#include <fmt/format.h>

#include <QAction>
#include <QDateTime>
#include <QPainter>
#include <QTimer>

#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/Device.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

namespace
{
const QColor C_STICK_GATE_COLOR = Qt::yellow;
const QColor CURSOR_TV_COLOR = 0xaed6f1;
const QColor TILT_GATE_COLOR = 0xa2d9ce;
const QColor SWING_GATE_COLOR = 0xcea2d9;

constexpr int INPUT_DOT_RADIUS = 2;
}  // namespace

QPen MappingIndicator::GetBBoxPen() const
{
  return palette().shadow().color();
}

QBrush MappingIndicator::GetBBoxBrush() const
{
  return palette().base();
}

QColor MappingIndicator::GetRawInputColor() const
{
  return palette().shadow().color();
}

QPen MappingIndicator::GetInputShapePen() const
{
  return QPen{GetRawInputColor(), 1.0, Qt::DashLine};
}

QColor MappingIndicator::GetAdjustedInputColor() const
{
  // Using highlight color works (typically blue) but the contrast is pretty low.
  // return palette().highlight().color();
  return Qt::red;
}

QColor MappingIndicator::GetCenterColor() const
{
  return Qt::blue;
}

QColor MappingIndicator::GetDeadZoneColor() const
{
  return palette().shadow().color();
}

QPen MappingIndicator::GetDeadZonePen() const
{
  return GetDeadZoneColor();
}

QBrush MappingIndicator::GetDeadZoneBrush() const
{
  return QBrush{GetDeadZoneColor(), Qt::BDiagPattern};
}

QColor MappingIndicator::GetTextColor() const
{
  return palette().text().color();
}

// Text color that is visible atop GetAdjustedInputColor():
QColor MappingIndicator::GetAltTextColor() const
{
  return palette().highlightedText().color();
}

QColor MappingIndicator::GetGateColor() const
{
  return palette().mid().color();
}

MappingIndicator::MappingIndicator(ControllerEmu::ControlGroup* group) : m_group(group)
{
  // TODO: Make these magic numbers less ugly.
  int required_height = 106;

  if (group && ControllerEmu::GroupType::MixedTriggers == group->type)
    required_height = 64 + 1;

  setFixedHeight(required_height);
}

double MappingIndicator::GetScale() const
{
  return height() / 2 - 2;
}

namespace
{
constexpr float SPHERE_SIZE = 0.7f;
constexpr float SPHERE_INDICATOR_DIST = 0.85f;
constexpr int SPHERE_POINT_COUNT = 200;

// Constructs a polygon by querying a radius at varying angles:
template <typename F>
QPolygonF GetPolygonFromRadiusGetter(F&& radius_getter, double scale,
                                     Common::DVec2 center = {0.0, 0.0})
{
  // A multiple of 8 (octagon) and enough points to be visibly pleasing:
  constexpr int shape_point_count = 32;
  QPolygonF shape{shape_point_count};

  int p = 0;
  for (auto& point : shape)
  {
    const double angle = MathUtil::TAU * p / shape.size();
    const double radius = radius_getter(angle) * scale;

    point = {std::cos(angle) * radius + center.x * scale,
             std::sin(angle) * radius + center.y * scale};
    ++p;
  }

  return shape;
}

// Used to check if the user seems to have attempted proper calibration.
bool IsCalibrationDataSensible(const ControllerEmu::ReshapableInput::CalibrationData& data)
{
  // Test that the average input radius is not below a threshold.
  // This will make sure the user has actually moved their stick from neutral.

  // Even the GC controller's small range would pass this test.
  constexpr double REASONABLE_AVERAGE_RADIUS = 0.6;

  const double sum = std::accumulate(data.begin(), data.end(), 0.0);
  const double mean = sum / data.size();

  if (mean < REASONABLE_AVERAGE_RADIUS)
  {
    return false;
  }

  // Test that the standard deviation is below a threshold.
  // This will make sure the user has not just filled in one side of their input.

  // Approx. deviation of a square input gate, anything much more than that would be unusual.
  constexpr double REASONABLE_DEVIATION = 0.14;

  // Population standard deviation.
  const double square_sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
  const double standard_deviation = std::sqrt(square_sum / data.size() - mean * mean);

  return standard_deviation < REASONABLE_DEVIATION;
}

// Used to test for a miscalibrated stick so the user can be informed.
bool IsPointOutsideCalibration(Common::DVec2 point, ControllerEmu::ReshapableInput& input)
{
  const auto center = input.GetCenter();
  const double current_radius = (point - center).Length();
  const double input_radius = input.GetInputRadiusAtAngle(
      std::atan2(point.y - center.y, point.x - center.x) + MathUtil::TAU);

  constexpr double ALLOWED_ERROR = 1.3;

  return current_radius > input_radius * ALLOWED_ERROR;
}

template <typename F>
void GenerateFibonacciSphere(int point_count, F&& callback)
{
  const float golden_angle = MathUtil::PI * (3.f - std::sqrt(5.f));

  for (int i = 0; i != point_count; ++i)
  {
    const float z = (1.f / point_count - 1.f) + (2.f / point_count) * i;
    const float r = std::sqrt(1.f - z * z);
    const float x = std::cos(golden_angle * i) * r;
    const float y = std::sin(golden_angle * i) * r;

    callback(Common::Vec3{x, y, z});
  }
}

}  // namespace

void MappingIndicator::DrawCursor(ControllerEmu::Cursor& cursor)
{
  const auto center = cursor.GetCenter();

  const QColor tv_brush_color = CURSOR_TV_COLOR;
  const QColor tv_pen_color = tv_brush_color.darker(125);

  const auto raw_coord = cursor.GetState(false);
  const auto adj_coord = cursor.GetState(true);

  UpdateCalibrationWidget({raw_coord.x, raw_coord.y});

  // Bounding box size:
  const double scale = GetScale();

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  if (IsCalibrating())
  {
    DrawCalibration(p, {raw_coord.x, raw_coord.y});
    return;
  }

  // TV screen or whatever you want to call this:
  constexpr double TV_SCALE = 0.75;

  p.setPen(tv_pen_color);
  p.setBrush(tv_brush_color);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&cursor](double ang) { return cursor.GetGateRadiusAtAngle(ang); }, scale * TV_SCALE));

  // Deadzone.
  p.setPen(GetDeadZonePen());
  p.setBrush(GetDeadZoneBrush());
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&cursor](double ang) { return cursor.GetDeadzoneRadiusAtAngle(ang); }, scale, center));

  // Input shape.
  p.setPen(GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&cursor](double ang) { return cursor.GetInputRadiusAtAngle(ang); }, scale, center));

  // Center.
  if (center.x || center.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetCenterColor());
    p.drawEllipse(QPointF{center.x, center.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Raw stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Adjusted cursor position (if not hidden):
  if (adj_coord.IsVisible())
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetAdjustedInputColor());
    p.drawEllipse(QPointF{adj_coord.x, adj_coord.y} * scale * TV_SCALE, INPUT_DOT_RADIUS,
                  INPUT_DOT_RADIUS);
  }
}

void MappingIndicator::DrawReshapableInput(ControllerEmu::ReshapableInput& stick)
{
  // Some hacks for pretty colors:
  const bool is_c_stick = m_group->name == "C-Stick";
  const bool is_tilt = m_group->name == "Tilt";

  const auto center = stick.GetCenter();

  QColor gate_brush_color = GetGateColor();

  if (is_c_stick)
    gate_brush_color = C_STICK_GATE_COLOR;
  else if (is_tilt)
    gate_brush_color = TILT_GATE_COLOR;

  const QColor gate_pen_color = gate_brush_color.darker(125);

  const auto raw_coord = stick.GetReshapableState(false);

  Common::DVec2 adj_coord;
  if (is_tilt)
  {
    WiimoteEmu::EmulateTilt(&m_motion_state, static_cast<ControllerEmu::Tilt*>(&stick),
                            1.f / INDICATOR_UPDATE_FREQ);
    adj_coord = Common::DVec2{-m_motion_state.angle.y, m_motion_state.angle.x} / MathUtil::PI;
  }
  else
  {
    adj_coord = stick.GetReshapableState(true);
  }

  UpdateCalibrationWidget(raw_coord);

  // Bounding box size:
  const double scale = GetScale();

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  if (IsCalibrating())
  {
    DrawCalibration(p, raw_coord);
    return;
  }

  // Input gate. (i.e. the octagon shape)
  p.setPen(gate_pen_color);
  p.setBrush(gate_brush_color);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetGateRadiusAtAngle(ang); }, scale));

  // Deadzone.
  p.setPen(GetDeadZonePen());
  p.setBrush(GetDeadZoneBrush());
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetDeadzoneRadiusAtAngle(ang); }, scale, center));

  // Input shape.
  p.setPen(GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetInputRadiusAtAngle(ang); }, scale, center));

  // Center.
  if (center.x || center.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetCenterColor());
    p.drawEllipse(QPointF{center.x, center.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Raw stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Adjusted stick position.
  if (adj_coord.x || adj_coord.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetAdjustedInputColor());
    p.drawEllipse(QPointF{adj_coord.x, adj_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }
}

void MappingIndicator::DrawMixedTriggers()
{
  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  const auto& triggers = *static_cast<ControllerEmu::MixedTriggers*>(m_group);
  const ControlState threshold = triggers.GetThreshold();
  const ControlState deadzone = triggers.GetDeadzone();

  // MixedTriggers interface is a bit ugly:
  constexpr int TRIGGER_COUNT = 2;
  std::array<ControlState, TRIGGER_COUNT> raw_analog_state;
  std::array<ControlState, TRIGGER_COUNT> adj_analog_state;
  const std::array<u16, TRIGGER_COUNT> button_masks = {0x1, 0x2};
  u16 button_state = 0;

  triggers.GetState(&button_state, button_masks.data(), raw_analog_state.data(), false);
  triggers.GetState(&button_state, button_masks.data(), adj_analog_state.data(), true);

  // Rectangle sizes:
  const int trigger_height = 32;
  const int trigger_width = width() - 1;
  const int trigger_button_width = 32;
  const int trigger_analog_width = trigger_width - trigger_button_width;

  // Bounding box background:
  p.setPen(Qt::NoPen);
  p.setBrush(GetBBoxBrush());
  p.drawRect(0, 0, trigger_width, trigger_height * TRIGGER_COUNT);

  for (int t = 0; t != TRIGGER_COUNT; ++t)
  {
    const double raw_analog = raw_analog_state[t];
    const double adj_analog = adj_analog_state[t];
    const bool trigger_button = button_state & button_masks[t];
    auto const analog_name = QString::fromStdString(triggers.controls[TRIGGER_COUNT + t]->ui_name);
    auto const button_name = QString::fromStdString(triggers.controls[t]->ui_name);

    const QRectF trigger_rect(0, 0, trigger_width, trigger_height);

    const QRectF analog_rect(0, 0, trigger_analog_width, trigger_height);

    // Unactivated analog text:
    p.setPen(GetTextColor());
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);

    const QRectF adj_analog_rect(0, 0, adj_analog * trigger_analog_width, trigger_height);

    // Trigger analog:
    p.setPen(Qt::NoPen);
    p.setBrush(GetRawInputColor());
    p.drawEllipse(QPoint(raw_analog * trigger_analog_width, trigger_height - INPUT_DOT_RADIUS),
                  INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
    p.setBrush(GetAdjustedInputColor());
    p.drawRect(adj_analog_rect);

    // Deadzone:
    p.setPen(GetDeadZonePen());
    p.setBrush(GetDeadZoneBrush());
    p.drawRect(0, 0, trigger_analog_width * deadzone, trigger_height);

    // Threshold setting:
    const int threshold_x = trigger_analog_width * threshold;
    p.setPen(GetInputShapePen());
    p.drawLine(threshold_x, 0, threshold_x, trigger_height);

    const QRectF button_rect(trigger_analog_width, 0, trigger_button_width, trigger_height);

    // Trigger button:
    p.setPen(GetBBoxPen());
    p.setBrush(trigger_button ? GetAdjustedInputColor() : GetBBoxBrush());
    p.drawRect(button_rect);

    // Bounding box outline:
    p.setPen(GetBBoxPen());
    p.setBrush(Qt::NoBrush);
    p.drawRect(trigger_rect);

    // Button text:
    p.setPen(GetTextColor());
    p.setPen(trigger_button ? GetAltTextColor() : GetTextColor());
    p.drawText(button_rect, Qt::AlignCenter, button_name);

    // Activated analog text:
    p.setPen(GetAltTextColor());
    p.setClipping(true);
    p.setClipRect(adj_analog_rect);
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);
    p.setClipping(false);

    // Move down for next trigger:
    p.translate(0.0, trigger_height);
  }
}

void MappingIndicator::DrawForce(ControllerEmu::Force& force)
{
  const auto center = force.GetCenter();

  const QColor gate_brush_color = SWING_GATE_COLOR;
  const QColor gate_pen_color = gate_brush_color.darker(125);

  const auto raw_coord = force.GetState(false);
  WiimoteEmu::EmulateSwing(&m_motion_state, &force, 1.f / INDICATOR_UPDATE_FREQ);
  const auto& adj_coord = m_motion_state.position;

  UpdateCalibrationWidget({raw_coord.x, raw_coord.y});

  // Bounding box size:
  const double scale = GetScale();

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  if (IsCalibrating())
  {
    DrawCalibration(p, {raw_coord.x, raw_coord.y});
    return;
  }

  // Deadzone for Z (forward/backward):
  const double deadzone = force.GetDeadzonePercentage();
  if (deadzone > 0.0)
  {
    p.setPen(GetDeadZonePen());
    p.setBrush(GetDeadZoneBrush());
    p.drawRect(QRectF(-scale, -deadzone * scale, scale * 2, deadzone * scale * 2));
  }

  // Raw Z:
  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());
  p.drawRect(
      QRectF(-scale, raw_coord.z * scale - INPUT_DOT_RADIUS / 2, scale * 2, INPUT_DOT_RADIUS));

  // Adjusted Z:
  const auto curve_point =
      std::max(std::abs(m_motion_state.angle.x), std::abs(m_motion_state.angle.z)) / MathUtil::TAU;
  if (adj_coord.y || curve_point)
  {
    // Show off the angle somewhat with a curved line.
    QPainterPath path;
    path.moveTo(-scale, (adj_coord.y + curve_point) * -scale);
    path.quadTo({0, (adj_coord.y - curve_point) * -scale},
                {scale, (adj_coord.y + curve_point) * -scale});

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(GetAdjustedInputColor(), INPUT_DOT_RADIUS));
    p.drawPath(path);
  }

  // Draw "gate" shape.
  p.setPen(gate_pen_color);
  p.setBrush(gate_brush_color);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&force](double ang) { return force.GetGateRadiusAtAngle(ang); }, scale));

  // Deadzone.
  p.setPen(GetDeadZoneColor());
  p.setBrush(GetDeadZoneBrush());
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&force](double ang) { return force.GetDeadzoneRadiusAtAngle(ang); }, scale, center));

  // Input shape.
  p.setPen(GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&force](double ang) { return force.GetInputRadiusAtAngle(ang); }, scale, center));

  // Center.
  if (center.x || center.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetCenterColor());
    p.drawEllipse(QPointF{center.x, center.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Raw stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Adjusted position:
  if (adj_coord.x || adj_coord.z)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetAdjustedInputColor());
    p.drawEllipse(QPointF{-adj_coord.x, adj_coord.z} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }
}

void MappingIndicator::paintEvent(QPaintEvent*)
{
  switch (m_group->type)
  {
  case ControllerEmu::GroupType::Cursor:
    DrawCursor(*static_cast<ControllerEmu::Cursor*>(m_group));
    break;
  case ControllerEmu::GroupType::Stick:
  case ControllerEmu::GroupType::Tilt:
    DrawReshapableInput(*static_cast<ControllerEmu::ReshapableInput*>(m_group));
    break;
  case ControllerEmu::GroupType::MixedTriggers:
    DrawMixedTriggers();
    break;
  case ControllerEmu::GroupType::Force:
    DrawForce(*static_cast<ControllerEmu::Force*>(m_group));
    break;
  default:
    break;
  }
}

ShakeMappingIndicator::ShakeMappingIndicator(ControllerEmu::Shake* group)
    : MappingIndicator(group), m_shake_group(*group)
{
}

void ShakeMappingIndicator::paintEvent(QPaintEvent*)
{
  DrawShake();
}

void ShakeMappingIndicator::DrawShake()
{
  constexpr std::size_t HISTORY_COUNT = INDICATOR_UPDATE_FREQ;

  WiimoteEmu::EmulateShake(&m_motion_state, &m_shake_group, 1.f / INDICATOR_UPDATE_FREQ);

  constexpr float MAX_DISTANCE = 0.5f;

  m_position_samples.push_front(m_motion_state.position / MAX_DISTANCE);
  // This also holds the current state so +1.
  if (m_position_samples.size() > HISTORY_COUNT + 1)
    m_position_samples.pop_back();

  // Bounding box size:
  const double scale = GetScale();

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of acceleration Z.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Deadzone.
  p.setPen(GetDeadZonePen());
  p.setBrush(GetDeadZoneBrush());
  p.drawRect(-scale, 0, scale * 2, m_shake_group.GetDeadzone() * scale);

  // Raw input.
  const auto raw_coord = m_shake_group.GetState(false);
  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());
  for (std::size_t c = 0; c != raw_coord.data.size(); ++c)
  {
    p.drawEllipse(QPointF{-0.5 + c * 0.5, raw_coord.data[c]} * scale, INPUT_DOT_RADIUS,
                  INPUT_DOT_RADIUS);
  }

  // Grid line.
  if (m_grid_line_position ||
      std::any_of(m_position_samples.begin(), m_position_samples.end(),
                  [](const Common::Vec3& v) { return v.LengthSquared() != 0.0; }))
  {
    // Only start moving the line if there's non-zero data.
    m_grid_line_position = (m_grid_line_position + 1) % HISTORY_COUNT;
  }
  const double grid_line_x = 1.0 - m_grid_line_position * 2.0 / HISTORY_COUNT;
  p.setPen(GetRawInputColor());
  p.drawLine(QPointF{grid_line_x, -1.0} * scale, QPointF{grid_line_x, 1.0} * scale);

  // Position history.
  const QColor component_colors[] = {Qt::red, Qt::green, Qt::blue};
  p.setBrush(Qt::NoBrush);
  for (std::size_t c = 0; c != raw_coord.data.size(); ++c)
  {
    QPolygonF polyline;

    int i = 0;
    for (auto& sample : m_position_samples)
    {
      polyline.append(QPointF{1.0 - i * 2.0 / HISTORY_COUNT, sample.data[c]} * scale);
      ++i;
    }

    p.setPen(component_colors[c]);
    p.drawPolyline(polyline);
  }
}

AccelerometerMappingIndicator::AccelerometerMappingIndicator(ControllerEmu::IMUAccelerometer* group)
    : MappingIndicator(group), m_accel_group(*group)
{
}

void AccelerometerMappingIndicator::paintEvent(QPaintEvent*)
{
  const auto accel_state = m_accel_group.GetState();
  const auto state = accel_state.value_or(Common::Vec3{});

  // Bounding box size:
  const double scale = GetScale();

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI axes are opposite that of Wii remote accelerometer.
  p.scale(-1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const auto angle = std::acos(state.Normalized().Dot({0, 0, 1}));
  const auto axis = state.Normalized().Cross({0, 0, 1}).Normalized();

  // Odd checks to handle case of 0g (draw no sphere) and perfect up/down orientation.
  const auto rotation = (!state.LengthSquared() || axis.LengthSquared() < 2) ?
                            Common::Matrix33::Rotate(angle, axis) :
                            Common::Matrix33::Identity();

  // Draw sphere.
  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());

  GenerateFibonacciSphere(SPHERE_POINT_COUNT, [&](const Common::Vec3& point) {
    const auto pt = rotation * point;

    if (pt.y > 0)
      p.drawEllipse(QPointF(pt.x, pt.z) * scale * SPHERE_SIZE, 0.5f, 0.5f);
  });

  // Sphere outline.
  p.setPen(GetRawInputColor());
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF{}, scale * SPHERE_SIZE, scale * SPHERE_SIZE);

  // Red dot upright target.
  p.setPen(QPen(GetAdjustedInputColor(), INPUT_DOT_RADIUS / 2));
  p.drawEllipse(QPointF{0, SPHERE_INDICATOR_DIST} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Red dot.
  const auto point = rotation * Common::Vec3{0, 0, SPHERE_INDICATOR_DIST};
  if (point.y > 0 || Common::Vec2(point.x, point.z).Length() > SPHERE_SIZE)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetAdjustedInputColor());
    p.drawEllipse(QPointF(point.x, point.z) * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Blue dot target.
  p.setPen(QPen(Qt::blue, INPUT_DOT_RADIUS / 2));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF{0, -SPHERE_INDICATOR_DIST} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Blue dot.
  const auto point2 = -point;
  if (point2.y > 0 || Common::Vec2(point2.x, point2.z).Length() > SPHERE_SIZE)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::blue);
    p.drawEllipse(QPointF(point2.x, point2.z) * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Only draw g-force text if acceleration data is present.
  if (!accel_state.has_value())
    return;

  // G-force text:
  p.setPen(GetTextColor());
  p.scale(-1.0, -1.0);
  p.drawText(QRectF(-2, 0, scale, scale), Qt::AlignBottom | Qt::AlignRight,
             QString::fromStdString(
                 // i18n: "g" is the symbol for "gravitational force equivalent" (g-force).
                 fmt::format("{:.2f} g", state.Length() / WiimoteEmu::GRAVITY_ACCELERATION)));
}

GyroMappingIndicator::GyroMappingIndicator(ControllerEmu::IMUGyroscope* group)
    : MappingIndicator(group), m_gyro_group(*group), m_state(Common::Matrix33::Identity())
{
}

void GyroMappingIndicator::paintEvent(QPaintEvent*)
{
  const auto gyro_state = m_gyro_group.GetState();
  const auto angular_velocity = gyro_state.value_or(Common::Vec3{});

  m_state *= Common::Matrix33::RotateX(angular_velocity.x / -INDICATOR_UPDATE_FREQ) *
             Common::Matrix33::RotateY(angular_velocity.y / INDICATOR_UPDATE_FREQ) *
             Common::Matrix33::RotateZ(angular_velocity.z / -INDICATOR_UPDATE_FREQ);

  // Reset orientation when stable for a bit:
  constexpr u32 STABLE_RESET_STEPS = INDICATOR_UPDATE_FREQ;
  // This works well with my DS4 but a potentially noisy device might not behave.
  const bool is_stable = angular_velocity.Length() < MathUtil::TAU / 30;

  if (!is_stable)
    m_stable_steps = 0;
  else if (m_stable_steps != STABLE_RESET_STEPS)
    ++m_stable_steps;

  if (STABLE_RESET_STEPS == m_stable_steps)
    m_state = Common::Matrix33::Identity();

  // Use an empty rotation matrix if gyroscope data is not present.
  const auto rotation = (gyro_state.has_value() ? m_state : Common::Matrix33{});

  // Bounding box size:
  const double scale = GetScale();

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  p.setPen(Qt::NoPen);
  p.setBrush(GetRawInputColor());

  GenerateFibonacciSphere(SPHERE_POINT_COUNT, [&, this](const Common::Vec3& point) {
    const auto pt = rotation * point;

    if (pt.y > 0)
      p.drawEllipse(QPointF(pt.x, pt.z) * scale * SPHERE_SIZE, 0.5f, 0.5f);
  });

  // Sphere outline.
  p.setPen(GetRawInputColor());
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF{}, scale * SPHERE_SIZE, scale * SPHERE_SIZE);

  // Red dot upright target.
  p.setPen(QPen(GetAdjustedInputColor(), INPUT_DOT_RADIUS / 2));
  p.drawEllipse(QPointF{0, -SPHERE_INDICATOR_DIST} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Red dot.
  const auto point = rotation * Common::Vec3{0, 0, -SPHERE_INDICATOR_DIST};
  if (point.y > 0 || Common::Vec2(point.x, point.z).Length() > SPHERE_SIZE)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetAdjustedInputColor());
    p.drawEllipse(QPointF(point.x, point.z) * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Blue dot target.
  p.setPen(QPen(Qt::blue, INPUT_DOT_RADIUS / 2));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF{}, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Blue dot.
  const auto point2 = rotation * Common::Vec3{0, SPHERE_INDICATOR_DIST, 0};
  if (point2.y > 0 || Common::Vec2(point2.x, point2.z).Length() > SPHERE_SIZE)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::blue);
    p.drawEllipse(QPointF(point2.x, point2.z) * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Only draw text if data is present.
  if (!gyro_state.has_value())
    return;

  // Angle of red dot from starting position.
  const auto angle = std::acos(point.Normalized().Dot({0, 0, -1}));

  // Angle text:
  p.setPen(GetTextColor());
  p.drawText(QRectF(-2, 0, scale, scale), Qt::AlignBottom | Qt::AlignRight,
             // i18n: "°" is the symbol for degrees (angular measurement).
             QString::fromStdString(fmt::format("{:.2f} °", angle / MathUtil::TAU * 360)));
}

void MappingIndicator::DrawCalibration(QPainter& p, Common::DVec2 point)
{
  // Bounding box size:
  const double scale = GetScale();
  const auto center = m_calibration_widget->GetCenter();

  // Input shape.
  p.setPen(GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [this](double angle) { return m_calibration_widget->GetCalibrationRadiusAtAngle(angle); },
      scale, center));

  // Center.
  if (center.x || center.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(GetCenterColor());
    p.drawEllipse(QPointF{center.x, center.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }

  // Stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(GetAdjustedInputColor());
  p.drawEllipse(QPointF{point.x, point.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
}

void MappingIndicator::UpdateCalibrationWidget(Common::DVec2 point)
{
  if (m_calibration_widget)
    m_calibration_widget->Update(point);
}

bool MappingIndicator::IsCalibrating() const
{
  return m_calibration_widget && m_calibration_widget->IsCalibrating();
}

void MappingIndicator::SetCalibrationWidget(CalibrationWidget* widget)
{
  m_calibration_widget = widget;
}

CalibrationWidget::CalibrationWidget(ControllerEmu::ReshapableInput& input,
                                     MappingIndicator& indicator)
    : m_input(input), m_indicator(indicator), m_completion_action{}
{
  m_indicator.SetCalibrationWidget(this);

  // Make it more apparent that this is a menu with more options.
  setPopupMode(ToolButtonPopupMode::MenuButtonPopup);

  SetupActions();

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  m_informative_timer = new QTimer(this);
  connect(m_informative_timer, &QTimer::timeout, this, [this] {
    // If the user has started moving we'll assume they know what they are doing.
    if (*std::max_element(m_calibration_data.begin(), m_calibration_data.end()) > 0.5)
      return;

    ModalMessageBox::information(
        this, tr("Calibration"),
        tr("For best results please slowly move your input to all possible regions."));
  });
  m_informative_timer->setSingleShot(true);
}

void CalibrationWidget::SetupActions()
{
  const auto calibrate_action = new QAction(tr("Calibrate"), this);
  const auto center_action = new QAction(tr("Center and Calibrate"), this);
  const auto reset_action = new QAction(tr("Reset"), this);

  connect(calibrate_action, &QAction::triggered, [this]() {
    StartCalibration();
    m_input.SetCenter({0, 0});
  });
  connect(center_action, &QAction::triggered, [this]() {
    StartCalibration();
    m_is_centering = true;
  });
  connect(reset_action, &QAction::triggered, [this]() {
    m_input.SetCalibrationToDefault();
    m_input.SetCenter({0, 0});
  });

  for (auto* action : actions())
    removeAction(action);

  addAction(calibrate_action);
  addAction(center_action);
  addAction(reset_action);
  setDefaultAction(calibrate_action);

  m_completion_action = new QAction(tr("Finish Calibration"), this);
  connect(m_completion_action, &QAction::triggered, [this]() {
    m_input.SetCenter(m_new_center);
    m_input.SetCalibrationData(std::move(m_calibration_data));
    m_informative_timer->stop();
    SetupActions();
  });
}

void CalibrationWidget::StartCalibration()
{
  m_calibration_data.assign(m_input.CALIBRATION_SAMPLE_COUNT, 0.0);

  m_new_center = {0, 0};

  // Cancel calibration.
  const auto cancel_action = new QAction(tr("Cancel Calibration"), this);
  connect(cancel_action, &QAction::triggered, [this]() {
    m_calibration_data.clear();
    m_informative_timer->stop();
    SetupActions();
  });

  for (auto* action : actions())
    removeAction(action);

  addAction(cancel_action);
  addAction(m_completion_action);
  setDefaultAction(cancel_action);

  // If the user doesn't seem to know what they are doing after a bit inform them.
  m_informative_timer->start(2000);
}

void CalibrationWidget::Update(Common::DVec2 point)
{
  QFont f = parentWidget()->font();
  QPalette p = parentWidget()->palette();

  if (m_is_centering)
  {
    m_new_center = point;
    m_is_centering = false;
  }
  else if (IsCalibrating())
  {
    m_input.UpdateCalibrationData(m_calibration_data, point - m_new_center);

    if (IsCalibrationDataSensible(m_calibration_data))
    {
      setDefaultAction(m_completion_action);
    }
  }
  else if (IsPointOutsideCalibration(point, m_input))
  {
    // Bold and red on miscalibration.
    f.setBold(true);
    p.setColor(QPalette::ButtonText, Qt::red);
  }

  setFont(f);
  setPalette(p);
}

bool CalibrationWidget::IsCalibrating() const
{
  return !m_calibration_data.empty();
}

double CalibrationWidget::GetCalibrationRadiusAtAngle(double angle) const
{
  return m_input.GetCalibrationDataRadiusAtAngle(m_calibration_data, angle);
}

Common::DVec2 CalibrationWidget::GetCenter() const
{
  return m_new_center;
}
