#ifndef SEGY_LOAD_DIALOG_HPP
#define SEGY_LOAD_DIALOG_HPP

#include <QDialog>
#include <QString>
#include <vector>

class QRadioButton;
class QPushButton;
class QLabel;
class QWidget;

/// Диалог настроек загрузки SEG-Y: выбор пары байтов координат и превью.
class SegyLoadDialog : public QDialog {
    Q_OBJECT

public:
    explicit SegyLoadDialog(QWidget* parent = nullptr);

    /// Прочитать файл, оценить пары байтов и выбрать первую валидную.
    void analyzeFile(const QString& path);

    int xByte() const;
    int yByte() const;

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onSelectionChanged();

private:
    struct CoordPairOption {
        int x_byte = 0;
        int y_byte = 0;
        QString title;
        std::vector<double> xs;
        std::vector<double> ys;
        bool valid = false;
        QString range_text;
    };

    void setupUi();
    void updatePreviewMap();

    QRadioButton* option_radios_[3] = {nullptr, nullptr, nullptr};
    QLabel* option_labels_[3] = {nullptr, nullptr, nullptr};
    QWidget* preview_map_ = nullptr;
    QPushButton* ok_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;

    std::vector<CoordPairOption> options_;
    int selected_index_ = 0;
    int x_byte_ = 181;
    int y_byte_ = 185;
};

#endif // SEGY_LOAD_DIALOG_HPP
