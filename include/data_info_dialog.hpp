#ifndef DATA_INFO_DIALOG_HPP
#define DATA_INFO_DIALOG_HPP

#include <QString>
#include <vector>

#include "types.hpp"

class QWidget;

namespace DataInfoDialog {

void showSegyProfile(QWidget* parent,
                     const QString& file_path,
                     const StreamMetadata& metadata,
                     int x_byte,
                     int y_byte,
                     const std::vector<double>& trace_x,
                     const std::vector<double>& trace_y);

void showStatics(QWidget* parent,
                 const QString& file_path,
                 const std::vector<StaticPoint>& points,
                 const std::vector<double>& profile_x,
                 const std::vector<double>& profile_y,
                 int n_traces);

void showHorizon(QWidget* parent,
                 const QString& file_path,
                 const std::vector<std::pair<double, double>>& points,
                 const std::vector<double>& profile_x,
                 const std::vector<double>& profile_y,
                 int n_traces);

} // namespace DataInfoDialog

#endif // DATA_INFO_DIALOG_HPP
