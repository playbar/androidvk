// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/CheatsManager.h"

#include <algorithm>
#include <cstring>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

#include "UICommon/GameFile.h"

#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/Settings.h"

constexpr u32 MAX_RESULTS = 50;

constexpr int INDEX_ROLE = Qt::UserRole;
constexpr int COLUMN_ROLE = Qt::UserRole + 1;

enum class CompareType : int
{
  Equal = 0,
  NotEqual = 1,
  Less = 2,
  LessEqual = 3,
  More = 4,
  MoreEqual = 5
};

enum class DataType : int
{
  Byte = 0,
  Short = 1,
  Int = 2,
  Float = 3,
  Double = 4,
  String = 5
};

struct Result
{
  u32 address;
  DataType type;
  QString name;
  bool locked = false;
  u32 locked_value;
};

CheatsManager::CheatsManager(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Cheats Manager"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &CheatsManager::OnStateChanged);

  OnStateChanged(Core::GetState());

  CreateWidgets();
  ConnectWidgets();
  Reset();
  Update();
}

CheatsManager::~CheatsManager() = default;

void CheatsManager::OnStateChanged(Core::State state)
{
  if (state != Core::State::Running && state != Core::State::Paused)
    return;

  auto* model = Settings::Instance().GetGameListModel();

  for (int i = 0; i < model->rowCount(QModelIndex()); i++)
  {
    auto file = model->GetGameFile(i);

    if (file->GetGameID() == SConfig::GetInstance().GetGameID())
    {
      m_game_file = file;
      if (m_tab_widget->count() == 3)
      {
        m_tab_widget->removeTab(0);
        m_tab_widget->removeTab(0);
      }

      if (m_tab_widget->count() == 1)
      {
        if (m_ar_code)
          m_ar_code->deleteLater();

        m_ar_code = new ARCodeWidget(*m_game_file, false);
        m_tab_widget->insertTab(0, m_ar_code, tr("AR Code"));
        m_tab_widget->insertTab(1, new GeckoCodeWidget(*m_game_file, false), tr("Gecko Codes"));
      }
    }
  }
}

void CheatsManager::CreateWidgets()
{
  m_tab_widget = new QTabWidget;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_cheat_search = CreateCheatSearch();

  m_tab_widget->addTab(m_cheat_search, tr("Cheat Search"));

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void CheatsManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_match_new, &QPushButton::pressed, this, &CheatsManager::NewSearch);
  connect(m_match_next, &QPushButton::pressed, this, &CheatsManager::NextSearch);
  connect(m_match_refresh, &QPushButton::pressed, this, &CheatsManager::Update);
  connect(m_match_reset, &QPushButton::pressed, this, &CheatsManager::Reset);

  m_match_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_watch_table->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(m_match_table, &QTableWidget::customContextMenuRequested, this,
          &CheatsManager::OnMatchContextMenu);
  connect(m_watch_table, &QTableWidget::customContextMenuRequested, this,
          &CheatsManager::OnWatchContextMenu);
  connect(m_watch_table, &QTableWidget::itemChanged, this, &CheatsManager::OnWatchItemChanged);
}

void CheatsManager::OnWatchContextMenu()
{
  if (m_watch_table->selectedItems().isEmpty())
    return;

  QMenu* menu = new QMenu(this);

  menu->addAction(tr("Remove from Watch"), this, [this] {
    auto* item = m_watch_table->selectedItems()[0];

    int index = item->data(INDEX_ROLE).toInt();

    m_watch.erase(m_watch.begin() + index);

    Update();
  });

  menu->addSeparator();

  menu->addAction(tr("Generate Action Replay Code"), this, &CheatsManager::GenerateARCode);

  menu->exec(QCursor::pos());
}

void CheatsManager::OnMatchContextMenu()
{
  if (m_match_table->selectedItems().isEmpty())
    return;

  QMenu* menu = new QMenu(this);

  menu->addAction(tr("Add to Watch"), this, [this] {
    auto* item = m_match_table->selectedItems()[0];

    int index = item->data(INDEX_ROLE).toInt();

    m_watch.push_back(m_results[index]);

    Update();
  });

  menu->exec(QCursor::pos());
}

static ActionReplay::AREntry ResultToAREntry(Result result)
{
  u8 cmd;

  switch (result.type)
  {
  case DataType::Byte:
    cmd = 0x00;
    break;
  case DataType::Short:
    cmd = 0x02;
    break;
  default:
  case DataType::Int:
    cmd = 0x04;
    break;
  }

  u32 address = result.address & 0xffffff;

  return ActionReplay::AREntry(cmd << 24 | address, result.locked_value);
}

void CheatsManager::GenerateARCode()
{
  if (!m_ar_code)
    return;

  auto* item = m_watch_table->selectedItems()[0];

  int index = item->data(INDEX_ROLE).toInt();
  ActionReplay::ARCode ar_code;

  ar_code.active = true;
  ar_code.user_defined = true;
  ar_code.name = tr("Generated by search (Address %1)")
                     .arg(m_watch[index].address, 8, 16, QLatin1Char('0'))
                     .toStdString();

  ar_code.ops.push_back(ResultToAREntry(m_watch[index]));

  m_ar_code->AddCode(ar_code);
}

void CheatsManager::OnWatchItemChanged(QTableWidgetItem* item)
{
  if (m_updating)
    return;

  int index = item->data(INDEX_ROLE).toInt();
  int column = item->data(COLUMN_ROLE).toInt();

  switch (column)
  {
  case 0:
    m_watch[index].name = item->text();
    break;
  case 3:
    m_watch[index].locked = item->checkState() == Qt::Checked;
    break;
  case 4:
  {
    const auto text = item->text();
    u32 value = 0;

    switch (static_cast<DataType>(m_match_length->currentIndex()))
    {
    case DataType::Byte:
      value = text.toUShort(nullptr, 16) & 0xFF;
      break;
    case DataType::Short:
      value = text.toUShort(nullptr, 16);
      break;
    case DataType::Int:
      value = text.toUInt(nullptr, 16);
      break;
    case DataType::Float:
    {
      float f = text.toFloat();
      std::memcpy(&value, &f, sizeof(float));
      break;
    }
    default:
      break;
    }

    m_watch[index].locked_value = value;
    break;
  }
  }

  Update();
}

QWidget* CheatsManager::CreateCheatSearch()
{
  m_match_table = new QTableWidget;
  m_watch_table = new QTableWidget;

  m_match_table->verticalHeader()->hide();
  m_watch_table->verticalHeader()->hide();

  m_match_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_watch_table->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Options
  m_result_label = new QLabel;
  m_match_length = new QComboBox;
  m_match_operation = new QComboBox;
  m_match_value = new QLineEdit;
  m_match_new = new QPushButton(tr("New Search"));
  m_match_next = new QPushButton(tr("Next Search"));
  m_match_refresh = new QPushButton(tr("Refresh"));
  m_match_reset = new QPushButton(tr("Reset"));

  auto* options = new QWidget;
  auto* layout = new QVBoxLayout;
  options->setLayout(layout);

  for (const auto& option : {tr("8-bit Integer"), tr("16-bit Integer"), tr("32-bit Integer"),
                             tr("Float"), tr("Double"), tr("String")})
  {
    m_match_length->addItem(option);
  }

  for (const auto& option : {tr("Equals to"), tr("Not equals to"), tr("Less than"),
                             tr("Less or equal to"), tr("More than"), tr("More or equal to")})
  {
    m_match_operation->addItem(option);
  }

  auto* group_box = new QGroupBox(tr("Type"));
  auto* group_layout = new QHBoxLayout;
  group_box->setLayout(group_layout);

  // i18n: The base 10 numeral system. Not related to non-integer numbers
  m_match_decimal = new QRadioButton(tr("Decimal"));
  m_match_hexadecimal = new QRadioButton(tr("Hexadecimal"));
  m_match_octal = new QRadioButton(tr("Octal"));

  group_layout->addWidget(m_match_decimal);
  group_layout->addWidget(m_match_hexadecimal);
  group_layout->addWidget(m_match_octal);

  layout->addWidget(m_result_label);
  layout->addWidget(m_match_length);
  layout->addWidget(m_match_operation);
  layout->addWidget(m_match_value);
  layout->addWidget(group_box);
  layout->addWidget(m_match_new);
  layout->addWidget(m_match_next);
  layout->addWidget(m_match_refresh);
  layout->addWidget(m_match_reset);

  // Splitters
  m_option_splitter = new QSplitter(Qt::Horizontal);
  m_table_splitter = new QSplitter(Qt::Vertical);

  m_table_splitter->addWidget(m_match_table);
  m_table_splitter->addWidget(m_watch_table);

  m_option_splitter->addWidget(m_table_splitter);
  m_option_splitter->addWidget(options);

  return m_option_splitter;
}

size_t CheatsManager::GetTypeSize() const
{
  switch (static_cast<DataType>(m_match_length->currentIndex()))
  {
  case DataType::Byte:
    return sizeof(u8);
  case DataType::Short:
    return sizeof(u16);
  case DataType::Int:
    return sizeof(u32);
  case DataType::Float:
    return sizeof(float);
  case DataType::Double:
    return sizeof(double);
  default:
    return m_match_value->text().toStdString().size();
  }
}

template <typename T>
static bool Compare(T mem_value, T value, CompareType op)
{
  switch (op)
  {
  case CompareType::Equal:
    return mem_value == value;
  case CompareType::NotEqual:
    return mem_value != value;
  case CompareType::Less:
    return mem_value < value;
  case CompareType::LessEqual:
    return mem_value <= value;
  case CompareType::More:
    return mem_value > value;
  case CompareType::MoreEqual:
    return mem_value >= value;
  default:
    return false;
  }
}

bool CheatsManager::MatchesSearch(u32 addr) const
{
  const auto text = m_match_value->text();
  const auto op = static_cast<CompareType>(m_match_operation->currentIndex());

  const int base =
      (m_match_decimal->isChecked() ? 10 : (m_match_hexadecimal->isChecked() ? 16 : 8));

  switch (static_cast<DataType>(m_match_length->currentIndex()))
  {
  case DataType::Byte:
    return Compare<u8>(PowerPC::HostRead_U8(addr), text.toUShort(nullptr, base) & 0xFF, op);
  case DataType::Short:
    return Compare(PowerPC::HostRead_U16(addr), text.toUShort(nullptr, base), op);
  case DataType::Int:
    return Compare(PowerPC::HostRead_U32(addr), text.toUInt(nullptr, base), op);
  case DataType::Float:
    return Compare(PowerPC::HostRead_F32(addr), text.toFloat(), op);
  case DataType::Double:
    return Compare(PowerPC::HostRead_F64(addr), text.toDouble(), op);
  case DataType::String:
  {
    bool is_equal = std::equal(text.toUtf8().cbegin(), text.toUtf8().cend(),
                               reinterpret_cast<char*>(Memory::m_pRAM + addr - 0x80000000));

    // String only supports equals and not equals comparisons because the other ones frankly don't
    // make any sense here
    switch (op)
    {
    case CompareType::Equal:
      return is_equal;
    case CompareType::NotEqual:
      return !is_equal;
    default:
      return false;
    }
  }
  }

  return false;
}

void CheatsManager::NewSearch()
{
  m_results.clear();
  const u32 base_address = 0x80000000;

  if (!Memory::m_pRAM)
  {
    m_result_label->setText(tr("Memory Not Ready"));
    return;
  }

  Core::RunAsCPUThread([&] {
    for (u32 i = 0; i < Memory::REALRAM_SIZE - GetTypeSize(); i++)
    {
      if (PowerPC::HostIsRAMAddress(base_address + i) && MatchesSearch(base_address + i))
        m_results.push_back(
            {base_address + i, static_cast<DataType>(m_match_length->currentIndex())});
    }
  });

  m_match_next->setEnabled(true);

  Update();
}

void CheatsManager::NextSearch()
{
  if (!Memory::m_pRAM)
  {
    m_result_label->setText(tr("Memory Not Ready"));
    return;
  }

  Core::RunAsCPUThread([this] {
    m_results.erase(std::remove_if(m_results.begin(), m_results.end(),
                                   [this](Result r) {
                                     return !PowerPC::HostIsRAMAddress(r.address) ||
                                            !MatchesSearch(r.address);
                                   }),
                    m_results.end());
  });

  Update();
}

void CheatsManager::Update()
{
  m_match_table->clear();
  m_watch_table->clear();
  m_match_table->setColumnCount(2);
  m_watch_table->setColumnCount(4);

  m_match_table->setHorizontalHeaderLabels({tr("Address"), tr("Value")});
  m_watch_table->setHorizontalHeaderLabels({tr("Name"), tr("Address"), tr("Lock"), tr("Value")});

  if (m_results.size() > MAX_RESULTS)
  {
    m_result_label->setText(tr("Too many matches to display (%1)").arg(m_results.size()));
    return;
  }

  m_result_label->setText(tr("%1 Match(es)").arg(m_results.size()));
  m_match_table->setRowCount(static_cast<int>(m_results.size()));

  if (m_results.empty())
    return;

  m_updating = true;

  Core::RunAsCPUThread([this] {
    for (size_t i = 0; i < m_results.size(); i++)
    {
      auto* address_item = new QTableWidgetItem(
          QStringLiteral("%1").arg(m_results[i].address, 8, 16, QLatin1Char('0')));
      auto* value_item = new QTableWidgetItem;

      address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

      if (PowerPC::HostIsRAMAddress(m_results[i].address))
      {
        switch (m_results[i].type)
        {
        case DataType::Byte:
          value_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U8(m_results[i].address),
                                                       2, 16, QLatin1Char('0')));
          break;
        case DataType::Short:
          value_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U16(m_results[i].address),
                                                       4, 16, QLatin1Char('0')));
          break;
        case DataType::Int:
          value_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U32(m_results[i].address),
                                                       8, 16, QLatin1Char('0')));
          break;
        case DataType::Float:
          value_item->setText(QString::number(PowerPC::HostRead_F32(m_results[i].address)));
          break;
        case DataType::Double:
          value_item->setText(QString::number(PowerPC::HostRead_F64(m_results[i].address)));
          break;
        case DataType::String:
          value_item->setText(tr("String Match"));
          break;
        }
      }
      else
      {
        value_item->setText(QStringLiteral("---"));
      }

      address_item->setData(INDEX_ROLE, static_cast<int>(i));
      value_item->setData(INDEX_ROLE, static_cast<int>(i));

      m_match_table->setItem(static_cast<int>(i), 0, address_item);
      m_match_table->setItem(static_cast<int>(i), 1, value_item);
    }

    m_watch_table->setRowCount(static_cast<int>(m_watch.size()));

    for (size_t i = 0; i < m_watch.size(); i++)
    {
      auto* name_item = new QTableWidgetItem(m_watch[i].name);
      auto* address_item = new QTableWidgetItem(
          QStringLiteral("%1").arg(m_watch[i].address, 8, 16, QLatin1Char('0')));
      auto* lock_item = new QTableWidgetItem;
      auto* value_item = new QTableWidgetItem;

      if (PowerPC::HostIsRAMAddress(m_watch[i].address))
      {
        if (m_watch[i].locked)
        {
          PowerPC::debug_interface.SetPatch(m_watch[i].address, m_watch[i].locked_value);
        }

        switch (m_watch[i].type)
        {
        case DataType::Byte:
          value_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U8(m_watch[i].address), 2,
                                                       16, QLatin1Char('0')));
          break;
        case DataType::Short:
          value_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U16(m_watch[i].address), 4,
                                                       16, QLatin1Char('0')));
          break;
        case DataType::Int:
          value_item->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U32(m_watch[i].address), 8,
                                                       16, QLatin1Char('0')));
          break;
        case DataType::Float:
          value_item->setText(QString::number(PowerPC::HostRead_F32(m_watch[i].address)));
          break;
        case DataType::Double:
          value_item->setText(QString::number(PowerPC::HostRead_F64(m_watch[i].address)));
          break;
        case DataType::String:
          value_item->setText(tr("String Match"));
          break;
        }
      }
      else
      {
        value_item->setText(QStringLiteral("---"));
      }

      name_item->setData(INDEX_ROLE, static_cast<int>(i));
      name_item->setData(COLUMN_ROLE, 0);
      address_item->setData(INDEX_ROLE, static_cast<int>(i));
      address_item->setData(COLUMN_ROLE, 1);
      value_item->setData(INDEX_ROLE, static_cast<int>(i));
      value_item->setData(COLUMN_ROLE, 2);
      lock_item->setData(INDEX_ROLE, static_cast<int>(i));
      lock_item->setData(COLUMN_ROLE, 3);
      value_item->setData(INDEX_ROLE, static_cast<int>(i));
      value_item->setData(COLUMN_ROLE, 4);

      name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
      address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      lock_item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
      value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

      lock_item->setCheckState(m_watch[i].locked ? Qt::Checked : Qt::Unchecked);

      m_watch_table->setItem(static_cast<int>(i), 0, name_item);
      m_watch_table->setItem(static_cast<int>(i), 1, address_item);
      m_watch_table->setItem(static_cast<int>(i), 2, lock_item);
      m_watch_table->setItem(static_cast<int>(i), 3, value_item);
    }
  });

  m_updating = false;
}

void CheatsManager::Reset()
{
  m_results.clear();
  m_watch.clear();
  m_match_next->setEnabled(false);
  m_match_table->clear();
  m_watch_table->clear();
  m_match_decimal->setChecked(true);
  m_result_label->setText(QStringLiteral(""));

  Update();
}
