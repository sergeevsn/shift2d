#include "main_window.hpp"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QAction>
#include <QActionGroup>
#include <QScrollBar>
#include <QFileInfo>
#include <QSizePolicy>
#include <Qt>
#include <QScrollArea>
#include <QFrame>
#include <QToolButton>

#include "seismic_view.hpp"
#include "segy_load_dialog.hpp"
#include "statics_load_dialog.hpp"
#include "horizon_load_dialog.hpp"
#include "segy_reader.hpp"
#include "segy_writer.hpp"
#include "data_info_dialog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <limits>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , reader_(std::make_unique<SegyReader>())
{
    setupUi();
    setupMenu();
    setupToolbar();
    updateInfoMenuActions();
    setWindowTitle(tr("Shift2D — SEG-Y Statics"));
    setMinimumSize(1200, 800);
    resize(1440, 920);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    auto* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // Левая панель с настройками
    createLeftPanel();
    left_panel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    main_layout->addWidget(left_panel_, 0);

    // Правая часть - виджет сейсмики и скроллбары
    auto* right_widget = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right_widget);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(0);

    seismic_view_ = new SeismicView(this);
    seismic_view_->setObjectName(QStringLiteral("seismicCanvas"));
    right_layout->addWidget(seismic_view_, 1);

    // Горизонтальный скроллбар
    h_scrollbar_ = new QScrollBar(Qt::Horizontal, this);
    h_scrollbar_->setEnabled(false);
    h_scrollbar_->setVisible(false);
    connect(h_scrollbar_, &QScrollBar::valueChanged, this, &MainWindow::onHScrollChanged);
    right_layout->addWidget(h_scrollbar_);

    // Вертикальный скроллбар (справа от виджета)
    auto* seis_layout = new QHBoxLayout();
    seis_layout->addWidget(right_widget, 1);

    v_scrollbar_ = new QScrollBar(Qt::Vertical, this);
    v_scrollbar_->setEnabled(false);
    v_scrollbar_->setVisible(false);
    connect(v_scrollbar_, &QScrollBar::valueChanged, this, &MainWindow::onVScrollChanged);
    seis_layout->addWidget(v_scrollbar_);

    main_layout->addLayout(seis_layout, 1);

    setCentralWidget(central);

    // Статус-бар
    status_label_ = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(status_label_, 1);

    // Подключаем сигналы seismic_view_
    connect(seismic_view_, &SeismicView::panRequested, this, &MainWindow::onPanRequested);
    connect(seismic_view_, &SeismicView::traceZoomRequested, this, &MainWindow::onTraceZoomRequested);
    connect(seismic_view_, &SeismicView::timeZoomRequested, this, &MainWindow::onTimeZoomRequested);
    connect(seismic_view_, &SeismicView::resetZoomRequested, this, &MainWindow::onResetZoomRequested);
    connect(seismic_view_, &SeismicView::hoverInfoChanged, this, &MainWindow::onHoverInfoChanged);
}

void MainWindow::setupMenu()
{
    auto* file_menu = menuBar()->addMenu(tr("&File"));

    auto* open_segy_action = new QAction(tr("&Open SEG-Y..."), this);
    open_segy_action->setShortcut(QKeySequence::Open);
    connect(open_segy_action, &QAction::triggered, this, &MainWindow::onOpenSegy);
    file_menu->addAction(open_segy_action);

    auto* open_statics_action = new QAction(tr("Open &Statics CSV..."), this);
    connect(open_statics_action, &QAction::triggered, this, &MainWindow::onOpenStatics);
    file_menu->addAction(open_statics_action);

    auto* open_horizon_action = new QAction(tr("Open &Horizon CSV..."), this);
    connect(open_horizon_action, &QAction::triggered, this, &MainWindow::onOpenHorizon);
    file_menu->addAction(open_horizon_action);

    file_menu->addSeparator();

    auto* save_action = new QAction(tr("&Save SEG-Y As..."), this);
    save_action->setShortcut(QKeySequence::SaveAs);
    connect(save_action, &QAction::triggered, this, &MainWindow::onSaveSegy);
    file_menu->addAction(save_action);

    file_menu->addSeparator();

    auto* exit_action = new QAction(tr("E&xit"), this);
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);
    file_menu->addAction(exit_action);

    auto* info_menu = menuBar()->addMenu(tr("&Info"));

    info_segy_action_ = new QAction(tr("SEG-Y Profile"), this);
    connect(info_segy_action_, &QAction::triggered, this, &MainWindow::onInfoSegyProfile);
    info_menu->addAction(info_segy_action_);

    info_statics_action_ = new QAction(tr("Statics"), this);
    connect(info_statics_action_, &QAction::triggered, this, &MainWindow::onInfoStatics);
    info_menu->addAction(info_statics_action_);

    info_horizon_action_ = new QAction(tr("Horizon"), this);
    connect(info_horizon_action_, &QAction::triggered, this, &MainWindow::onInfoHorizon);
    info_menu->addAction(info_horizon_action_);
}

void MainWindow::setupToolbar()
{
    auto* toolbar = addToolBar(tr("View"));
    toolbar->setObjectName(QStringLiteral("viewToolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(18, 18));

    auto* action_group = new QActionGroup(this);

    zoom_action_ = new QAction(tr("Zoom"), this);
    zoom_action_->setCheckable(true);
    zoom_action_->setChecked(true);
    zoom_action_->setToolTip(tr("Drag to zoom (right-click to reset)"));
    connect(zoom_action_, &QAction::toggled, [this](bool on) {
        if (on) seismic_view_->setInteractionMode(SeismicView::InteractionMode::Zoom);
    });
    action_group->addAction(zoom_action_);
    toolbar->addAction(zoom_action_);

    pan_action_ = new QAction(tr("Pan"), this);
    pan_action_->setCheckable(true);
    pan_action_->setToolTip(tr("Drag to pan the section"));
    connect(pan_action_, &QAction::toggled, [this](bool on) {
        if (on) seismic_view_->setInteractionMode(SeismicView::InteractionMode::Pan);
    });
    action_group->addAction(pan_action_);
    toolbar->addAction(pan_action_);

    toolbar->addSeparator();

    auto* reset_zoom_action = new QAction(tr("Reset view"), this);
    reset_zoom_action->setToolTip(tr("Show full section"));
    connect(reset_zoom_action, &QAction::triggered, this, &MainWindow::onResetZoomRequested);
    toolbar->addAction(reset_zoom_action);

    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(tr("Palette"), this));

    palette_combo_ = new QComboBox(this);
    for (const auto& name : ColorSchemes::getAvailableSchemes()) {
        palette_combo_->addItem(name);
    }
    palette_combo_->setCurrentIndex(0);
    palette_combo_->setToolTip(tr("Amplitude color map"));
    connect(palette_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPaletteChanged);
    toolbar->addWidget(palette_combo_);

    toolbar->addWidget(new QLabel(tr("Clip"), this));

    clip_spin_ = new QDoubleSpinBox(this);
    clip_spin_->setRange(0.0, 1000000.0);
    clip_spin_->setDecimals(2);
    clip_spin_->setValue(0.0);
    clip_spin_->setSingleStep(0.1);
    clip_spin_->setToolTip(tr("Amplitude clip (0 = auto)"));
    connect(clip_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onClipChanged);
    toolbar->addWidget(clip_spin_);

    toolbar->addSeparator();
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
}

void MainWindow::createLeftPanel()
{
    left_panel_ = new QWidget(this);
    left_panel_->setObjectName(QStringLiteral("sidebar"));
    left_panel_->setFixedWidth(300);

    auto* outer_layout = new QVBoxLayout(left_panel_);
    outer_layout->setContentsMargins(12, 12, 12, 12);
    outer_layout->setSpacing(8);

    auto* title = new QLabel(tr("Shift2D"), left_panel_);
    title->setObjectName(QStringLiteral("appTitle"));
    outer_layout->addWidget(title);

    auto* subtitle = new QLabel(tr("SEG-Y statics correction"), left_panel_);
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    outer_layout->addWidget(subtitle);
    outer_layout->addSpacing(4);

    auto* scroll = new QScrollArea(left_panel_);
    scroll->setObjectName(QStringLiteral("sidebarScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* scroll_content = new QWidget(scroll);
    auto* left_layout = new QVBoxLayout(scroll_content);
    left_layout->setContentsMargins(0, 0, 4, 0);
    left_layout->setSpacing(4);

    auto makeFileRow = [scroll_content](const QString& kind, QLabel** name_label,
                                        QPushButton** open_button) -> QFrame* {
        auto* row = new QWidget(scroll_content);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->setSpacing(6);

        auto* card = new QFrame(row);
        card->setObjectName(QStringLiteral("fileCard"));
        card->setProperty("loaded", false);

        auto* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(8, 4, 8, 4);
        card_layout->setSpacing(0);

        auto* kind_label = new QLabel(kind, card);
        kind_label->setObjectName(QStringLiteral("fileStatusLabel"));
        card_layout->addWidget(kind_label);

        *name_label = new QLabel(card);
        (*name_label)->setObjectName(QStringLiteral("fileNameLabel"));
        (*name_label)->setProperty("empty", true);
        (*name_label)->setWordWrap(true);
        card_layout->addWidget(*name_label);

        *open_button = new QPushButton(tr("Open"), row);
        (*open_button)->setObjectName(QStringLiteral("openFileButton"));
        (*open_button)->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        row_layout->addWidget(card, 1);
        row_layout->addWidget(*open_button, 0, Qt::AlignVCenter);

        return card;
    };

    // --- Data ---
    auto* file_group = new QGroupBox(tr("Data"), scroll_content);
    auto* file_layout = new QVBoxLayout(file_group);
    file_layout->setSpacing(6);

    QPushButton* open_segy_btn = nullptr;
    segy_card_ = makeFileRow(tr("SEG-Y"), &segy_name_label_, &open_segy_btn);
    updateFileCard(segy_card_, segy_name_label_, tr("Not loaded"), false);
    connect(open_segy_btn, &QPushButton::clicked, this, &MainWindow::onOpenSegy);
    file_layout->addWidget(segy_card_->parentWidget());

    QPushButton* open_statics_btn = nullptr;
    statics_card_ = makeFileRow(tr("Statics"), &statics_name_label_, &open_statics_btn);
    updateFileCard(statics_card_, statics_name_label_, tr("Not loaded"), false);
    connect(open_statics_btn, &QPushButton::clicked, this, &MainWindow::onOpenStatics);
    file_layout->addWidget(statics_card_->parentWidget());

    QPushButton* open_horizon_btn = nullptr;
    horizon_card_ = makeFileRow(tr("Horizon"), &horizon_name_label_, &open_horizon_btn);
    updateFileCard(horizon_card_, horizon_name_label_, tr("Not loaded"), false);
    connect(open_horizon_btn, &QPushButton::clicked, this, &MainWindow::onOpenHorizon);
    file_layout->addWidget(horizon_card_->parentWidget());

    auto* save_btn = new QPushButton(tr("Save SEG-Y As…"), file_group);
    save_btn->setObjectName(QStringLiteral("primaryButton"));
    connect(save_btn, &QPushButton::clicked, this, &MainWindow::onSaveSegy);
    file_layout->addWidget(save_btn);

    left_layout->addWidget(file_group);

    // --- Statics processing ---
    auto* mode_group_box = new QGroupBox(tr("Processing"), scroll_content);
    auto* mode_layout = new QVBoxLayout(mode_group_box);
    mode_layout->setSpacing(4);

    mode_group_ = new QButtonGroup(this);

    mode_none_radio_ = new QRadioButton(tr("Preview only"), mode_group_box);
    mode_none_radio_->setChecked(true);
    mode_group_->addButton(mode_none_radio_, 0);
    mode_layout->addWidget(mode_none_radio_);

    mode_forward_radio_ = new QRadioButton(tr("Apply forward statics (+)"), mode_group_box);
    mode_group_->addButton(mode_forward_radio_, 1);
    mode_layout->addWidget(mode_forward_radio_);

    mode_inverse_radio_ = new QRadioButton(tr("Apply inverse statics (−)"), mode_group_box);
    mode_group_->addButton(mode_inverse_radio_, 2);
    mode_layout->addWidget(mode_inverse_radio_);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(mode_group_, QOverload<int>::of(&QButtonGroup::idClicked),
#else
    connect(mode_group_, QOverload<int>::of(&QButtonGroup::buttonClicked),
#endif
            this, &MainWindow::onModeChanged);

    interpolate_checkbox_ = new QCheckBox(tr("Interpolate gaps"), mode_group_box);
    interpolate_checkbox_->setChecked(true);
    connect(interpolate_checkbox_, &QCheckBox::toggled, this, [this]() {
        applyStaticsAndUpdate();
    });
    mode_layout->addWidget(interpolate_checkbox_);

    extrapolate_checkbox_ = new QCheckBox(tr("Extrapolate edges"), mode_group_box);
    extrapolate_checkbox_->setChecked(true);
    connect(extrapolate_checkbox_, &QCheckBox::toggled, this, [this]() {
        applyStaticsAndUpdate();
    });
    mode_layout->addWidget(extrapolate_checkbox_);

    smooth_statics_checkbox_ = new QCheckBox(tr("Smooth statics curve"), mode_group_box);
    connect(smooth_statics_checkbox_, &QCheckBox::toggled, this, [this](bool) {
        updateSmoothStaticsControls();
        applyStaticsAndUpdate();
    });
    mode_layout->addWidget(smooth_statics_checkbox_);

    smooth_statics_options_ = new QWidget(mode_group_box);
    auto* smooth_layout = new QFormLayout(smooth_statics_options_);
    smooth_layout->setContentsMargins(16, 0, 0, 0);
    smooth_layout->setHorizontalSpacing(12);
    smooth_layout->setVerticalSpacing(8);

    smooth_filter_type_combo_ = new QComboBox(smooth_statics_options_);
    smooth_filter_type_combo_->addItem(tr("Mean"));
    smooth_filter_type_combo_->addItem(tr("Median"));
    smooth_layout->addRow(tr("Filter"), smooth_filter_type_combo_);

    smooth_filter_size_spin_ = new QSpinBox(smooth_statics_options_);
    smooth_filter_size_spin_->setRange(1, 999999);
    smooth_filter_size_spin_->setValue(5);
    smooth_filter_size_spin_->setSingleStep(1);
    smooth_layout->addRow(tr("Window"), smooth_filter_size_spin_);

    mode_layout->addWidget(smooth_statics_options_);

    connect(smooth_filter_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (smooth_statics_checkbox_->isChecked())
                    applyStaticsAndUpdate();
            });
    connect(smooth_filter_size_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                if (smooth_statics_checkbox_->isChecked())
                    applyStaticsAndUpdate();
            });

    updateSmoothStaticsControls();

    auto* shift_layout = new QFormLayout();
    shift_layout->setContentsMargins(0, 8, 0, 0);
    shift_layout->setHorizontalSpacing(12);

    extra_shift_spin_ = new QDoubleSpinBox(mode_group_box);
    extra_shift_spin_->setRange(-10.0, 10.0);
    extra_shift_spin_->setDecimals(4);
    extra_shift_spin_->setSingleStep(0.001);
    extra_shift_spin_->setValue(0.0);
    extra_shift_spin_->setSuffix(tr(" s"));
    connect(extra_shift_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onExtraShiftChanged);
    shift_layout->addRow(tr("Extra shift"), extra_shift_spin_);
    mode_layout->addLayout(shift_layout);

    left_layout->addWidget(mode_group_box);

    // --- View limits ---
    auto* limits_group = new QGroupBox(tr("View window"), scroll_content);
    auto* limits_layout = new QFormLayout(limits_group);
    limits_layout->setHorizontalSpacing(12);
    limits_layout->setVerticalSpacing(8);

    trace_min_spin_ = new QSpinBox(limits_group);
    trace_min_spin_->setRange(0, 99999999);
    trace_min_spin_->setValue(0);
    limits_layout->addRow(tr("Trace from"), trace_min_spin_);

    trace_max_spin_ = new QSpinBox(limits_group);
    trace_max_spin_->setRange(0, 99999999);
    trace_max_spin_->setValue(0);
    limits_layout->addRow(tr("Trace to"), trace_max_spin_);

    time_min_spin_ = new QSpinBox(limits_group);
    time_min_spin_->setRange(0, 99999999);
    time_min_spin_->setValue(0);
    time_min_spin_->setSuffix(tr(" ms"));
    limits_layout->addRow(tr("Time from"), time_min_spin_);

    time_max_spin_ = new QSpinBox(limits_group);
    time_max_spin_->setRange(0, 99999999);
    time_max_spin_->setValue(0);
    time_max_spin_->setSuffix(tr(" ms"));
    limits_layout->addRow(tr("Time to"), time_max_spin_);

    auto* limits_buttons = new QHBoxLayout();
    limits_buttons->setSpacing(8);

    apply_limits_btn_ = new QPushButton(tr("Apply"), limits_group);
    apply_limits_btn_->setObjectName(QStringLiteral("secondaryButton"));
    connect(apply_limits_btn_, &QPushButton::clicked, this, &MainWindow::onUpdateView);
    limits_buttons->addWidget(apply_limits_btn_);

    reset_zoom_btn_ = new QPushButton(tr("Reset"), limits_group);
    reset_zoom_btn_->setObjectName(QStringLiteral("secondaryButton"));
    connect(reset_zoom_btn_, &QPushButton::clicked, this, &MainWindow::onResetZoomRequested);
    limits_buttons->addWidget(reset_zoom_btn_);

    limits_layout->addRow(limits_buttons);

    left_layout->addWidget(limits_group);

    scroll->setWidget(scroll_content);
    outer_layout->addWidget(scroll, 1);
}

void MainWindow::updateFileCard(QFrame* card, QLabel* name_label, const QString& name, bool loaded)
{
    if (!card || !name_label)
        return;

    name_label->setText(name);
    name_label->setProperty("empty", !loaded);
    card->setProperty("loaded", loaded);

    name_label->style()->unpolish(name_label);
    name_label->style()->polish(name_label);
    card->style()->unpolish(card);
    card->style()->polish(card);
}

void MainWindow::updateInfoMenuActions()
{
    const bool segy_loaded = reader_ && reader_->isOpen();
    const bool statics_loaded = !current_statics_path_.isEmpty() && !static_points_.empty();
    const bool horizon_loaded = !current_horizon_path_.isEmpty() && !horizon_points_.empty();

    if (info_segy_action_)
        info_segy_action_->setEnabled(segy_loaded);
    if (info_statics_action_)
        info_statics_action_->setEnabled(statics_loaded);
    if (info_horizon_action_)
        info_horizon_action_->setEnabled(horizon_loaded);
}

void MainWindow::onInfoSegyProfile()
{
    if (!reader_ || !reader_->isOpen())
        return;

    DataInfoDialog::showSegyProfile(this, current_segy_path_, metadata_,
                                    x_byte_, y_byte_,
                                    trace_x_coords_, trace_y_coords_);
}

void MainWindow::onInfoStatics()
{
    if (current_statics_path_.isEmpty() || static_points_.empty())
        return;

    const int n_traces = reader_ && reader_->isOpen() ? metadata_.n_traces : 0;
    DataInfoDialog::showStatics(this, current_statics_path_, static_points_,
                                trace_x_coords_, trace_y_coords_, n_traces);
}

void MainWindow::onInfoHorizon()
{
    if (current_horizon_path_.isEmpty() || horizon_points_.empty())
        return;

    const int n_traces = reader_ && reader_->isOpen() ? metadata_.n_traces : 0;
    DataInfoDialog::showHorizon(this, current_horizon_path_, horizon_points_,
                                trace_x_coords_, trace_y_coords_, n_traces);
}

void MainWindow::onOpenSegy()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open SEG-Y file"),
        QString(),
        tr("SEG-Y files (*.sgy *.segy);;All files (*)"));

    if (path.isEmpty())
        return;

    // Показываем диалог настроек загрузки
    SegyLoadDialog dialog(this);
    dialog.analyzeFile(path);

    if (dialog.exec() != QDialog::Accepted)
        return;

    x_byte_ = dialog.xByte();
    y_byte_ = dialog.yByte();

    loadSegyFile(path, x_byte_, y_byte_);
}

void MainWindow::loadSegyFile(const QString& path, int x_byte, int y_byte)
{
    try {
        reader_->close();
        reader_->open(path.toStdString());
        metadata_ = reader_->metadata();

        // Загружаем все трассы
        original_traces_.clear();
        original_traces_.reserve(metadata_.n_traces);
        trace_x_coords_.clear();
        trace_x_coords_.reserve(metadata_.n_traces);
        trace_y_coords_.clear();
        trace_y_coords_.reserve(metadata_.n_traces);

        for (int i = 0; i < metadata_.n_traces; ++i) {
            Trace t = reader_->readTrace(i);
            original_traces_.push_back(std::move(t));

            // Читаем координаты
            trace_x_coords_.push_back(reader_->getTraceHeaderValue(i, x_byte));
            trace_y_coords_.push_back(reader_->getTraceHeaderValue(i, y_byte));
        }

        shifted_traces_ = original_traces_;

        current_segy_path_ = path;
        updateFileCard(segy_card_, segy_name_label_, QFileInfo(path).fileName(), true);

        // Обновляем пределы
        trace_min_spin_->setRange(0, metadata_.n_traces - 1);
        trace_max_spin_->setRange(0, metadata_.n_traces - 1);
        trace_max_spin_->setValue(metadata_.n_traces - 1);

        if (metadata_.dt > 0) {
            int max_time_ms = static_cast<int>(metadata_.n_samples * metadata_.dt * 1000);
            time_min_spin_->setRange(0, max_time_ms);
            time_max_spin_->setRange(0, max_time_ms);
            time_max_spin_->setValue(max_time_ms);
        }

        // Настраиваем скроллбары
        view_first_trace_ = 0;
        view_trace_count_ = metadata_.n_traces;
        view_first_sample_ = 0;
        view_sample_count_ = metadata_.n_samples;

        // Обновляем вид
        applyStaticsAndUpdate();

        statusBar()->showMessage(tr("Loaded: %1 traces, %2 samples, dt=%3s")
                                 .arg(metadata_.n_traces)
                                 .arg(metadata_.n_samples)
                                 .arg(metadata_.dt), 5000);
        // Zoom logging handled elsewhere

    updateScrollbars();

    updateInfoMenuActions();

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to load SEG-Y file:\n%1").arg(e.what()));
    }
}

void MainWindow::onOpenStatics()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Statics CSV file"),
        QString(),
        tr("All files (*.*)"));

    if (path.isEmpty())
        return;

    // Читаем CSV для предпросмотра
    std::ifstream file(path.toStdString());
    if (!file.is_open()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file"));
        return;
    }

    std::vector<std::vector<QString>> preview_data;
    QStringList headers;
    bool has_header = false;

    std::string line;
    int line_count = 0;
    while (std::getline(file, line) && line_count < 10) {
        std::istringstream iss(line);
        std::string cell;
        std::vector<QString> row;

        while (std::getline(iss, cell, ',')) {
            row.push_back(QString::fromStdString(cell).trimmed());
        }

        if (line_count == 0) {
            // Проверяем, является ли первая строка заголовком
            bool all_numeric = true;
            for (const auto& val : row) {
                bool ok;
                val.toDouble(&ok);
                if (!ok) {
                    all_numeric = false;
                    break;
                }
            }

            if (all_numeric) {
                has_header = false;
                preview_data.push_back(row);
                for (int i = 0; i < row.size(); ++i) {
                    headers.append(tr("Column %1").arg(i));
                }
            } else {
                has_header = true;
                for (const auto& val : row) {
                    headers.append(val);
                }
            }
        } else {
            preview_data.push_back(row);
        }

        line_count++;
    }

    file.close();

    // Показываем диалог настроек
    StaticsLoadDialog dialog(this);
    dialog.setSourceFile(path);
    dialog.setColumnHeaders(headers, preview_data);
    dialog.setMatchOptions(coord_match_nearest_, coord_match_x_range_, coord_match_y_range_);
    if (reader_->isOpen())
        dialog.setProfileCoordinates(trace_x_coords_, trace_y_coords_);

    if (dialog.exec() != QDialog::Accepted)
        return;

    loadStaticsFile(path, dialog.useXYMode(), dialog.xColumn(), dialog.yColumn(),
                    dialog.traceColumn(), dialog.staticColumn(),
                    dialog.useNearestTraceMatch(), dialog.matchXRange(), dialog.matchYRange(),
                    dialog.valueInMilliseconds());
}

void MainWindow::loadStaticsFile(const QString& path, bool xy_mode, int x_col, int y_col,
                                 int trace_col, int static_col, bool nearest_match,
                                 double match_x_range, double match_y_range, bool value_in_ms)
{
    std::ifstream file(path.toStdString());
    if (!file.is_open()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file"));
        return;
    }

    if (xy_mode) {
        coord_match_nearest_ = nearest_match;
        if (!nearest_match) {
            coord_match_x_range_ = match_x_range;
            coord_match_y_range_ = match_y_range;
        }
    }

    static_points_.clear();

    std::string line;
    bool first_line = true;
    bool has_header = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string cell;
        std::vector<std::string> cells;

        while (std::getline(iss, cell, ',')) {
            cells.push_back(cell);
        }

        if (first_line) {
            // Проверяем, является ли первая строка заголовком
            bool all_numeric = true;
            for (const auto& c : cells) {
                try {
                    std::stod(c);
                } catch (...) {
                    all_numeric = false;
                    break;
                }
            }
            has_header = !all_numeric;
            first_line = false;

            if (has_header)
                continue;
        }

        // Парсим строку
        try {
            StaticPoint sp;

            if (xy_mode) {
                if (x_col < static_cast<int>(cells.size()))
                    sp.x = std::stod(cells[x_col]);
                if (y_col < static_cast<int>(cells.size()))
                    sp.y = std::stod(cells[y_col]);
            } else {
                if (trace_col < static_cast<int>(cells.size()))
                    sp.trace_num = std::stod(cells[trace_col]);
            }

            if (static_col < static_cast<int>(cells.size())) {
                sp.value = std::stod(cells[static_col]);
                if (value_in_ms)
                    sp.value /= 1000.0;
            }

            static_points_.push_back(sp);
        } catch (...) {
            // Пропускаем невалидные строки
        }
    }

    current_statics_path_ = path;
    updateFileCard(statics_card_, statics_name_label_,
                   tr("%1 · %2 pts").arg(QFileInfo(path).fileName()).arg(static_points_.size()),
                   true);

    applyStaticsAndUpdate();

    statusBar()->showMessage(tr("Loaded %1 static points").arg(static_points_.size()), 3000);
    updateInfoMenuActions();
}

void MainWindow::onOpenHorizon()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Horizon CSV file"),
        QString(),
        tr("All files (*.*)"));

    if (path.isEmpty())
        return;

    std::ifstream file(path.toStdString());
    if (!file.is_open()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file"));
        return;
    }

    std::vector<std::vector<QString>> preview_data;
    QStringList headers;

    std::string line;
    int line_count = 0;
    while (std::getline(file, line) && line_count < 10) {
        std::istringstream iss(line);
        std::string cell;
        std::vector<QString> row;

        while (std::getline(iss, cell, ',')) {
            row.push_back(QString::fromStdString(cell).trimmed());
        }

        if (line_count == 0) {
            bool all_numeric = true;
            for (const auto& val : row) {
                bool ok;
                val.toDouble(&ok);
                if (!ok) {
                    all_numeric = false;
                    break;
                }
            }

            if (all_numeric) {
                preview_data.push_back(row);
                for (int i = 0; i < row.size(); ++i) {
                    headers.append(tr("Column %1").arg(i));
                }
            } else {
                for (const auto& val : row) {
                    headers.append(val);
                }
            }
        } else {
            preview_data.push_back(row);
        }

        line_count++;
    }

    file.close();

    HorizonLoadDialog dialog(this);
    dialog.setSourceFile(path);
    dialog.setColumnHeaders(headers, preview_data);
    dialog.setMatchOptions(coord_match_nearest_, coord_match_x_range_, coord_match_y_range_);
    if (reader_->isOpen())
        dialog.setProfileCoordinates(trace_x_coords_, trace_y_coords_);

    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.useXYMode() && !reader_->isOpen()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("Load a SEG-Y file before importing horizon by X/Y coordinates."));
        return;
    }

    loadHorizonFile(path, dialog.useXYMode(), dialog.xColumn(), dialog.yColumn(),
                    dialog.traceColumn(), dialog.timeColumn(),
                    dialog.useNearestTraceMatch(), dialog.matchXRange(), dialog.matchYRange(),
                    dialog.timeInMilliseconds());
}

void MainWindow::loadHorizonFile(const QString& path, bool xy_mode, int x_col, int y_col,
                                 int trace_col, int time_col, bool nearest_match,
                                 double match_x_range, double match_y_range, bool time_in_ms)
{
    std::ifstream file(path.toStdString());
    if (!file.is_open()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file"));
        return;
    }

    horizon_points_.clear();

    if (xy_mode) {
        coord_match_nearest_ = nearest_match;
        if (!nearest_match) {
            coord_match_x_range_ = match_x_range;
            coord_match_y_range_ = match_y_range;
        }
    }

    std::string line;
    bool first_line = true;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string cell;
        std::vector<std::string> cells;

        while (std::getline(iss, cell, ',')) {
            cells.push_back(cell);
        }

        if (first_line) {
            bool all_numeric = true;
            for (const auto& c : cells) {
                try {
                    std::stod(c);
                } catch (...) {
                    all_numeric = false;
                    break;
                }
            }
            first_line = false;
            if (!all_numeric)
                continue;
        }

        try {
            double time_ms = 0.0;
            if (time_col < static_cast<int>(cells.size())) {
                time_ms = std::stod(cells[time_col]);
                if (!time_in_ms)
                    time_ms *= 1000.0;
            }

            double trace = 0.0;

            if (xy_mode) {
                double x = 0.0;
                double y = 0.0;
                if (x_col < static_cast<int>(cells.size()))
                    x = std::stod(cells[x_col]);
                if (y_col < static_cast<int>(cells.size()))
                    y = std::stod(cells[y_col]);

                int nearest_trace = findNearestTraceInRange(x, y);
                if (nearest_trace < 0)
                    continue;
                trace = static_cast<double>(nearest_trace);
            } else {
                if (trace_col < static_cast<int>(cells.size()))
                    trace = std::stod(cells[trace_col]);
            }

            horizon_points_.push_back({trace, time_ms});
        } catch (...) {
        }
    }

    std::sort(horizon_points_.begin(), horizon_points_.end(),
              [](const std::pair<double, double>& a, const std::pair<double, double>& b) {
                  return a.first < b.first;
              });

    current_horizon_path_ = path;
    updateFileCard(horizon_card_, horizon_name_label_,
                   tr("%1 · %2 pts").arg(QFileInfo(path).fileName()).arg(horizon_points_.size()),
                   true);

    seismic_view_->setHorizon(horizon_points_);

    statusBar()->showMessage(tr("Loaded %1 horizon points").arg(horizon_points_.size()), 3000);
    updateInfoMenuActions();
}

void MainWindow::onSaveSegy()
{
    if (!reader_->isOpen()) {
        QMessageBox::warning(this, tr("Warning"), tr("No SEG-Y file loaded"));
        return;
    }

    QString default_name = QFileInfo(current_segy_path_).baseName() + "_shifted.sgy";
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save SEG-Y file"),
        default_name,
        tr("SEG-Y files (*.sgy *.segy);;All files (*)"));

    if (path.isEmpty())
        return;

    try {
        SegyWriter writer;
        writer.open(path.toStdString());

        FileHeaders headers = reader_->fileHeaders();
        writer.writeHeaders(headers, metadata_.dt, metadata_.n_samples, metadata_.n_traces);

        // Записываем сдвинутые трассы
        for (int i = 0; i < static_cast<int>(shifted_traces_.size()); ++i) {
            writer.writeTrace(shifted_traces_[i], i, metadata_.n_samples, metadata_.dt);
        }

        writer.close();

        QMessageBox::information(this, tr("Success"),
                                  tr("File saved successfully:\n%1").arg(path));

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save SEG-Y file:\n%1").arg(e.what()));
    }
}

void MainWindow::onUpdateView()
{
    applyViewLimitsFromControls();
    applyStaticsAndUpdate();
}

void MainWindow::onModeChanged(int id)
{
    extra_shift_mode_ = id;
    applyStaticsAndUpdate();
}

void MainWindow::onPaletteChanged(int index)
{
    QString name = palette_combo_->itemText(index);
    current_palette_ = ColorSchemes::getSchemeFromName(name);
    seismic_view_->setColorMap(current_palette_);
}

void MainWindow::onClipChanged(double value)
{
    Q_UNUSED(value);
    applyStaticsAndUpdate();
}

void MainWindow::onExtraShiftChanged(double value)
{
    extra_shift_value_ = value;
    applyStaticsAndUpdate();
}

void MainWindow::applyStaticsAndUpdate()
{
    if (original_traces_.empty())
        return;

    // Применяем статические сдвиги
    shifted_traces_ = original_traces_;
    auto statics = computeInterpolatedStatics();
    std::vector<float> applied_shifts(metadata_.n_traces);

    if (extra_shift_mode_ != 0) {
        double mode_sign = (extra_shift_mode_ == 1) ? 1.0 : -1.0;

        for (int i = 0; i < metadata_.n_traces; ++i) {
            auto& trace = shifted_traces_[i];
            double shift = mode_sign * (static_cast<double>(statics[i]) + extra_shift_value_);

            if (std::abs(shift) > 1e-9) {
                std::vector<float> shifted_data(trace.data.size(), 0.0f);

                for (int s = 0; s < metadata_.n_samples; ++s) {
                    double t = s * metadata_.dt;
                    double t_shifted = t - shift;

                    int s0 = static_cast<int>(t_shifted / metadata_.dt);
                    float frac = static_cast<float>(t_shifted / metadata_.dt - s0);

                    if (s0 >= 0 && s0 < metadata_.n_samples - 1) {
                        shifted_data[s] = trace.data[s0] * (1.0f - frac) + trace.data[s0 + 1] * frac;
                    } else if (s0 == metadata_.n_samples - 1) {
                        shifted_data[s] = trace.data[s0];
                    } else {
                        shifted_data[s] = 0.0f;
                    }
                }

                trace.data = std::move(shifted_data);
            }
            applied_shifts[i] = static_cast<float>(shift);
        }
    } else {
        for (int i = 0; i < metadata_.n_traces; ++i) {
            applied_shifts[i] = statics[i];
        }
    }

    view_first_trace_ = std::clamp(view_first_trace_, 0, std::max(0, metadata_.n_traces - 1));
    view_first_sample_ = std::clamp(view_first_sample_, 0, std::max(0, metadata_.n_samples - 1));
    view_trace_count_ = std::clamp(view_trace_count_, 1, metadata_.n_traces);
    view_sample_count_ = std::clamp(view_sample_count_, 1, metadata_.n_samples);
    view_trace_count_ = std::min(view_trace_count_, metadata_.n_traces - view_first_trace_);
    view_sample_count_ = std::min(view_sample_count_, metadata_.n_samples - view_first_sample_);

    // Обновляем вид
    seismic_view_->setData(shifted_traces_, metadata_.dt);
    seismic_view_->setTraceWindow(view_first_trace_, view_trace_count_);
    seismic_view_->setTimeWindow(view_first_sample_, view_sample_count_);
    seismic_view_->setColorMap(current_palette_);
    float clip_setting = static_cast<float>(clip_spin_->value());
    if (clip_setting <= 0.0f)
        clip_setting = computeClipForTraces(shifted_traces_);
    seismic_view_->setClipValue(clip_setting);
    if (statics.empty())
        seismic_view_->clearStaticsCurve();
    else
        seismic_view_->setStaticsCurve(statics);

    // Обновляем статус
    status_label_->setText(tr("Traces %1–%2  ·  Time %3–%4 ms")
                           .arg(view_first_trace_)
                           .arg(view_first_trace_ + view_trace_count_ - 1)
                           .arg(static_cast<int>(view_first_sample_ * metadata_.dt * 1000))
                           .arg(static_cast<int>((view_first_sample_ + view_sample_count_) * metadata_.dt * 1000)));
    syncLimitControls();
    updateScrollbars();
}

void MainWindow::applyViewLimitsFromControls()
{
    if (!reader_ || !reader_->isOpen())
        return;

    int trace_min = trace_min_spin_->value();
    int trace_max = trace_max_spin_->value();
    if (trace_max < trace_min)
        std::swap(trace_min, trace_max);

    view_first_trace_ = std::clamp(trace_min, 0, std::max(0, metadata_.n_traces - 1));
    view_trace_count_ = std::clamp(trace_max - trace_min + 1, 1, metadata_.n_traces - view_first_trace_);

    if (metadata_.dt > 0) {
        int time_min_ms = time_min_spin_->value();
        int time_max_ms = time_max_spin_->value();
        if (time_max_ms < time_min_ms)
            std::swap(time_min_ms, time_max_ms);

        int sample_min = static_cast<int>(time_min_ms / (metadata_.dt * 1000.0));
        int sample_max = static_cast<int>(time_max_ms / (metadata_.dt * 1000.0));
        if (sample_max <= sample_min)
            sample_max = sample_min + 1;

        view_first_sample_ = std::clamp(sample_min, 0, std::max(0, metadata_.n_samples - 1));
        view_sample_count_ = std::clamp(sample_max - sample_min, 1, metadata_.n_samples - view_first_sample_);
    } else {
        view_first_sample_ = 0;
        view_sample_count_ = metadata_.n_samples;
    }
}

std::vector<float> MainWindow::computeInterpolatedStatics()
{
    std::vector<float> statics(metadata_.n_traces, 0.0f);
    std::vector<bool> known_mask(metadata_.n_traces, false);

    if (static_points_.empty())
        return statics;

    // Определяем режим по данным
    bool xy_mode = false;
    for (const auto& sp : static_points_) {
        if (sp.x != 0 || sp.y != 0) {
            xy_mode = true;
            break;
        }
    }

    if (xy_mode) {
        // XY режим: каждая точка статики назначается ближайшей трассе.
        std::vector<double> sums(metadata_.n_traces, 0.0);
        std::vector<int> counts(metadata_.n_traces, 0);

        for (const auto& sp : static_points_) {
            int nearest_trace = findNearestTraceInRange(sp.x, sp.y);
            if (nearest_trace < 0)
                continue;

            sums[nearest_trace] += sp.value;
            counts[nearest_trace] += 1;
        }

        for (int i = 0; i < metadata_.n_traces; ++i) {
            if (counts[i] > 0) {
                statics[i] = static_cast<float>(sums[i] / counts[i]);
                known_mask[i] = true;
            }
        }
    } else {
        // Trace номер режим
        std::vector<double> sums(metadata_.n_traces, 0.0);
        std::vector<int> counts(metadata_.n_traces, 0);

        for (const auto& sp : static_points_) {
            int trace_idx = static_cast<int>(std::lround(sp.trace_num));
            if (trace_idx >= 0 && trace_idx < metadata_.n_traces &&
                std::abs(sp.trace_num - trace_idx) < 0.5) {
                sums[trace_idx] += sp.value;
                counts[trace_idx] += 1;
            }
        }

        for (int i = 0; i < metadata_.n_traces; ++i) {
            if (counts[i] > 0) {
                statics[i] = static_cast<float>(sums[i] / counts[i]);
                known_mask[i] = true;
            }
        }
    }

    return smoothStatics(fillMissingStatics(statics, known_mask));
}

void MainWindow::updateSmoothStaticsControls()
{
    const bool enabled = smooth_statics_checkbox_ && smooth_statics_checkbox_->isChecked();
    if (smooth_statics_options_)
        smooth_statics_options_->setVisible(enabled);
}

std::vector<float> MainWindow::smoothStatics(const std::vector<float>& statics) const
{
    if (!smooth_statics_checkbox_ || !smooth_statics_checkbox_->isChecked() || statics.empty())
        return statics;

    const int filter_size = smooth_filter_size_spin_ ? smooth_filter_size_spin_->value() : 1;
    if (filter_size <= 1)
        return statics;

    const bool use_median = smooth_filter_type_combo_
        && smooth_filter_type_combo_->currentIndex() == 1;
    const int n = static_cast<int>(statics.size());
    const int left_half = filter_size / 2;
    const int right_half = filter_size - 1 - left_half;

    std::vector<float> result(statics.size());
    std::vector<float> window;
    window.reserve(static_cast<size_t>(filter_size));

    for (int i = 0; i < n; ++i) {
        const int start = std::max(0, i - left_half);
        const int end = std::min(n - 1, i + right_half);

        window.clear();
        for (int j = start; j <= end; ++j)
            window.push_back(statics[static_cast<size_t>(j)]);

        if (use_median) {
            std::sort(window.begin(), window.end());
            result[static_cast<size_t>(i)] = window[window.size() / 2];
        } else {
            float sum = 0.0f;
            for (float value : window)
                sum += value;
            result[static_cast<size_t>(i)] = sum / static_cast<float>(window.size());
        }
    }

    return result;
}

int MainWindow::findNearestTraceInRange(double x, double y) const
{
    if (metadata_.n_traces <= 0 || trace_x_coords_.empty() || trace_y_coords_.empty())
        return -1;

    int nearest_trace = -1;
    double min_dist_sq = std::numeric_limits<double>::max();

    for (int i = 0; i < metadata_.n_traces; ++i) {
        const double dx = std::abs(trace_x_coords_[i] - x);
        const double dy = std::abs(trace_y_coords_[i] - y);

        if (!coord_match_nearest_) {
            if (coord_match_x_range_ > 0.0 && dx > coord_match_x_range_)
                continue;
            if (coord_match_y_range_ > 0.0 && dy > coord_match_y_range_)
                continue;
        }

        const double dist_sq = dx * dx + dy * dy;
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            nearest_trace = i;
        }
    }

    return nearest_trace;
}

std::vector<float> MainWindow::fillMissingStatics(const std::vector<float>& statics,
                                                  const std::vector<bool>& known_mask) const
{
    std::vector<float> result = statics;
    if (statics.empty() || known_mask.empty())
        return result;

    const bool interpolate = !interpolate_checkbox_ || interpolate_checkbox_->isChecked();
    const bool extrapolate = !extrapolate_checkbox_ || extrapolate_checkbox_->isChecked();

    int first_known = -1;
    int last_known = -1;
    for (int i = 0; i < metadata_.n_traces; ++i) {
        if (known_mask[i]) {
            if (first_known < 0)
                first_known = i;
            last_known = i;
        }
    }

    if (first_known < 0)
        return result;

    if (extrapolate) {
        for (int i = 0; i < first_known; ++i) {
            result[i] = statics[first_known];
        }

        for (int i = last_known + 1; i < metadata_.n_traces; ++i) {
            result[i] = statics[last_known];
        }
    }

    if (!interpolate)
        return result;

    int last_valid = first_known;
    for (int i = first_known + 1; i <= last_known; ++i) {
        if (!known_mask[i])
            continue;

        for (int j = last_valid + 1; j < i; ++j) {
            float t = static_cast<float>(j - last_valid) / static_cast<float>(i - last_valid);
            result[j] = statics[last_valid] * (1.0f - t) + statics[i] * t;
        }
        last_valid = i;
    }

    return result;
}

void MainWindow::onPanRequested(int delta_traces, int delta_samples)
{
    if (!reader_->isOpen())
        return;

    view_first_trace_ = std::clamp(view_first_trace_ + delta_traces, 0,
                                  metadata_.n_traces - view_trace_count_);
    view_first_sample_ = std::clamp(view_first_sample_ + delta_samples, 0,
                                    metadata_.n_samples - view_sample_count_);

    // Обновляем скроллбары без сигнала
    h_scrollbar_->blockSignals(true);
    h_scrollbar_->setValue(view_first_trace_);
    h_scrollbar_->blockSignals(false);

    v_scrollbar_->blockSignals(true);
    v_scrollbar_->setValue(view_first_sample_);
    v_scrollbar_->blockSignals(false);

    applyStaticsAndUpdate();
}

void MainWindow::onTraceZoomRequested(int first_trace, int count)
{
    if (!reader_->isOpen())
        return;

    view_first_trace_ = std::clamp(first_trace, 0, metadata_.n_traces - 1);
    view_trace_count_ = std::clamp(count, 1, metadata_.n_traces - view_first_trace_);

    // Обновляем скроллбары
    h_scrollbar_->setRange(0, metadata_.n_traces - view_trace_count_);
    h_scrollbar_->setPageStep(view_trace_count_);
    h_scrollbar_->blockSignals(true);
    h_scrollbar_->setValue(view_first_trace_);
    h_scrollbar_->blockSignals(false);

    applyStaticsAndUpdate();
}

void MainWindow::onTimeZoomRequested(int first_sample, int count)
{
    if (!reader_->isOpen())
        return;

    view_first_sample_ = std::clamp(first_sample, 0, metadata_.n_samples - 1);
    view_sample_count_ = std::clamp(count, 1, metadata_.n_samples - view_first_sample_);

    // Обновляем скроллбары
    v_scrollbar_->setRange(0, metadata_.n_samples - view_sample_count_);
    v_scrollbar_->setPageStep(view_sample_count_);
    v_scrollbar_->blockSignals(true);
    v_scrollbar_->setValue(view_first_sample_);
    v_scrollbar_->blockSignals(false);

    applyStaticsAndUpdate();
}

void MainWindow::onResetZoomRequested()
{
    if (!reader_->isOpen())
        return;

    view_first_trace_ = 0;
    view_trace_count_ = metadata_.n_traces;
    view_first_sample_ = 0;
    view_sample_count_ = metadata_.n_samples;

    // Сбрасываем поля лимитов
    trace_min_spin_->setValue(0);
    trace_max_spin_->setValue(metadata_.n_traces - 1);

    if (metadata_.dt > 0) {
        time_min_spin_->setValue(0);
        time_max_spin_->setValue(static_cast<int>(metadata_.n_samples * metadata_.dt * 1000));
    }

    applyStaticsAndUpdate();
}

void MainWindow::onHScrollChanged(int value)
{
    view_first_trace_ = value;
    applyStaticsAndUpdate();
}

void MainWindow::onVScrollChanged(int value)
{
    view_first_sample_ = value;
    applyStaticsAndUpdate();
}

void MainWindow::onHoverInfoChanged(int trace_index, double time_ms, float amplitude, bool valid)
{
    if (valid) {
        statusBar()->showMessage(tr("Trace: %1 | Time: %2 ms | Amplitude: %3")
                                 .arg(trace_index)
                                 .arg(time_ms, 0, 'f', 2)
                                 .arg(amplitude, 0, 'g', 4));
    } else {
        statusBar()->clearMessage();
    }
}

float MainWindow::computeClipForTraces(const std::vector<Trace>& traces) const
{
    std::vector<float> amps;
    amps.reserve(traces.size() * 10);
    for (const auto& trace : traces) {
        for (float v : trace.data) {
            amps.push_back(std::abs(v));
        }
    }
    if (amps.empty())
        return 1.0f;

    size_t k = static_cast<size_t>(std::floor(0.98 * (amps.size() - 1)));
    std::nth_element(amps.begin(), amps.begin() + k, amps.end());
    float val = amps[k];
    if (val <= 0.0f)
        val = *std::max_element(amps.begin(), amps.end());
    return std::max(0.001f, val);
}

void MainWindow::updateScrollbars()
{
    if (!reader_ || !reader_->isOpen())
        return;

    const bool show_h = metadata_.n_traces > view_trace_count_;
    const bool show_v = metadata_.n_samples > view_sample_count_;

    h_scrollbar_->blockSignals(true);
    h_scrollbar_->setRange(0, std::max(0, metadata_.n_traces - view_trace_count_));
    h_scrollbar_->setPageStep(view_trace_count_);
    h_scrollbar_->setEnabled(show_h);
    h_scrollbar_->setVisible(show_h);
    h_scrollbar_->setValue(std::clamp(view_first_trace_, 0,
                                      std::max(0, metadata_.n_traces - view_trace_count_)));
    h_scrollbar_->blockSignals(false);

    v_scrollbar_->blockSignals(true);
    v_scrollbar_->setRange(0, std::max(0, metadata_.n_samples - view_sample_count_));
    v_scrollbar_->setPageStep(view_sample_count_);
    v_scrollbar_->setEnabled(show_v);
    v_scrollbar_->setVisible(show_v);
    v_scrollbar_->setValue(std::clamp(view_first_sample_, 0,
                                      std::max(0, metadata_.n_samples - view_sample_count_)));
    v_scrollbar_->blockSignals(false);
}

void MainWindow::syncLimitControls()
{
    if (!reader_ || !reader_->isOpen())
        return;

    trace_min_spin_->blockSignals(true);
    trace_max_spin_->blockSignals(true);
    int min_trace = std::clamp(view_first_trace_, 0, std::max(0, metadata_.n_traces - 1));
    int max_trace = std::clamp(view_first_trace_ + view_trace_count_ - 1, 0, std::max(0, metadata_.n_traces - 1));
    trace_min_spin_->setValue(min_trace);
    trace_max_spin_->setValue(max_trace);
    trace_min_spin_->blockSignals(false);
    trace_max_spin_->blockSignals(false);

    if (metadata_.dt > 0 && metadata_.n_samples > 0) {
        int min_ms = static_cast<int>(view_first_sample_ * metadata_.dt * 1000);
        int max_ms = static_cast<int>((view_first_sample_ + view_sample_count_) * metadata_.dt * 1000);
        time_min_spin_->blockSignals(true);
        time_max_spin_->blockSignals(true);
        time_min_spin_->setValue(std::clamp(min_ms, time_min_spin_->minimum(), time_min_spin_->maximum()));
        time_max_spin_->setValue(std::clamp(max_ms, time_max_spin_->minimum(), time_max_spin_->maximum()));
        time_min_spin_->blockSignals(false);
        time_max_spin_->blockSignals(false);
    }
}
