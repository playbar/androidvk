// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/TAS/GCTASInputWindow.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"

#include "InputCommon/GCPadStatus.h"

GCTASInputWindow::GCTASInputWindow(QWidget* parent, int num) : TASInputWindow(parent)
{
  setWindowTitle(tr("GameCube TAS Input %1").arg(num + 1));

  m_main_stick_box = CreateStickInputs(tr("Main Stick"), m_x_main_stick_value, m_y_main_stick_value,
                                       255, 255, Qt::Key_F, Qt::Key_G);
  m_c_stick_box = CreateStickInputs(tr("C Stick"), m_x_c_stick_value, m_y_c_stick_value, 255, 255,
                                    Qt::Key_H, Qt::Key_J);

  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(m_main_stick_box);
  top_layout->addWidget(m_c_stick_box);

  m_triggers_box = new QGroupBox(tr("Triggers"));

  auto* l_trigger_layout =
      CreateSliderValuePairLayout(tr("Left"), m_l_trigger_value, 255, Qt::Key_N, m_triggers_box);
  auto* r_trigger_layout =
      CreateSliderValuePairLayout(tr("Right"), m_r_trigger_value, 255, Qt::Key_M, m_triggers_box);

  auto* triggers_layout = new QVBoxLayout;
  triggers_layout->addLayout(l_trigger_layout);
  triggers_layout->addLayout(r_trigger_layout);
  m_triggers_box->setLayout(triggers_layout);

  m_a_button = new QCheckBox(QStringLiteral("&A"));
  m_b_button = new QCheckBox(QStringLiteral("&B"));
  m_x_button = new QCheckBox(QStringLiteral("&X"));
  m_y_button = new QCheckBox(QStringLiteral("&Y"));
  m_z_button = new QCheckBox(QStringLiteral("&Z"));
  m_l_button = new QCheckBox(QStringLiteral("&L"));
  m_r_button = new QCheckBox(QStringLiteral("&R"));
  m_start_button = new QCheckBox(QStringLiteral("&START"));
  m_left_button = new QCheckBox(QStringLiteral("L&eft"));
  m_up_button = new QCheckBox(QStringLiteral("&Up"));
  m_down_button = new QCheckBox(QStringLiteral("&Down"));
  m_right_button = new QCheckBox(QStringLiteral("R&ight"));

  auto* buttons_layout1 = new QHBoxLayout;
  buttons_layout1->addWidget(m_a_button);
  buttons_layout1->addWidget(m_b_button);
  buttons_layout1->addWidget(m_x_button);
  buttons_layout1->addWidget(m_y_button);
  buttons_layout1->addWidget(m_z_button);
  buttons_layout1->addWidget(m_l_button);
  buttons_layout1->addWidget(m_r_button);

  auto* buttons_layout2 = new QHBoxLayout;
  buttons_layout2->addWidget(m_start_button);
  buttons_layout2->addWidget(m_left_button);
  buttons_layout2->addWidget(m_up_button);
  buttons_layout2->addWidget(m_down_button);
  buttons_layout2->addWidget(m_right_button);

  auto* buttons_layout = new QVBoxLayout;
  buttons_layout->setSizeConstraint(QLayout::SetFixedSize);
  buttons_layout->addLayout(buttons_layout1);
  buttons_layout->addLayout(buttons_layout2);

  m_buttons_box = new QGroupBox(tr("Buttons"));
  m_buttons_box->setLayout(buttons_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout);
  layout->addWidget(m_triggers_box);
  layout->addWidget(m_buttons_box);
  layout->addWidget(m_use_controller);

  setLayout(layout);
}

void GCTASInputWindow::GetValues(GCPadStatus* pad)
{
  if (!isVisible())
    return;

  GetButton<u16>(m_a_button, pad->button, PAD_BUTTON_A);
  GetButton<u16>(m_b_button, pad->button, PAD_BUTTON_B);
  GetButton<u16>(m_x_button, pad->button, PAD_BUTTON_X);
  GetButton<u16>(m_y_button, pad->button, PAD_BUTTON_Y);
  GetButton<u16>(m_z_button, pad->button, PAD_TRIGGER_Z);
  GetButton<u16>(m_l_button, pad->button, PAD_TRIGGER_L);
  GetButton<u16>(m_r_button, pad->button, PAD_TRIGGER_R);
  GetButton<u16>(m_left_button, pad->button, PAD_BUTTON_LEFT);
  GetButton<u16>(m_up_button, pad->button, PAD_BUTTON_UP);
  GetButton<u16>(m_down_button, pad->button, PAD_BUTTON_DOWN);
  GetButton<u16>(m_right_button, pad->button, PAD_BUTTON_RIGHT);
  GetButton<u16>(m_start_button, pad->button, PAD_BUTTON_START);

  if (m_a_button->isChecked())
    pad->analogA = 0xFF;
  else
    pad->analogA = 0x00;

  if (m_b_button->isChecked())
    pad->analogB = 0xFF;
  else
    pad->analogB = 0x00;

  GetSpinBoxU8(m_l_trigger_value, pad->triggerLeft);
  GetSpinBoxU8(m_r_trigger_value, pad->triggerRight);

  GetSpinBoxU8(m_x_main_stick_value, pad->stickX);
  GetSpinBoxU8(m_y_main_stick_value, pad->stickY);

  GetSpinBoxU8(m_x_c_stick_value, pad->substickX);
  GetSpinBoxU8(m_y_c_stick_value, pad->substickY);
}
