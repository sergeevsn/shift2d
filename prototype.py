import csv
import os
import sys
from typing import Optional, Tuple

import numpy as np
import segyio
from scipy.spatial import cKDTree
from scipy import interpolate

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QApplication,
    QButtonGroup,
    QComboBox,
    QDoubleSpinBox,
    QFileDialog,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QRadioButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from matplotlib.figure import Figure


class StaticsInteractiveApp(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Interactive SEGY statics viewer")
        self.resize(1500, 1000)

        self.segy_path: Optional[str] = None
        self.statics_path: Optional[str] = None
        self.horizon_path: Optional[str] = None

        self.segy_data: Optional[np.ndarray] = None
        self.segy_dt_sec: float = 0.0
        self.segy_time_axis: Optional[np.ndarray] = None
        self.segy_trace_coords: Optional[np.ndarray] = None
        self.segy_trace_numbers: Optional[np.ndarray] = None
        self.segy_n_traces: int = 0
        self.segy_n_samples: int = 0

        self.static_points: Optional[np.ndarray] = None
        self.static_values: Optional[np.ndarray] = None
        self.interpolated_statics: Optional[np.ndarray] = None
        self.match_mask: Optional[np.ndarray] = None
        self.static_location_mode: str = "xy"
        self.csv_headers: Optional[list] = None
        self.csv_rows: Optional[list] = None

        self.horizon_trace_values: Optional[np.ndarray] = None
        self.horizon_time_values: Optional[np.ndarray] = None

        self._build_ui()
        self._connect_signals()
        self._update_column_visibility()
        self._update_preview()

    def _build_ui(self) -> None:
        central = QWidget(self)
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        top = QHBoxLayout()
        root.addLayout(top)

        left_panel = QVBoxLayout()
        top.addLayout(left_panel, 1)

        right_panel = QVBoxLayout()
        top.addLayout(right_panel, 3)

        left_panel.addWidget(self._create_file_group())
        left_panel.addWidget(self._create_segy_group())
        left_panel.addWidget(self._create_statics_group())
        left_panel.addWidget(self._create_horizon_group())

        right_panel.addWidget(self._create_preview_group())

        self.status_label = QLabel("No data loaded")
        self.status_label.setWordWrap(True)
        root.addWidget(self.status_label)

    def _create_file_group(self) -> QGroupBox:
        group = QGroupBox("Files")
        layout = QVBoxLayout(group)

        self.segy_path_label = QLabel("SEGY: not selected")
        self.static_path_label = QLabel("Statics CSV: not selected")
        self.horizon_path_label = QLabel("Horizon CSV: not selected")

        open_segy_btn = QPushButton("Open SEGY")
        open_segy_btn.clicked.connect(self._open_segy_file)
        open_static_btn = QPushButton("Open statics CSV")
        open_static_btn.clicked.connect(self._open_statics_file)
        open_horizon_btn = QPushButton("Open horizon CSV")
        open_horizon_btn.clicked.connect(self._open_horizon_file)
        save_btn = QPushButton("Save current view as SEGY")
        save_btn.clicked.connect(self._save_current_section)

        layout.addWidget(self.segy_path_label)
        layout.addWidget(open_segy_btn)
        layout.addWidget(self.static_path_label)
        layout.addWidget(open_static_btn)
        layout.addWidget(self.horizon_path_label)
        layout.addWidget(open_horizon_btn)
        layout.addWidget(save_btn)
        return group

    def _create_segy_group(self) -> QGroupBox:
        group = QGroupBox("SEGY coordinates")
        form = QFormLayout(group)

        self.x_byte_spin = QSpinBox()
        self.x_byte_spin.setRange(0, 4000)
        self.x_byte_spin.setValue(181)
        self.y_byte_spin = QSpinBox()
        self.y_byte_spin.setRange(0, 4000)
        self.y_byte_spin.setValue(185)

        form.addRow("X header byte", self.x_byte_spin)
        form.addRow("Y header byte", self.y_byte_spin)
        return group

    def _create_statics_group(self) -> QGroupBox:
        group = QGroupBox("Statics CSV")
        layout = QVBoxLayout(group)

        self.location_mode_box = QComboBox()
        self.location_mode_box.addItems(["X/Y", "Trace Number"])

        self.x_col_box = QComboBox()
        self.y_col_box = QComboBox()
        self.trace_col_box = QComboBox()
        self.static_col_box = QComboBox()

        form = QFormLayout()
        form.addRow("Location key(s)", self.location_mode_box)
        form.addRow("X column", self.x_col_box)
        form.addRow("Y column", self.y_col_box)
        form.addRow("Trace Number column", self.trace_col_box)
        form.addRow("Static column", self.static_col_box)
        layout.addLayout(form)

        reload_btn = QPushButton("Load statics")
        reload_btn.clicked.connect(self._load_statics_from_csv)
        layout.addWidget(reload_btn)
        return group

    def _create_horizon_group(self) -> QGroupBox:
        group = QGroupBox("Horizon CSV")
        layout = QVBoxLayout(group)

        self.horizon_trace_col_box = QComboBox()
        self.horizon_time_col_box = QComboBox()

        form = QFormLayout()
        form.addRow("Trace column", self.horizon_trace_col_box)
        form.addRow("Time column", self.horizon_time_col_box)
        layout.addLayout(form)

        load_btn = QPushButton("Load horizon")
        load_btn.clicked.connect(self._load_horizon_from_csv)
        layout.addWidget(load_btn)
        return group

    def _create_preview_group(self) -> QGroupBox:
        group = QGroupBox("Preview")
        layout = QVBoxLayout(group)

        mode_box = QGroupBox("Static mode")
        mode_layout = QHBoxLayout(mode_box)
        self.mode_group = QButtonGroup(self)
        self.mode_none = QRadioButton("No statics")
        self.mode_forward = QRadioButton("Forward statics")
        self.mode_inverse = QRadioButton("Inverse statics")
        self.mode_none.setChecked(True)
        self.mode_group.addButton(self.mode_none, 0)
        self.mode_group.addButton(self.mode_forward, 1)
        self.mode_group.addButton(self.mode_inverse, 2)
        mode_layout.addWidget(self.mode_none)
        mode_layout.addWidget(self.mode_forward)
        mode_layout.addWidget(self.mode_inverse)
        layout.addWidget(mode_box)

        shift_form = QFormLayout()
        self.extra_shift_spin = QDoubleSpinBox()
        self.extra_shift_spin.setRange(-10.0, 10.0)
        self.extra_shift_spin.setDecimals(4)
        self.extra_shift_spin.setSingleStep(0.001)
        self.extra_shift_spin.setValue(0.0)
        shift_form.addRow("Extra shift, s", self.extra_shift_spin)
        layout.addLayout(shift_form)

        buttons = QHBoxLayout()
        update_btn = QPushButton("Refresh preview")
        update_btn.clicked.connect(self._update_preview)
        buttons.addWidget(update_btn)
        layout.addLayout(buttons)

        self.figure = Figure(figsize=(10, 8), dpi=100)
        self.figure.patch.set_facecolor("white")
        self.canvas = FigureCanvasQTAgg(self.figure)
        self.canvas.setFocusPolicy(Qt.StrongFocus)
        layout.addWidget(self.canvas, 1)
        return group

    def _connect_signals(self) -> None:
        self.location_mode_box.currentIndexChanged.connect(self._update_column_visibility)
        self.x_byte_spin.valueChanged.connect(self._update_preview)
        self.y_byte_spin.valueChanged.connect(self._update_preview)
        self.mode_group.idClicked.connect(self._update_preview)
        self.extra_shift_spin.valueChanged.connect(self._update_preview)

    def _update_column_visibility(self) -> None:
        mode = self.location_mode_box.currentText()
        self.x_col_box.setVisible(mode == "X/Y")
        self.y_col_box.setVisible(mode == "X/Y")
        self.trace_col_box.setVisible(mode == "Trace Number")

    def _open_segy_file(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Open SEGY", os.getcwd(), "SEGY files (*.sgy *.segy)")
        if not path:
            return
        self.segy_path = path
        self.segy_path_label.setText(f"SEGY: {os.path.basename(path)}")
        self._load_segy(path)

    def _load_segy(self, path: str) -> None:
        try:
            with segyio.open(path, "r", ignore_geometry=True) as src:
                ns = src.samples.size
                dt_micro = int(src.bin[segyio.BinField.Interval])
                dt_sec = dt_micro * 1e-6
                n_traces = src.tracecount
                self.segy_n_traces = n_traces
                self.segy_n_samples = ns
                self.segy_dt_sec = dt_sec
                self.segy_time_axis = np.arange(ns, dtype=np.float64) * dt_sec

                data = np.zeros((n_traces, ns), dtype=np.float32)
                coords = np.zeros((n_traces, 2), dtype=np.float64)
                trace_ids = np.arange(1, n_traces + 1, dtype=np.int64)

                for i in range(n_traces):
                    data[i, :] = src.trace[i].astype(np.float32)
                    coords[i, 0] = float(src.header[i][self.x_byte_spin.value()])
                    coords[i, 1] = float(src.header[i][self.y_byte_spin.value()])

                self.segy_data = data
                self.segy_trace_coords = coords
                self.segy_trace_numbers = trace_ids
        except Exception as exc:
            QMessageBox.critical(self, "SEGY load failed", str(exc))
            return

        self._update_preview()
        self._update_status()

    def _open_statics_file(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Open statics CSV", os.getcwd(), "CSV files (*.csv *.txt), All files (*)")
        if not path:
            return
        self.statics_path = path
        self.static_path_label.setText(f"Statics CSV: {os.path.basename(path)}")
        self._load_statics_columns(path)

    def _load_statics_columns(self, path: str) -> None:
        try:
            with open(path, "r", encoding="utf-8-sig", newline="") as handle:
                rows = list(csv.reader(handle))
        except Exception as exc:
            QMessageBox.critical(self, "CSV load failed", str(exc))
            return

        self.csv_rows = rows
        self.csv_headers = None
        if rows:
            first_row = rows[0]
            if any((cell or "").strip() and not self._is_numeric((cell or "").strip()) for cell in first_row):
                self.csv_headers = [cell.strip() for cell in first_row]
                data_rows = rows[1:]
            else:
                data_rows = rows
            self.csv_rows = data_rows

        self._populate_column_boxes(self.csv_headers)

    def _populate_column_boxes(self, headers: Optional[list]) -> None:
        columns = self._build_column_options(headers)
        for combo in [self.x_col_box, self.y_col_box, self.trace_col_box, self.static_col_box, self.horizon_trace_col_box, self.horizon_time_col_box]:
            combo.blockSignals(True)
            combo.clear()
            for idx, label in columns:
                combo.addItem(label, idx)
            combo.blockSignals(False)

        if self.static_col_box.count() > 0:
            self.static_col_box.setCurrentIndex(min(0, self.static_col_box.count() - 1))
        if self.x_col_box.count() > 0:
            self.x_col_box.setCurrentIndex(min(0, self.x_col_box.count() - 1))
        if self.y_col_box.count() > 0:
            self.y_col_box.setCurrentIndex(min(0, self.y_col_box.count() - 1))
        if self.trace_col_box.count() > 0:
            self.trace_col_box.setCurrentIndex(min(0, self.trace_col_box.count() - 1))
        if self.horizon_trace_col_box.count() > 0:
            self.horizon_trace_col_box.setCurrentIndex(min(0, self.horizon_trace_col_box.count() - 1))
        if self.horizon_time_col_box.count() > 0:
            self.horizon_time_col_box.setCurrentIndex(min(0, self.horizon_time_col_box.count() - 1))

    def _build_column_options(self, headers: Optional[list]) -> list:
        if not headers:
            return [(idx, f"Column {idx}") for idx in range(10)]
        return [(idx, headers[idx] if idx < len(headers) else f"Column {idx}") for idx in range(len(headers))]

    def _load_statics_from_csv(self) -> None:
        if not self.statics_path:
            QMessageBox.information(self, "Statics CSV", "Select a statics CSV file first")
            return
        try:
            rows = self.csv_rows or []
            if not rows:
                raise ValueError("CSV is empty")
            if self.location_mode_box.currentText() == "X/Y":
                x_idx = int(self.x_col_box.currentData())
                y_idx = int(self.y_col_box.currentData())
                static_idx = int(self.static_col_box.currentData())
                x_vals = []
                y_vals = []
                stat_vals = []
                for row in rows:
                    if x_idx >= len(row) or y_idx >= len(row) or static_idx >= len(row):
                        continue
                    try:
                        x_vals.append(float(row[x_idx].strip()))
                        y_vals.append(float(row[y_idx].strip()))
                        stat_vals.append(float(row[static_idx].strip()))
                    except ValueError:
                        continue
                if not x_vals:
                    raise ValueError("No valid X/Y/static rows were found")
                self.static_points = np.column_stack((np.asarray(x_vals, dtype=np.float64), np.asarray(y_vals, dtype=np.float64)))
                self.static_values = np.asarray(stat_vals, dtype=np.float64)
                self.static_location_mode = "xy"
            else:
                trace_idx = int(self.trace_col_box.currentData())
                static_idx = int(self.static_col_box.currentData())
                trace_vals = []
                stat_vals = []
                for row in rows:
                    if trace_idx >= len(row) or static_idx >= len(row):
                        continue
                    try:
                        trace_vals.append(float(row[trace_idx].strip()))
                        stat_vals.append(float(row[static_idx].strip()))
                    except ValueError:
                        continue
                if not trace_vals:
                    raise ValueError("No valid trace/static rows were found")
                self.static_points = np.asarray(trace_vals, dtype=np.float64)
                self.static_values = np.asarray(stat_vals, dtype=np.float64)
                self.static_location_mode = "trace"
            self._update_preview()
            self._update_status()
        except Exception as exc:
            QMessageBox.critical(self, "Statics load failed", str(exc))

    def _open_horizon_file(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Open horizon CSV", os.getcwd(), "CSV files (*.csv *.txt)")
        if not path:
            return
        self.horizon_path = path
        self.horizon_path_label.setText(f"Horizon CSV: {os.path.basename(path)}")
        self._load_horizon_from_csv(path)

    def _load_horizon_from_csv(self, path: Optional[str] = None) -> None:
        if path is None:
            path = self.horizon_path
        if not path:
            QMessageBox.information(self, "Horizon CSV", "Select a horizon CSV first")
            return
        try:
            with open(path, "r", encoding="utf-8-sig", newline="") as handle:
                rows = list(csv.reader(handle))
        except Exception as exc:
            QMessageBox.critical(self, "Horizon load failed", str(exc))
            return

        headers = None
        data_rows = rows
        if rows:
            first_row = rows[0]
            if any((cell or "").strip() and not self._is_numeric((cell or "").strip()) for cell in first_row):
                headers = [cell.strip() for cell in first_row]
                data_rows = rows[1:]
                self._populate_column_boxes(headers)

        if not data_rows:
            self.horizon_trace_values = None
            self.horizon_time_values = None
            self._update_preview()
            return

        trace_idx = int(self.horizon_trace_col_box.currentData()) if self.horizon_trace_col_box.count() > 0 else 0
        time_idx = int(self.horizon_time_col_box.currentData()) if self.horizon_time_col_box.count() > 0 else 1
        trace_vals = []
        time_vals = []
        for row in data_rows:
            if trace_idx >= len(row) or time_idx >= len(row):
                continue
            try:
                trace_vals.append(float(row[trace_idx].strip()))
                time_vals.append(float(row[time_idx].strip()))
            except ValueError:
                continue
        if not trace_vals:
            self.horizon_trace_values = None
            self.horizon_time_values = None
        else:
            self.horizon_trace_values = np.asarray(trace_vals, dtype=np.float64)
            self.horizon_time_values = np.asarray(time_vals, dtype=np.float64)
        self._update_preview()
        self._update_status()

    def _update_preview(self) -> None:
        if self.segy_data is None or self.segy_time_axis is None:
            return

        if self.segy_data.shape[0] == 0:
            return

        section = self.segy_data.copy()
        applied_statics = np.zeros(self.segy_n_traces, dtype=np.float64)
        self.interpolated_statics = np.zeros(self.segy_n_traces, dtype=np.float64)
        self.match_mask = np.zeros(self.segy_n_traces, dtype=bool)

        if self.static_values is not None and self.static_points is not None and self.segy_trace_coords is not None:
            if self.static_location_mode == "xy":
                scale = np.array([5.0, 5.0], dtype=np.float64)
                tree = cKDTree(self.static_points / scale)
                dists, idxs = tree.query(self.segy_trace_coords / scale, p=np.inf)
                statics = np.where(dists <= 1.0, self.static_values[idxs], 0.0)
                self.match_mask = statics != 0.0
                self.interpolated_statics = self._interpolate_statics(statics)
            else:
                trace_ids = np.asarray(self.segy_trace_numbers, dtype=np.float64)
                values = np.asarray(self.static_points, dtype=np.float64)
                mapping = {float(v): float(s) for v, s in zip(values, self.static_values)}
                statics = np.zeros(self.segy_n_traces, dtype=np.float64)
                for idx, trace_id in enumerate(trace_ids):
                    if float(trace_id) in mapping:
                        statics[idx] = mapping[float(trace_id)]
                self.match_mask = statics != 0.0
                self.interpolated_statics = self._interpolate_statics(statics)

            mode_sign = 0.0
            if self.mode_forward.isChecked():
                mode_sign = 1.0
            elif self.mode_inverse.isChecked():
                mode_sign = -1.0
            applied_statics = mode_sign * self.interpolated_statics + self.extra_shift_spin.value()

            for i in range(self.segy_n_traces):
                static = applied_statics[i]
                if static != 0.0:
                    shifted = np.interp(self.segy_time_axis - static, self.segy_time_axis, section[i, :], left=0.0, right=0.0)
                    section[i, :] = shifted.astype(np.float32)

        self._draw_section(section, applied_statics)

    def _draw_section(self, section: np.ndarray, applied_statics: np.ndarray) -> None:
        self.figure.clear()
        gs = self.figure.add_gridspec(2, 1, height_ratios=[3, 1], hspace=0.2)
        ax1 = self.figure.add_subplot(gs[0])
        ax2 = self.figure.add_subplot(gs[1], sharex=ax1)

        vmax = np.percentile(np.abs(section), 98)
        if vmax == 0:
            vmax = 1.0
        image = ax1.imshow(
            section.T,
            aspect="auto",
            cmap="seismic",
            vmin=-vmax,
            vmax=vmax,
            extent=[0, self.segy_n_traces - 1, self.segy_time_axis[-1] * 1000, 0],
            interpolation="nearest",
        )
        self.figure.colorbar(image, ax=ax1, shrink=0.9)
        ax1.set_ylabel("Time, ms")
        ax1.set_title("SEGY section")
        ax1.grid(alpha=0.2)

        if self.horizon_trace_values is not None and self.horizon_time_values is not None:
            horizon_time_ms = self.horizon_time_values * 1000.0
            ax1.plot(self.horizon_trace_values, horizon_time_ms, color="yellow", linewidth=1.5, alpha=0.9)

        trace_indices = np.arange(self.segy_n_traces)
        nonzero = applied_statics != 0.0
        if np.any(nonzero):
            ax2.plot(trace_indices[nonzero], applied_statics[nonzero] * 1000.0, color="steelblue", linewidth=1.2)
            ax2.scatter(trace_indices[nonzero], applied_statics[nonzero] * 1000.0, color="darkred", s=10)
        ax2.axhline(0.0, color="gray", linestyle="--", linewidth=1)
        ax2.set_xlabel("Trace")
        ax2.set_ylabel("Static, ms")
        ax2.grid(alpha=0.2)

        self.canvas.draw_idle()

    def _interpolate_statics(self, statics: np.ndarray) -> np.ndarray:
        n_traces = len(statics)
        trace_indices = np.arange(n_traces)
        valid_mask = statics != 0.0
        valid_indices = trace_indices[valid_mask]
        valid_values = statics[valid_mask]
        if len(valid_indices) == 0:
            return np.zeros(n_traces, dtype=np.float64)
        if len(valid_indices) == n_traces:
            return statics.copy()
        try:
            return np.interp(trace_indices, valid_indices, valid_values, left=valid_values[0], right=valid_values[-1])
        except Exception:
            return statics.copy()

    def _save_current_section(self) -> None:
        if self.segy_path is None or self.segy_data is None:
            QMessageBox.information(self, "Save SEGY", "Open a SEGY file first")
            return

        out_path, _ = QFileDialog.getSaveFileName(self, "Save SEGY", os.path.splitext(self.segy_path)[0] + "_processed.sgy", "SEGY files (*.sgy)")
        if not out_path:
            return

        try:
            with segyio.open(self.segy_path, "r", ignore_geometry=True) as src:
                ns = src.samples.size
                dt_micro = int(src.bin[segyio.BinField.Interval])
                format_code = src.bin[segyio.BinField.Format]
                n_traces = src.tracecount
                t_original = np.arange(ns, dtype=np.float64) * (dt_micro * 1e-6)

                spec = segyio.spec()
                spec.samples = np.arange(ns, dtype=np.int32)
                spec.format = format_code
                spec.tracecount = n_traces

                with segyio.create(out_path, spec) as dst:
                    dst.text[0] = src.text[0]
                    dst.bin = src.bin

                    applied_statics = np.zeros(n_traces, dtype=np.float64)
                    if self.static_values is not None and self.static_points is not None and self.segy_trace_coords is not None:
                        if self.static_location_mode == "xy":
                            scale = np.array([5.0, 5.0], dtype=np.float64)
                            tree = cKDTree(self.static_points / scale)
                            dists, idxs = tree.query(self.segy_trace_coords / scale, p=np.inf)
                            statics = np.where(dists <= 1.0, self.static_values[idxs], 0.0)
                            interpolated = self._interpolate_statics(statics)
                        else:
                            trace_ids = np.asarray(self.segy_trace_numbers, dtype=np.float64)
                            values = np.asarray(self.static_points, dtype=np.float64)
                            mapping = {float(v): float(s) for v, s in zip(values, self.static_values)}
                            statics = np.zeros(n_traces, dtype=np.float64)
                            for idx, trace_id in enumerate(trace_ids):
                                if float(trace_id) in mapping:
                                    statics[idx] = mapping[float(trace_id)]
                            interpolated = self._interpolate_statics(statics)
                        mode_sign = 0.0
                        if self.mode_forward.isChecked():
                            mode_sign = 1.0
                        elif self.mode_inverse.isChecked():
                            mode_sign = -1.0
                        applied_statics = mode_sign * interpolated + self.extra_shift_spin.value()

                    for i in range(n_traces):
                        trace = src.trace[i]
                        static = applied_statics[i]
                        if static != 0.0:
                            shifted = np.interp(t_original - static, t_original, trace, left=0.0, right=0.0)
                            trace_out = shifted.astype(np.float32)
                        else:
                            trace_out = trace.astype(np.float32)
                        dst.header[i] = src.header[i]
                        dst.trace[i] = trace_out

            QMessageBox.information(self, "Save SEGY", f"Saved to {out_path}")
        except Exception as exc:
            QMessageBox.critical(self, "Save SEGY failed", str(exc))

    def _update_status(self) -> None:
        parts = []
        if self.segy_path:
            parts.append(f"SEGY: {os.path.basename(self.segy_path)}")
        if self.statics_path:
            parts.append(f"Statics: {os.path.basename(self.statics_path)}")
        if self.horizon_path:
            parts.append(f"Horizon: {os.path.basename(self.horizon_path)}")
        if parts:
            self.status_label.setText(" | ".join(parts))
        else:
            self.status_label.setText("No data loaded")

    @staticmethod
    def _is_numeric(text: str) -> bool:
        try:
            float(text)
            return True
        except ValueError:
            return False


def main() -> None:
    app = QApplication(sys.argv)
    window = StaticsInteractiveApp()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
