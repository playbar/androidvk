// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingCommon.h"

#include <QRegExp>
#include <QString>

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace MappingCommon
{
QString GetExpressionForControl(const QString& control_name,
                                const ciface::Core::DeviceQualifier& control_device,
                                const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  QString expr;

  // non-default device
  if (control_device != default_device)
  {
    expr += QString::fromStdString(control_device.ToString());
    expr += QStringLiteral(":");
  }

  // append the control name
  expr += control_name;

  if (quote == Quote::On)
  {
    QRegExp reg(QStringLiteral("[a-zA-Z]+"));
    if (!reg.exactMatch(expr))
      expr = QStringLiteral("`%1`").arg(expr);
  }

  return expr;
}

QString DetectExpression(ControlReference* reference, ciface::Core::Device* device,
                         const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  ciface::Core::Device::Control* const ctrl = reference->Detect(5000, device);

  if (ctrl)
  {
    return MappingCommon::GetExpressionForControl(QString::fromStdString(ctrl->GetName()),
                                                  default_device, default_device, quote);
  }
  return QStringLiteral("");
}
}  // namespace MappingCommon
